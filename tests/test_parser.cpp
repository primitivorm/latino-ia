// test_parser.cpp
//
// Pruebas unitarias del Parser (Fase 3). Parsea fragmentos de código Latino y
// verifica el AST resultante volcándolo con ImpresorAST.

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "ast.h"
#include "ast_impresor.h"
#include "lexer.h"
#include "parser.h"

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

static std::string volcar(const std::string& src) {
    auto prog = parsear(src);
    if (!prog)
        return "<nullptr>";
    std::ostringstream os;
    ImpresorAST imp(os);
    imp.imprimir(*prog);
    return os.str();
}

static bool contiene(const std::string& t, const std::string& sub) {
    return t.find(sub) != std::string::npos;
}
static bool enOrden(const std::string& t, const std::string& a, const std::string& b) {
    size_t pa = t.find(a), pb = t.find(b);
    return pa != std::string::npos && pb != std::string::npos && pa < pb;
}
// Sangría (número de espacios iniciales) de la línea que contiene `sub`.
static int indentDe(const std::string& t, const std::string& sub) {
    size_t p = t.find(sub);
    if (p == std::string::npos) return -1;
    size_t ini = t.rfind('\n', p);
    ini = (ini == std::string::npos) ? 0 : ini + 1;
    return static_cast<int>(p - ini);
}

// --- Pruebas ---------------------------------------------------------------

static void prueba_precedencia_aritmetica() {
    // x = 1 + 2 * 3   ->   '+' arriba, '*' como hijo derecho (mas indentado)
    std::string t = volcar("x = 1 + 2 * 3\n");
    CHECK(contiene(t, "Asignacion (1 = 1)"), "asignacion simple");
    CHECK(contiene(t, "Binaria '+'") && contiene(t, "Binaria '*'"), "ambas binarias");
    CHECK(indentDe(t, "Binaria '*'") > indentDe(t, "Binaria '+'"),
          "la multiplicacion debe anidarse bajo la suma");
    CHECK(indentDe(t, "Numero 2") > indentDe(t, "Numero 1"),
          "operando de '*' mas profundo que el de '+'");
}

static void prueba_precedencia_concatenacion() {
    // "x" .. a + b   ->  '..' arriba, '+' como hijo (mas profundo)
    std::string t = volcar("y = \"x\" .. a + b\n");
    CHECK(contiene(t, "Binaria '..'") && contiene(t, "Binaria '+'"), "concat y suma");
    CHECK(indentDe(t, "Binaria '+'") > indentDe(t, "Binaria '..'"),
          "la suma se anida bajo la concatenacion");
}

static void prueba_asignacion_multiple() {
    std::string t = volcar("a, b, c = 1, 2, 3\n");
    CHECK(contiene(t, "Asignacion (3 = 3)"), "asignacion 3=3");
    CHECK(contiene(t, "Identificador 'a'") && contiene(t, "Identificador 'c'"), "destinos");
    CHECK(contiene(t, "Numero 1") && contiene(t, "Numero 3"), "valores");
}

static void prueba_si_osi_sino() {
    std::string src =
        "si a\n"
        "  x = 1\n"
        "osi b\n"
        "  x = 2\n"
        "sino\n"
        "  x = 3\n"
        "fin\n";
    std::string t = volcar(src);
    CHECK(contiene(t, "Si"), "si");
    CHECK(contiene(t, "entonces:"), "entonces");
    CHECK(contiene(t, "osi:"), "osi");
    CHECK(contiene(t, "sino:"), "sino");
    CHECK(enOrden(t, "entonces:", "osi:") && enOrden(t, "osi:", "sino:"),
          "orden entonces/osi/sino");
}

static void prueba_ternario() {
    std::string t = volcar("m = (n < 0) ? \"neg\" : \"pos\"\n");
    CHECK(contiene(t, "Ternaria"), "ternaria");
    CHECK(contiene(t, "Binaria '<'"), "condicion <");
    CHECK(contiene(t, "Cadena 'neg'") && contiene(t, "Cadena 'pos'"), "ramas");
}

static void prueba_desde() {
    std::string src =
        "desde (i = 0; i <= 10; i++)\n"
        "  escribir(i)\n"
        "fin\n";
    std::string t = volcar(src);
    CHECK(contiene(t, "Desde"), "desde");
    CHECK(contiene(t, "PostOperador '++'"), "incremento i++");
    CHECK(contiene(t, "Binaria '<='"), "condicion <=");
    CHECK(contiene(t, "Llamada") && contiene(t, "Identificador 'escribir'"), "cuerpo");
}

static void prueba_mientras_y_repetir() {
    std::string t1 = volcar("mientras i < 10\n  i++\nfin\n");
    CHECK(contiene(t1, "Mientras") && contiene(t1, "Binaria '<'"), "mientras");

    std::string t2 = volcar("repetir\n  i++\nhasta i == 10\n");
    CHECK(contiene(t2, "Repetir") && contiene(t2, "hasta:"), "repetir-hasta");
    CHECK(contiene(t2, "Binaria '=='"), "condicion hasta");
}

static void prueba_funcion_y_retorno() {
    std::string src =
        "fun sumar(a, b)\n"
        "  ret a + b\n"
        "fin\n";
    std::string t = volcar(src);
    CHECK(contiene(t, "Funcion 'sumar' (a, b)"), "firma");
    CHECK(contiene(t, "Retornar"), "retornar");
    CHECK(contiene(t, "Binaria '+'"), "expresion del retorno");
}

static void prueba_funcion_variadica() {
    std::string src =
        "funcion f(a, b, ...)\n"
        "  va = [...]\n"
        "fin\n";
    std::string t = volcar(src);
    CHECK(contiene(t, "Funcion 'f' (a, b, ...)"), "firma variadica");
    CHECK(contiene(t, "Lista (1)") && contiene(t, "VarArgs ..."), "[...] en el cuerpo");
}

static void prueba_lista_indice_negativo() {
    std::string src =
        "numeros = [1, 2, 3]\n"
        "escribir(numeros[-1])\n";
    std::string t = volcar(src);
    CHECK(contiene(t, "Lista (3)"), "lista de 3");
    CHECK(contiene(t, "AccesoIndice"), "acceso por indice");
    CHECK(contiene(t, "Unaria '-'"), "indice negativo");
}

static void prueba_diccionario_multilinea() {
    std::string src =
        "actores = { \"a\": \"x\",\n"
        "            \"b\": \"y\" }\n";
    std::string t = volcar(src);
    CHECK(contiene(t, "Diccionario (2)"), "diccionario de 2 (multilinea)");
    CHECK(contiene(t, "Cadena 'a'") && contiene(t, "Cadena 'y'"), "claves/valores");
}

static void prueba_elegir() {
    std::string src =
        "elegir(c)\n"
        "  caso 'A':\n"
        "    escribir(\"a\")\n"
        "  defecto:\n"
        "    escribir(\"otro\")\n"
        "fin\n";
    std::string t = volcar(src);
    CHECK(contiene(t, "Elegir"), "elegir");
    CHECK(contiene(t, "caso:"), "caso");
    CHECK(contiene(t, "defecto:"), "defecto");
    CHECK(enOrden(t, "caso:", "defecto:"), "orden caso/defecto");
}

static void prueba_var_y_const() {
    std::string t1 = volcar("var x = 10\n");
    CHECK(contiene(t1, "Asignacion (1 = 1) [var]"), "var con inicializador");
    CHECK(contiene(t1, "Identificador 'x'"), "destino de var");
    CHECK(contiene(t1, "Numero 10"), "valor de var");

    std::string t2 = volcar("var y\n");
    CHECK(contiene(t2, "Asignacion (1 = 1) [var]"), "var sin inicializador");
    CHECK(contiene(t2, "Identificador 'y'"), "destino de var sin inicializador");
    CHECK(contiene(t2, "Nulo"), "valor nulo de var sin inicializador");

    std::string t3 = volcar("const PI = 3.1416\n");
    CHECK(contiene(t3, "Asignacion (1 = 1) [const]"), "const con inicializador");
    CHECK(contiene(t3, "Identificador 'PI'"), "destino de const");
    CHECK(contiene(t3, "Numero 3.1416"), "valor de const");

    std::ostringstream cap;
    std::streambuf* viejo = std::cerr.rdbuf(cap.rdbuf());
    auto prog = parsear("const PI\n");
    std::cerr.rdbuf(viejo);
    CHECK(prog == nullptr, "const sin inicializador debe fallar");
}

static void prueba_error_de_sintaxis() {
    // 'x =' sin expresión a la derecha debe fallar y devolver nullptr.
    std::ostringstream cap;
    std::streambuf* viejo = std::cerr.rdbuf(cap.rdbuf());
    auto prog = parsear("x =\n");
    std::cerr.rdbuf(viejo);

    CHECK(prog == nullptr, "asignacion sin valor debe devolver nullptr");
    CHECK(contiene(cap.str(), "Error de sintaxis"), "debe reportar error de sintaxis");
}

// --- Módulos (PLAN_MODULOS.md, Fase 30 M2) ---------------------------------

static void prueba_exportar_declaraciones() {
    std::string t1 = volcar("exportar const PI = 3.14159\n");
    CHECK(contiene(t1, "Asignacion (1 = 1) [const] [exportado]"), "exportar const");

    std::string t2 = volcar("exportar funcion area(r)\n  retornar r\nfin\n");
    CHECK(contiene(t2, "Funcion 'area' (r)") && contiene(t2, "[exportado]"),
          "exportar funcion");

    std::string t3 = volcar("funcion privada(r)\n  retornar r\nfin\n");
    CHECK(!contiene(t3, "[exportado]"), "funcion sin exportar no lleva la marca");

    std::string t4 = volcar("exportar clase Circulo\n  funcion Circulo(r)\n    este.r = r\n  fin\nfin\n");
    CHECK(contiene(t4, "Clase 'Circulo'") && contiene(t4, "[exportado]"), "exportar clase");

    std::string t5 = volcar("exportar estructura Punto\n  x: numero\nfin\n");
    CHECK(contiene(t5, "Estructura 'Punto'") && contiene(t5, "[exportado]"),
          "exportar estructura");

    std::string t6 = volcar("exportar interfaz Figura\n  funcion area()\nfin\n");
    CHECK(contiene(t6, "Interfaz 'Figura'") && contiene(t6, "[exportado]"),
          "exportar interfaz");

    std::string t7 = volcar("exportar var x = 1\n");
    CHECK(contiene(t7, "Asignacion (1 = 1) [var] [exportado]"), "exportar var");

    std::string t8 = volcar("exportar x = 1\n");
    CHECK(contiene(t8, "Asignacion (1 = 1) [exportado]"), "exportar asignacion simple");
}

static void prueba_exportar_por_defecto() {
    std::string t1 = volcar("exportar por defecto funcion saludar(nombre)\n"
                             "  retornar nombre\n"
                             "fin\n");
    CHECK(contiene(t1, "Funcion 'saludar' (nombre)") &&
          contiene(t1, "[exportado por defecto]"), "exportar por defecto funcion");

    std::string t2 = volcar("exportar por defecto { \"version\": \"1.0\" }\n");
    CHECK(contiene(t2, "Asignacion (1 = 1) [const] [exportado por defecto]"),
          "exportar por defecto expresion");
    CHECK(contiene(t2, "Identificador '__defecto__'"), "destino sintetico __defecto__");
    CHECK(contiene(t2, "Diccionario (1)"), "valor del export por defecto");
}

static void prueba_importar_nombrado() {
    std::string t = volcar("importar { area_circulo, Circulo como C } desde \"geometria.lat\"\n");
    CHECK(contiene(t, "Importar { area_circulo, Circulo como C } desde \"geometria.lat\""),
          "importar nombrado con alias");
}

static void prueba_importar_espacio() {
    std::string t = volcar("importar * como geo desde \"geometria.lat\"\n");
    CHECK(contiene(t, "Importar * como geo desde \"geometria.lat\""), "importar espacio de nombres");
}

static void prueba_importar_por_defecto() {
    std::string t = volcar("importar Config desde \"config.lat\"\n");
    CHECK(contiene(t, "Importar Config desde \"config.lat\""), "importar por defecto");
}

static void prueba_exportar_reexport() {
    std::string t = volcar("exportar { Circulo, area_circulo } desde \"geometria.lat\"\n");
    CHECK(contiene(t, "ExportarDesde { Circulo, area_circulo } desde \"geometria.lat\""),
          "reexport (barril)");
}

int main() {
    prueba_precedencia_aritmetica();
    prueba_precedencia_concatenacion();
    prueba_asignacion_multiple();
    prueba_si_osi_sino();
    prueba_ternario();
    prueba_desde();
    prueba_mientras_y_repetir();
    prueba_funcion_y_retorno();
    prueba_funcion_variadica();
    prueba_lista_indice_negativo();
    prueba_diccionario_multilinea();
    prueba_elegir();
    prueba_error_de_sintaxis();
    prueba_var_y_const();
    prueba_exportar_declaraciones();
    prueba_exportar_por_defecto();
    prueba_importar_nombrado();
    prueba_importar_espacio();
    prueba_importar_por_defecto();
    prueba_exportar_reexport();

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DEL PARSER PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
