// test_genericos_e2e.cpp — Pruebas de extremo a extremo de genéricos al
// estilo de Rust (PLAN_GENERICOS.md, Fase 31).
//
// A diferencia de test_genericos.cpp (unitario: parser/semántico/codegen en
// memoria), estas pruebas compilan y ejecutan cada caso a través del binario
// 'latino' real (ver harness::CasoTest/CasoTestMulti en test_harness.h),
// confirmando el comportamiento observable de punta a punta — incluida la
// interacción con módulos (exportar/importar, Fase 30), que este plan
// documenta como "sin cambios esperados en ResolutorModulos".

#include "test_harness.h"

static const harness::CasoTest CASOS[] = {

    { "gen_identidad_inferida",
      "funcion identidad<T>(x: T): T\n"
      "    retornar x\n"
      "fin\n"
      "escribir(identidad(5))\n"
      "escribir(identidad(\"hola\"))\n",
      "5 hola" },

    { "gen_turbofish_explicito",
      "funcion identidad<T>(x: T): T\n"
      "    retornar x\n"
      "fin\n"
      "escribir(identidad::<cadena>(\"chau\"))\n",
      "chau" },

    { "gen_bound_satisfecho_metodo",
      "interfaz Comparable\n"
      "    funcion compararCon(otroValor: Comparable): numero\n"
      "fin\n"
      "clase Entero implementa Comparable\n"
      "    publico valor: numero\n"
      "    funcion Entero(valor: numero)\n"
      "        este.valor = valor\n"
      "    fin\n"
      "    publico funcion compararCon(otroValor: Comparable): numero\n"
      "        retornar este.valor - otroValor.valor\n"
      "    fin\n"
      "fin\n"
      "funcion maximo<T: Comparable>(a: T, b: T): T\n"
      "    retornar a.compararCon(b) >= 0 ? a : b\n"
      "fin\n"
      "x = nuevo Entero(3)\n"
      "y = nuevo Entero(7)\n"
      "escribir(maximo::<Entero>(x, y).valor)\n",
      "7" },

    { "gen_clausula_donde",
      "interfaz Comparable\n"
      "    funcion compararCon(otroValor: Comparable): numero\n"
      "fin\n"
      "clase Entero implementa Comparable\n"
      "    publico valor: numero\n"
      "    funcion Entero(valor: numero)\n"
      "        este.valor = valor\n"
      "    fin\n"
      "    publico funcion compararCon(otroValor: Comparable): numero\n"
      "        retornar este.valor - otroValor.valor\n"
      "    fin\n"
      "fin\n"
      "funcion maximo<T>(a: T, b: T): T donde T: Comparable\n"
      "    retornar a.compararCon(b) >= 0 ? a : b\n"
      "fin\n"
      "x = nuevo Entero(9)\n"
      "y = nuevo Entero(4)\n"
      "escribir(maximo::<Entero>(x, y).valor)\n",
      "9" },

    { "gen_pila_completa",
      "clase Pila<T>\n"
      "    privado items: lista\n"
      "    funcion Pila()\n"
      "        este.items = []\n"
      "    fin\n"
      "    publico funcion apilar(valor: T)\n"
      "        lista.agregar(este.items, valor)\n"
      "    fin\n"
      "    publico funcion cima(): T\n"
      "        retornar lista.ultimo(este.items)\n"
      "    fin\n"
      "fin\n"
      "p: Pila<numero> = nuevo Pila<numero>()\n"
      "p.apilar(1)\n"
      "p.apilar(2)\n"
      "escribir(p.cima())\n",
      "2" },

    { "gen_par_dos_parametros",
      "estructura Par<A, B>\n"
      "    primero: A\n"
      "    segundo: B\n"
      "    funcion Par(primero: A, segundo: B)\n"
      "        este.primero = primero\n"
      "        este.segundo = segundo\n"
      "    fin\n"
      "fin\n"
      "par = nuevo Par<cadena, numero>(\"edad\", 30)\n"
      "escribir(par.primero .. \": \" .. par.segundo)\n",
      "edad: 30" },
};

static const harness::CasoTestMulti CASOS_MULTI[] = {

    // PLAN_GENERICOS.md, "Interacción con módulos": una clase genérica
    // exportada e importada debe funcionar sin que ResolutorModulos sepa
    // nada de genéricos (mangla por nombre de declaración, no por firma).
    { "genmulti_pila_exportada",
      "importar { Pila } desde \"pila_gen_mm.lat\"\n"
      "p = nuevo Pila<numero>()\n"
      "p.apilar(1)\n"
      "p.apilar(2)\n"
      "escribir(p.cima())\n",
      "2",
      { { "pila_gen_mm.lat",
          "exportar clase Pila<T>\n"
          "    privado items: lista\n"
          "    funcion Pila()\n"
          "        este.items = []\n"
          "    fin\n"
          "    publico funcion apilar(valor: T)\n"
          "        lista.agregar(este.items, valor)\n"
          "    fin\n"
          "    publico funcion cima(): T\n"
          "        retornar lista.ultimo(este.items)\n"
          "    fin\n"
          "fin\n" } } },

    // Hallazgo real (auditoría de input/, ver PLAN_MODULOS.md/Reto de
    // "Interacción con módulos"): un "bound" (T: Comparable) que nombra una
    // interfaz importada de OTRO módulo -- a diferencia del caso anterior,
    // que solo importa una clase genérica sin bounds. ResolutorModulos no
    // manglaba ParametroGenerico::bounds, así que AnalizadorSemantico
    // comparaba "Comparable" (sin manglar) contra el registro, que solo
    // tiene la clave manglada -- "restricción genérica desconocida
    // 'Comparable'" en un programa por otro lado válido.
    { "genmulti_bound_interfaz_importada",
      "importar { Comparable } desde \"contrato_mm.lat\"\n"
      "clase Entero implementa Comparable\n"
      "    publico valor: numero\n"
      "    funcion Entero(valor: numero)\n"
      "        este.valor = valor\n"
      "    fin\n"
      "    publico funcion compararCon(otroValor: Comparable): numero\n"
      "        retornar este.valor - otroValor.valor\n"
      "    fin\n"
      "fin\n"
      "funcion maximo<T>(a: T, b: T): T donde T: Comparable\n"
      "    retornar a.compararCon(b) >= 0 ? a : b\n"
      "fin\n"
      "x = nuevo Entero(9)\n"
      "y = nuevo Entero(4)\n"
      "escribir(maximo::<Entero>(x, y).valor)\n",
      "9",
      { { "contrato_mm.lat",
          "exportar interfaz Comparable\n"
          "    funcion compararCon(otroValor: Comparable): numero\n"
          "fin\n" } } },
};

int main(int argc, char* argv[]) {
    int r1 = harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
    int r2 = harness::ejecutar_main_multi(argc, argv, CASOS_MULTI, std::size(CASOS_MULTI));
    return (r1 == 0 && r2 == 0) ? 0 : 1;
}
