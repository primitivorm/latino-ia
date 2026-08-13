// parser.h
//
// Analizador sintáctico por descenso recursivo. Consume los tokens del Lexer
// y construye el AST (ver ast.h). Devuelve un Programa, o nullptr si encuentra
// un error de sintaxis (reportado por stderr con número de línea).

#ifndef PARSER_H
#define PARSER_H

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "ast.h"
#include "lexer.h"

class Parser {
public:
    explicit Parser(Lexer& lexer);

    std::unique_ptr<Programa> parse();

private:
    Lexer& lexer;
    Token actual;

    // Buffer de un token para retroceder un paso (usado en la detección de
    // anotaciones de tipo: se consume el identificador y si el ":" no va
    // seguido de un tipo válido se devuelve al flujo).
    bool  tieneTokenDevuelto_ = false;
    Token tokenDevuelto_;

    // --- Manejo del flujo de tokens ---
    void avanzar();
    bool esEOF() const;
    bool esFinDeLinea() const;
    bool esOperador(const std::string& s) const;
    bool esDelimitador(const std::string& s) const;
    bool esReservada(const std::string& s) const;
    bool esTerminadorBloque() const;
    bool esAlgunaReservada(std::initializer_list<const char*> palabras) const;

    void esperarOperador(const std::string& s);
    void esperarDelimitador(const std::string& s);
    void esperarReservada(const std::string& s);

    void saltarNuevasLineas();
    void consumirFinDeSentencia();

    [[noreturn]] void error(const std::string& mensaje) const;

    // Convierte un nombre de tipo Latino ("numero", "cadena", etc.) al enum
    // TipoAnotado. Devuelve TipoAnotado::Ninguno si no es un tipo reconocido.
    static TipoAnotado mapearNombreTipo(const std::string& nombre);

    // --- Sentencias ---
    std::unique_ptr<Programa> parsePrograma();
    SentPtr parseSentencia();
    SentPtr parseSentenciaSimple();      // asignación o expresión + terminador
    SentPtr parseAsignacionOExpr();      // igual, pero sin consumir terminador
    SentPtr parseSi();
    SentPtr parseElegir();
    SentPtr parseDesde();
    SentPtr parseMientras();
    SentPtr parseRepetir();
    SentPtr parseFuncion();
    SentPtr parseRetornar();
    SentPtr parseIncluir();
    SentPtr parseVar();
    SentPtr parseConst();

    // Nuevos parseos para POO
    SentPtr parseClase(bool esAbstracta = false);
    SentPtr parseEstructura();
    SentPtr parseInterfaz();
    SentPtr parseLlamadaBase();

    // Auxiliares para parseo dentro de clases
    ModificadorAcceso parseModificadorAcceso();
    MetodoDef parseMetodoDef(const std::string& nombreClase, bool fuerzaAbstracto = false);
    CampoDef parseCampoDef();

    ListaSent parseBloque(std::initializer_list<const char*> terminadores);
    std::vector<ExprPtr> parseListaExpresiones();

    // --- Expresiones (de menor a mayor precedencia) ---
    ExprPtr parseExpresion();
    ExprPtr parseTernario();
    ExprPtr parseO();
    ExprPtr parseY();
    ExprPtr parseIgualdad();
    ExprPtr parseRelacional();
    ExprPtr parseConcatenacion();
    ExprPtr parseAditivo();
    ExprPtr parseMultiplicativo();
    ExprPtr parseUnario();
    ExprPtr parsePotencia();
    ExprPtr parsePostfijo();
    ExprPtr parsePrimario();
    ExprPtr parseNuevo();

    ExprPtr parseLlamada(ExprPtr destino);
    ExprPtr parseLista();
    ExprPtr parseDiccionario();
};

#endif  // PARSER_H
