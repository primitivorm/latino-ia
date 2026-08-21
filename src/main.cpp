#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

#include "analizador_semantico.h"
#include "ast_impresor.h"
#include "compiler.h"
#include "invocador_c.h"
#include "lexer.h"
#include "parser.h"

#ifdef LATINO_CON_LLVM
#include <llvm/IR/Module.h>

#include "compiler_llvm.h"
#include "invocador_llvm.h"
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// 17.3: Resolución de inclusiones .lat
// Expande recursivamente cada nodo Incluir cuyo módulo termina en ".lat",
// insertando las sentencias del archivo incluido en el lugar del nodo.
// Los archivos ya procesados se rastrean con 'visitados' para evitar ciclos.
// ---------------------------------------------------------------------------
static bool procesarInclusioneesLat(Programa& programa, const fs::path& dirBase,
                                    std::set<std::string>& visitados) {
    ListaSent nueva;
    bool ok = true;

    for (auto& s : programa.sentencias) {
        auto* inc = dynamic_cast<Incluir*>(s.get());
        if (!inc) {
            nueva.push_back(std::move(s));
            continue;
        }

        const std::string& mod = inc->modulo;
        // Solo expandir archivos .lat; las librerías estándar las maneja el compilador.
        bool esLat = mod.size() >= 4 && mod.substr(mod.size() - 4) == ".lat";
        if (!esLat) {
            nueva.push_back(std::move(s));
            continue;
        }

        fs::path ruta = dirBase / mod;
        std::string rutaStr = ruta.generic_string();

        if (visitados.count(rutaStr)) {
            // Inclusión circular o duplicada: ignorar silenciosamente.
            continue;
        }
        visitados.insert(rutaStr);

        std::ifstream archivo(ruta, std::ios::binary);
        if (!archivo) {
            std::cerr << "Error: no se pudo abrir el archivo incluido: " << ruta << std::endl;
            ok = false;
            continue;
        }
        std::stringstream ss;
        ss << archivo.rdbuf();
        std::string fuente = ss.str();

        Lexer lexerSub(fuente);
        Parser parserSub(lexerSub);
        auto subProg = parserSub.parse();
        if (!subProg) {
            std::cerr << "Error de sintaxis en el archivo incluido: " << ruta << std::endl;
            ok = false;
            continue;
        }

        // Procesar recursivamente los includes del archivo incluido.
        if (!procesarInclusioneesLat(*subProg, ruta.parent_path(), visitados))
            ok = false;

        // Insertar las sentencias del archivo incluido en lugar del nodo Incluir.
        for (auto& ss2 : subProg->sentencias)
            nueva.push_back(std::move(ss2));
    }

    programa.sentencias = std::move(nueva);
    return ok;
}

static void uso() {
    std::cerr <<
        "Uso: latino <archivo.lat> [opciones]\n"
        "  (por defecto)      compila el programa a un ejecutable\n"
        "  -o <ruta>          ruta de salida (ejecutable, o el .c con --solo-c)\n"
        "  --solo-c           emite el código C (a -o si se indica, si no a stdout)\n"
        "  --ast              vuelca el AST (depuración)\n"
        "  --runtime <dir>    carpeta del runtime (latino.h/latino.c)\n"
        "  --backend <c|llvm> backend de generación de código (por defecto: c)\n"
        "                     'llvm' requiere un build con LATINO_LLVM_BACKEND\n"
        "                     (ver input/PLAN_LLVM.md)\n";
}

int main(int argc, char** argv) {
    std::string ruta;
    std::string salida;
    std::string runtimeDir;
    std::string backend = "c";
    bool modoAst = false;
    bool soloC = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--ast") {
            modoAst = true;
        } else if (arg == "--solo-c") {
            soloC = true;
        } else if (arg == "-o") {
            if (i + 1 >= argc) { uso(); return 2; }
            salida = argv[++i];
        } else if (arg == "--runtime") {
            if (i + 1 >= argc) { uso(); return 2; }
            runtimeDir = argv[++i];
        } else if (arg == "--backend") {
            if (i + 1 >= argc) { uso(); return 2; }
            backend = argv[++i];
            if (backend != "c" && backend != "llvm") {
                std::cerr << "Backend desconocido: " << backend << " (use 'c' o 'llvm')\n";
                return 2;
            }
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Opción desconocida: " << arg << std::endl;
            uso();
            return 2;
        } else if (ruta.empty()) {
            ruta = arg;
        }
    }

    if (ruta.empty()) {
        std::cerr << "Falta el archivo de entrada.\n";
        uso();
        return 2;
    }

    std::ifstream archivo(ruta, std::ios::binary);
    if (!archivo) {
        std::cerr << "No se pudo abrir el archivo: " << ruta << std::endl;
        return 1;
    }
    std::stringstream ss;
    ss << archivo.rdbuf();
    std::string codigoFuente = ss.str();

    // Análisis léxico + sintáctico.
    Lexer lexer(codigoFuente);
    Parser parser(lexer);
    std::unique_ptr<Programa> programa = parser.parse();
    if (!programa)
        return 1;  // error de sintaxis (ya reportado)

    // 17.3: Expansión de archivos .lat antes del análisis semántico.
    {
        fs::path dirBase = fs::path(ruta).parent_path();
        std::set<std::string> visitados;
        visitados.insert(fs::absolute(ruta).generic_string());
        if (!procesarInclusioneesLat(*programa, dirBase, visitados))
            return 1;
    }

    // Análisis semántico.
    AnalizadorSemantico semantico;
    if (!semantico.analizar(*programa))
        return 1;  // errores semánticos (ya reportados)

    // Volcado del AST (depuración).
    if (modoAst) {
        ImpresorAST impresor(std::cout);
        impresor.imprimir(*programa);
        return 0;
    }

    if (backend == "llvm") {
#ifdef LATINO_CON_LLVM
        if (soloC) {
            std::cerr << "--solo-c no es válido con --backend llvm "
                         "(--solo-ir llega en la Fase L9 de PLAN_LLVM.md)\n";
            return 2;
        }
        if (salida.empty()) {
            fs::path p(ruta);
            salida = p.stem().string();
#ifdef _WIN32
            salida += ".exe";
#endif
        }
        std::string salidaAbs = fs::absolute(salida).string();

        // Fase L1 (ver input/PLAN_LLVM.md): GeneradorLLVM todavía no recorre
        // el AST real, solo valida el plumbing de compilación + enlace.
        GeneradorLLVM generadorLlvm;
        std::unique_ptr<llvm::Module> modulo = generadorLlvm.generar(*programa);
        if (!modulo) {
            std::cerr << "Error: el generador LLVM produjo un módulo inválido." << std::endl;
            return 1;
        }

        OpcionesLLVM opcLlvm;
        opcLlvm.runtimeDir = runtimeDir;
        int codigo = compilarLLVMAEjecutable(*modulo, salidaAbs, opcLlvm);
        if (codigo != 0)
            return 1;

        std::cerr << "Ejecutable generado (backend LLVM): " << salida << std::endl;
        return 0;
#else
        std::cerr << "Este build de latino no incluye el backend LLVM "
                     "(LATINO_LLVM_BACKEND estaba OFF al configurar CMake). "
                     "Ver input/PLAN_LLVM.md para instalar LLVM 17.x.\n";
        return 2;
#endif
    }

    // Generación de código C.
    GeneradorC generador;
    std::string codigoC = generador.generar(*programa);

    // --solo-c: emitir el C a un archivo (si se indicó -o) o por stdout.
    if (soloC) {
        if (salida.empty()) {
            std::cout << codigoC;
        } else {
            std::ofstream f(salida, std::ios::binary);
            if (!f) {
                std::cerr << "No se pudo escribir el archivo: " << salida << std::endl;
                return 1;
            }
            f << codigoC;
            std::cerr << "Código C generado: " << salida << std::endl;
        }
        return 0;
    }

    // Determinar la ruta del ejecutable de salida.
    if (salida.empty()) {
        fs::path p(ruta);
        salida = p.stem().string();
#ifdef _WIN32
        salida += ".exe";
#endif
    }

    // Escribir el C generado a un archivo temporal.
    fs::path archivoC = fs::temp_directory_path() / (fs::path(ruta).stem().string() + ".c");
    {
        std::ofstream f(archivoC, std::ios::binary);
        if (!f) {
            std::cerr << "No se pudo escribir el archivo temporal: " << archivoC << std::endl;
            return 1;
        }
        f << codigoC;
    }

    // Invocar el compilador de C para producir el ejecutable. Se usan rutas
    // absolutas porque el entorno del compilador puede cambiar el directorio.
    std::string salidaAbs = fs::absolute(salida).string();
    OpcionesC opc;
    opc.runtimeDir = runtimeDir;
    int codigo = compilarAEjecutable(archivoC.string(), salidaAbs, opc);
    if (codigo != 0)
        return 1;

    std::cerr << "Ejecutable generado: " << salida << std::endl;
    return 0;
}
