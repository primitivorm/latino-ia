// lexer.h

#ifndef LEXER_H
#define LEXER_H

#include <string>

enum class TokenType {
    Identificador,
    Entero,
    Flotante,
    Cadena,
    PalabraReservada,
    Operador,
    Delimitador,
    FinDeArchivo
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
};

class Lexer {
public:
    Lexer(const std::string& sourceCode);

    Token getNextToken();

private:
    char getNextChar();
    void retractChar();
    void reportError(const std::string& message, int line);

    std::string sourceCode;
    int currentPosition;
    int currentLine;
    bool esPalabraReservada(const std::string& palabra);
};

#endif  // LEXER_H
