// lexer.h

#ifndef LEXER_H
#define LEXER_H

#include <string>

enum class TokenType {
    Identificador,     // nombre, _x, calificacion
    Entero,            // 10, 42
    Flotante,          // 3.14159
    Cadena,            // "hola", 'A'  (el lexema NO incluye las comillas)
    PalabraReservada,  // si, sino, desde, funcion, ...
    Operador,          // + - * / % ^  && ||  .. ++ --  == != < > <= >= ~=  =  .  ? :  ...
    Delimitador,       // ( ) [ ] { } , ;
    FinDeLinea,        // salto(s) de línea: terminador de sentencia en Latino
    FinDeArchivo       // fin del código fuente
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
    // Devuelve el carácter en currentPosition y avanza. '\0' al final.
    char getNextChar();
    // Mira un carácter sin consumirlo. offset 0 = el siguiente a leer.
    char peekChar(int offset = 0) const;
    // Retrocede una posición (usado para "devolver" el carácter terminador).
    void retractChar();

    // Salta espacios, tabuladores, '\r' y comentarios (# // /* */).
    // NO salta '\n' (los saltos de línea producen tokens FinDeLinea).
    void skipEspaciosYComentarios();

    void reportError(const std::string& message, int line);
    bool esPalabraReservada(const std::string& palabra) const;

    std::string sourceCode;
    int currentPosition;
    int currentLine;
};

#endif  // LEXER_H
