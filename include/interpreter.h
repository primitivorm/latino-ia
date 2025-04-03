// interpreter.h

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "parser.h"

class Interpreter {
public:
    Interpreter(Parser& parser);

    void interpret();

private:
    Parser& parser;

    void ejecutarSentencia();
    // Agrega aquí más funciones para ejecutar las estructuras del lenguaje
};

#endif  // INTERPRETER_H
