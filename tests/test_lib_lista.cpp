// test_lib_lista.cpp — Suite de prueba para la librería lista (Fase 20).
// Cubre las 13 funciones documentadas en Manual-Latino/docs/librerias/lista.rst

#include "test_harness.h"

#define INC "incluir \"lista\"\n"

static const harness::CasoTest CASOS[] = {

    // lista.longitud
    { "lst_longitud_tres",
      INC "escribir(lista.longitud([1, 2, 3]))",
      "3" },

    { "lst_longitud_vacia",
      INC "escribir(lista.longitud([]))",
      "0" },

    // lista.agregar
    { "lst_agregar_longitud",
      INC "l = [1, 2]\n"
          "lista.agregar(l, 3)\n"
          "escribir(lista.longitud(l))",
      "3" },

    { "lst_agregar_valor",
      INC "l = [\"a\"]\n"
          "lista.agregar(l, \"b\")\n"
          "escribir(l[1])",
      "b" },

    // lista.contiene
    { "lst_contiene_si",
      INC "escribir(lista.contiene([3, 1, 4, 1, 5], 4))",
      "cierto" },

    { "lst_contiene_no",
      INC "escribir(lista.contiene([1, 2, 3], 9))",
      "falso" },

    // lista.encontrar / indice
    { "lst_encontrar_existe",
      INC "escribir(lista.encontrar([10, 20, 30], 20))",
      "1" },

    { "lst_encontrar_no_existe",
      INC "escribir(lista.encontrar([1, 2, 3], 9))",
      "-1" },

    // lista.invertir
    { "lst_invertir",
      INC "l = [1, 2, 3]\n"
          "lista.invertir(l)\n"
          "escribir(l[0])",
      "3" },

    { "lst_invertir_longitud_intacta",
      INC "l = [9, 5, 1]\n"
          "lista.invertir(l)\n"
          "escribir(lista.longitud(l))",
      "3" },

    // lista.concatenar
    { "lst_concatenar_longitud",
      INC "c = lista.concatenar([1, 2], [3, 4])\n"
          "escribir(lista.longitud(c))",
      "4" },

    { "lst_concatenar_elementos",
      INC "c = lista.concatenar([\"a\"], [\"b\", \"c\"])\n"
          "escribir(c[2])",
      "c" },

    // lista.insertar
    { "lst_insertar_longitud",
      INC "l = [1, 2, 3]\n"
          "lista.insertar(l, 1, 99)\n"
          "escribir(lista.longitud(l))",
      "4" },

    { "lst_insertar_valor",
      INC "l = [1, 2, 3]\n"
          "lista.insertar(l, 1, 99)\n"
          "escribir(l[1])",
      "99" },

    // lista.eliminar
    { "lst_eliminar_longitud",
      INC "l = [1, 2, 3]\n"
          "lista.eliminar(l, 2)\n"
          "escribir(lista.longitud(l))",
      "2" },

    { "lst_eliminar_primera_ocurrencia",
      INC "l = [1, 2, 2, 3]\n"
          "lista.eliminar(l, 2)\n"
          "escribir(lista.longitud(l))",
      "3" },

    // lista.eliminar_indice
    { "lst_eliminar_indice",
      INC "l = [\"a\", \"b\", \"c\"]\n"
          "lista.eliminar_indice(l, 1)\n"
          "escribir(l[1])",
      "c" },

    { "lst_eliminar_indice_longitud",
      INC "l = [1, 2, 3]\n"
          "lista.eliminar_indice(l, 0)\n"
          "escribir(lista.longitud(l))",
      "2" },

    // lista.extender
    { "lst_extender_longitud",
      INC "a = [1, 2]\n"
          "lista.extender(a, [3, 4, 5])\n"
          "escribir(lista.longitud(a))",
      "5" },

    { "lst_extender_elemento",
      INC "a = [1, 2]\n"
          "lista.extender(a, [3, 4])\n"
          "escribir(a[3])",
      "4" },

    // lista.separador
    { "lst_separador_guion",
      INC "escribir(lista.separador([1, 2, 3], \"-\"))",
      "1-2-3" },

    { "lst_separador_coma",
      INC "escribir(lista.separador([\"a\", \"b\", \"c\"], \",\"))",
      "a,b,c" },

    // lista.crear
    { "lst_crear_longitud",
      INC "l = lista.crear(5, 0)\n"
          "escribir(lista.longitud(l))",
      "5" },

    { "lst_crear_valor",
      INC "l = lista.crear(3, 7)\n"
          "escribir(l[2])",
      "7" },

    // lista.comparar
    { "lst_comparar_igual",
      INC "escribir(lista.comparar([1, 2, 3], [1, 2, 3]))",
      "0" },

    { "lst_comparar_diferente",
      INC "r = lista.comparar([1, 2], [1, 3])\n"
          "escribir(r != 0)",
      "cierto" },
};

#undef INC

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
