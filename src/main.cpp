#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "interpreter.h"

int main() {
    std::string codigoFuente = "escribir('hola chat gpt')";
    
    // Paso 1: Análisis léxico
    Lexer lexer(codigoFuente);

    // Paso 2: Análisis sintáctico
    Parser parser(lexer);
    parser.parse();

    // Opción 1: Compilación
    Compiler compiler(parser);
    compiler.compile();
    // El código de salida se genera en este paso

    // Opción 2: Interpretación
    // Interpreter interpreter(parser);
    // interpreter.interpret();
    // Se ejecuta el código directamente durante la interpretación

    return 0;
}
