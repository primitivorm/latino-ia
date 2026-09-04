// test_lexer.cpp
//
// Pruebas unitarias del Lexer (Fase 1). Framework mínimo, sin dependencias:
// cada CHECK incrementa un contador y reporta los fallos. El programa retorna
// un código distinto de cero si alguna comprobación falla (lo usa CTest).

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lexer.h"

// ---------------------------------------------------------------------------
// Framework mínimo de aserciones
// ---------------------------------------------------------------------------
static int g_fallos = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_fallos;                                                        \
            std::cerr << "  FALLO [linea " << __LINE__ << "]: " << msg << "\n"; \
        }                                                                      \
    } while (0)

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

// Tokeniza la fuente. La lista resultante termina siempre con FinDeArchivo.
// Si incluirSaltos == false, se omiten los tokens FinDeLinea.
static std::vector<Token> lex(const std::string& src, bool incluirSaltos = false) {
    Lexer lexer(src);
    std::vector<Token> out;
    for (;;) {
        Token t = lexer.getNextToken();
        if (t.type == TokenType::FinDeArchivo) {
            out.push_back(t);
            break;
        }
        if (!incluirSaltos && t.type == TokenType::FinDeLinea)
            continue;
        out.push_back(t);
    }
    return out;
}

struct Esperado {
    TokenType tipo;
    std::string lexema;
};

// Compara la secuencia de tokens (sin contar el FinDeArchivo final) con lo esperado.
static void esperarSecuencia(const std::string& nombre,
                             const std::vector<Token>& toks,
                             const std::vector<Esperado>& esp) {
    size_t n = toks.empty() ? 0 : toks.size() - 1;  // excluye FinDeArchivo
    CHECK(n == esp.size(),
          nombre << ": numero de tokens esperado=" << esp.size() << " real=" << n);
    size_t m = std::min(n, esp.size());
    for (size_t i = 0; i < m; ++i) {
        CHECK(toks[i].type == esp[i].tipo,
              nombre << ": token " << i << " tipo esperado=" << nombreTipo(esp[i].tipo)
                     << " real=" << nombreTipo(toks[i].type)
                     << " (lexema='" << toks[i].lexeme << "')");
        CHECK(toks[i].lexeme == esp[i].lexema,
              nombre << ": token " << i << " lexema esperado='" << esp[i].lexema
                     << "' real='" << toks[i].lexeme << "'");
    }
}

// ---------------------------------------------------------------------------
// Pruebas
// ---------------------------------------------------------------------------

static void prueba_identificadores_y_reservadas() {
    auto t = lex("nombre _x Mensaje x1 si sino funcion fun nulo verdadero falso");
    esperarSecuencia("identificadores", t, {
        {TokenType::Identificador, "nombre"},
        {TokenType::Identificador, "_x"},
        {TokenType::Identificador, "Mensaje"},
        {TokenType::Identificador, "x1"},
        {TokenType::PalabraReservada, "si"},
        {TokenType::PalabraReservada, "sino"},
        {TokenType::PalabraReservada, "funcion"},
        {TokenType::PalabraReservada, "fun"},
        {TokenType::PalabraReservada, "nulo"},
        {TokenType::PalabraReservada, "verdadero"},
        {TokenType::PalabraReservada, "falso"},
    });
}

static void prueba_numeros() {
    // Incluye la desambiguación de "5..6" (Entero, "..", Entero) frente a "5.6".
    auto t = lex("10 3.14159 5.6 5..6 0 123)");
    esperarSecuencia("numeros", t, {
        {TokenType::Entero, "10"},
        {TokenType::Flotante, "3.14159"},
        {TokenType::Flotante, "5.6"},
        {TokenType::Entero, "5"},
        {TokenType::Operador, ".."},
        {TokenType::Entero, "6"},
        {TokenType::Entero, "0"},
        {TokenType::Entero, "123"},
        {TokenType::Delimitador, ")"},
    });
}

static void prueba_cadenas() {
    // Comillas dobles, simples y cadena vacía. El lexema NO incluye las comillas.
    auto t = lex("\"hola\" 'A' \"\"");
    esperarSecuencia("cadenas", t, {
        {TokenType::Cadena, "hola"},
        {TokenType::Cadena, "A"},
        {TokenType::Cadena, ""},
    });
}

static void prueba_cadena_con_escape() {
    // Fuente Latino:  "a\"b"   ->  lexema:  a\"b   (la barra se conserva)
    auto t = lex("\"a\\\"b\"");
    esperarSecuencia("cadena_escape", t, {
        {TokenType::Cadena, "a\\\"b"},
    });
}

static void prueba_cadena_no_terminada() {
    // Debe reportar un error y devolver FinDeArchivo. Capturamos std::cerr.
    std::ostringstream capturado;
    std::streambuf* viejo = std::cerr.rdbuf(capturado.rdbuf());
    auto t = lex("\"abc");
    std::cerr.rdbuf(viejo);

    CHECK(t.size() == 1 && t[0].type == TokenType::FinDeArchivo,
          "cadena_no_terminada: debe devolver solo FinDeArchivo");
    CHECK(capturado.str().find("no terminada") != std::string::npos,
          "cadena_no_terminada: debe reportar 'no terminada'");
}

static void prueba_operadores() {
    auto t = lex("+ - * / % ^ ++ -- == != < > <= >= ~= && || .. ... . ? : =");
    esperarSecuencia("operadores", t, {
        {TokenType::Operador, "+"},  {TokenType::Operador, "-"},
        {TokenType::Operador, "*"},  {TokenType::Operador, "/"},
        {TokenType::Operador, "%"},  {TokenType::Operador, "^"},
        {TokenType::Operador, "++"}, {TokenType::Operador, "--"},
        {TokenType::Operador, "=="}, {TokenType::Operador, "!="},
        {TokenType::Operador, "<"},  {TokenType::Operador, ">"},
        {TokenType::Operador, "<="}, {TokenType::Operador, ">="},
        {TokenType::Operador, "~="}, {TokenType::Operador, "&&"},
        {TokenType::Operador, "||"}, {TokenType::Operador, ".."},
        {TokenType::Operador, "..."},{TokenType::Operador, "."},
        {TokenType::Operador, "?"},  {TokenType::Operador, ":"},
        {TokenType::Operador, "="},
    });
}

static void prueba_delimitadores() {
    auto t = lex("( ) [ ] { } , ;");
    esperarSecuencia("delimitadores", t, {
        {TokenType::Delimitador, "("}, {TokenType::Delimitador, ")"},
        {TokenType::Delimitador, "["}, {TokenType::Delimitador, "]"},
        {TokenType::Delimitador, "{"}, {TokenType::Delimitador, "}"},
        {TokenType::Delimitador, ","}, {TokenType::Delimitador, ";"},
    });
}

static void prueba_comentarios() {
    // Comentario de línea (#, //) y multilínea (/* */). Todos se descartan.
    std::string src =
        "# comentario python\n"
        "x = 1  // comentario C\n"
        "/* comentario\n"
        "   multilinea */ y = 2\n";
    auto t = lex(src);
    esperarSecuencia("comentarios", t, {
        {TokenType::Identificador, "x"}, {TokenType::Operador, "="}, {TokenType::Entero, "1"},
        {TokenType::Identificador, "y"}, {TokenType::Operador, "="}, {TokenType::Entero, "2"},
    });
}

static void prueba_conteo_de_lineas() {
    // 'x' está en la línea 2; 'y' en la línea 4 (tras el comentario multilínea).
    std::string src =
        "# comentario python\n"
        "x = 1  // comentario C\n"
        "/* comentario\n"
        "   multilinea */ y = 2\n";
    auto t = lex(src);
    // t[0]=x  t[1]==  t[2]=1  t[3]=y ...
    CHECK(t.size() >= 4, "conteo_lineas: faltan tokens");
    if (t.size() >= 4) {
        CHECK(t[0].lexeme == "x" && t[0].line == 2,
              "conteo_lineas: 'x' deberia estar en la linea 2 (real=" << t[0].line << ")");
        CHECK(t[3].lexeme == "y" && t[3].line == 4,
              "conteo_lineas: 'y' deberia estar en la linea 4 (real=" << t[3].line << ")");
    }
}

static void prueba_fin_de_linea_colapsa_blancos() {
    // Varias líneas en blanco consecutivas producen un solo FinDeLinea.
    std::string src = "a = 1\n\n\nb = 2\n";
    auto t = lex(src, /*incluirSaltos=*/true);

    int saltos = 0;
    for (const auto& tok : t)
        if (tok.type == TokenType::FinDeLinea) ++saltos;

    // Un salto tras "1" (que colapsa las 2 líneas en blanco) y otro tras "2".
    CHECK(saltos == 2, "fin_de_linea: se esperaban 2 FinDeLinea, real=" << saltos);
    CHECK(!t.empty() && t.back().type == TokenType::FinDeArchivo,
          "fin_de_linea: debe terminar con FinDeArchivo");
}

static void prueba_regresion_caracter_perdido() {
    // Regresión del bug original: el lexer consumía el caracter terminador de
    // identificadores/numeros sin retroceder, perdiendolo. Sin espacios entre
    // tokens, cada lexema debe quedar completo.
    auto t = lex("escribir(numeros[1])");
    esperarSecuencia("regresion_caracter_perdido", t, {
        {TokenType::Identificador, "escribir"},
        {TokenType::Delimitador, "("},
        {TokenType::Identificador, "numeros"},
        {TokenType::Delimitador, "["},
        {TokenType::Entero, "1"},
        {TokenType::Delimitador, "]"},
        {TokenType::Delimitador, ")"},
    });
}

static void prueba_concatenacion_vs_punto() {
    // a..b  -> concatenacion ;  a.b -> acceso a miembro ;  ...
    auto t = lex("a..b a.b");
    esperarSecuencia("concatenacion_vs_punto", t, {
        {TokenType::Identificador, "a"}, {TokenType::Operador, ".."}, {TokenType::Identificador, "b"},
        {TokenType::Identificador, "a"}, {TokenType::Operador, "."},  {TokenType::Identificador, "b"},
    });
}

static void prueba_palabras_reservadas_modulos() {
    // PLAN_MODULOS.md: exportar / importar / como.
    auto t = lex("exportar importar como");
    esperarSecuencia("palabras_reservadas_modulos", t, {
        {TokenType::PalabraReservada, "exportar"},
        {TokenType::PalabraReservada, "importar"},
        {TokenType::PalabraReservada, "como"},
    });
}

static void prueba_archivo_vacio() {
    auto t = lex("");
    CHECK(t.size() == 1 && t[0].type == TokenType::FinDeArchivo,
          "archivo_vacio: solo debe haber FinDeArchivo");
}

int main() {
    prueba_identificadores_y_reservadas();
    prueba_numeros();
    prueba_cadenas();
    prueba_cadena_con_escape();
    prueba_cadena_no_terminada();
    prueba_operadores();
    prueba_delimitadores();
    prueba_comentarios();
    prueba_conteo_de_lineas();
    prueba_fin_de_linea_colapsa_blancos();
    prueba_regresion_caracter_perdido();
    prueba_concatenacion_vs_punto();
    prueba_palabras_reservadas_modulos();
    prueba_archivo_vacio();

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DEL LEXER PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
