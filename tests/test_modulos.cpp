// test_modulos.cpp
//
// Pruebas unitarias de ResolutorModulos (PLAN_MODULOS.md, M3: resolución de
// un solo módulo, sin "importar" todavía). Parsea fragmentos .lat, corre el
// resolutor y verifica el AST resultante volcándolo con ImpresorAST (mismo
// mecanismo que tests/test_parser.cpp), además de inspeccionar la tabla de
// exportación devuelta.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "ast.h"
#include "ast_impresor.h"
#include "lexer.h"
#include "parser.h"
#include "resolutor_modulos.h"

namespace fs = std::filesystem;

// --- Framework mínimo de aserciones ---------------------------------------
static int g_fallos = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_fallos;                                                        \
            std::cerr << "  FALLO [linea " << __LINE__ << "]: " << msg << "\n"; \
        }                                                                      \
    } while (0)

static std::unique_ptr<Programa> parsear(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer);
    return parser.parse();
}

static std::string volcar(Programa& prog) {
    std::ostringstream os;
    ImpresorAST imp(os);
    imp.imprimir(prog);
    return os.str();
}

static bool contiene(const std::string& t, const std::string& sub) {
    return t.find(sub) != std::string::npos;
}

// Ruta sintética usada en la mayoría de las pruebas: slug esperado
// "carpeta_geometria_lat" (cada carácter no alfanumérico -> '_').
static const std::string RUTA = "carpeta/geometria.lat";
static const std::string PREFIJO = "__mod_carpeta_geometria_lat__";

// ---------------------------------------------------------------------------

static void prueba_slug_desde_ruta() {
    CHECK(ResolutorModulos::slugDesdeRuta("carpeta/geometria.lat") == "carpeta_geometria_lat",
          "slug: separadores y punto a '_'");
    CHECK(ResolutorModulos::slugDesdeRuta("C:/x/y.lat") == "C__x_y_lat",
          "slug: dos separadores consecutivos -> dos '_'");
    CHECK(ResolutorModulos::slugDesdeRuta("abc123") == "abc123",
          "slug: alfanumérico se preserva tal cual");
}

static void prueba_participa_de_modulos() {
    auto sinNada = parsear("x = 1\nescribir(x)\n");
    CHECK(!ResolutorModulos::participaDeModulos(*sinNada), "script plano no participa");

    auto conExportar = parsear("exportar const PI = 3.14\n");
    CHECK(ResolutorModulos::participaDeModulos(*conExportar), "exportar activa participacion");

    auto conImportar = parsear("importar { a } desde \"otro.lat\"\n");
    CHECK(ResolutorModulos::participaDeModulos(*conImportar), "importar activa participacion");

    auto conReexport = parsear("exportar { a } desde \"otro.lat\"\n");
    CHECK(ResolutorModulos::participaDeModulos(*conReexport), "re-export activa participacion");
}

static void prueba_funcion_exportada_y_privada() {
    std::string src =
        "exportar funcion area_circulo(r)\n"
        "  retornar normalizar(r) * r\n"
        "fin\n"
        "funcion normalizar(r)\n"
        "  retornar r < 0 ? 0 : r\n"
        "fin\n";
    auto prog = parsear(src);
    auto tabla = ResolutorModulos::resolverModuloUnico(*prog, RUTA);

    CHECK(tabla.size() == 1, "tabla de exportacion: 1 entrada (solo la exportada)");
    CHECK(tabla.count("area_circulo") == 1, "tabla: 'area_circulo' exportada");
    CHECK(tabla["area_circulo"] == PREFIJO + "area_circulo", "tabla: nombre interno correcto");
    CHECK(tabla.count("normalizar") == 0, "tabla: 'normalizar' (privada) no aparece");

    std::string t = volcar(*prog);
    CHECK(contiene(t, "Funcion '" + PREFIJO + "area_circulo'"), "declaracion exportada renombrada");
    CHECK(contiene(t, "Funcion '" + PREFIJO + "normalizar'"), "declaracion privada tambien renombrada");
    CHECK(contiene(t, "Identificador '" + PREFIJO + "normalizar'"),
          "llamada interna a la funcion privada reescrita");
    CHECK(!contiene(t, "[exportado]"), "el flag 'exportado' se descarta tras resolver (paso 8)");
}

static void prueba_por_defecto_no_crea_binding_nombrado() {
    std::string src =
        "exportar por defecto funcion saludar(nombre)\n"
        "  retornar \"Hola, \" .. nombre\n"
        "fin\n";
    auto prog = parsear(src);
    auto tabla = ResolutorModulos::resolverModuloUnico(*prog, RUTA);

    CHECK(tabla.size() == 1, "solo la entrada __defecto__");
    CHECK(tabla.count("__defecto__") == 1, "tabla: clave __defecto__ presente");
    CHECK(tabla.count("saludar") == 0, "el nombre original no queda como export nombrado");
    CHECK(tabla["__defecto__"] == PREFIJO + "saludar", "__defecto__ apunta al nombre interno");
}

static void prueba_sombreado_de_parametro() {
    // El parámetro 'area_circulo' de 'usar' no debe reescribirse: sombrea al
    // nombre de nivel superior homónimo dentro de esa función. Fuera de ella
    // (en 'otra'), el mismo identificador SÍ es una llamada al de nivel
    // superior y debe reescribirse.
    std::string src =
        "exportar funcion area_circulo(r)\n"
        "  retornar r * r\n"
        "fin\n"
        "funcion usar(area_circulo)\n"
        "  retornar area_circulo + 1\n"
        "fin\n"
        "funcion otra()\n"
        "  retornar area_circulo(2)\n"
        "fin\n";
    auto prog = parsear(src);
    ResolutorModulos::resolverModuloUnico(*prog, RUTA);
    std::string t = volcar(*prog);

    CHECK(contiene(t, "Funcion '" + PREFIJO + "usar' (area_circulo)"),
          "el parametro homonimo conserva su nombre original en la firma");
    CHECK(contiene(t, "Identificador 'area_circulo'"),
          "dentro de 'usar', el uso del parametro no se reescribe (sombreado)");
    CHECK(contiene(t, "Funcion '" + PREFIJO + "otra'"), "otra funcion tambien renombrada");
    CHECK(contiene(t, "Identificador '" + PREFIJO + "area_circulo'"),
          "dentro de 'otra', la llamada al top-level SI se reescribe");
}

static void prueba_variable_local_shadow_por_asignacion() {
    // Sin 'var'/'const', cualquier asignacion dentro de una funcion declara
    // una local (no hay 'global' implementado, ver PLAN_MODULOS.md /
    // AnalizadorSemantico::analizarBloque). 'contador' local a 'f' sombrea
    // al 'contador' exportado de nivel superior en TODO el cuerpo de 'f',
    // incluida la lectura anterior a la asignacion (hoisting).
    std::string src =
        "exportar var contador = 0\n"
        "funcion f()\n"
        "  escribir(contador)\n"
        "  contador = 5\n"
        "fin\n";
    auto prog = parsear(src);
    ResolutorModulos::resolverModuloUnico(*prog, RUTA);
    std::string t = volcar(*prog);

    // 'f' también es un nombre de nivel superior, así que se mangla igual
    // que 'contador' (privacidad y mangling son independientes) -- lo que
    // esta prueba busca es que, DENTRO del cuerpo de 'f', no quede ninguna
    // referencia mangleada a 'contador' (solo la declaración de nivel
    // superior, antes de 'f', debe tenerla).
    size_t inicioF = t.find("Funcion '" + PREFIJO + "f'");
    CHECK(inicioF != std::string::npos, "'f' tambien fue renombrada");
    std::string cuerpoF = t.substr(inicioF);

    CHECK(contiene(cuerpoF, "Identificador 'contador'"),
          "la lectura de 'contador' dentro de 'f' queda sombreada (sin renombrar)");
    CHECK(!contiene(cuerpoF, "Identificador '" + PREFIJO + "contador'"),
          "no debe quedar ninguna referencia renombrada de 'contador' dentro de 'f'");
}

static void prueba_clase_herencia_nuevo_y_es() {
    std::string src =
        "exportar clase Figura\n"
        "  funcion area(): numero\n"
        "  fin\n"
        "fin\n"
        "exportar clase Circulo extiende Figura\n"
        "  publico r: numero\n"
        "  funcion Circulo(r: numero)\n"
        "    este.r = r\n"
        "  fin\n"
        "fin\n"
        "funcion crear(r)\n"
        "  c = nuevo Circulo(r)\n"
        "  si c es Figura\n"
        "    retornar c\n"
        "  fin\n"
        "  retornar nulo\n"
        "fin\n";
    auto prog = parsear(src);
    auto tabla = ResolutorModulos::resolverModuloUnico(*prog, RUTA);

    CHECK(tabla["Figura"] == PREFIJO + "Figura", "Figura exportada");
    CHECK(tabla["Circulo"] == PREFIJO + "Circulo", "Circulo exportada");

    std::string t = volcar(*prog);
    CHECK(contiene(t, "Clase '" + PREFIJO + "Circulo' extiende " + PREFIJO + "Figura"),
          "extiende reescrito al nombre interno del padre");
    CHECK(contiene(t, "Nuevo '" + PREFIJO + "Circulo'"), "nuevo Circulo reescrito");
    CHECK(contiene(t, "Es '" + PREFIJO + "Figura'"), "es Figura reescrito");
}

static void prueba_tipo_por_nombre_en_campo_y_retorno() {
    std::string src =
        "exportar estructura Punto\n"
        "  x: numero\n"
        "fin\n"
        "exportar clase Caja\n"
        "  publico p: Punto\n"
        "  funcion obtener(): Punto\n"
        "    retornar este.p\n"
        "  fin\n"
        "fin\n";
    auto prog = parsear(src);
    ResolutorModulos::resolverModuloUnico(*prog, RUTA);
    std::string t = volcar(*prog);

    CHECK(contiene(t, "Estructura '" + PREFIJO + "Punto'"), "estructura renombrada");
    CHECK(contiene(t, "Campo 'p' [publico]: " + PREFIJO + "Punto"),
          "campo tipado por nombre de clase reescrito");
    CHECK(contiene(t, "-> " + PREFIJO + "Punto"), "tipo de retorno por nombre de clase reescrito");
}

static void prueba_asignacion_multiple_top_level() {
    std::string src = "exportar a, b = 1, 2\n";
    auto prog = parsear(src);
    auto tabla = ResolutorModulos::resolverModuloUnico(*prog, RUTA);
    CHECK(tabla["a"] == PREFIJO + "a", "primer destino exportado");
    CHECK(tabla["b"] == PREFIJO + "b", "segundo destino exportado");
}

// ---------------------------------------------------------------------------
// M4 — resolución multi-módulo (importar { a, b como c } desde "ruta")
//
// resolverProyecto lee del disco cada módulo referenciado por "importar"
// (igual que 'incluir "archivo.lat"' hace hoy con procesarInclusioneesLat),
// así que estas pruebas escriben archivos .lat reales en un directorio
// temporal propio de la suite.
// ---------------------------------------------------------------------------

static fs::path dirTemporalM4() {
    fs::path dir = fs::temp_directory_path() / "latino_test_modulos_m4";
    fs::create_directories(dir);
    return dir;
}

static std::string escribirArchivoM4(const std::string& nombre, const std::string& contenido) {
    fs::path ruta = dirTemporalM4() / nombre;
    std::ofstream f(ruta, std::ios::binary);
    f << contenido;
    f.close();
    return ruta.generic_string();
}

static void prueba_import_nombrado_con_alias() {
    escribirArchivoM4("geometria_m4.lat",
        "exportar funcion area_circulo(r)\n"
        "  retornar r * r\n"
        "fin\n"
        "funcion privada_geo()\n"
        "  retornar 0\n"
        "fin\n");

    std::string rutaEntrada = escribirArchivoM4("main_m4.lat",
        "importar { area_circulo como area } desde \"geometria_m4.lat\"\n"
        "escribir(area(2))\n");

    auto entrada = parsear(
        "importar { area_circulo como area } desde \"geometria_m4.lat\"\n"
        "escribir(area(2))\n");
    auto resultado = ResolutorModulos::resolverProyecto(std::move(entrada), rutaEntrada);
    CHECK(resultado != nullptr, "import nombrado con alias entre dos archivos resuelve sin error");
    if (!resultado) return;

    std::string t = volcar(*resultado);
    CHECK(contiene(t, "area_circulo"), "la declaracion del modulo importado esta en el Programa final");
    CHECK(!contiene(t, "Identificador 'area'"),
          "el alias local 'area' quedo reescrito al nombre interno (no aparece sin reescribir)");
    CHECK(contiene(t, "privada_geo"),
          "la funcion privada del modulo importado tambien viaja al Programa final (mangleada)");
    CHECK(!contiene(t, "Importar"),
          "el nodo ImportarDecl ya resuelto no queda en el arbol final");
}

static void prueba_import_nombre_no_exportado() {
    escribirArchivoM4("geo_sin_export_m4.lat",
        "exportar funcion area_circulo(r)\n"
        "  retornar r * r\n"
        "fin\n"
        "funcion normalizar(r)\n"
        "  retornar r\n"
        "fin\n");

    std::string rutaEntrada = escribirArchivoM4("main_falla_m4.lat",
        "importar { normalizar } desde \"geo_sin_export_m4.lat\"\n"
        "escribir(normalizar(1))\n");

    auto entrada = parsear(
        "importar { normalizar } desde \"geo_sin_export_m4.lat\"\n"
        "escribir(normalizar(1))\n");

    std::ostringstream cap;
    std::streambuf* viejo = std::cerr.rdbuf(cap.rdbuf());
    auto resultado = ResolutorModulos::resolverProyecto(std::move(entrada), rutaEntrada);
    std::cerr.rdbuf(viejo);

    CHECK(resultado == nullptr, "importar un nombre no exportado falla");
    CHECK(contiene(cap.str(), "no exporta"), "el mensaje de error menciona 'no exporta'");
}

static void prueba_import_modulo_sin_exportar() {
    escribirArchivoM4("script_plano_m4.lat",
        "funcion f(x)\n"
        "  retornar x\n"
        "fin\n");

    std::string rutaEntrada = escribirArchivoM4("main_plano_m4.lat",
        "importar { f } desde \"script_plano_m4.lat\"\n"
        "escribir(f(1))\n");

    auto entrada = parsear(
        "importar { f } desde \"script_plano_m4.lat\"\n"
        "escribir(f(1))\n");

    std::ostringstream cap;
    std::streambuf* viejo = std::cerr.rdbuf(cap.rdbuf());
    auto resultado = ResolutorModulos::resolverProyecto(std::move(entrada), rutaEntrada);
    std::cerr.rdbuf(viejo);

    CHECK(resultado == nullptr, "importar un modulo que nunca usa 'exportar' falla");
    CHECK(contiene(cap.str(), "no usa 'exportar'"), "el mensaje sugiere usar 'incluir'");
}

static void prueba_import_modulo_inexistente() {
    std::string rutaEntrada = escribirArchivoM4("main_no_existe_m4.lat",
        "importar { x } desde \"no_existe_m4.lat\"\n"
        "escribir(x)\n");

    auto entrada = parsear(
        "importar { x } desde \"no_existe_m4.lat\"\n"
        "escribir(x)\n");

    std::ostringstream cap;
    std::streambuf* viejo = std::cerr.rdbuf(cap.rdbuf());
    auto resultado = ResolutorModulos::resolverProyecto(std::move(entrada), rutaEntrada);
    std::cerr.rdbuf(viejo);

    CHECK(resultado == nullptr, "importar un archivo inexistente falla");
    CHECK(contiene(cap.str(), "no se pudo abrir"), "el mensaje reporta que no se pudo abrir el modulo");
}

static void prueba_import_circular() {
    // a_m4.lat importa de b_m4.lat, que a su vez importa de a_m4.lat.
    escribirArchivoM4("a_m4.lat",
        "exportar funcion desde_a()\n"
        "  retornar 1\n"
        "fin\n"
        "importar { desde_b } desde \"b_m4.lat\"\n");
    escribirArchivoM4("b_m4.lat",
        "exportar funcion desde_b()\n"
        "  retornar 2\n"
        "fin\n"
        "importar { desde_a } desde \"a_m4.lat\"\n");

    std::string rutaEntrada = escribirArchivoM4("main_circular_m4.lat",
        "importar { desde_a } desde \"a_m4.lat\"\n"
        "escribir(desde_a())\n");

    auto entrada = parsear(
        "importar { desde_a } desde \"a_m4.lat\"\n"
        "escribir(desde_a())\n");

    std::ostringstream cap;
    std::streambuf* viejo = std::cerr.rdbuf(cap.rdbuf());
    auto resultado = ResolutorModulos::resolverProyecto(std::move(entrada), rutaEntrada);
    std::cerr.rdbuf(viejo);

    CHECK(resultado == nullptr, "un import circular entre dos modulos falla");
    CHECK(contiene(cap.str(), "import circular"), "el mensaje reporta el import circular");
}

static void prueba_import_memoizado_una_sola_vez() {
    // Dos importadores distintos de "compartido_m4.lat": debe procesarse una
    // sola vez (sus sentencias no deben duplicarse en el Programa final).
    escribirArchivoM4("compartido_m4.lat",
        "exportar funcion valor()\n"
        "  retornar 42\n"
        "fin\n");
    escribirArchivoM4("puente_m4.lat",
        "exportar funcion desde_puente()\n"
        "  retornar 1\n"
        "fin\n"
        "importar { valor } desde \"compartido_m4.lat\"\n"
        "exportar funcion usa_valor()\n"
        "  retornar valor()\n"
        "fin\n");

    std::string rutaEntrada = escribirArchivoM4("main_memo_m4.lat",
        "importar { valor } desde \"compartido_m4.lat\"\n"
        "importar { usa_valor } desde \"puente_m4.lat\"\n"
        "escribir(valor() + usa_valor())\n");

    auto entrada = parsear(
        "importar { valor } desde \"compartido_m4.lat\"\n"
        "importar { usa_valor } desde \"puente_m4.lat\"\n"
        "escribir(valor() + usa_valor())\n");

    auto resultado = ResolutorModulos::resolverProyecto(std::move(entrada), rutaEntrada);
    CHECK(resultado != nullptr, "dos importadores del mismo modulo resuelven sin error");
    if (!resultado) return;

    // '42' es un literal único de compartido_m4.lat en todo este programa: si
    // el módulo se procesara dos veces (una por cada importador, sin
    // memoización) su declaración -- y por lo tanto el literal -- aparecería
    // duplicada en el Programa final.
    std::string t = volcar(*resultado);
    size_t apariciones = 0, pos = 0;
    while ((pos = t.find("Numero 42", pos)) != std::string::npos) {
        apariciones++;
        pos += 1;
    }
    CHECK(apariciones == 1, "el modulo compartido por dos importadores se procesa una sola vez");
}

int main() {
    prueba_slug_desde_ruta();
    prueba_participa_de_modulos();
    prueba_funcion_exportada_y_privada();
    prueba_por_defecto_no_crea_binding_nombrado();
    prueba_sombreado_de_parametro();
    prueba_variable_local_shadow_por_asignacion();
    prueba_clase_herencia_nuevo_y_es();
    prueba_tipo_por_nombre_en_campo_y_retorno();
    prueba_asignacion_multiple_top_level();

    prueba_import_nombrado_con_alias();
    prueba_import_nombre_no_exportado();
    prueba_import_modulo_sin_exportar();
    prueba_import_modulo_inexistente();
    prueba_import_circular();
    prueba_import_memoizado_una_sola_vez();

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE MODULOS PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
