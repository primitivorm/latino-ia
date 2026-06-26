// test_codegen.cpp
//
// Pruebas unitarias de la generación de código C (Fase 5). Comprueban que el
// código C emitido contiene los fragmentos esperados (no compila ni ejecuta;
// eso se valida en las pruebas de extremo a extremo).

#include <iostream>
#include <memory>
#include <string>

#include "ast.h"
#include "compiler.h"
#include "lexer.h"
#include "parser.h"

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

static std::string generar(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer);
    auto prog = parser.parse();
    if (!prog) return "<nullptr>";
    GeneradorC gen;
    return gen.generar(*prog);
}

static bool contiene(const std::string& t, const std::string& sub) {
    return t.find(sub) != std::string::npos;
}

static void esperar(const std::string& nombre, const std::string& src,
                    const std::string& fragmento) {
    std::string c = generar(src);
    CHECK(contiene(c, fragmento),
          nombre << ": deberia contener '" << fragmento << "'\n--- generado ---\n" << c);
}

static void prueba_estructura_basica() {
    std::string c = generar("escribir(\"hola mundo\")\n");
    CHECK(contiene(c, "#include \"latino.h\""), "include del runtime");
    CHECK(contiene(c, "int main(int argc, char *argv[]) {"), "funcion main");
    CHECK(contiene(c, "lat_escribir(lat_cadena(\"hola mundo\"))"), "llamada a escribir");
}

static void prueba_aritmetica_precedencia() {
    esperar("aritmetica", "x = 1 + 2 * 3\n",
            "v_x = lat_sumar(lat_numero(1), lat_multiplicar(lat_numero(2), lat_numero(3)));");
}

static void prueba_si() {
    std::string src =
        "edad = 18\n"
        "si edad >= 18\n"
        "  escribir(\"mayor\")\n"
        "sino\n"
        "  escribir(\"menor\")\n"
        "fin\n";
    std::string c = generar(src);
    CHECK(contiene(c, "if (lat_es_verdadero(lat_mayor_igual(v_edad, lat_numero(18)))) {"),
          "condicion del si");
    CHECK(contiene(c, "} else {"), "rama sino");
}

static void prueba_desde() {
    std::string src =
        "desde (i = 0; i <= 10; i++)\n"
        "  escribir(i)\n"
        "fin\n";
    std::string c = generar(src);
    CHECK(contiene(c, "v_i = lat_numero(0);"), "inicializacion");
    CHECK(contiene(c, "while (lat_es_verdadero(lat_menor_igual(v_i, lat_numero(10)))) {"),
          "condicion del bucle");
    CHECK(contiene(c, "(v_i = lat_sumar(v_i, lat_numero(1)));"), "incremento");
}

static void prueba_funcion() {
    std::string src =
        "fun sumar(a, b)\n"
        "  ret a + b\n"
        "fin\n"
        "r = sumar(2, 3)\n";
    std::string c = generar(src);
    CHECK(contiene(c, "static LatValor lat_fn_sumar(LatValor, LatValor);"),
          "prototipo");
    CHECK(contiene(c, "static LatValor lat_fn_sumar(LatValor v_a, LatValor v_b) {"),
          "definicion");
    CHECK(contiene(c, "return lat_sumar(v_a, v_b);"), "cuerpo del retorno");
    CHECK(contiene(c, "v_r = lat_fn_sumar(lat_numero(2), lat_numero(3));"), "llamada");
}

static void prueba_funcion_variadica() {
    std::string src =
        "funcion f(a, ...)\n"
        "  va = [...]\n"
        "fin\n"
        "f(1, 2, 3)\n";
    std::string c = generar(src);
    CHECK(contiene(c, "static LatValor lat_fn_f(LatValor v_a, LatValor lat_resto) {"),
          "firma variadica");
    CHECK(contiene(c, "v_va = lat_resto;"), "[...] -> lat_resto");
    CHECK(contiene(c, "lat_fn_f(lat_numero(1), lat_lista_de(2, lat_numero(2), lat_numero(3)))"),
          "empaquetado de varargs en la llamada");
}

static void prueba_lista_y_diccionario() {
    esperar("lista", "n = [1, 2, 3]\n",
            "lat_lista_de(3, lat_numero(1), lat_numero(2), lat_numero(3))");
    esperar("dic", "d = { \"a\": 1, \"b\": 2 }\n",
            "lat_dic_de(2, lat_cadena(\"a\"), lat_numero(1), lat_cadena(\"b\"), lat_numero(2))");
}

static void prueba_indices() {
    esperar("indice_neg", "n = [1, 2, 3]\nescribir(n[-1])\n",
            "lat_obtener_indice(v_n, lat_negar(lat_numero(1)))");
    esperar("asignar_indice", "n = [1, 2, 3]\nn[0] = 99\n",
            "lat_asignar_indice(v_n, lat_numero(0), lat_numero(99));");
}

static void prueba_ternario() {
    esperar("ternario", "m = (x < 0) ? \"neg\" : \"pos\"\n",
            "(lat_es_verdadero(lat_menor(v_x, lat_numero(0))) ? lat_cadena(\"neg\") : lat_cadena(\"pos\"))");
}

static void prueba_repetir() {
    std::string src = "i = 0\nrepetir\n  i++\nhasta i == 10\n";
    std::string c = generar(src);
    CHECK(contiene(c, "do {"), "do");
    CHECK(contiene(c, "} while (!lat_es_verdadero(lat_igual(v_i, lat_numero(10))));"),
          "condicion hasta negada");
}

static void prueba_elegir() {
    std::string src =
        "c = 1\n"
        "elegir(c)\n"
        "  caso 1:\n"
        "    escribir(\"uno\")\n"
        "  defecto:\n"
        "    escribir(\"otro\")\n"
        "fin\n";
    std::string c = generar(src);
    CHECK(contiene(c, "lat_igual("), "comparacion de caso");
    CHECK(contiene(c, "} else {"), "rama defecto");
}

static void prueba_asignacion_multiple() {
    std::string c = generar("a, b = 1, 2\n");
    CHECK(contiene(c, "LatValor _t0 = lat_numero(1);"), "temporal 0");
    CHECK(contiene(c, "LatValor _t1 = lat_numero(2);"), "temporal 1");
    CHECK(contiene(c, "v_a = _t0;") && contiene(c, "v_b = _t1;"), "asignaciones");
}

int main() {
    prueba_estructura_basica();
    prueba_aritmetica_precedencia();
    prueba_si();
    prueba_desde();
    prueba_funcion();
    prueba_funcion_variadica();
    prueba_lista_y_diccionario();
    prueba_indices();
    prueba_ternario();
    prueba_repetir();
    prueba_elegir();
    prueba_asignacion_multiple();

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE GENERACION DE CODIGO PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
