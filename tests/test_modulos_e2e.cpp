// test_modulos_e2e.cpp — Pruebas de extremo a extremo de PLAN_MODULOS.md
// (M3: resolución de un solo módulo, sin "importar" todavía).
//
// Compila y ejecuta archivos .lat reales que usan "exportar" (sin
// "importar"), a través del binario 'latino' real. El mangling que aplica
// ResolutorModulos es puramente interno: si estas pruebas pasan, el
// comportamiento observable del programa es idéntico al de antes de M3
// (mismo valor de salida), aunque los nombres C generados por dentro ya
// estén renombrados.

#include "test_harness.h"

static const harness::CasoTest CASOS[] = {

    { "modulos_exportar_funcion_privada",
      "exportar funcion area_circulo(r)\n"
      "  retornar normalizar(r) * r * r\n"
      "fin\n"
      "funcion normalizar(r)\n"
      "  retornar r < 0 ? 0 : r\n"
      "fin\n"
      "escribir(area_circulo(2))\n",
      "8" },

    { "modulos_exportar_por_defecto",
      "exportar por defecto funcion saludar(nombre)\n"
      "  retornar \"Hola, \" .. nombre\n"
      "fin\n"
      "escribir(saludar(\"Mundo\"))\n",
      "Hola, Mundo" },

    // Latino no soporta leer/escribir una variable de nivel superior desde
    // dentro de una funcion sin pasarla como parametro (limitacion previa a
    // este plan, ni GeneradorC la resuelve hoy: ver el hallazgo de esta
    // fase en PLAN_MODULOS.md), asi que el unico uso valido de un
    // 'exportar var/const' es leerlo directamente a nivel de modulo.
    { "modulos_exportar_var_y_const",
      "exportar const PI = 3\n"
      "exportar var contador = 10\n"
      "contador = contador + 5\n"
      "escribir(PI)\n"
      "escribir(contador)\n",
      "3 15" },

    // El parametro 'area_circulo' sombrea al nombre de nivel superior
    // homonimo: si el mangling no respetara el sombreado, esta funcion
    // llamaria (o referenciaria) el simbolo C equivocado y el resultado
    // seria distinto de 6.
    { "modulos_sombreado_parametro",
      "exportar funcion area_circulo(r)\n"
      "  retornar r * r\n"
      "fin\n"
      "funcion usar(area_circulo)\n"
      "  retornar area_circulo + 1\n"
      "fin\n"
      "escribir(usar(5))\n",
      "6" },

    { "modulos_clase_exportada_herencia",
      "exportar clase Figura\n"
      "  publico funcion area(): numero\n"
      "    retornar 0\n"
      "  fin\n"
      "fin\n"
      "exportar clase Circulo extiende Figura\n"
      "  publico r: numero\n"
      "  funcion Circulo(r: numero)\n"
      "    este.r = r\n"
      "  fin\n"
      "  publico funcion area(): numero sobreescribir\n"
      "    retornar este.r * este.r\n"
      "  fin\n"
      "fin\n"
      "c = nuevo Circulo(4)\n"
      "escribir(c.area())\n"
      "si c es Figura\n"
      "  escribir(\"es figura\")\n"
      "fin\n",
      "16 es figura" },

    // Un archivo sin 'exportar'/'importar' no debe verse afectado en
    // absoluto (Decisión de diseño 1: script plano, comportamiento previo).
    { "modulos_script_plano_sin_cambios",
      "funcion doble(x)\n"
      "  retornar x * 2\n"
      "fin\n"
      "escribir(doble(21))\n",
      "42" },
};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
