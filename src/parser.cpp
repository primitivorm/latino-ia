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
    return TipoAnotado::Ninguno;
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
        if (p == "romper") {
            int l = actual.line;
            avanzar();
            consumirFinDeSentencia();
            auto r = std::make_unique<Romper>();
            r->linea = l;
            return r;
        }
        // cierto/verdadero/falso/nulo son expresiones válidas como sentencia.
        if (p == "cierto" || p == "verdadero" || p == "falso" || p == "nulo")
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
            if (actual.type != TokenType::Identificador)
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
