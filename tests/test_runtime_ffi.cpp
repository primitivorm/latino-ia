// test_runtime_ffi.cpp
//
// Pruebas del runtime de FFI con C (PLAN_FFI.md, F3): LAT_PUNTERO y
// lat_ffi_verificar_tipo. Enlaza directamente contra latino_runtime (sin
// pasar por el compilador de Latino, que todavía no genera código FFI --
// eso llega en F5/F6). Solo se ejercitan las rutas exitosas: la ruta de
// error de lat_ffi_verificar_tipo termina el proceso con exit(1), igual que
// lat_verificar_tipo/lat_dividir, y ninguna prueba del proyecto ejercita esas
// rutas en el mismo proceso.

#include <iostream>
#include <cstring>

extern "C" {
#include "latino.h"
}

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

static void prueba_lat_puntero_construye() {
    int x = 42;
    LatValor p = lat_puntero(&x);
    CHECK(p.tipo == LAT_PUNTERO, "lat_puntero: tipo LAT_PUNTERO");
    CHECK(p.como.puntero == &x, "lat_puntero: guarda el puntero tal cual");

    LatValor nulo = lat_puntero(nullptr);
    CHECK(nulo.tipo == LAT_PUNTERO, "lat_puntero(NULL): sigue siendo LAT_PUNTERO");
    CHECK(nulo.como.puntero == nullptr, "lat_puntero(NULL): guarda NULL");
}

// p == nulo / p != nulo cuando p es un LAT_PUNTERO -- no se usa el operador
// "es" para esto (ver "Sintaxis propuesta" y Decisión de diseño en
// PLAN_FFI.md: son_iguales trata un LAT_PUNTERO con valor NULL como
// equivalente a "nulo").
static void prueba_puntero_igualdad_con_nulo() {
    int x = 1;
    LatValor punteroValido = lat_puntero(&x);
    LatValor punteroNulo = lat_puntero(nullptr);
    LatValor nulo = lat_nulo();

    CHECK(!lat_es_verdadero(lat_igual(punteroValido, nulo)),
          "puntero no-NULL == nulo debe ser falso");
    CHECK(lat_es_verdadero(lat_distinto(punteroValido, nulo)),
          "puntero no-NULL != nulo debe ser cierto");

    CHECK(lat_es_verdadero(lat_igual(punteroNulo, nulo)),
          "puntero NULL == nulo debe ser cierto");
    CHECK(lat_es_verdadero(lat_igual(nulo, punteroNulo)),
          "nulo == puntero NULL debe ser cierto (simetria)");
    CHECK(!lat_es_verdadero(lat_distinto(punteroNulo, nulo)),
          "puntero NULL != nulo debe ser falso");
}

// Identidad entre dos LAT_PUNTERO (sin involucrar "nulo").
static void prueba_puntero_igualdad_identidad() {
    int x = 1, y = 1;
    LatValor a = lat_puntero(&x);
    LatValor b = lat_puntero(&x);
    LatValor c = lat_puntero(&y);

    CHECK(lat_es_verdadero(lat_igual(a, b)), "mismo puntero nativo -> iguales");
    CHECK(!lat_es_verdadero(lat_igual(a, c)), "distinto puntero nativo -> no iguales");
}

static void prueba_lat_es_verdadero_puntero() {
    int x = 1;
    CHECK(lat_es_verdadero(lat_puntero(&x)), "puntero no-NULL es verdadero");
    CHECK(!lat_es_verdadero(lat_puntero(nullptr)), "puntero NULL es falso");
}

static void prueba_lat_tipo_y_a_cadena_puntero() {
    int x = 1;
    LatValor p = lat_puntero(&x);

    LatValor t = lat_tipo(p);
    CHECK(t.tipo == LAT_CADENA, "lat_tipo(puntero): devuelve una cadena");
    CHECK(std::strcmp(t.como.cadena, "puntero") == 0, "lat_tipo(puntero) == \"puntero\"");

    char* s = lat_a_cadena(p);
    CHECK(std::strcmp(s, "<puntero>") == 0, "lat_a_cadena(puntero) == \"<puntero>\"");
    free(s);
}

// Ruta exitosa de lat_ffi_verificar_tipo (la ruta de error termina el
// proceso con exit(1), ver comentario de arriba).
static void prueba_lat_ffi_verificar_tipo_ok() {
    LatValor n = lat_numero(5);
    LatValor r = lat_ffi_verificar_tipo(n, LAT_NUMERO, "abs", 1);
    CHECK(r.tipo == LAT_NUMERO && r.como.numero == 5,
          "lat_ffi_verificar_tipo: devuelve el valor sin modificar si el tipo coincide");

    int x = 1;
    LatValor p = lat_puntero(&x);
    LatValor r2 = lat_ffi_verificar_tipo(p, LAT_PUNTERO, "free", 1);
    CHECK(r2.tipo == LAT_PUNTERO && r2.como.puntero == &x,
          "lat_ffi_verificar_tipo: acepta LAT_PUNTERO cuando se espera LAT_PUNTERO");

    LatValor c = lat_cadena("hola");
    LatValor r3 = lat_ffi_verificar_tipo(c, LAT_CADENA, "strlen", 1);
    CHECK(r3.tipo == LAT_CADENA, "lat_ffi_verificar_tipo: acepta LAT_CADENA cuando se espera LAT_CADENA");
}

int main() {
    prueba_lat_puntero_construye();
    prueba_puntero_igualdad_con_nulo();
    prueba_puntero_igualdad_identidad();
    prueba_lat_es_verdadero_puntero();
    prueba_lat_tipo_y_a_cadena_puntero();
    prueba_lat_ffi_verificar_tipo_ok();

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DEL RUNTIME FFI PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
