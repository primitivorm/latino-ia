#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "analizador_semantico.h"
#include "ast_impresor.h"
#include "compiler.h"
#include "invocador_c.h"
#include "lexer.h"
#include "parser.h"

namespace fs = std::filesystem;

static void uso() {
    std::cerr <<
        "Uso: latino <archivo.lat> [opciones]\n"
        "  (por defecto)      compila el programa a un ejecutable\n"
        "  -o <ruta>          ruta del ejecutable de salida\n"
        "  --solo-c           emite el código C por stdout y termina\n"
        "  --ast              vuelca el AST (depuración)\n"
        "  --runtime <dir>    carpeta del runtime (latino.h/latino.c)\n";
}

int main(int argc, char** argv) {
    std::string ruta;
    std::string salida;
    std::string runtimeDir;
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

    // Generación de código C.
    GeneradorC generador;
    std::string codigoC = generador.generar(*programa);

    // --solo-c: emitir el C por stdout y terminar.
    if (soloC) {
        std::cout << codigoC;
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
