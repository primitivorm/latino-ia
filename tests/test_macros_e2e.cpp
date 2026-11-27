// test_macros_e2e.cpp — Programas completos con macros al estilo de Julia
// (input/PLAN_MACROS.md). Igual que el resto de suites E2E, compila con el
// binario 'latino' real y compara tokens de salida.

#include "test_harness.h"

static const harness::CasoTest CASOS[] = {

    { "macros_doble",
      "macro doble(x)\n"
      "  retornar quote $x * 2 fin\n"
      "fin\n"
      "escribir(@doble 21)\n",
      "42" },

    { "macros_dos_argumentos",
      "macro suma(a, b)\n"
      "  retornar quote ($a) + ($b) fin\n"
      "fin\n"
      "escribir(@suma 2, 3)\n",
      "5" },

    { "macros_higiene",
      "macro con_temporal(x)\n"
      "  retornar quote\n"
      "    temporal = $x\n"
      "    escribir(temporal + 1)\n"
      "  fin\n"
      "fin\n"
      "y = 3\n"
      "@con_temporal y\n"
      "escribir(y)\n",
      "4 3" },

    { "macros_esc",
      "macro usar(x)\n"
      "  retornar quote esc(x) + 1 fin\n"
      "fin\n"
      "y = 5\n"
      "escribir(@usar y)\n",
      "6" },

    { "macros_expr_llamada",
      "macro llamar(f, x)\n"
      "  retornar Expr(:llamada, f, x)\n"
      "fin\n"
      "funcion cuadrado(v)\n"
      "  retornar v * v\n"
      "fin\n"
      "escribir(@llamar cuadrado, 4)\n",
      "16" },

    { "macros_inspeccion_ast",
      "macro tipo_de(e)\n"
      "  retornar quote $(string(e.tipo)) fin\n"
      "fin\n"
      "escribir(@tipo_de 1 + 2)\n",
      "binaria" },

    { "macros_funcion_interna",
      "macro generar(n)\n"
      "  funcion pieza(x)\n"
      "    retornar Expr(:binaria, :+, x, 1)\n"
      "  fin\n"
      "  retornar pieza(n)\n"
      "fin\n"
      "escribir(@generar 7)\n",
      "8" },

    { "macros_anidadas",
      "macro doble(x)\n"
      "  retornar quote $x * 2 fin\n"
      "fin\n"
      "macro cuadruple(x)\n"
      "  retornar Expr(:macro, :doble, Expr(:macro, :doble, x))\n"
      "fin\n"
      "escribir(@cuadruple 3)\n",
      "12" },

    { "macros_generar_simbolo",
      "macro numerar()\n"
      "  v = generar_simbolo(\"v\")\n"
      "  retornar Expr(:bloque,\n"
      "               Expr(:asignacion, v, 5),\n"
      "               Expr(:llamada, :escribir, v))\n"
      "fin\n"
      "@numerar()\n",
      "5" },

    { "macros_mostrar",
      "x = 7\n"
      "@mostrar x + 1\n",
      "(x + 1) = 8" },

    { "macros_afirmar",
      "@afirmar 1 < 2\n"
      "escribir(\"ok\")\n",
      "ok" },
};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
