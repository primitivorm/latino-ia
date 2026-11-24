// test_runtime_memoria.cpp
//
// Pruebas del contrato de memoria de PLAN_PERF: registro global de bloques,
// marcos de raíces, temporales y barrido shallow. No miden fugas con Valgrind
// (eso hace tools/valgrind.sh): comprueban la semántica observable del registro
// para que una regresión en el marcado se detecte sin depender de Valgrind.

#include <cstdlib>
#include <cstring>
#include <iostream>

extern "C" {
#include "latino.h"
}

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

// Barrido con un marco ya visible: lat_recoger solo barre en el marco más
// externo (ver lat_recoger en runtime/latino.c).
static void barrer(LatValor** raices, size_t cantidad, LatValor extra) {
    lat_raices_empujar(raices, cantidad);
    lat_recoger(extra);
    lat_raices_sacar();
}

// Cada prueba parte de un registro vacío: lat_liberar_todo es idempotente y
// deja g_limpiando a 0 al terminar, así que puede llamarse entre casos.
static void limpiar() {
    lat_liberar_todo();
}

static void prueba_raices_sobreviven_y_no_raices_se_libera() {
    limpiar();
    size_t base = lat_registro_cantidad();

    LatValor viva = lat_cadena("viva");
    LatValor muerta = lat_cadena("muerta");
    CHECK(lat_registro_cantidad() == base + 2, "dos cadenas registradas");

    LatValor* raices[] = {&viva};
    barrer(raices, 1, lat_nulo());

    CHECK(lat_registro_cantidad() == base + 1, "la cadena sin raíz se libera");
    CHECK(viva.tipo == LAT_CADENA && std::strcmp(viva.como.cadena, "viva") == 0,
          "la cadena con raíz sigue viva y conserva su contenido");
}

static void prueba_extra_sobrevive_al_barrido() {
    limpiar();
    size_t base = lat_registro_cantidad();
    LatValor extra = lat_cadena("retorno");
    barrer(nullptr, 0, extra);
    CHECK(lat_registro_cantidad() == base + 1, "el valor extra del retorno es raíz");
    CHECK(std::strcmp(extra.como.cadena, "retorno") == 0, "el valor extra conserva su contenido");
}

static void prueba_temporales() {
    limpiar();
    size_t base = lat_registro_cantidad();
    LatValor temporal = lat_cadena("temporal");

    LatValor* sinRaices = nullptr;
    lat_raices_empujar(&sinRaices, 0);
    lat_raices_empujar_temporal(&temporal);
    lat_recoger(lat_nulo());
    CHECK(lat_registro_cantidad() == base + 1, "el temporal es raíz mientras está publicado");

    lat_raices_sacar_temporal();
    lat_recoger(lat_nulo());
    CHECK(lat_registro_cantidad() == base, "el temporal se libera al sacarlo");
    lat_raices_sacar();
}

static void prueba_barrido_anidado_no_libera_temporales_del_llamador() {
    limpiar();
    size_t base = lat_registro_cantidad();
    LatValor pendiente = lat_cadena("pendiente");

    LatValor* sinRaices = nullptr;
    lat_raices_empujar(&sinRaices, 0);  // marco del "llamador"
    lat_raices_empujar_temporal(&pendiente);
    lat_raices_empujar(&sinRaices, 0);  // marco de la llamada anidada
    lat_recoger(lat_nulo());
    CHECK(lat_registro_cantidad() == base + 1,
          "un barrido anidado no toca los temporales ya evaluados por el llamador");
    lat_raices_sacar();       // cierra la llamada anidada
    lat_raices_sacar_temporal();
    lat_recoger(lat_nulo());  // ahora sí: el marco más externo puede barrer
    CHECK(lat_registro_cantidad() == base,
          "el temporal se libera cuando lo suelta el llamador");
    lat_raices_sacar();
}

static void prueba_ciclo_no_recursa_y_se_libera() {
    limpiar();
    size_t base = lat_registro_cantidad();
    LatValor a = lat_lista_de(1, lat_nulo());
    LatValor b = lat_lista_de(1, lat_nulo());
    lat_asignar_indice(a, lat_numero(0), b);
    lat_asignar_indice(b, lat_numero(0), a);

    LatValor* raices[] = {&a};
    barrer(raices, 1, lat_nulo());
    CHECK(lat_registro_cantidad() == base + 2, "el ciclo marcado sobrevive sin recursion infinita");

    barrer(nullptr, 0, lat_nulo());
    CHECK(lat_registro_cantidad() == base, "el ciclo sin raíces se libera por completo");
}

static void prueba_diccionario_marca_sus_valores() {
    limpiar();
    size_t base = lat_registro_cantidad();
    LatValor valor = lat_cadena("dentro");
    LatValor d = lat_dic_de(1, lat_cadena("k"), valor);

    LatValor* raices[] = {&d};
    barrer(raices, 1, lat_nulo());
    CHECK(lat_registro_cantidad() == base + 2,
          "un diccionario vivo mantiene vivas su clave y su valor");

    barrer(nullptr, 0, lat_nulo());
    CHECK(lat_registro_cantidad() == base, "el diccionario y sus valores se liberan juntos");
}

// Regresión de test_lib_paquete: LatModulo tiene entrada propia en el registro.
// Sin el caso LAT_MODULO en marcar_valor, el barrido liberaba el struct aunque
// la variable del módulo estuviera viva y la siguiente llamada crasheaba.
static void prueba_modulo_se_marca_como_raiz() {
    limpiar();
    size_t base = lat_registro_cantidad();
    /* Bloque ficticio: lat_paquete_cargar no está disponible en esta suite y
     * liberar_bloque_shallow solo hace free(ptr) para LAT_BLOQUE_MODULO. */
    LatModulo* mod = (LatModulo*)malloc(1);
    CHECK(mod != nullptr, "bloque de módulo asignado");
    lat_registro_registrar(mod, LAT_BLOQUE_MODULO);
    LatValor v;
    v.tipo = LAT_MODULO;
    v.como.modulo = mod;

    LatValor* raices[] = {&v};
    barrer(raices, 1, lat_nulo());
    CHECK(lat_registro_cantidad() == base + 1, "el módulo vivo sobrevive al barrido");

    barrer(nullptr, 0, lat_nulo());
    CHECK(lat_registro_cantidad() == base, "el módulo se libera al perder su raíz");
}

static void prueba_liberar_todo_idempotente() {
    limpiar();
    LatValor s = lat_cadena("superviviente");
    CHECK(lat_registro_cantidad() >= 1, "hay un bloque registrado antes de lat_liberar_todo");
    lat_liberar_todo();
    CHECK(lat_registro_cantidad() == 0, "lat_liberar_todo vacía el registro");
    lat_liberar_todo();
    CHECK(lat_registro_cantidad() == 0, "lat_liberar_todo es idempotente");
    (void)s;  // no se toca tras la liberación, igual que en el atexit real
}

int main() {
    prueba_raices_sobreviven_y_no_raices_se_libera();
    prueba_extra_sobrevive_al_barrido();
    prueba_temporales();
    prueba_barrido_anidado_no_libera_temporales_del_llamador();
    prueba_ciclo_no_recursa_y_se_libera();
    prueba_diccionario_marca_sus_valores();
    prueba_modulo_se_marca_como_raiz();
    prueba_liberar_todo_idempotente();

    std::cout << "Comprobaciones: " << g_checks << "  Fallos: " << g_fallos << "\n";
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE MEMORIA DEL RUNTIME PASARON.\n";
    return g_fallos == 0 ? 0 : 1;
}
