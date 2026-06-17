// lexer.cpp

#include <iostream>
#include <cctype>
#include <set>
#include "lexer.h"

Lexer::Lexer(const std::string& sourceCode)
    : sourceCode(sourceCode), currentPosition(0), currentLine(1) {}

char Lexer::getNextChar() {
    if (currentPosition < static_cast<int>(sourceCode.length()))
        return sourceCode[currentPosition++];
    return '\0';
}

char Lexer::peekChar(int offset) const {
    int pos = currentPosition + offset;
    if (pos >= 0 && pos < static_cast<int>(sourceCode.length()))
        return sourceCode[pos];
    return '\0';
}

void Lexer::retractChar() {
    if (currentPosition > 0)
        currentPosition--;
}

void Lexer::skipEspaciosYComentarios() {
    for (;;) {
        char c = peekChar();
        if (c == ' ' || c == '\t' || c == '\r') {
            getNextChar();
        } else if (c == '#') {
            // Comentario de línea estilo Python: hasta el fin de línea.
            while (peekChar() != '\n' && peekChar() != '\0')
                getNextChar();
        } else if (c == '/' && peekChar(1) == '/') {
            // Comentario de línea estilo C.
            while (peekChar() != '\n' && peekChar() != '\0')
                getNextChar();
        } else if (c == '/' && peekChar(1) == '*') {
            // Comentario multilínea estilo C: /* ... */
            getNextChar();  // '/'
            getNextChar();  // '*'
            while (!(peekChar() == '*' && peekChar(1) == '/')) {
                if (peekChar() == '\0') {
                    reportError("Comentario multilínea no terminado", currentLine);
                    return;
                }
                if (peekChar() == '\n')
                    currentLine++;
                getNextChar();
            }
            getNextChar();  // '*'
            getNextChar();  // '/'
        } else {
            break;
        }
    }
}

Token Lexer::getNextToken() {
    skipEspaciosYComentarios();

    char c = peekChar();

    if (c == '\0')
        return Token{TokenType::FinDeArchivo, "", currentLine};

    // Salto(s) de línea: terminador de sentencia. Se colapsan los saltos
    // consecutivos (líneas en blanco y comentarios intermedios) en un solo token.
    if (c == '\n') {
        int line = currentLine;
        do {
            getNextChar();  // consume el '\n'
            currentLine++;
            skipEspaciosYComentarios();
        } while (peekChar() == '\n');
        return Token{TokenType::FinDeLinea, "\\n", line};
    }

    int startLine = currentLine;
    c = getNextChar();

    // Identificador o palabra reservada: letra o '_' inicial.
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        std::string lexeme(1, c);
        while (true) {
            char nc = peekChar();
            if (std::isalnum(static_cast<unsigned char>(nc)) || nc == '_')
                lexeme += getNextChar();
            else
                break;
        }
        if (esPalabraReservada(lexeme))
            return Token{TokenType::PalabraReservada, lexeme, startLine};
        return Token{TokenType::Identificador, lexeme, startLine};
    }

    // Número: entero o flotante.
    if (std::isdigit(static_cast<unsigned char>(c))) {
        std::string lexeme(1, c);
        while (std::isdigit(static_cast<unsigned char>(peekChar())))
            lexeme += getNextChar();

        // Punto decimal sólo si va seguido de un dígito; así "5 .. x" no se
        // confunde con un flotante y el operador de concatenación "..".
        if (peekChar() == '.' && std::isdigit(static_cast<unsigned char>(peekChar(1)))) {
            lexeme += getNextChar();  // '.'
            while (std::isdigit(static_cast<unsigned char>(peekChar())))
                lexeme += getNextChar();
            return Token{TokenType::Flotante, lexeme, startLine};
        }
        return Token{TokenType::Entero, lexeme, startLine};
    }

    // Cadena: admite comillas dobles (") y simples ('). El lexema NO incluye
    // las comillas. Las secuencias de escape (\n, \", \\, ...) se conservan tal cual.
    if (c == '"' || c == '\'') {
        char quote = c;
        std::string lexeme;
        for (;;) {
            char ch = getNextChar();
            if (ch == '\0' || ch == '\n') {
                reportError("Cadena de caracteres no terminada", startLine);
                return Token{TokenType::FinDeArchivo, "", startLine};
            }
            if (ch == '\\') {
                char esc = getNextChar();
                if (esc == '\0') {
                    reportError("Cadena de caracteres no terminada", startLine);
                    return Token{TokenType::FinDeArchivo, "", startLine};
                }
                lexeme += '\\';
                lexeme += esc;
                continue;
            }
            if (ch == quote)
                break;
            lexeme += ch;
        }
        return Token{TokenType::Cadena, lexeme, startLine};
    }

    // Delimitadores.
    switch (c) {
        case '(': case ')': case '[': case ']':
        case '{': case '}': case ',': case ';':
            return Token{TokenType::Delimitador, std::string(1, c), startLine};
    }

    // Operadores (incluidos los de varios caracteres).
    switch (c) {
        case '+':
            if (peekChar() == '+') { getNextChar(); return Token{TokenType::Operador, "++", startLine}; }
            return Token{TokenType::Operador, "+", startLine};
        case '-':
            if (peekChar() == '-') { getNextChar(); return Token{TokenType::Operador, "--", startLine}; }
            return Token{TokenType::Operador, "-", startLine};
        case '*':
            return Token{TokenType::Operador, "*", startLine};
        case '/':
            // Los comentarios ya fueron filtrados; aquí '/' es división.
            return Token{TokenType::Operador, "/", startLine};
        case '%':
            return Token{TokenType::Operador, "%", startLine};
        case '^':
            return Token{TokenType::Operador, "^", startLine};
        case '=':
            if (peekChar() == '=') { getNextChar(); return Token{TokenType::Operador, "==", startLine}; }
            return Token{TokenType::Operador, "=", startLine};
        case '!':
            if (peekChar() == '=') { getNextChar(); return Token{TokenType::Operador, "!=", startLine}; }
            break;
        case '<':
            if (peekChar() == '=') { getNextChar(); return Token{TokenType::Operador, "<=", startLine}; }
            return Token{TokenType::Operador, "<", startLine};
        case '>':
            if (peekChar() == '=') { getNextChar(); return Token{TokenType::Operador, ">=", startLine}; }
            return Token{TokenType::Operador, ">", startLine};
        case '~':
            if (peekChar() == '=') { getNextChar(); return Token{TokenType::Operador, "~=", startLine}; }
            break;
        case '&':
            if (peekChar() == '&') { getNextChar(); return Token{TokenType::Operador, "&&", startLine}; }
            break;
        case '|':
            if (peekChar() == '|') { getNextChar(); return Token{TokenType::Operador, "||", startLine}; }
            break;
        case '.':
            if (peekChar() == '.') {
                getNextChar();  // segundo '.'
                if (peekChar() == '.') { getNextChar(); return Token{TokenType::Operador, "...", startLine}; }
                return Token{TokenType::Operador, "..", startLine};
            }
            return Token{TokenType::Operador, ".", startLine};
        case '?':
            return Token{TokenType::Operador, "?", startLine};
        case ':':
            return Token{TokenType::Operador, ":", startLine};
    }

    reportError(std::string("Caracter no reconocido '") + c + "'", startLine);
    return Token{TokenType::FinDeArchivo, "", startLine};
}

void Lexer::reportError(const std::string& message, int line) {
    std::cerr << "Error en línea " << line << ": " << message << std::endl;
}

bool Lexer::esPalabraReservada(const std::string& palabra) const {
    static const std::set<std::string> palabrasReservadas = {
        "caso", "cierto", "verdadero", "defecto", "otro", "desde", "elegir", "falso", "fin",
        "funcion", "fun", "global", "hasta", "incluir", "mientras", "nulo", "para", "repetir",
        "regresar", "retornar", "ret", "romper", "si", "sino", "osi"
    };

    return palabrasReservadas.count(palabra) > 0;
}
