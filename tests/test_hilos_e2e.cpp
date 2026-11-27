// test_hilos_e2e.cpp — Hilos estilo C# (input/PLAN_HILOS.md).
// Compila programas completos con el binario 'latino' real y compara tokens
// de salida, igual que el resto de suites E2E.

#include "test_harness.h"

static const harness::CasoTest CASOS[] = {

    { "hilos_basico",
      "hilo t(x: 41, s: \"hilo\")\n"
      "    imprimir(x + 1)\n"
      "    imprimir(s)\n"
      "fin\n"
      "esperar t\n"
      "imprimir(\"fin\")\n",
      "42 hilo fin" },

    { "hilos_cerrojo_global",
      "global cerrojo = hilos.cerrojo()\n"
      "global total = 0\n"
      "hilo uno(n: 150)\n"
      "    desde(i = 0; i < n; i++)\n"
      "        bloquear(cerrojo)\n"
      "            total = total + 1\n"
      "        fin\n"
      "    fin\n"
      "fin\n"
      "hilo dos(n: 150)\n"
      "    desde(i = 0; i < n; i++)\n"
      "        bloquear(cerrojo)\n"
      "            total = total + 1\n"
      "        fin\n"
      "    fin\n"
      "fin\n"
      "esperar uno\n"
      "esperar dos\n"
      "imprimir(total)\n",
      "300" },

    { "hilos_api_explicita",
      "f = fun(x) imprimir(\"api \" .. x) fin\n"
      "t = hilos.crear(f, [7])\n"
      "hilos.esperar(t)\n"
      "c = hilos.cerrojo()\n"
      "hilos.entrar(c, fun(y) imprimir(\"lock \" .. y) fin, [8])\n"
      "imprimir(\"listo\")\n",
      "api 7 lock 8 listo" },

    { "hilos_retornar_termina",
      "hilo t()\n"
      "    imprimir(\"antes\")\n"
      "    retornar\n"
      "    imprimir(\"despues\")\n"
      "fin\n"
      "esperar t\n"
      "imprimir(\"fin\")\n",
      "antes fin" },

    { "hilos_romper_dentro_bloqueo",
      "global c = hilos.cerrojo()\n"
      "global n = 0\n"
      "desde(i = 0; i < 5; i++)\n"
      "    bloquear(c)\n"
      "        n = n + 1\n"
      "        romper\n"
      "    fin\n"
      "fin\n"
      "imprimir(n)\n",
      "1" },

    { "hilos_retornar_libera_cerrojo",
      "global c = hilos.cerrojo()\n"
      "funcion f()\n"
      "    bloquear(c)\n"
      "        imprimir(\"dentro\")\n"
      "        retornar\n"
      "    fin\n"
      "fin\n"
      "hilo t1() f() fin\n"
      "esperar t1\n"
      "hilo t2()\n"
      "    bloquear(c)\n"
      "        imprimir(\"libre\")\n"
      "    fin\n"
      "fin\n"
      "esperar t2\n",
      "dentro libre" },

    { "hilos_desde_macro",
      "macro lanzar(x)\n"
      "  retornar quote\n"
      "    hilo t(v: $x)\n"
      "      imprimir(v)\n"
      "    fin\n"
      "    esperar t\n"
      "  fin\n"
      "fin\n"
      "@lanzar 7\n",
      "7" },
};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
