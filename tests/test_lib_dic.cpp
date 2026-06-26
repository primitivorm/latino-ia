// test_lib_dic.cpp — Suite de prueba para la librería dic (Fase 20).
// Cubre las 5 funciones documentadas en Manual-Latino/docs/librerias/dic.rst

#include "test_harness.h"

#define INC "incluir \"dic\"\n"

static const harness::CasoTest CASOS[] = {

    // dic.longitud
    { "dic_longitud_dos",
      INC "d = {\"a\": 1, \"b\": 2}\n"
          "escribir(dic.longitud(d))",
      "2" },

    { "dic_longitud_vacio",
      INC "d = {}\n"
          "escribir(dic.longitud(d))",
      "0" },

    { "dic_longitud_uno",
      INC "d = {\"x\": 42}\n"
          "escribir(dic.longitud(d))",
      "1" },

    // dic.contiene
    { "dic_contiene_si",
      INC "d = {\"nombre\": \"Ana\", \"edad\": 30}\n"
          "escribir(dic.contiene(d, \"nombre\"))",
      "cierto" },

    { "dic_contiene_no",
      INC "d = {\"nombre\": \"Ana\"}\n"
          "escribir(dic.contiene(d, \"edad\"))",
      "falso" },

    // dic.eliminar
    { "dic_eliminar_longitud",
      INC "d = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
          "dic.eliminar(d, \"b\")\n"
          "escribir(dic.longitud(d))",
      "2" },

    { "dic_eliminar_clave_ausente",
      INC "d = {\"a\": 1, \"b\": 2}\n"
          "dic.eliminar(d, \"b\")\n"
          "escribir(dic.contiene(d, \"b\"))",
      "falso" },

    // dic.llaves
    { "dic_llaves_longitud",
      INC "incluir \"lista\"\n"
          "d = {\"x\": 1, \"y\": 2, \"z\": 3}\n"
          "k = dic.llaves(d)\n"
          "escribir(lista.longitud(k))",
      "3" },

    { "dic_llaves_contiene_clave",
      INC "incluir \"lista\"\n"
          "d = {\"nombre\": \"Latino\"}\n"
          "k = dic.llaves(d)\n"
          "escribir(lista.contiene(k, \"nombre\"))",
      "cierto" },

    // dic.valores
    { "dic_valores_longitud",
      INC "incluir \"lista\"\n"
          "d = {\"a\": 10, \"b\": 20}\n"
          "v = dic.valores(d)\n"
          "escribir(lista.longitud(v))",
      "2" },

    { "dic_valores_contiene_valor",
      INC "incluir \"lista\"\n"
          "d = {\"pi\": 3}\n"
          "v = dic.valores(d)\n"
          "escribir(lista.contiene(v, 3))",
      "cierto" },

    // Acceso por clave (operación fundamental del diccionario)
    { "dic_acceso_cadena",
      INC "d = {\"ciudad\": \"Managua\"}\n"
          "escribir(d[\"ciudad\"])",
      "Managua" },

    { "dic_acceso_numero",
      INC "d = {\"edad\": 25}\n"
          "escribir(d[\"edad\"])",
      "25" },

    // Modificación de valor existente
    { "dic_modificar_valor",
      INC "d = {\"n\": 1}\n"
          "d[\"n\"] = 99\n"
          "escribir(d[\"n\"])",
      "99" },

    // ---- Fase 22 ----

    // dic.combinar
    { "dic_combinar_longitud",
      INC "d1 = {\"a\": 1}\n"
          "d2 = {\"b\": 2}\n"
          "c = dic.combinar(d1, d2)\n"
          "escribir(dic.longitud(c))",
      "2" },

    { "dic_combinar_precedencia_d2",
      INC "d1 = {\"x\": 1}\n"
          "d2 = {\"x\": 99}\n"
          "c = dic.combinar(d1, d2)\n"
          "escribir(c[\"x\"])",
      "99" },

    // dic.elementos
    { "dic_elementos_longitud",
      INC "incluir \"lista\"\n"
          "d = {\"a\": 1, \"b\": 2}\n"
          "it = dic.elementos(d)\n"
          "escribir(lista.longitud(it))",
      "2" },

    { "dic_elementos_par_es_lista",
      INC "incluir \"lista\"\n"
          "d = {\"clave\": \"valor\"}\n"
          "it = dic.elementos(d)\n"
          "par = it[0]\n"
          "escribir(lista.longitud(par))",
      "2" },

    // dic.copiar
    { "dic_copiar_independiente",
      INC "original = {\"k\": 1}\n"
          "copia = dic.copiar(original)\n"
          "copia[\"k\"] = 99\n"
          "escribir(original[\"k\"])",
      "1" },

    { "dic_copiar_longitud",
      INC "d = {\"a\": 1, \"b\": 2}\n"
          "c = dic.copiar(d)\n"
          "escribir(dic.longitud(c))",
      "2" },

    // dic.actualizar
    { "dic_actualizar_agrega",
      INC "d = {\"a\": 1}\n"
          "dic.actualizar(d, {\"b\": 2})\n"
          "escribir(dic.longitud(d))",
      "2" },

    { "dic_actualizar_sobreescribe",
      INC "d = {\"x\": 1}\n"
          "dic.actualizar(d, {\"x\": 99})\n"
          "escribir(d[\"x\"])",
      "99" },
};

#undef INC

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
