// compiler.h

#ifndef COMPILER_H
#define COMPILER_H

#include "parser.h"

class Compiler {
public:
    Compiler(Parser& parser);

    void compile();

private:
    Parser& parser;

    void generarCodigo();
    // Agrega aquí más funciones para generar código de salida
};

#endif  // COMPILER_H
