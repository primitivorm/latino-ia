// parser.h

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

class Parser {
public:
    Parser(Lexer& lexer);

    void parse();

private:
    Lexer& lexer;
    Token currentToken;

    void eat(TokenType type);
    void programa();
    void sentencia();
    void expresion();
    // Agrega aquí más funciones para las reglas gramaticales
};

#endif  // PARSER_H
