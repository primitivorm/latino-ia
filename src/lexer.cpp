// lexer.cpp

#include <iostream>
#include <set>
#include "lexer.h"

Lexer::Lexer(const std::string& sourceCode)
    : sourceCode(sourceCode), currentPosition(0), currentLine(1) {}

Token Lexer::getNextToken() {
    std::string lexeme;
    char currentChar = getNextChar();

    while (currentChar != '\0') {
        // Lógica de análisis léxico
        if (isalpha(currentChar)) {
            lexeme += currentChar;

            // Identificador o palabra reservada
            while (isalnum(currentChar = getNextChar()))
                lexeme += currentChar;

            // Verificar si es una palabra reservada
            if (esPalabraReservada(lexeme))
                return Token{TokenType::PalabraReservada, lexeme, currentLine};
            else
                return Token{TokenType::Identificador, lexeme, currentLine};
        } else if (isdigit(currentChar)) {
            lexeme += currentChar;

            // Entero o flotante
            while (isdigit(currentChar = getNextChar()))
                lexeme += currentChar;

            // Flotante (opcional)
            if (currentChar == '.') {
                lexeme += currentChar;
                while (isdigit(currentChar = getNextChar()))
                    lexeme += currentChar;

                return Token{TokenType::Flotante, lexeme, currentLine};
            } else {
                return Token{TokenType::Entero, lexeme, currentLine};
            }
        } else if (currentChar == '\"') {
            lexeme += currentChar;

            // Cadena de caracteres
            while ((currentChar = getNextChar()) != '\"') {
                if (currentChar == '\0') {
                    reportError("Cadena de caracteres no terminada", currentLine);
                    return Token{TokenType::FinDeArchivo, "", currentLine};
                }

                lexeme += currentChar;
            }

            lexeme += currentChar;
            return Token{TokenType::Cadena, lexeme, currentLine};
        } else if (currentChar == '+') {
            // Operador +
            return Token{TokenType::Operador, "+", currentLine};
        } else if (currentChar == ';') {
            // Delimitador ;
            return Token{TokenType::Delimitador, ";", currentLine};
        } else if (currentChar == '\n') {
            // Incrementar número de línea
            currentLine++;
        } else if (currentChar != ' ' && currentChar != '\t') {
            reportError("Caracter no reconocido", currentLine);
            return Token{TokenType::FinDeArchivo, "", currentLine};
        }

        currentChar = getNextChar();
    }

    return Token{TokenType::FinDeArchivo, "", currentLine};
}

char Lexer::getNextChar() {
    if (currentPosition < sourceCode.length())
        return sourceCode[currentPosition++];
    else
        return '\0';
}

void Lexer::retractChar() {
    if (currentPosition > 0)
        currentPosition--;
}

void Lexer::reportError(const std::string& message, int line) {
    std::cerr << "Error en línea " << line << ": " << message << std::endl;
}

bool Lexer::esPalabraReservada(const std::string& palabra) {
    // Implementa aquí la lógica para determinar si una palabra es una palabra reservada.
    // Puedes comparar la palabra con una lista predefinida de palabras reservadas
    // o utilizar alguna estructura de datos eficiente, como un conjunto (set), para
    // buscar rápidamente si la palabra está en la lista.

    // Ejemplo básico:
    static const std::set<std::string> palabrasReservadas = {
        "caso", "cierto", "verdadero", "defecto", "otro", "desde", "elegir", "falso", "fin",
        "funcion", "fun", "global", "hasta", "mientras", "nulo", "para", "repetir",
        "regresar", "retornar", "ret", "romper", "si", "sino", "osi"
    };

    return palabrasReservadas.count(palabra) > 0;
}