// parser.cpp

#include "parser.h"

#include <iostream>
#include <stdexcept>
#include <string>

// Excepción interna para abortar el análisis ante el primer error de sintaxis.
namespace {
struct ErrorSintaxis {
    std::string mensaje;
    int linea;
};
}  // namespace

Parser::Parser(Lexer& lexer) : lexer(lexer) {
    actual = lexer.getNextToken();
}

// ---------------------------------------------------------------------------
// Manejo del flujo de tokens
// ---------------------------------------------------------------------------
void Parser::avanzar() {
    if (tieneTokenDevuelto_) {
        actual = tokenDevuelto_;
        tieneTokenDevuelto_ = false;
    } else {
        actual = lexer.getNextToken();
    }
}

TipoAnotado Parser::mapearNombreTipo(const std::string& s) {
    if (s == "numero")  return TipoAnotado::Numero;
    if (s == "cadena")  return TipoAnotado::Cadena;
    if (s == "logico")  return TipoAnotado::Logico;
    if (s == "lista")   return TipoAnotado::Lista;
    if (s == "dic")     return TipoAnotado::Dic;
    if (s == "nulo")    return TipoAnotado::Nulo;
    return TipoAnotado::Objeto;
}

bool Parser::esEOF() const {
    return actual.type == TokenType::FinDeArchivo;
}

bool Parser::esFinDeLinea() const {
    return actual.type == TokenType::FinDeLinea;
}

bool Parser::esOperador(const std::string& s) const {
    return actual.type == TokenType::Operador && actual.lexeme == s;
}

bool Parser::esDelimitador(const std::string& s) const {
    return actual.type == TokenType::Delimitador && actual.lexeme == s;
}

bool Parser::esReservada(const std::string& s) const {
    return actual.type == TokenType::PalabraReservada && actual.lexeme == s;
}

bool Parser::esAlgunaReservada(std::initializer_list<const char*> palabras) const {
    if (actual.type != TokenType::PalabraReservada)
        return false;
    for (const char* p : palabras)
        if (actual.lexeme == p)
            return true;
    return false;
}

bool Parser::esTerminadorBloque() const {
    return esAlgunaReservada({"fin", "sino", "osi", "caso", "defecto", "otro", "hasta"});
}

void Parser::esperarOperador(const std::string& s) {
    if (esOperador(s)) { avanzar(); return; }
    error("se esperaba el operador '" + s + "'");
}

void Parser::esperarDelimitador(const std::string& s) {
    if (esDelimitador(s)) { avanzar(); return; }
    error("se esperaba '" + s + "'");
}

void Parser::esperarReservada(const std::string& s) {
    if (esReservada(s)) { avanzar(); return; }
    error("se esperaba '" + s + "'");
}

void Parser::saltarNuevasLineas() {
    while (esFinDeLinea())
        avanzar();
}

void Parser::consumirFinDeSentencia() {
    if (esFinDeLinea()) { avanzar(); return; }
    if (esEOF() || esTerminadorBloque() || esDelimitador(")") || esDelimitador(";"))
        return;
    error("se esperaba un salto de línea para terminar la sentencia (se encontró '" +
          actual.lexeme + "')");
}

void Parser::error(const std::string& mensaje) const {
    throw ErrorSintaxis{mensaje, actual.line};
}

// ---------------------------------------------------------------------------
// Punto de entrada
// ---------------------------------------------------------------------------
std::unique_ptr<Programa> Parser::parse() {
    try {
        return parsePrograma();
    } catch (const ErrorSintaxis& e) {
        std::cerr << "Error de sintaxis en línea " << e.linea << ": " << e.mensaje
                  << std::endl;
        return nullptr;
    }
}

std::unique_ptr<Programa> Parser::parsePrograma() {
    auto prog = std::make_unique<Programa>();
    saltarNuevasLineas();
    while (!esEOF()) {
        prog->sentencias.push_back(parseSentencia());
        saltarNuevasLineas();
    }
    return prog;
}

// ---------------------------------------------------------------------------
// Sentencias
// ---------------------------------------------------------------------------
SentPtr Parser::parseSentencia() {
    if (actual.type == TokenType::PalabraReservada) {
        const std::string& p = actual.lexeme;
        if (p == "si")       return parseSi();
        if (p == "elegir")   return parseElegir();
        if (p == "desde")    return parseDesde();
        if (p == "mientras") return parseMientras();
        if (p == "repetir")  return parseRepetir();
        if (p == "funcion" || p == "fun")                 return parseFuncion();
        if (p == "regresar" || p == "retornar" || p == "ret") return parseRetornar();
        if (p == "incluir") return parseIncluir();
        if (p == "var")     return parseVar();
        if (p == "const")   return parseConst();
        if (p == "exportar") return parseExportar();
        if (p == "importar") return parseImportar();
        if (p == "romper") {
            int l = actual.line;
            avanzar();
            consumirFinDeSentencia();
            auto r = std::make_unique<Romper>();
            r->linea = l;
            return r;
        }
        // nuevo: declaraciones de tipos POO
        if (p == "clase")    return parseClase();
        if (p == "estructura") return parseEstructura();
        if (p == "interfaz")  return parseInterfaz();
        if (p == "abstracto") {
            // lookahead: abstracto clase ...
            avanzar();
            if (!esReservada("clase"))
                error("se esperaba 'clase' después de 'abstracto'");
            return parseClase(true);
        }
        if (p == "base") {
            return parseLlamadaBase();
        }
        // Expresiones válidas como sentencia: nuevos tipos, instancias y literales.
        if (p == "nuevo" || p == "este" || p == "cierto" || p == "verdadero" ||
            p == "falso" || p == "nulo")
            return parseSentenciaSimple();

        error("palabra reservada inesperada '" + p + "'");
    }
    return parseSentenciaSimple();
}

SentPtr Parser::parseAsignacionOExpr() {
    int l = actual.line;

    // Detección de anotación de tipo: "identificador : tipo = valor"
    // Solo se intenta cuando la sentencia comienza con un identificador simple.
    if (actual.type == TokenType::Identificador) {
        Token tk = actual;
        avanzar();  // actual = token siguiente al identificador
        if (esOperador(":")) {
            avanzar();  // consume ":"
            TipoAnotado tipo = TipoAnotado::Ninguno;
            if (actual.type == TokenType::Identificador)
                tipo = mapearNombreTipo(actual.lexeme);
            if (tipo != TipoAnotado::Ninguno) {
                avanzar();  // consume nombre del tipo
                esperarOperador("=");
                auto valores = parseListaExpresiones();
                auto id = std::make_unique<Identificador>();
                id->nombre = tk.lexeme;
                id->linea  = tk.line;
                auto a = std::make_unique<Asignacion>();
                a->linea = l;
                a->destinos.push_back(std::move(id));
                a->valores = std::move(valores);
                a->tiposDestino.push_back(tipo);
                return a;
            }
            // ":" existe pero lo que sigue no es un tipo válido.
            error("se esperaba un tipo válido después de ':' "
                  "(numero, cadena, logico, lista, dic, nulo)");
        }
        // No hay ":". Devolver el identificador al flujo normal.
        tieneTokenDevuelto_ = true;
        tokenDevuelto_ = actual;
        actual = tk;
    }

    std::vector<ExprPtr> izqs = parseListaExpresiones();

    if (esOperador("=")) {
        avanzar();
        std::vector<ExprPtr> ders = parseListaExpresiones();
        auto a = std::make_unique<Asignacion>();
        a->linea = l;
        a->destinos = std::move(izqs);
        a->valores = std::move(ders);
        // tiposDestino vacío == sin anotaciones (retrocompatible)
        return a;
    }

    if (izqs.size() != 1)
        error("se esperaba '=' en una asignación múltiple");

    auto e = std::make_unique<ExprSentencia>();
    e->linea = l;
    e->expr = std::move(izqs[0]);
    return e;
}

SentPtr Parser::parseSentenciaSimple() {
    SentPtr s = parseAsignacionOExpr();
    consumirFinDeSentencia();
    return s;
}

ListaSent Parser::parseBloque(std::initializer_list<const char*> terminadores) {
    ListaSent cuerpo;
    saltarNuevasLineas();
    while (!esEOF() && !esAlgunaReservada(terminadores)) {
        cuerpo.push_back(parseSentencia());
        saltarNuevasLineas();
    }
    return cuerpo;
}

SentPtr Parser::parseSi() {
    int l = actual.line;
    avanzar();  // si
    auto nodo = std::make_unique<Si>();
    nodo->linea = l;
    nodo->condicion = parseExpresion();
    nodo->entonces = parseBloque({"sino", "osi", "fin"});

    while (esReservada("osi")) {
        avanzar();
        RamaOsi rama;
        rama.condicion = parseExpresion();
        rama.cuerpo = parseBloque({"sino", "osi", "fin"});
        nodo->osis.push_back(std::move(rama));
    }

    if (esReservada("sino")) {
        avanzar();
        nodo->sino = parseBloque({"fin"});
        nodo->tieneSino = true;
    }

    esperarReservada("fin");
    return nodo;
}

SentPtr Parser::parseElegir() {
    int l = actual.line;
    avanzar();  // elegir
    auto nodo = std::make_unique<Elegir>();
    nodo->linea = l;
    nodo->opcion = parseExpresion();  // los paréntesis se parsean como agrupación
    saltarNuevasLineas();

    while (esReservada("caso")) {
        avanzar();
        CasoElegir caso;
        caso.valor = parseExpresion();
        esperarOperador(":");
        caso.cuerpo = parseBloque({"caso", "defecto", "otro", "fin"});
        nodo->casos.push_back(std::move(caso));
    }

    if (esReservada("defecto") || esReservada("otro")) {
        avanzar();
        esperarOperador(":");
        nodo->defecto = parseBloque({"fin"});
        nodo->tieneDefecto = true;
    }

    esperarReservada("fin");
    return nodo;
}

SentPtr Parser::parseDesde() {
    int l = actual.line;
    avanzar();  // desde
    esperarDelimitador("(");
    auto nodo = std::make_unique<Desde>();
    nodo->linea = l;
    nodo->inicio = parseAsignacionOExpr();
    esperarDelimitador(";");
    nodo->condicion = parseExpresion();
    esperarDelimitador(";");
    nodo->incremento = parseAsignacionOExpr();
    esperarDelimitador(")");
    nodo->cuerpo = parseBloque({"fin"});
    esperarReservada("fin");
    return nodo;
}

SentPtr Parser::parseMientras() {
    int l = actual.line;
    avanzar();  // mientras
    auto nodo = std::make_unique<Mientras>();
    nodo->linea = l;
    nodo->condicion = parseExpresion();
    nodo->cuerpo = parseBloque({"fin"});
    esperarReservada("fin");
    return nodo;
}

SentPtr Parser::parseRepetir() {
    int l = actual.line;
    avanzar();  // repetir
    auto nodo = std::make_unique<Repetir>();
    nodo->linea = l;
    nodo->cuerpo = parseBloque({"hasta"});
    esperarReservada("hasta");
    nodo->condicionHasta = parseExpresion();
    consumirFinDeSentencia();
    return nodo;
}

SentPtr Parser::parseFuncion() {
    int l = actual.line;
    avanzar();  // funcion / fun
    if (actual.type != TokenType::Identificador)
        error("se esperaba el nombre de la función");
    auto nodo = std::make_unique<FuncionDef>();
    nodo->linea = l;
    nodo->nombre = actual.lexeme;
    avanzar();

    esperarDelimitador("(");
    if (!esDelimitador(")")) {
        for (;;) {
            if (esOperador("...")) {
                nodo->variadico = true;
                avanzar();
                break;  // el variádico debe ser el último parámetro
            }
            if (actual.type != TokenType::Identificador)
                error("se esperaba el nombre de un parámetro");
            ParamFuncion param;
            param.nombre = actual.lexeme;
            avanzar();
            if (esOperador(":")) {
                avanzar();
                if (actual.type != TokenType::Identificador)
                    error("se esperaba un tipo válido después de ':'");
                param.tipo = mapearNombreTipo(actual.lexeme);
                if (param.tipo == TipoAnotado::Ninguno)
                    error("tipo no reconocido '" + actual.lexeme + "'");
                if (param.tipo == TipoAnotado::Objeto)
                    param.tipoClase = actual.lexeme;
                avanzar();
            }
            nodo->parametros.push_back(std::move(param));
            if (esDelimitador(",")) { avanzar(); continue; }
            break;
        }
    }
    esperarDelimitador(")");

    // Tipo de retorno opcional: ": tipo"
    if (esOperador(":")) {
        avanzar();
        if (actual.type != TokenType::Identificador)
            error("se esperaba un tipo de retorno válido");
        nodo->tipoRetorno = mapearNombreTipo(actual.lexeme);
        if (nodo->tipoRetorno == TipoAnotado::Ninguno)
            error("tipo de retorno no reconocido '" + actual.lexeme + "'");
        if (nodo->tipoRetorno == TipoAnotado::Objeto)
            nodo->tipoRetornoClase = actual.lexeme;
        avanzar();
    }

    nodo->cuerpo = parseBloque({"fin"});
    esperarReservada("fin");
    return nodo;
}

SentPtr Parser::parseRetornar() {
    int l = actual.line;
    avanzar();  // regresar / retornar / ret
    auto nodo = std::make_unique<Retornar>();
    nodo->linea = l;
    if (!esFinDeLinea() && !esEOF() && !esTerminadorBloque())
        nodo->valor = parseExpresion();
    consumirFinDeSentencia();
    return nodo;
}

SentPtr Parser::parseIncluir() {
    int l = actual.line;
    avanzar();  // incluir
    if (actual.type != TokenType::Cadena)
        error("se esperaba el nombre del módulo o ruta entre comillas después de 'incluir'");
    auto nodo = std::make_unique<Incluir>();
    nodo->linea = l;
    nodo->modulo = actual.lexeme;
    avanzar();
    consumirFinDeSentencia();
    return nodo;
}

SentPtr Parser::parseVar() {
    int l = actual.line;
    avanzar();  // consume "var"
    auto s = parseAsignacionOExpr();
    if (auto* a = dynamic_cast<Asignacion*>(s.get())) {
        a->esVar = true;
        a->esConst = false;
        consumirFinDeSentencia();
        return s;
    } else if (auto* es = dynamic_cast<ExprSentencia*>(s.get())) {
        if (auto* id = dynamic_cast<Identificador*>(es->expr.get())) {
            auto a = std::make_unique<Asignacion>();
            a->linea = es->linea;
            
            auto destId = std::make_unique<Identificador>();
            destId->linea = id->linea;
            destId->nombre = id->nombre;
            a->destinos.push_back(std::move(destId));
            
            auto nullVal = std::make_unique<LitNulo>();
            nullVal->linea = id->linea;
            a->valores.push_back(std::move(nullVal));
            
            a->esVar = true;
            a->esConst = false;
            consumirFinDeSentencia();
            return a;
        }
    }
    error("se esperaba una declaración de variable o asignación válida después de 'var'");
}

SentPtr Parser::parseConst() {
    int l = actual.line;
    avanzar();  // consume "const"
    auto s = parseAsignacionOExpr();
    if (auto* a = dynamic_cast<Asignacion*>(s.get())) {
        if (a->valores.empty()) {
            error("las constantes deben estar inicializadas");
        }
        a->esVar = false;
        a->esConst = true;
        consumirFinDeSentencia();
        return s;
    }
    error("se esperaba una asignación válida para la constante después de 'const'");
}

// ---------------------------------------------------------------------------
// Módulos (PLAN_MODULOS.md): exportar / importar
// ---------------------------------------------------------------------------

SentPtr Parser::parseImportar() {
    int l = actual.line;
    avanzar();  // consume "importar"

    auto nodo = std::make_unique<ImportarDecl>();
    nodo->linea = l;

    if (esDelimitador("{")) {
        nodo->tipo = TipoImportar::Nombrado;
        avanzar();  // "{"
        saltarNuevasLineas();
        if (!esDelimitador("}")) {
            for (;;) {
                saltarNuevasLineas();
                if (actual.type != TokenType::Identificador)
                    error("se esperaba un nombre exportado dentro de '{ }'");
                NombreImportado ni;
                ni.origen = actual.lexeme;
                avanzar();
                if (esReservada("como")) {
                    avanzar();
                    if (actual.type != TokenType::Identificador)
                        error("se esperaba un alias después de 'como'");
                    ni.alias = actual.lexeme;
                    avanzar();
                } else {
                    ni.alias = ni.origen;
                }
                nodo->nombres.push_back(std::move(ni));
                saltarNuevasLineas();
                if (esDelimitador(",")) { avanzar(); continue; }
                break;
            }
        }
        saltarNuevasLineas();
        esperarDelimitador("}");
    } else if (esOperador("*")) {
        nodo->tipo = TipoImportar::Espacio;
        avanzar();  // "*"
        esperarReservada("como");
        if (actual.type != TokenType::Identificador)
            error("se esperaba un nombre después de 'como'");
        nodo->aliasEspacio = actual.lexeme;
        avanzar();
    } else if (actual.type == TokenType::Identificador) {
        nodo->tipo = TipoImportar::PorDefecto;
        nodo->nombreLocal = actual.lexeme;
        avanzar();
    } else {
        error("se esperaba '{', '*' o un nombre después de 'importar'");
    }

    esperarReservada("desde");
    if (actual.type != TokenType::Cadena)
        error("se esperaba la ruta del módulo entre comillas después de 'desde'");
    nodo->ruta = actual.lexeme;
    avanzar();
    consumirFinDeSentencia();
    return nodo;
}

SentPtr Parser::parseExportar() {
    int l = actual.line;
    avanzar();  // consume "exportar"

    // Re-export (barril): exportar { a, b como c } desde "ruta"
    if (esDelimitador("{")) {
        auto nodo = std::make_unique<ExportarDesde>();
        nodo->linea = l;
        avanzar();  // "{"
        saltarNuevasLineas();
        if (!esDelimitador("}")) {
            for (;;) {
                saltarNuevasLineas();
                if (actual.type != TokenType::Identificador)
                    error("se esperaba un nombre exportado dentro de '{ }'");
                NombreImportado ni;
                ni.origen = actual.lexeme;
                avanzar();
                if (esReservada("como")) {
                    avanzar();
                    if (actual.type != TokenType::Identificador)
                        error("se esperaba un alias después de 'como'");
                    ni.alias = actual.lexeme;
                    avanzar();
                } else {
                    ni.alias = ni.origen;
                }
                nodo->nombres.push_back(std::move(ni));
                saltarNuevasLineas();
                if (esDelimitador(",")) { avanzar(); continue; }
                break;
            }
        }
        saltarNuevasLineas();
        esperarDelimitador("}");
        esperarReservada("desde");
        if (actual.type != TokenType::Cadena)
            error("se esperaba la ruta del módulo entre comillas después de 'desde'");
        nodo->ruta = actual.lexeme;
        avanzar();
        consumirFinDeSentencia();
        return nodo;
    }

    // "exportar por defecto ...": "por" no es palabra reservada (solo tiene
    // sentido aquí), así que llega como Identificador de lexema "por".
    bool esDefecto = false;
    if (actual.type == TokenType::Identificador && actual.lexeme == "por") {
        avanzar();  // "por"
        esperarReservada("defecto");
        esDefecto = true;
    }

    if (esReservada("funcion") || esReservada("fun")) {
        SentPtr s = parseFuncion();
        auto* f = static_cast<FuncionDef*>(s.get());
        f->exportado = true;
        f->esDefecto = esDefecto;
        return s;
    }
    if (esReservada("abstracto")) {
        avanzar();
        if (!esReservada("clase"))
            error("se esperaba 'clase' después de 'abstracto'");
        SentPtr s = parseClase(true);
        auto* c = static_cast<ClaseDef*>(s.get());
        c->exportado = true;
        c->esDefecto = esDefecto;
        return s;
    }
    if (esReservada("clase")) {
        SentPtr s = parseClase();
        auto* c = static_cast<ClaseDef*>(s.get());
        c->exportado = true;
        c->esDefecto = esDefecto;
        return s;
    }
    if (esReservada("estructura")) {
        if (esDefecto) error("'exportar por defecto' no admite 'estructura'");
        SentPtr s = parseEstructura();
        auto* e = static_cast<EstructuraDef*>(s.get());
        e->exportado = true;
        return s;
    }
    if (esReservada("interfaz")) {
        if (esDefecto) error("'exportar por defecto' no admite 'interfaz'");
        SentPtr s = parseInterfaz();
        auto* i = static_cast<InterfazDef*>(s.get());
        i->exportado = true;
        return s;
    }
    if (esReservada("var")) {
        if (esDefecto) error("'exportar por defecto' no admite 'var'");
        SentPtr s = parseVar();
        auto* a = static_cast<Asignacion*>(s.get());
        a->exportado = true;
        return s;
    }
    if (esReservada("const")) {
        if (esDefecto) error("'exportar por defecto' no admite 'const'");
        SentPtr s = parseConst();
        auto* a = static_cast<Asignacion*>(s.get());
        a->exportado = true;
        return s;
    }

    if (esDefecto) {
        // exportar por defecto <expresión>  (p.ej. un literal de diccionario)
        int le = actual.line;
        ExprPtr valor = parseExpresion();
        consumirFinDeSentencia();
        auto a = std::make_unique<Asignacion>();
        a->linea = le;
        auto id = std::make_unique<Identificador>();
        id->linea = le;
        id->nombre = "__defecto__";
        a->destinos.push_back(std::move(id));
        a->valores.push_back(std::move(valor));
        a->esConst = true;
        a->exportado = true;
        a->esDefecto = true;
        return a;
    }

    // exportar <identificador> = <valor>   (asignación simple de nivel superior)
    SentPtr s = parseSentenciaSimple();
    auto* a = dynamic_cast<Asignacion*>(s.get());
    if (!a)
        error("se esperaba una declaración exportable (funcion, clase, estructura, "
              "interfaz, var, const o asignación) después de 'exportar'");
    a->exportado = true;
    return s;
}

std::vector<ExprPtr> Parser::parseListaExpresiones() {
    std::vector<ExprPtr> v;
    v.push_back(parseExpresion());
    while (esDelimitador(",")) {
        avanzar();
        v.push_back(parseExpresion());
    }
    return v;
}

// ---------------------------------------------------------------------------
// Expresiones (descenso recursivo con precedencia tipo C/Lua)
// ---------------------------------------------------------------------------
namespace {
ExprPtr mkBinaria(const std::string& op, ExprPtr a, ExprPtr b) {
    auto n = std::make_unique<Binaria>();
    n->op = op;
    n->izq = std::move(a);
    n->der = std::move(b);
    return n;
}
}  // namespace

ExprPtr Parser::parseExpresion() {
    return parseTernario();
}

ExprPtr Parser::parseTernario() {
    ExprPtr cond = parseO();
    if (esOperador("?")) {
        avanzar();
        ExprPtr siCierto = parseExpresion();
        esperarOperador(":");
        ExprPtr siFalso = parseExpresion();
        auto t = std::make_unique<Ternaria>();
        t->condicion = std::move(cond);
        t->siCierto = std::move(siCierto);
        t->siFalso = std::move(siFalso);
        return t;
    }
    return cond;
}

ExprPtr Parser::parseO() {
    ExprPtr e = parseY();
    while (esOperador("||")) { avanzar(); e = mkBinaria("||", std::move(e), parseY()); }
    return e;
}

ExprPtr Parser::parseY() {
    ExprPtr e = parseIgualdad();
    while (esOperador("&&")) { avanzar(); e = mkBinaria("&&", std::move(e), parseIgualdad()); }
    return e;
}

ExprPtr Parser::parseIgualdad() {
    ExprPtr e = parseRelacional();
    while (esOperador("==") || esOperador("!=") || esOperador("~=")) {
        std::string op = actual.lexeme;
        avanzar();
        e = mkBinaria(op, std::move(e), parseRelacional());
    }
    return e;
}

ExprPtr Parser::parseRelacional() {
    ExprPtr e = parseConcatenacion();
    while (esOperador("<") || esOperador(">") || esOperador("<=") || esOperador(">=")) {
        std::string op = actual.lexeme;
        avanzar();
        e = mkBinaria(op, std::move(e), parseConcatenacion());
    }
    // Operador "es": expr es NombreClase
    if (esReservada("es")) {
        avanzar();
        if (actual.type != TokenType::Identificador)
            error("se esperaba un nombre de clase después de 'es'");
        std::string clase = actual.lexeme;
        avanzar();
        auto n = std::make_unique<EsExpr>();
        n->objeto = std::move(e);
        n->clase = clase;
        e = std::move(n);
    }
    return e;
}

ExprPtr Parser::parseConcatenacion() {
    ExprPtr e = parseAditivo();
    while (esOperador("..")) { avanzar(); e = mkBinaria("..", std::move(e), parseAditivo()); }
    return e;
}

ExprPtr Parser::parseAditivo() {
    ExprPtr e = parseMultiplicativo();
    while (esOperador("+") || esOperador("-")) {
        std::string op = actual.lexeme;
        avanzar();
        e = mkBinaria(op, std::move(e), parseMultiplicativo());
    }
    return e;
}

ExprPtr Parser::parseMultiplicativo() {
    ExprPtr e = parseUnario();
    while (esOperador("*") || esOperador("/") || esOperador("%")) {
        std::string op = actual.lexeme;
        avanzar();
        e = mkBinaria(op, std::move(e), parseUnario());
    }
    return e;
}

ExprPtr Parser::parseUnario() {
    if (esOperador("-")) {
        avanzar();
        auto u = std::make_unique<Unaria>();
        u->op = "-";
        u->operando = parseUnario();
        return u;
    }
    return parsePotencia();
}

ExprPtr Parser::parsePotencia() {
    ExprPtr base = parsePostfijo();
    if (esOperador("^")) {
        avanzar();
        // Asociatividad derecha: el exponente puede llevar unario (p.ej. 2^-3).
        return mkBinaria("^", std::move(base), parseUnario());
    }
    return base;
}

ExprPtr Parser::parsePostfijo() {
    ExprPtr e = parsePrimario();
    for (;;) {
        if (esDelimitador("(")) {
            e = parseLlamada(std::move(e));
        } else if (esDelimitador("[")) {
            avanzar();
            saltarNuevasLineas();
            ExprPtr idx = parseExpresion();
            saltarNuevasLineas();
            esperarDelimitador("]");
            auto a = std::make_unique<AccesoIndice>();
            a->objeto = std::move(e);
            a->indice = std::move(idx);
            e = std::move(a);
        } else if (esOperador(".")) {
            avanzar();
            // Tras '.', el nombre de miembro se acepta aunque coincida con una
            // palabra reservada de POO (p. ej. "mate.base"): el contexto ya
            // desambigua que es un nombre de campo/método, no la palabra clave.
            if (actual.type != TokenType::Identificador && actual.type != TokenType::PalabraReservada)
                error("se esperaba un nombre de miembro después de '.'");
            auto m = std::make_unique<AccesoMiembro>();
            m->objeto = std::move(e);
            m->miembro = actual.lexeme;
            avanzar();
            e = std::move(m);
        } else if (esOperador("++") || esOperador("--")) {
            auto p = std::make_unique<PostOperador>();
            p->op = actual.lexeme;
            avanzar();
            p->operando = std::move(e);
            e = std::move(p);
        } else {
            break;
        }
    }
    return e;
}

ExprPtr Parser::parsePrimario() {
    Token tk = actual;

    if (tk.type == TokenType::Entero || tk.type == TokenType::Flotante) {
        avanzar();
        auto n = std::make_unique<LitNumero>();
        n->valor = std::stod(tk.lexeme);
        n->esEntero = (tk.type == TokenType::Entero);
        n->lexema = tk.lexeme;
        n->linea = tk.line;
        return n;
    }

    if (tk.type == TokenType::Cadena) {
        avanzar();
        auto n = std::make_unique<LitCadena>();
        n->valor = tk.lexeme;
        n->linea = tk.line;
        return n;
    }

    if (tk.type == TokenType::PalabraReservada) {
        if (tk.lexeme == "cierto" || tk.lexeme == "verdadero") {
            avanzar();
            auto n = std::make_unique<LitLogico>();
            n->valor = true;
            n->linea = tk.line;
            return n;
        }
        if (tk.lexeme == "falso") {
            avanzar();
            auto n = std::make_unique<LitLogico>();
            n->valor = false;
            n->linea = tk.line;
            return n;
        }
        if (tk.lexeme == "nulo") {
            avanzar();
            auto n = std::make_unique<LitNulo>();
            n->linea = tk.line;
            return n;
        }
        if (tk.lexeme == "nuevo") {
            return parseNuevo();
        }
        if (tk.lexeme == "este") {
            avanzar();
            auto n = std::make_unique<AccesoEste>();
            n->linea = tk.line;
            return n;
        }
        error("expresión inesperada: palabra reservada '" + tk.lexeme + "'");
    }

    if (tk.type == TokenType::Identificador) {
        avanzar();
        auto n = std::make_unique<Identificador>();
        n->nombre = tk.lexeme;
        n->linea = tk.line;
        return n;
    }

    if (esOperador("...")) {
        avanzar();
        auto n = std::make_unique<VarArgs>();
        n->linea = tk.line;
        return n;
    }

    if (esDelimitador("(")) {
        avanzar();
        saltarNuevasLineas();
        ExprPtr e = parseExpresion();
        saltarNuevasLineas();
        esperarDelimitador(")");
        return e;
    }

    if (esDelimitador("[")) return parseLista();
    if (esDelimitador("{")) return parseDiccionario();

    error("se esperaba una expresión (se encontró '" + actual.lexeme + "')");
}

ExprPtr Parser::parseLlamada(ExprPtr destino) {
    avanzar();  // (
    auto c = std::make_unique<Llamada>();
    c->destino = std::move(destino);
    saltarNuevasLineas();
    if (!esDelimitador(")")) {
        for (;;) {
            c->argumentos.push_back(parseExpresion());
            saltarNuevasLineas();
            if (esDelimitador(",")) { avanzar(); saltarNuevasLineas(); continue; }
            break;
        }
    }
    esperarDelimitador(")");
    return c;
}

ExprPtr Parser::parseLista() {
    avanzar();  // [
    auto l = std::make_unique<ListaLiteral>();
    saltarNuevasLineas();
    if (!esDelimitador("]")) {
        for (;;) {
            l->elementos.push_back(parseExpresion());
            saltarNuevasLineas();
            if (esDelimitador(",")) { avanzar(); saltarNuevasLineas(); continue; }
            break;
        }
    }
    esperarDelimitador("]");
    return l;
}

ExprPtr Parser::parseDiccionario() {
    avanzar();  // {
    auto d = std::make_unique<DiccionarioLiteral>();
    saltarNuevasLineas();
    if (!esDelimitador("}")) {
        for (;;) {
            ParDic par;
            par.clave = parseExpresion();
            esperarOperador(":");
            saltarNuevasLineas();
            par.valor = parseExpresion();
            d->pares.push_back(std::move(par));
            saltarNuevasLineas();
            if (esDelimitador(",")) { avanzar(); saltarNuevasLineas(); continue; }
            break;
        }
    }
    esperarDelimitador("}");
    return d;
}

// ---------------------------------------------------------------------------
// Nuevos parseos para POO
// ---------------------------------------------------------------------------

ExprPtr Parser::parseNuevo() {
    int l = actual.line;
    avanzar(); // consume 'nuevo'
    if (actual.type != TokenType::Identificador)
        error("se esperaba el nombre de la clase después de 'nuevo'");
    auto nodo = std::make_unique<NuevoExpr>();
    nodo->linea = l;
    nodo->clase = actual.lexeme;
    avanzar();
    esperarDelimitador("(");
    saltarNuevasLineas();
    if (!esDelimitador(")")) {
        for (;;) {
            nodo->argumentos.push_back(parseExpresion());
            saltarNuevasLineas();
            if (esDelimitador(",")) { avanzar(); saltarNuevasLineas(); continue; }
            break;
        }
    }
    esperarDelimitador(")");
    return nodo;
}

SentPtr Parser::parseLlamadaBase() {
    int l = actual.line;
    avanzar(); // consume 'base'
    auto nodo = std::make_unique<LlamadaBase>();
    nodo->linea = l;
    esperarDelimitador("(");
    saltarNuevasLineas();
    if (!esDelimitador(")")) {
        for (;;) {
            nodo->argumentos.push_back(parseExpresion());
            saltarNuevasLineas();
            if (esDelimitador(",")) { avanzar(); saltarNuevasLineas(); continue; }
            break;
        }
    }
    esperarDelimitador(")");
    consumirFinDeSentencia();
    return nodo;
}

CampoDef Parser::parseCampoDef() {
    if (actual.type != TokenType::Identificador)
        error("se esperaba el nombre del campo");
    CampoDef c;
    c.nombre = actual.lexeme;
    c.linea = actual.line;
    avanzar();
    esperarOperador(":");
    // Leer tipo: puede ser nombre de tipo primitivo o nombre de clase
    if (actual.type != TokenType::Identificador)
        error("se esperaba un tipo después de ':' en la declaración de campo");
    std::string tipoLex = actual.lexeme;
    c.tipoAnotado = mapearNombreTipo(tipoLex);
    if (c.tipoAnotado == TipoAnotado::Objeto)
        c.tipoClase = tipoLex;
    avanzar();
    if (esOperador("=")) {
        avanzar();
        c.valorDefecto = parseExpresion();
    }
    consumirFinDeSentencia();
    return c;
}

MetodoDef Parser::parseMetodoDef(const std::string& nombreClase, bool fuerzaAbstracto) {
    int l = actual.line;
    // Consume 'funcion' o 'fun'
    if (!(esReservada("funcion") || esReservada("fun")))
        error("se esperaba 'funcion' en la definición de método");
    avanzar();
    if (actual.type != TokenType::Identificador)
        error("se esperaba el nombre del método");
    MetodoDef m;
    m.linea = l;
    m.nombre = actual.lexeme;
    m.esConstructor = (m.nombre == nombreClase);
    m.esAbstracto = fuerzaAbstracto;
    avanzar();

    esperarDelimitador("(");
    if (!esDelimitador(")")) {
        for (;;) {
            if (esOperador("...")) {
                // variádico no soportado en métodos por ahora; tratar como parámetro especial
                avanzar();
                break;
            }
            if (actual.type != TokenType::Identificador)
                error("se esperaba el nombre de un parámetro");
            ParamFuncion param;
            param.nombre = actual.lexeme;
            avanzar();
            if (esOperador(":")) {
                avanzar();
                if (actual.type != TokenType::Identificador)
                    error("se esperaba un tipo de parámetro válido");
                param.tipo = mapearNombreTipo(actual.lexeme);
                if (param.tipo == TipoAnotado::Ninguno)
                    error("tipo no reconocido '" + actual.lexeme + "'");
                if (param.tipo == TipoAnotado::Objeto)
                    param.tipoClase = actual.lexeme;
                avanzar();
            }
            m.parametros.push_back(std::move(param));
            if (esDelimitador(",")) { avanzar(); continue; }
            break;
        }
    }
    esperarDelimitador(")");

    // Tipo de retorno opcional
    if (esOperador(":")) {
        avanzar();
        if (actual.type != TokenType::Identificador)
            error("se esperaba un tipo de retorno válido");
        m.tipoRetorno = mapearNombreTipo(actual.lexeme);
        if (m.tipoRetorno == TipoAnotado::Objeto)
            m.tipoRetornoClase = actual.lexeme;
        avanzar();
    }

    // Marcador 'sobreescribir' opcional
    if (esReservada("sobreescribir")) {
        m.esSobreescritura = true;
        avanzar();
    }

    if (m.esAbstracto) {
        // si está marcado como abstracto, no habrá cuerpo
        m.cuerpo = ListaSent();
        consumirFinDeSentencia();
    } else {
        m.cuerpo = parseBloque({"fin"});
        esperarReservada("fin");
    }
    return m;
}

SentPtr Parser::parseClase(bool esAbstracta) {
    int l = actual.line;
    avanzar(); // consume 'clase'
    if (actual.type != TokenType::Identificador)
        error("se esperaba el nombre de la clase");
    std::string nombre = actual.lexeme;
    avanzar();

    std::string padre = "";
    std::vector<std::string> interfaces;

    if (esReservada("extiende")) {
        avanzar();
        if (actual.type != TokenType::Identificador)
            error("se esperaba el nombre de la clase padre después de 'extiende'");
        padre = actual.lexeme;
        avanzar();
    }

    if (esReservada("implementa")) {
        avanzar();
        for (;;) {
            if (actual.type != TokenType::Identificador)
                error("se esperaba un nombre de interfaz después de 'implementa'");
            interfaces.push_back(actual.lexeme);
            avanzar();
            if (esDelimitador(",")) { avanzar(); continue; }
            break;
        }
    }

    saltarNuevasLineas();
    std::vector<CampoDef> campos;
    std::vector<MetodoDef> metodos;

    while (!esReservada("fin")) {
        ModificadorAcceso acceso = ModificadorAcceso::Publico;
        bool esEstatico = false;
        bool esAbstractoMiembro = false;

        if (esReservada("publico")) { acceso = ModificadorAcceso::Publico; avanzar(); }
        else if (esReservada("privado")) { acceso = ModificadorAcceso::Privado; avanzar(); }
        else if (esReservada("protegido")) { acceso = ModificadorAcceso::Protegido; avanzar(); }

        if (esReservada("estatico")) { esEstatico = true; avanzar(); }
        if (esReservada("abstracto")) { esAbstractoMiembro = true; avanzar(); }

        if (esReservada("funcion") || esReservada("fun")) {
            MetodoDef m = parseMetodoDef(nombre, esAbstractoMiembro);
            m.acceso = acceso;
            m.esEstatico = esEstatico;
            metodos.push_back(std::move(m));
        } else if (actual.type == TokenType::Identificador) {
            CampoDef c = parseCampoDef();
            c.acceso = acceso;
            c.esEstatico = esEstatico;
            campos.push_back(std::move(c));
        } else {
            error("se esperaba un campo o método dentro de la clase");
        }

        saltarNuevasLineas();
    }

    esperarReservada("fin");
    auto nodo = std::make_unique<ClaseDef>();
    nodo->linea = l;
    nodo->nombre = nombre;
    nodo->padre = padre;
    nodo->interfaces = interfaces;
    nodo->esAbstracta = esAbstracta;
    nodo->campos = std::move(campos);
    nodo->metodos = std::move(metodos);
    return nodo;
}

SentPtr Parser::parseEstructura() {
    int l = actual.line;
    avanzar(); // consume 'estructura'
    if (actual.type != TokenType::Identificador)
        error("se esperaba el nombre de la estructura");
    std::string nombre = actual.lexeme;
    avanzar();
    saltarNuevasLineas();
    std::vector<CampoDef> campos;
    std::vector<MetodoDef> metodos;
    while (!esReservada("fin")) {
        // No admite 'protegido' ni 'abstracto' por diseño
        ModificadorAcceso acceso = ModificadorAcceso::Publico;
        bool esEstatico = false;
        if (esReservada("publico")) { acceso = ModificadorAcceso::Publico; avanzar(); }
        else if (esReservada("privado")) { acceso = ModificadorAcceso::Privado; avanzar(); }
        if (esReservada("estatico")) { esEstatico = true; avanzar(); }

        if (esReservada("funcion") || esReservada("fun")) {
            MetodoDef m = parseMetodoDef(nombre);
            m.acceso = acceso;
            m.esEstatico = esEstatico;
            metodos.push_back(std::move(m));
        } else if (actual.type == TokenType::Identificador) {
            CampoDef c = parseCampoDef();
            c.acceso = acceso;
            c.esEstatico = esEstatico;
            campos.push_back(std::move(c));
        } else {
            error("se esperaba un campo o método dentro de la estructura");
        }
        saltarNuevasLineas();
    }
    esperarReservada("fin");
    auto nodo = std::make_unique<EstructuraDef>();
    nodo->linea = l;
    nodo->nombre = nombre;
    nodo->campos = std::move(campos);
    nodo->metodos = std::move(metodos);
    return nodo;
}

SentPtr Parser::parseInterfaz() {
    int l = actual.line;
    avanzar(); // consume 'interfaz'
    if (actual.type != TokenType::Identificador)
        error("se esperaba el nombre de la interfaz");
    std::string nombre = actual.lexeme;
    avanzar();
    saltarNuevasLineas();
    std::vector<MetodoDef> metodos;
    while (!esReservada("fin")) {
        if (esReservada("funcion") || esReservada("fun")) {
            MetodoDef m = parseMetodoDef(nombre, /*fuerzaAbstracto=*/true);
            m.acceso = ModificadorAcceso::Publico;
            metodos.push_back(std::move(m));
        } else {
            error("las interfaces solo pueden contener firmas de métodos");
        }
        saltarNuevasLineas();
    }
    esperarReservada("fin");
    auto nodo = std::make_unique<InterfazDef>();
    nodo->linea = l;
    nodo->nombre = nombre;
    nodo->metodos = std::move(metodos);
    return nodo;
}
