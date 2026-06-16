#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "analizador_semantico.h"
#include "ast_impresor.h"
#include "lexer.h"
#include "parser.h"

// Driver mínimo (la CLI completa llega en la Fase 7).
// Uso:
//   latino <archivo.lat>   -> parsea el archivo y vuelca su AST
//   latino                 -> parsea un programa de demostración
int main(int argc, char** argv) {
    std::string codigoFuente;

    if (argc >= 2) {
        std::ifstream archivo(argv[1], std::ios::binary);
        if (!archivo) {
            std::cerr << "No se pudo abrir el archivo: " << argv[1] << std::endl;
            return 1;
        }
        std::stringstream ss;
        ss << archivo.rdbuf();
        codigoFuente = ss.str();
    } else {
        codigoFuente = "escribir(\"hola mundo\")\n";
    }

    // Análisis léxico + sintáctico.
    Lexer lexer(codigoFuente);
    Parser parser(lexer);
    std::unique_ptr<Programa> programa = parser.parse();

    if (!programa)
        return 1;  // hubo un error de sintaxis (ya reportado)

    // Análisis semántico.
    AnalizadorSemantico semantico;
    if (!semantico.analizar(*programa))
        return 1;  // hubo errores semánticos (ya reportados)

    // Por ahora volcamos el AST. La generación de código C llega en la Fase 5.
    ImpresorAST impresor(std::cout);
    impresor.imprimir(*programa);

    return 0;
}
