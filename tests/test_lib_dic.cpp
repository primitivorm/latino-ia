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
};

#undef INC

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
