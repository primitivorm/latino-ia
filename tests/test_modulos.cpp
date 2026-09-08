// test_modulos.cpp
//
// Pruebas unitarias de ResolutorModulos (PLAN_MODULOS.md, M3: resolución de
// un solo módulo, sin "importar" todavía). Parsea fragmentos .lat, corre el
// resolutor y verifica el AST resultante volcándolo con ImpresorAST (mismo
// mecanismo que tests/test_parser.cpp), además de inspeccionar la tabla de
// exportación devuelta.

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "ast.h"
#include "ast_impresor.h"
#include "lexer.h"
#include "parser.h"
#include "resolutor_modulos.h"

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

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE MODULOS PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
