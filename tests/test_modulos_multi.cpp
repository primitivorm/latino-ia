// test_modulos_multi.cpp — Pruebas de extremo a extremo de PLAN_MODULOS.md
// (M4: "importar { a, b como c } desde \"ruta\"" entre archivos reales; M5:
// "importar * como ns desde ..." e "importar Nombre desde ..." + "exportar
// por defecto").
//
// A diferencia de test_modulos_e2e.cpp (un solo archivo, sin "importar"),
// cada caso aquí escribe uno o más archivos .lat auxiliares junto al de
// entrada (ver harness::CasoTestMulti/Harness::ejecutarMulti en
// test_harness.h) y compila/ejecuta el archivo de entrada a través del
// binario 'latino' real -- el mangling y la resolución de imports deben ser
// invisibles en el comportamiento observable del programa.

#include "test_harness.h"

static const harness::CasoTestMulti CASOS[] = {

    { "modmulti_import_nombrado",
      "importar { area_circulo } desde \"geometria_mm.lat\"\n"
      "escribir(area_circulo(2))\n",
      "12.56",
      { { "geometria_mm.lat",
          "exportar funcion area_circulo(r)\n"
          "  retornar 3.14 * r * r\n"
          "fin\n" } } },

    { "modmulti_import_con_alias",
      "importar { area_circulo como area } desde \"geometria_mm.lat\"\n"
      "escribir(area(2))\n",
      "12.56",
      { { "geometria_mm.lat",
          "exportar funcion area_circulo(r)\n"
          "  retornar 3.14 * r * r\n"
          "fin\n" } } },

    // La función privada del módulo importado sigue siendo invisible desde
    // fuera (AnalizadorSemantico no la conoce con su nombre original) pero
    // sí es usada internamente por la exportada -- confirma que el mangling
    // no rompe las referencias internas del módulo importado.
    { "modmulti_import_usa_funcion_privada_internamente",
      "importar { area_circulo } desde \"geometria_priv_mm.lat\"\n"
      "escribir(area_circulo(-3))\n",
      "9",
      { { "geometria_priv_mm.lat",
          "exportar funcion area_circulo(r)\n"
          "  retornar normalizar(r) * normalizar(r)\n"
          "fin\n"
          "funcion normalizar(r)\n"
          "  retornar r < 0 ? 0 - r : r\n"
          "fin\n" } } },

    // Varios nombres importados del mismo módulo en una sola sentencia.
    { "modmulti_import_multiples_nombres",
      "importar { sumar, restar } desde \"aritmetica_mm.lat\"\n"
      "escribir(sumar(4, 3))\n"
      "escribir(restar(4, 3))\n",
      "7 1",
      { { "aritmetica_mm.lat",
          "exportar funcion sumar(a, b)\n"
          "  retornar a + b\n"
          "fin\n"
          "exportar funcion restar(a, b)\n"
          "  retornar a - b\n"
          "fin\n" } } },

    // Clase exportada, importada y usada con 'nuevo' desde el módulo que
    // importa -- confirma que el mangling de tipos POO (constructor incluido)
    // sobrevive al cruce de archivos.
    { "modmulti_import_clase",
      "importar { Circulo } desde \"figuras_mm.lat\"\n"
      "c = nuevo Circulo(3)\n"
      "escribir(c.area())\n",
      "28.26",
      { { "figuras_mm.lat",
          "exportar clase Circulo\n"
          "  publico r: numero\n"
          "  funcion Circulo(r: numero)\n"
          "    este.r = r\n"
          "  fin\n"
          "  funcion area()\n"
          "    retornar 3.14 * este.r * este.r\n"
          "  fin\n"
          "fin\n" } } },

    // Import transitivo: main importa de 'puente_mm.lat', que a su vez
    // importa de 'base_mm.lat' -- confirma la resolución multi-nivel (DFS).
    { "modmulti_import_transitivo",
      "importar { desde_puente } desde \"puente_mm.lat\"\n"
      "escribir(desde_puente())\n",
      "10",
      { { "base_mm.lat",
          "exportar funcion valor_base()\n"
          "  retornar 10\n"
          "fin\n" },
        { "puente_mm.lat",
          "importar { valor_base } desde \"base_mm.lat\"\n"
          "exportar funcion desde_puente()\n"
          "  retornar valor_base()\n"
          "fin\n" } } },

    // M5 — import de espacio de nombres: "geo.X" se resuelve en tiempo de
    // compilacion al nombre interno de X, sin diccionario "geo" en runtime.
    { "modmulti_import_namespace",
      "importar * como geo desde \"geo_ns_mm.lat\"\n"
      "escribir(geo.area_circulo(2))\n",
      "12.56",
      { { "geo_ns_mm.lat",
          "exportar funcion area_circulo(r)\n"
          "  retornar 3.14 * r * r\n"
          "fin\n" } } },

    // Namespace calificando una constante exportada (no solo funciones):
    // "geo.PI" tambien se resuelve al nombre interno. Se lee a nivel
    // superior (no dentro de una funcion): Latino no soporta hoy leer una
    // variable/const de nivel superior desde dentro de una funcion, ver el
    // hallazgo de M3 en PLAN_MODULOS.md -- limitacion preexistente del
    // lenguaje, no de este resolutor. (Calificar un nombre de clase en
    // "nuevo geo.Clase(...)" es una interaccion POO/modulos que queda fuera
    // de alcance de M5 -- ver PLAN_MODULOS.md, M6: "nuevo" solo acepta hoy
    // un identificador simple, no un nombre calificado.)
    { "modmulti_import_namespace_constante",
      "importar * como geo desde \"geo_const_ns_mm.lat\"\n"
      "escribir(geo.PI)\n",
      "3.14",
      { { "geo_const_ns_mm.lat",
          "exportar const PI = 3.14\n" } } },

    // M5 — import por defecto: "importar Nombre desde ..." se resuelve
    // contra la entrada especial "__defecto__" de la tabla de exportacion.
    { "modmulti_import_por_defecto",
      "importar Saludar desde \"saludo_mm.lat\"\n"
      "escribir(Saludar(\"Ana\"))\n",
      "Hola, Ana",
      { { "saludo_mm.lat",
          "exportar por defecto funcion saludar(nombre)\n"
          "  retornar \"Hola, \" .. nombre\n"
          "fin\n" } } },

};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main_multi(argc, argv, CASOS, std::size(CASOS));
}
