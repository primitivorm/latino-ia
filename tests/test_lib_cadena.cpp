// test_lib_cadena.cpp — Suite de prueba para la librería cadena (Fase 20).
// Cubre las 22+ funciones documentadas en Manual-Latino/docs/librerias/cadena.rst

#include "test_harness.h"

// Prefijo "incluir" añadido a cada programa de prueba.
#define INC "incluir \"cadena\"\n"

static const harness::CasoTest CASOS[] = {

    // cadena.longitud
    { "cad_longitud_basica",
      INC "escribir(cadena.longitud(\"hola\"))",
      "4" },

    { "cad_longitud_vacia",
      INC "escribir(cadena.longitud(\"\"))",
      "0" },

    { "cad_longitud_espacios",
      INC "escribir(cadena.longitud(\"hola mundo\"))",
      "10" },

    // cadena.mayusculas / minusculas
    { "cad_mayusculas",
      INC "escribir(cadena.mayusculas(\"hola\"))",
      "HOLA" },

    { "cad_minusculas",
      INC "escribir(cadena.minusculas(\"HOLA MUNDO\"))",
      "hola mundo" },

    // cadena.contiene
    { "cad_contiene_si",
      INC "escribir(cadena.contiene(\"hola mundo\", \"mundo\"))",
      "cierto" },

    { "cad_contiene_no",
      INC "escribir(cadena.contiene(\"hola\", \"mundo\"))",
      "falso" },

    // cadena.inicia_con / termina_con
    { "cad_inicia_con_si",
      INC "escribir(cadena.inicia_con(\"hola mundo\", \"hola\"))",
      "cierto" },

    { "cad_inicia_con_no",
      INC "escribir(cadena.inicia_con(\"hola mundo\", \"mundo\"))",
      "falso" },

    { "cad_termina_con_si",
      INC "escribir(cadena.termina_con(\"hola mundo\", \"mundo\"))",
      "cierto" },

    { "cad_termina_con_no",
      INC "escribir(cadena.termina_con(\"hola mundo\", \"hola\"))",
      "falso" },

    // cadena.encontrar / ultimo_indice
    { "cad_encontrar_existe",
      INC "escribir(cadena.encontrar(\"hola mundo\", \"mundo\"))",
      "5" },

    { "cad_encontrar_no_existe",
      INC "escribir(cadena.encontrar(\"hola\", \"mundo\"))",
      "-1" },

    { "cad_ultimo_indice",
      INC "escribir(cadena.ultimo_indice(\"abcabc\", \"abc\"))",
      "3" },

    // cadena.separar
    { "cad_separar_longitud",
      INC "partes = cadena.separar(\"a,b,c\", \",\")\n"
          "escribir(lista.longitud(partes))",
      "3" },

    { "cad_separar_primer_elemento",
      INC "partes = cadena.separar(\"hola mundo\", \" \")\n"
          "escribir(partes[0])",
      "hola" },

    { "cad_separar_segundo_elemento",
      INC "partes = cadena.separar(\"hola mundo\", \" \")\n"
          "escribir(partes[1])",
      "mundo" },

    // cadena.subcadena
    { "cad_subcadena",
      INC "escribir(cadena.subcadena(\"hola mundo\", 5, 10))",
      "mundo" },

    { "cad_subcadena_inicio",
      INC "escribir(cadena.subcadena(\"hola mundo\", 0, 4))",
      "hola" },

    // cadena.reemplazar
    { "cad_reemplazar",
      INC "escribir(cadena.reemplazar(\"hola mundo\", \"mundo\", \"latino\"))",
      "hola latino" },

    { "cad_reemplazar_multiple",
      INC "escribir(cadena.reemplazar(\"aaa\", \"a\", \"b\"))",
      "bbb" },

    // cadena.recortar
    { "cad_recortar_ambos",
      INC "escribir(cadena.recortar(\"  hola  \"))",
      "hola" },

    { "cad_recortar_izquierda",
      INC "escribir(cadena.recortar(\"  hola\"))",
      "hola" },

    { "cad_recortar_derecha",
      INC "escribir(cadena.recortar(\"hola  \"))",
      "hola" },

    // cadena.invertir
    { "cad_invertir",
      INC "escribir(cadena.invertir(\"abc\"))",
      "cba" },

    { "cad_invertir_palindromo",
      INC "escribir(cadena.invertir(\"ana\"))",
      "ana" },

    // cadena.comparar
    { "cad_comparar_igual",
      INC "escribir(cadena.comparar(\"abc\", \"abc\"))",
      "0" },

    { "cad_comparar_menor",
      INC "escribir(cadena.comparar(\"abc\", \"def\"))",
      "-1" },

    { "cad_comparar_mayor",
      INC "escribir(cadena.comparar(\"def\", \"abc\"))",
      "1" },

    // cadena.es_alfa / es_numerico / esta_vacia
    { "cad_es_alfa_si",
      INC "escribir(cadena.es_alfa(\"hola\"))",
      "cierto" },

    { "cad_es_alfa_no",
      INC "escribir(cadena.es_alfa(\"hola123\"))",
      "falso" },

    { "cad_es_numerico_si",
      INC "escribir(cadena.es_numerico(\"3.14\"))",
      "cierto" },

    { "cad_es_numerico_no",
      INC "escribir(cadena.es_numerico(\"abc\"))",
      "falso" },

    { "cad_esta_vacia_si",
      INC "escribir(cadena.esta_vacia(\"\"))",
      "cierto" },

    { "cad_esta_vacia_no",
      INC "escribir(cadena.esta_vacia(\"a\"))",
      "falso" },

    // cadena.formato
    { "cad_formato_float",
      INC "escribir(cadena.formato(\"%.2f\", 3.14159))",
      "3.14" },

    { "cad_formato_entero",
      INC "escribir(cadena.formato(\"%d items\", 5))",
      "5 items" },

    // cadena.regex / regexl
    { "cad_regex_digitos",
      INC "escribir(cadena.regex(\"abc 123 def\", \"[0-9]+\"))",
      "123" },

    { "cad_regex_sin_coincidencia",
      INC "escribir(cadena.regex(\"abc\", \"[0-9]+\"))",
      "nulo" },

    { "cad_regexl_longitud",
      INC "resultados = cadena.regexl(\"a1 b2 c3\", \"[a-z][0-9]\")\n"
          "escribir(lista.longitud(resultados))",
      "3" },

    // cadena.insertar
    { "cad_insertar",
      INC "escribir(cadena.insertar(\"hola\", 4, \" mundo\"))",
      "hola mundo" },

    // cadena.concatenar
    { "cad_concatenar",
      INC "escribir(cadena.concatenar(\"hola\", \" mundo\"))",
      "hola mundo" },

    // cadena.char
    { "cad_char",
      INC "escribir(cadena.char(65))",
      "A" },

    // cadena.bytes
    { "cad_bytes_longitud",
      INC "b = cadena.bytes(\"ABC\")\n"
          "escribir(lista.longitud(b))",
      "3" },

    // cadena.rellenar_derecha / rellenar_izquierda
    { "cad_rellenar_derecha",
      INC "escribir(cadena.rellenar_derecha(\"hi\", 5, \".\"))",
      "hi..." },

    { "cad_rellenar_izquierda",
      INC "escribir(cadena.rellenar_izquierda(\"hi\", 5, \".\"))",
      "...hi" },

    // cadena.es_igual (comparación sin distinción de mayúsculas)
    { "cad_es_igual_si",
      INC "escribir(cadena.es_igual(\"Hola\", \"hola\"))",
      "cierto" },

    { "cad_es_igual_no",
      INC "escribir(cadena.es_igual(\"hola\", \"mundo\"))",
      "falso" },
};

#undef INC

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
