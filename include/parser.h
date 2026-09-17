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

    // Nombre de tipo/clase posiblemente calificado por namespace de módulo
    // (PLAN_MODULOS.md M6: "ns.Clase", ver "importar * como ns"). Asume que
    // `actual` ya es el Identificador base (verificado por el llamador);
    // consume ese identificador y, mientras siga un '.', encadena
    // ".Identificador" al resultado (p. ej. "geo.Circulo"). Usado en todo
    // punto donde el AST guarda un nombre de tipo como std::string suelto:
    // NuevoExpr::clase, EsExpr::clase, ClaseDef::padre/interfaces,
    // CampoDef::tipoClase, tipoRetornoClase, ParamFuncion::tipoClase.
    std::string parseNombreTipoCalificado();

    // --- Genéricos (PLAN_GENERICOS.md) -------------------------------------
    // Cierra una lista de tipos entre '<' '>'. El lexer no distingue '>' de
    // '>=' por contexto (ver PLAN_GENERICOS.md, "Resolución de la ambigüedad
    // '<'/'>'"), así que si el token actual es ">=" (p.ej. "Pila<numero>=x"
    // sin espacio) se separa en '>' + '=' devolviendo el '=' al flujo.
    void cerrarAngulo();

    // Lista de argumentos de tipo en posición de USO: "<numero>",
    // "<cadena, numero>". Solo se llama cuando el token actual es '<' (si no
    // lo es, devuelve una lista vacía sin consumir nada). v1 no admite
    // argumentos anidados (ver "Fuera de alcance" del plan): cada argumento
    // es un solo parseNombreTipoCalificado().
    std::vector<std::string> parseArgsTipoGenericos();

    // Lista de parámetros de tipo en posición de DECLARACIÓN:
    // "<T>", "<T: Comparable>", "<T: Comparable + Imprimible, U>". Solo se
    // llama cuando el token actual es '<' (si no lo es, devuelve una lista
    // vacía sin consumir nada).
    std::vector<ParametroGenerico> parseParametrosGenericos();

    // Cláusula "donde T: Bound, U: Bound" tras la firma de una función/método
    // genérico. Fusiona los bounds encontrados dentro de `genericos` (deben
    // haber sido declarados en el "<...>" de la firma); no hace nada si el
    // token actual no es 'donde'.
    void parseClausulaDonde(std::vector<ParametroGenerico>& genericos);

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

    // Módulos (PLAN_MODULOS.md): exportar / importar
    SentPtr parseExportar();
    SentPtr parseImportar();

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

    ExprPtr parseLlamada(ExprPtr destino, std::vector<std::string> tipoArgsExplicitos = {});
    ExprPtr parseLista();
    ExprPtr parseDiccionario();
};

#endif  // PARSER_H
