// test_poo_e2e.cpp — Pruebas de extremo a extremo de POO.
// Compila fragmentos .lat con clases/estructuras/herencia/interfaces,
// ejecuta el binario resultante y compara la salida.

#include "test_harness.h"

static const harness::CasoTest CASOS[] = {

    { "poo_clase_simple",
      "clase Saludo\n"
      "    publico nombre: cadena\n"
      "    funcion Saludo(nombre: cadena)\n"
      "        este.nombre = nombre\n"
      "    fin\n"
      "    publico funcion decir(): cadena\n"
      "        retornar \"Hola, \" .. este.nombre\n"
      "    fin\n"
      "fin\n"
      "s = nuevo Saludo(\"Mundo\")\n"
      "escribir(s.decir())\n",
      "Hola, Mundo" },

    { "poo_herencia_polimorfismo",
      "clase Animal\n"
      "    publico nombre: cadena\n"
      "    funcion Animal(nombre: cadena)\n"
      "        este.nombre = nombre\n"
      "    fin\n"
      "    publico funcion hablar(): cadena\n"
      "        retornar este.nombre .. \" hace un sonido\"\n"
      "    fin\n"
      "fin\n"
      "clase Perro extiende Animal\n"
      "    funcion Perro(nombre: cadena)\n"
      "        base(nombre)\n"
      "    fin\n"
      "    publico funcion hablar(): cadena sobreescribir\n"
      "        retornar este.nombre .. \" dice: Guau\"\n"
      "    fin\n"
      "fin\n"
      "p = nuevo Perro(\"Rex\")\n"
      "escribir(p.hablar())\n"
      "si p es Animal\n"
      "    escribir(\"Es un animal\")\n"
      "fin\n",
      "Rex dice: Guau Es un animal" },

    { "poo_metodo_estatico",
      "clase Animal\n"
      "    publico nombre: cadena\n"
      "    funcion Animal(nombre: cadena)\n"
      "        este.nombre = nombre\n"
      "    fin\n"
      "    publico funcion hablar(): cadena\n"
      "        retornar este.nombre .. \" hace un sonido\"\n"
      "    fin\n"
      "    estatico funcion crear(nombre: cadena): Animal\n"
      "        retornar nuevo Animal(nombre)\n"
      "    fin\n"
      "fin\n"
      "cachorro = Animal.crear(\"Luna\")\n"
      "escribir(cachorro.hablar())\n",
      "Luna hace un sonido" },

    { "poo_interfaces",
      "interfaz IImprimible\n"
      "    funcion aCadena(): cadena\n"
      "fin\n"
      "clase Producto implementa IImprimible\n"
      "    publico nombre: cadena\n"
      "    publico precio: numero\n"
      "    funcion Producto(nombre: cadena, precio: numero)\n"
      "        este.nombre = nombre\n"
      "        este.precio = precio\n"
      "    fin\n"
      "    publico funcion aCadena(): cadena\n"
      "        retornar este.nombre .. \": \" .. este.precio\n"
      "    fin\n"
      "fin\n"
      "p = nuevo Producto(\"Pan\", 10)\n"
      "escribir(p.aCadena())\n",
      "Pan: 10" },

    { "poo_estructura",
      "estructura Punto\n"
      "    x: numero\n"
      "    y: numero\n"
      "    funcion Punto(x: numero, y: numero)\n"
      "        este.x = x\n"
      "        este.y = y\n"
      "    fin\n"
      "    funcion suma(): numero\n"
      "        retornar este.x + este.y\n"
      "    fin\n"
      "fin\n"
      "p = nuevo Punto(3, 4)\n"
      "escribir(p.suma())\n",
      "7" },

    { "poo_clase_abstracta_polimorfismo",
      "abstracto clase Figura\n"
      "    abstracto funcion area(): numero\n"
      "    publico funcion describir(): cadena\n"
      "        retornar \"Area: \" .. este.area()\n"
      "    fin\n"
      "fin\n"
      "clase Cuadrado extiende Figura\n"
      "    publico lado: numero\n"
      "    funcion Cuadrado(lado: numero)\n"
      "        base()\n"
      "        este.lado = lado\n"
      "    fin\n"
      "    publico funcion area(): numero sobreescribir\n"
      "        retornar este.lado * este.lado\n"
      "    fin\n"
      "fin\n"
      "c = nuevo Cuadrado(5)\n"
      "escribir(c.describir())\n",
      "Area: 25" },
};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
