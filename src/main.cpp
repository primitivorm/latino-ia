#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "analizador_semantico.h"
#include "ast_impresor.h"
#include "compiler.h"
#include "lexer.h"
#include "parser.h"

// Driver del compilador (la CLI completa, con compilación a ejecutable, llega
// en la Fase 7).
// Uso:
//   latino <archivo.lat>          -> emite el código C equivalente por stdout
//   latino <archivo.lat> --ast    -> vuelca el AST (depuración)
int main(int argc, char** argv) {
    std::string ruta;
    bool modoAst = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--ast")
            modoAst = true;
        else if (ruta.empty())
            ruta = arg;
    }

    std::string codigoFuente;
    if (!ruta.empty()) {
        std::ifstream archivo(ruta, std::ios::binary);
        if (!archivo) {
            std::cerr << "No se pudo abrir el archivo: " << ruta << std::endl;
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
        return 1;  // error de sintaxis (ya reportado)

    // Análisis semántico.
    AnalizadorSemantico semantico;
    if (!semantico.analizar(*programa))
        return 1;  // errores semánticos (ya reportados)

    if (modoAst) {
        ImpresorAST impresor(std::cout);
        impresor.imprimir(*programa);
    } else {
        GeneradorC generador;
        std::cout << generador.generar(*programa);
    }

    return 0;
}
