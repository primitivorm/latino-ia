// compiler.cpp

#include "compiler.h"

Compiler::Compiler(Parser& parser) : parser(parser) {}

void Compiler::compile() {
    parser.parse();
    generarCodigo();
}

void Compiler::generarCodigo() {
    // Implementa aquí la lógica para generar el código de salida.
    // Puedes utilizar LLVM o cualquier otro enfoque según tus necesidades.
    // Por ejemplo, podrías crear un archivo de texto con el código de salida
    // o generar código de máquina directamente.
    // Agrega las funciones necesarias para traducir las estructuras del lenguaje
    // en código de salida en el formato deseado.
}
