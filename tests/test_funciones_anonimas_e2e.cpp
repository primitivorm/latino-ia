#include "test_harness.h"

static const harness::CasoTest CASOS[] = {
    { "anonima_variable",
      "f = fun(x)\n"
      "    retornar x + 1\n"
      "fin\n"
      "escribir(f(41))\n",
      "42" },

    { "anonima_directa",
      "escribir((fun(x) retornar x + 1 fin)(41))\n",
      "42" },

    { "anonima_lista",
      "funciones = [fun(a, b) retornar a + b fin, fun(a, b) retornar a * b fin]\n"
      "escribir(funciones[0](1, 2))\n"
      "escribir(funciones[1](2, 3))\n",
      "3 6" },

    { "anonima_anidada",
      "externa = fun(x)\n"
      "    interna = fun(y) retornar y + 1 fin\n"
      "    retornar interna(x)\n"
      "fin\n"
      "escribir(externa(41))\n",
      "42" },

    { "anonima_en_funcion",
      "funcion aplicar(x)\n"
      "    interna = fun(y) retornar y + 1 fin\n"
      "    retornar interna(x)\n"
      "fin\n"
      "escribir(aplicar(41))\n",
      "42" },

    { "anonima_en_metodo",
      "clase Calculadora\n"
      "    publico funcion aplicar(x)\n"
      "        interna = fun(y) retornar y + 1 fin\n"
      "        retornar interna(x)\n"
      "    fin\n"
      "fin\n"
      "calculadora = nuevo Calculadora()\n"
      "escribir(calculadora.aplicar(41))\n",
      "42" },

    { "anonima_usa_global",
      "desplazamiento = 40\n"
      "f = fun(x) retornar desplazamiento + x fin\n"
      "escribir(f(2))\n",
      "42" },

    { "anonima_parametro_ausente",
      "f = fun(x, y) retornar tipo(y) fin\n"
      "escribir(f(1))\n",
      "nulo" },

    { "anonima_variadica",
      "f = fun(x, ...) retornar [...] fin\n"
      "resto = f(0, 7, 9)\n"
      "escribir(resto[0])\n"
      "escribir(resto[1])\n"
      "escribir(lista.longitud(f(0)))\n",
      "7 9 0" },

    { "anonima_retorno_implicito",
      "f = fun() x = 1 fin\n"
      "escribir(tipo(f()))\n",
      "nulo" },

    { "anonima_en_bucle_para",
      "para i en rango(3)\n"
      "    f = fun(x) retornar x + 1 fin\n"
      "    escribir(f(i))\n"
      "fin\n",
      "1 2 3" },

    { "diccionario_con_funcion_y_longitud_lista",
      "funcion test_accion(arg1)\n"
      "    poner(\"test_accion: \" .. arg1)\n"
      "fin\n"
      "funcion id_accion(arg1)\n"
      "    poner(\"id_accion: \" .. arg1)\n"
      "fin\n"
      "triggers = [\n"
      "    {\"comando\" : \"/test\", \"accion\": test_accion},\n"
      "    {\"comando\" : \"/id\", \"accion\": id_accion}\n"
      "]\n"
      "funcion ejecutar(texto)\n"
      "    desde (i=0; i < lista.longitud(triggers); i++)\n"
      "        si texto == triggers[i].comando\n"
      "            fn = triggers[i].accion\n"
      "            fn(\"enviando desde triggers\")\n"
      "            triggers[i].accion(\"enviando desde triggers\")\n"
      "        fin\n"
      "    fin\n"
      "fin\n"
      "ejecutar(\"/test\")\n"
      "ejecutar(\"/id\")\n",
      "test_accion: enviando desde triggers\n"
      "test_accion: enviando desde triggers\n"
      "id_accion: enviando desde triggers\n"
      "id_accion: enviando desde triggers" },

    { "lista_longitud_como_metodo",
      "l = [1, 2, 3]\n"
      "escribir(l.longitud())\n",
      "3" },
};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
