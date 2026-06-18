// test_incluir.cpp — Suite de prueba para el sistema de módulos (Fase 20).
// Verifica que `incluir "nombre"` resuelve las librerías estándar correctamente
// y que sus funciones quedan disponibles tras la importación.

#include "test_harness.h"

static const harness::CasoTest CASOS[] = {

    // incluir "cadena" — función básica disponible
    { "inc_cadena_longitud",
      "incluir \"cadena\"\n"
      "escribir(cadena.longitud(\"hola\"))",
      "4" },

    { "inc_cadena_mayusculas",
      "incluir \"cadena\"\n"
      "escribir(cadena.mayusculas(\"latino\"))",
      "LATINO" },

    // incluir "lista"
    { "inc_lista_longitud",
      "incluir \"lista\"\n"
      "l = [1, 2, 3]\n"
      "escribir(lista.longitud(l))",
      "3" },

    { "inc_lista_agregar",
      "incluir \"lista\"\n"
      "l = []\n"
      "lista.agregar(l, 42)\n"
      "escribir(lista.longitud(l))",
      "1" },

    // incluir "dic"
    { "inc_dic_longitud",
      "incluir \"dic\"\n"
      "d = {\"a\": 1, \"b\": 2}\n"
      "escribir(dic.longitud(d))",
      "2" },

    { "inc_dic_contiene",
      "incluir \"dic\"\n"
      "d = {\"clave\": \"valor\"}\n"
      "escribir(dic.contiene(d, \"clave\"))",
      "cierto" },

    // incluir "mate"
    { "inc_mate_raiz",
      "incluir \"mate\"\n"
      "escribir(mate.raiz(9))",
      "3" },

    { "inc_mate_abs",
      "incluir \"mate\"\n"
      "escribir(mate.abs(-7))",
      "7" },

    // incluir "sis"
    { "inc_sis_dormir",
      "incluir \"sis\"\n"
      "escribir(sis.dormir(1))",
      "nulo" },

    // incluir "archivo"
    { "inc_archivo_round_trip",
      "incluir \"archivo\"\n"
      "archivo.escribir(\"_inc_test.txt\", \"modulo\")\n"
      "escribir(archivo.leer(\"_inc_test.txt\"))\n"
      "archivo.borrar(\"_inc_test.txt\")",
      "modulo" },

    // Múltiples incluir en el mismo programa
    { "inc_multiples",
      "incluir \"cadena\"\n"
      "incluir \"lista\"\n"
      "l = cadena.separar(\"a b c\", \" \")\n"
      "escribir(lista.longitud(l))",
      "3" },

    // incluir repetido (idempotente — no debe duplicar símbolos)
    { "inc_repetido",
      "incluir \"cadena\"\n"
      "incluir \"cadena\"\n"
      "escribir(cadena.longitud(\"ok\"))",
      "2" },

    // incluir "cadena" y usar resultado en una expresión
    { "inc_cadena_en_expresion",
      "incluir \"cadena\"\n"
      "s = \"Hola Mundo\"\n"
      "n = cadena.longitud(s)\n"
      "escribir(n > 5)",
      "cierto" },

    // incluir "mate" y combinar con operador
    { "inc_mate_en_expresion",
      "incluir \"mate\"\n"
      "x = mate.pot(2, 8)\n"
      "escribir(x == 256)",
      "cierto" },
};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
