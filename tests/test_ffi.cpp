// test_ffi.cpp
//
// Pruebas unitarias de generacion de codigo para "externo"/"inseguro" (FFI
// con C al estilo de Rust, ver input/PLAN_FFI.md, F5/F7). Mismo patron que
// test_codegen.cpp: comprueba que el codigo C emitido contiene los
// fragmentos esperados (no compila ni ejecuta -- eso lo cubre
// tests/test_ffi_e2e.cpp).
//
// El parser/analizador semantico de "externo"/"inseguro" ya tienen su propia
// cobertura en test_parser.cpp (F2) y test_semantico.cpp (F4); este archivo
// se enfoca en lo que quedaba sin probar: el marshalling real que emite
// GeneradorC::genLlamadaExterna/genArgumentoFFI (F5) y el preambulo
// (extern/#pragma comment/#include <stdint.h>) que arma GeneradorC::generar().

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

static void esperar(const std::string& nombre, const std::string& src, const std::string& fragmento) {
    std::string c = generar(src);
    CHECK(contiene(c, fragmento),
          nombre << ": deberia contener '" << fragmento << "'\n--- generado ---\n" << c);
}

// --- Preambulo: extern/#pragma comment/#include <stdint.h> -----------------

static void prueba_extern_sin_enlazar() {
    std::string src =
        "externo\n"
        "    funcion abs(n: entero32): entero32\n"
        "fin\n"
        "inseguro\n"
        "    r = abs(5)\n"
        "fin\n";
    std::string c = generar(src);
    CHECK(contiene(c, "#include <stdint.h>"), "debe incluir stdint.h para los anchos fijos");
    CHECK(contiene(c, "extern int32_t abs(int32_t);"),
          "debe declarar 'abs' con su firma C real, no la firma empaquetada LatValor\n--- generado ---\n"
              << c);
    CHECK(!contiene(c, "#pragma comment"),
          "sin 'enlazar', no debe emitirse ningun #pragma comment(lib,...)");
}

static void prueba_extern_con_enlazar() {
    std::string src =
        "externo enlazar \"user32\"\n"
        "    funcion MessageBoxA(hwnd: puntero, texto: cadena, titulo: cadena, tipo: entero32): entero32\n"
        "fin\n";
    std::string c = generar(src);
    CHECK(contiene(c, "#ifdef _MSC_VER"), "la biblioteca de 'enlazar' debe ir bajo #ifdef _MSC_VER");
    CHECK(contiene(c, "#pragma comment(lib, \"user32.lib\")"),
          "debe emitir el #pragma comment(lib,...) de la biblioteca declarada\n--- generado ---\n" << c);
    CHECK(contiene(c, "extern int32_t MessageBoxA(void*, const char*, const char*, int32_t);"),
          "debe declarar la firma C real completa (puntero/cadena/entero32)\n--- generado ---\n" << c);
}

static void prueba_tipo_retorno_por_defecto_es_nulo() {
    std::string src =
        "externo\n"
        "    funcion free(p: puntero)\n"
        "fin\n";
    std::string c = generar(src);
    CHECK(contiene(c, "extern void free(void*);"),
          "sin ':' de retorno, la firma C real debe usar 'void'\n--- generado ---\n" << c);
}

// --- Marshalling de argumentos/retorno --------------------------------------

static void prueba_marshalling_entero_sin_enlazar() {
    esperar("abs(5)",
            "externo\n"
            "    funcion abs(n: entero32): entero32\n"
            "fin\n"
            "inseguro\n"
            "    r = abs(5)\n"
            "fin\n",
            "v_r = lat_numero((double)(abs((int32_t)lat_ffi_verificar_tipo(lat_numero(5), "
            "LAT_NUMERO, \"abs\", 1).como.numero)));");
}

static void prueba_marshalling_cadena_y_entero64() {
    esperar("strlen(\"hola\")",
            "externo\n"
            "    funcion strlen(s: cadena): entero64\n"
            "fin\n"
            "inseguro\n"
            "    n = strlen(\"hola\")\n"
            "fin\n",
            "v_n = lat_numero((double)(strlen(lat_ffi_verificar_tipo(lat_cadena(\"hola\"), "
            "LAT_CADENA, \"strlen\", 1).como.cadena)));");
}

static void prueba_marshalling_puntero_nulo() {
    std::string src =
        "externo\n"
        "    funcion foo(p: puntero)\n"  // sin ':' de retorno -> TipoFFI::Nulo por defecto
        "fin\n"
        "inseguro\n"
        "    foo(nulo)\n"
        "fin\n";
    std::string c = generar(src);
    // Hallazgo de F4/F5 (ver PLAN_FFI.md): el literal "nulo" hacia un
    // parametro "puntero" debe desviarse a NULL ANTES de
    // lat_ffi_verificar_tipo (LAT_NULO != LAT_PUNTERO aborta el proceso ahi),
    // usando un temporal para no evaluar dos veces la expresion del
    // argumento.
    CHECK(contiene(c, "LatValor _t0 = lat_nulo();"), "debe evaluar el argumento una sola vez, en un temporal\n--- generado ---\n" << c);
    CHECK(contiene(c, "(_t0.tipo == LAT_NULO ? NULL : lat_ffi_verificar_tipo(_t0, LAT_PUNTERO, "
                      "\"foo\", 1).como.puntero)"),
          "'nulo' debe desviarse a NULL sin pasar por la verificacion dinamica\n--- generado ---\n" << c);
}

static void prueba_marshalling_puntero_no_nulo_encadenado() {
    std::string src =
        "externo\n"
        "    funcion malloc(tam: entero64): puntero\n"
        "    funcion free(p: puntero)\n"
        "fin\n"
        "inseguro\n"
        "    p = malloc(16)\n"
        "    free(p)\n"
        "fin\n";
    std::string c = generar(src);
    CHECK(contiene(c, "extern void* malloc(int64_t);"), "malloc debe declararse con retorno puntero real\n--- generado ---\n" << c);
    CHECK(contiene(c, "extern void free(void*);"), "free debe declararse con parametro puntero real\n--- generado ---\n" << c);
    CHECK(contiene(c, "v_p = lat_puntero((void*)(malloc((int64_t)lat_ffi_verificar_tipo(lat_numero(16), "
                      "LAT_NUMERO, \"malloc\", 1).como.numero)));"),
          "el retorno puntero de malloc debe empaquetarse con lat_puntero\n--- generado ---\n" << c);
    // free(p): 'p' no es el literal "nulo" -- debe pasar por la verificacion
    // dinamica real (mismo temporal + ternario que el caso "nulo", pero acá
    // la rama "verificar" es la que efectivamente aporta el valor).
    CHECK(contiene(c, "LatValor _t0 = v_p;"), "debe evaluar 'p' una sola vez, en un temporal\n--- generado ---\n" << c);
    CHECK(contiene(c, "lat_ffi_verificar_tipo(_t0, LAT_PUNTERO, \"free\", 1).como.puntero"),
          "debe verificar dinamicamente el puntero antes de pasarlo a free\n--- generado ---\n" << c);
}

static void prueba_marshalling_logico() {
    esperar("toupper('a' equivalente logico)",
            "externo\n"
            "    funcion es_par(n: entero32): logico\n"
            "fin\n"
            "inseguro\n"
            "    b = es_par(4)\n"
            "fin\n",
            "v_b = lat_logico((int)(es_par((int32_t)lat_ffi_verificar_tipo(lat_numero(4), "
            "LAT_NUMERO, \"es_par\", 1).como.numero)));");
}

int main() {
    prueba_extern_sin_enlazar();
    prueba_extern_con_enlazar();
    prueba_tipo_retorno_por_defecto_es_nulo();

    prueba_marshalling_entero_sin_enlazar();
    prueba_marshalling_cadena_y_entero64();
    prueba_marshalling_puntero_nulo();
    prueba_marshalling_puntero_no_nulo_encadenado();
    prueba_marshalling_logico();

    std::cout << "\nComprobaciones: " << g_checks << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE CODEGEN FFI (BACKEND C) PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
