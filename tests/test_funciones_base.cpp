// test_funciones_base.cpp — Suite de prueba para funciones base (Fase 20).
// Cubre: tipo(), acadena(), alogico(), anumero(), imprimirf().

#include "test_harness.h"

static const harness::CasoTest CASOS[] = {

    // tipo()
    { "fb_tipo_numero",
      "escribir(tipo(42))",
      "numero" },

    { "fb_tipo_cadena",
      "escribir(tipo(\"hola\"))",
      "cadena" },

    { "fb_tipo_logico",
      "escribir(tipo(cierto))",
      "logico" },

    { "fb_tipo_nulo",
      "escribir(tipo(nulo))",
      "nulo" },

    { "fb_tipo_lista",
      "escribir(tipo([1, 2]))",
      "lista" },

    { "fb_tipo_dic",
      "escribir(tipo({\"a\": 1}))",
      "dic" },

    // acadena()
    { "fb_acadena_numero",
      "escribir(acadena(123))",
      "123" },

    { "fb_acadena_decimal",
      "escribir(acadena(3.14))",
      "3.14" },

    { "fb_acadena_logico_cierto",
      "escribir(acadena(cierto))",
      "cierto" },

    { "fb_acadena_logico_falso",
      "escribir(acadena(falso))",
      "falso" },

    // alogico()
    { "fb_alogico_cero_es_falso",
      "escribir(alogico(0))",
      "falso" },

    { "fb_alogico_uno_es_cierto",
      "escribir(alogico(1))",
      "cierto" },

    { "fb_alogico_cadena_vacia_es_falso",
      "escribir(alogico(\"\"))",
      "falso" },

    { "fb_alogico_cadena_no_vacia_es_cierto",
      "escribir(alogico(\"si\"))",
      "cierto" },

    // anumero()
    { "fb_anumero_entero",
      "escribir(anumero(\"42\"))",
      "42" },

    { "fb_anumero_decimal",
      "escribir(anumero(\"3.1416\"))",
      "3.1416" },

    { "fb_anumero_logico_cierto",
      "escribir(anumero(cierto))",
      "1" },

    { "fb_anumero_logico_falso",
      "escribir(anumero(falso))",
      "0" },

    // imprimirf()
    { "fb_imprimirf_entero",
      "imprimirf(\"%d\\n\", 42)",
      "42" },

    { "fb_imprimirf_float",
      "imprimirf(\"%.2f\\n\", 3.14159)",
      "3.14" },

    { "fb_imprimirf_cadena",
      "imprimirf(\"%s\\n\", \"Latino\")",
      "Latino" },

    { "fb_imprimirf_multiples",
      "imprimirf(\"%d + %d = %d\\n\", 2, 3, 5)",
      "2 + 3 = 5" },
};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
