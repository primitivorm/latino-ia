// interpreter.cpp

#include "interpreter.h"

Interpreter::Interpreter(Parser& parser) : parser(parser) {}

void Interpreter::interpret() {
    parser.parse();
    ejecutarSentencia();
}

void Interpreter::ejecutarSentencia() {
    // Implementa aquí la lógica para ejecutar las sentencias reconocidas por el parser.
    // Puedes utilizar una estructura de datos o una pila de ejecución para mantener
    // el estado del programa y realizar las operaciones necesarias según las reglas
    // de tu lenguaje. Por ejemplo, puedes evaluar expresiones, asignar variables,
    // ejecutar bucles y llamadas a funciones, etc.
    // Agrega las funciones necesarias para ejecutar las diferentes estructuras del lenguaje.
}
