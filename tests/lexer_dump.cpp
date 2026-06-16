// Utilidad de verificación de la Fase 1: vuelca la secuencia de tokens de un
// archivo .lat. No forma parte del compilador final.
//
// Uso: lexer_dump <archivo.lat>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "lexer.h"

static const char* nombreTipo(TokenType t) {
    switch (t) {
        case TokenType::Identificador:    return "Identificador";
        case TokenType::Entero:           return "Entero";
        case TokenType::Flotante:         return "Flotante";
        case TokenType::Cadena:           return "Cadena";
        case TokenType::PalabraReservada: return "PalabraReservada";
        case TokenType::Operador:         return "Operador";
        case TokenType::Delimitador:      return "Delimitador";
        case TokenType::FinDeLinea:       return "FinDeLinea";
        case TokenType::FinDeArchivo:     return "FinDeArchivo";
    }
    return "?";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: lexer_dump <archivo.lat>" << std::endl;
        return 1;
    }

    std::ifstream f(argv[1], std::ios::binary);
    if (!f) {
        std::cerr << "No se pudo abrir: " << argv[1] << std::endl;
        return 1;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string codigo = ss.str();

    Lexer lexer(codigo);
    for (;;) {
        Token tok = lexer.getNextToken();
        std::cout << "L" << tok.line << "  " << nombreTipo(tok.type)
                  << "  [" << tok.lexeme << "]" << std::endl;
        if (tok.type == TokenType::FinDeArchivo)
            break;
    }
    return 0;
}
