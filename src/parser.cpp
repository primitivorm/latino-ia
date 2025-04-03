// parser.cpp
#include <iostream>

#include "parser.h"

Parser::Parser(Lexer& lexer) : lexer(lexer) {
    currentToken = lexer.getNextToken();
}

void Parser::eat(TokenType type) {
    if (currentToken.type == type)
        currentToken = lexer.getNextToken();
    else {
        std::cerr << "Error de sintaxis: se esperaba " << static_cast<int>(type) << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Parser::parse() {
    programa();
    eat(TokenType::FinDeArchivo);
}

void Parser::programa() {
    // Regla para el inicio del programa
    // Aquí se deben reconocer las estructuras de tu lenguaje
    // como declaraciones, funciones, bucles, etc.
    // Por ejemplo:
    while (currentToken.type != TokenType::FinDeArchivo) {
        sentencia();
    }
}

void Parser::sentencia() {
    // Regla para reconocer una sentencia
    // Por ejemplo:
    if (currentToken.type == TokenType::Identificador) {
        // Aquí se podría tratar de analizar una asignación
        eat(TokenType::Identificador);
        eat(TokenType::Operador);
        expresion();
        eat(TokenType::Delimitador);
    } else {
        std::cerr << "Error de sintaxis: sentencia no válida" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Parser::expresion() {
    // Regla para reconocer una expresión
    // Por ejemplo, si tu lenguaje permite expresiones aritméticas
    // podrías implementar esta parte aquí.
    // Agrega las reglas necesarias para la gramática de tu lenguaje.
}
