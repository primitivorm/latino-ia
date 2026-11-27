// expandidor_macros.cpp — Macros al estilo de Julia (PLAN_MACROS.md).
//
// El cuerpo de una macro se ejecuta AQUÍ, en tiempo de compilación, mediante un
// intérprete pequeño pero general: los parámetros se enlazan a valores Expr
// (AST sin evaluar) y el resultado debe ser otra expresión, que se inserta en
// el AST del programa antes del análisis semántico. No hay cambios en runtime
// ni en los backends: éstos nunca ven una MacroDef ni una MacroInvocacion.

#include "expandidor_macros.h"

#include "impresor_fuente.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ErrorMacro {
    std::string mensaje;
    int linea = 0;
};

[[noreturn]] void fallar(const std::string& mensaje, int linea = 0) {
    throw ErrorMacro{mensaje, linea};
}

// ---------------------------------------------------------------------------
// Valores del intérprete de macros. `Expr` es un subárbol del AST poseído por
// shared_ptr: se copia al insertarlo en el programa para no compartir
// propiedad con el cuerpo de la macro que lo generó.
// ---------------------------------------------------------------------------

struct ValorMacro;
struct Entorno;

struct ValorFuncionMacro {
    std::shared_ptr<FuncionDef> definicion;
    std::shared_ptr<Entorno> cierre;
};

struct ValorMacro {
    enum class Tipo { Nulo, Numero, Cadena, Logico, Simbolo, Expresion, Lista, Diccionario, Funcion };

    Tipo tipo = Tipo::Nulo;
    double numero = 0.0;
    std::string texto;                 // cadena o símbolo
    bool logico = false;
    bool escapado = false;             // Expr inyectado con esc(): sin higiene
    std::shared_ptr<Expresion> expresion;
    std::vector<ValorMacro> lista;
    std::vector<std::pair<std::string, ValorMacro>> diccionario;
    std::shared_ptr<ValorFuncionMacro> funcion;

    static ValorMacro nulo() { return ValorMacro{}; }
    static ValorMacro numeroV(double n) {
        ValorMacro v; v.tipo = Tipo::Numero; v.numero = n; return v;
    }
    static ValorMacro cadena(std::string s) {
        ValorMacro v; v.tipo = Tipo::Cadena; v.texto = std::move(s); return v;
    }
    static ValorMacro logicoV(bool b) {
        ValorMacro v; v.tipo = Tipo::Logico; v.logico = b; return v;
    }
    static ValorMacro simbolo(std::string s) {
        ValorMacro v; v.tipo = Tipo::Simbolo; v.texto = std::move(s); return v;
    }
    static ValorMacro expr(std::shared_ptr<Expresion> e) {
        ValorMacro v; v.tipo = Tipo::Expresion; v.expresion = std::move(e); return v;
    }
    static ValorMacro listaV(std::vector<ValorMacro> l) {
        ValorMacro v; v.tipo = Tipo::Lista; v.lista = std::move(l); return v;
    }
    static ValorMacro dicV(std::vector<std::pair<std::string, ValorMacro>> d) {
        ValorMacro v; v.tipo = Tipo::Diccionario; v.diccionario = std::move(d); return v;
    }
    static ValorMacro funcionV(std::shared_ptr<ValorFuncionMacro> f) {
        ValorMacro v; v.tipo = Tipo::Funcion; v.funcion = std::move(f); return v;
    }
};

struct Entorno : std::enable_shared_from_this<Entorno> {
    std::shared_ptr<Entorno> padre;
    std::map<std::string, ValorMacro> valores;

    explicit Entorno(std::shared_ptr<Entorno> p = nullptr) : padre(std::move(p)) {}

    ValorMacro* buscar(const std::string& nombre) {
        auto it = valores.find(nombre);
        if (it != valores.end()) return &it->second;
        return padre ? padre->buscar(nombre) : nullptr;
    }
    void definir(const std::string& nombre, ValorMacro valor) {
        valores[nombre] = std::move(valor);
    }
};

std::string nombreTipoValor(const ValorMacro& v) {
    switch (v.tipo) {
        case ValorMacro::Tipo::Nulo: return "nulo";
        case ValorMacro::Tipo::Numero: return "numero";
        case ValorMacro::Tipo::Cadena: return "cadena";
        case ValorMacro::Tipo::Logico: return "logico";
        case ValorMacro::Tipo::Simbolo: return "simbolo";
        case ValorMacro::Tipo::Expresion: return "expr";
        case ValorMacro::Tipo::Lista: return "lista";
        case ValorMacro::Tipo::Diccionario: return "dic";
        case ValorMacro::Tipo::Funcion: return "funcion";
    }
    return "nulo";
}

// ---------------------------------------------------------------------------
// Clonación del AST. Se usa tanto para copiar fragmentos al expander como para
// evaluar las interpolaciones `$` dentro de `quote`. `modo`:
//   0 = copiar tal cual; 1 = evaluar `$`; 2 = expander `@...`.
// ---------------------------------------------------------------------------

constexpr int M_COPIAR = 0;
constexpr int M_INTERPOLAR = 1;
constexpr int M_EXPANDER = 2;

class SistemaMacros {
public:
    explicit SistemaMacros(std::ostream& reporte) : reporte(reporte) {}

    void recolectar(Programa& programa);
    bool expandir(Programa& programa);

private:
    struct Salida {
        enum class Control { Normal, Retorno, Rompe };
        Control control = Control::Normal;
        ValorMacro valor;
    };

    std::ostream& reporte;
    std::map<std::string, MacroDef*> definiciones;
    std::vector<std::unique_ptr<MacroDef>> definicionesPropias;
    int contadorGensym = 0;
    int profundidad = 0;

    // --- intérprete del cuerpo de macro ---
    ValorMacro invocarMacro(const std::string& nombre, std::vector<ValorMacro> argumentos, int linea);
    ValorMacro ejecutarCuerpo(MacroDef& def, const std::vector<ValorMacro>& argumentos);
    Salida ejecutar(Sentencia& sentencia, Entorno& entorno);
    Salida ejecutarLista(ListaSent& lista, Entorno& entorno);
    ValorMacro evaluar(const Expresion* expresion, Entorno& entorno);
    ValorMacro evaluarLlamada(const Llamada& llamada, Entorno& entorno);
    ValorMacro evaluarBinaria(const Binaria& binaria, Entorno& entorno);
    ValorMacro llamarFuncion(const ValorMacro& funcion, std::vector<ValorMacro> argumentos, int linea);
    ValorMacro construirExpr(const std::string& tipo, const std::vector<ValorMacro>& argumentos, int linea);
    ValorMacro macroIncluida(const std::string& nombre, const std::vector<ValorMacro>& argumentos, int linea);

    // --- construcción / copia / higiene del AST ---
    ExprPtr aExpresion(const ValorMacro& valor, int linea);
    SentPtr aSentencia(const ValorMacro& valor, int linea);
    void agregarSentencias(std::vector<SentPtr>& destino, const ValorMacro& valor, int linea);
    ExprPtr copiarExpr(const Expresion* expresion, int modo, Entorno* entorno);
    SentPtr copiarSent(const Sentencia* sentencia, int modo, Entorno* entorno);
    ListaSent copiarLista(const ListaSent& lista, int modo, Entorno* entorno);
    ExprPtr expandirInvocacion(MacroInvocacion& invocacion);
    void recogerLocales(const Expresion* expresion, std::set<std::string>& locales);
    void recogerLocales(const Sentencia* sentencia, std::set<std::string>& locales);
    void renombrar(ExprPtr& expresion, const std::map<std::string, std::string>& mapa);
    void renombrar(std::shared_ptr<Expresion>& expresion,
                   const std::map<std::string, std::string>& mapa);
    void renombrar(SentPtr& sentencia, const std::map<std::string, std::string>& mapa);
    void renombrarLista(ListaSent& lista, const std::map<std::string, std::string>& mapa);
    bool transformarLista(ListaSent& lista);
};

// ---------------------------------------------------------------------------
// Entorno: asignar también actualiza un ámbito padre si la variable existe ahí.
// ---------------------------------------------------------------------------

void asignarEnEntorno(Entorno& entorno, const std::string& nombre, ValorMacro valor) {
    if (entorno.buscar(nombre)) {
        Entorno* actual = &entorno;
        while (actual && actual->valores.find(nombre) == actual->valores.end())
            actual = actual->padre.get();
        if (actual) { actual->valores[nombre] = std::move(valor); return; }
    }
    entorno.definir(nombre, std::move(valor));
}

bool esVerdadero(const ValorMacro& v) {
    switch (v.tipo) {
        case ValorMacro::Tipo::Nulo: return false;
        case ValorMacro::Tipo::Numero: return v.numero != 0.0;
        case ValorMacro::Tipo::Cadena: return !v.texto.empty();
        case ValorMacro::Tipo::Logico: return v.logico;
        case ValorMacro::Tipo::Lista: return !v.lista.empty();
        case ValorMacro::Tipo::Diccionario: return !v.diccionario.empty();
        default: return true;
    }
}

std::string valorATexto(const ValorMacro& v, int linea) {
    switch (v.tipo) {
        case ValorMacro::Tipo::Cadena: return v.texto;
        case ValorMacro::Tipo::Simbolo: return ":" + v.texto;
        case ValorMacro::Tipo::Numero: {
            std::ostringstream os;
            os << v.numero;
            return os.str();
        }
        case ValorMacro::Tipo::Logico: return v.logico ? "cierto" : "falso";
        case ValorMacro::Tipo::Nulo: return "nulo";
        case ValorMacro::Tipo::Expresion: return ImpresorFuente::aTexto(*v.expresion);
        case ValorMacro::Tipo::Lista: {
            std::string s = "[";
            for (size_t i = 0; i < v.lista.size(); i++) {
                if (i) s += ", ";
                s += valorATexto(v.lista[i], linea);
            }
            return s + "]";
        }
        default: fallar("no se puede convertir el valor " + nombreTipoValor(v) + " a cadena", linea);
    }
}

// ---------------------------------------------------------------------------
// Copia de AST con tres modos. Se mantienen línea y la marca macroEscapado.
// ---------------------------------------------------------------------------

ExprPtr SistemaMacros::copiarExpr(const Expresion* e, int modo, Entorno* entorno) {
    if (!e) return nullptr;

    if (modo == M_EXPANDER) {
        if (auto* m = dynamic_cast<const MacroInvocacion*>(e)) {
            auto copia = std::make_unique<MacroInvocacion>();
            copia->linea = e->linea;
            copia->macroEscapado = e->macroEscapado;
            copia->nombre = m->nombre;
            for (const auto& a : m->argumentos)
                copia->argumentos.push_back(copiarExpr(a.get(), M_COPIAR, nullptr));
            return expandirInvocacion(*copia);
        }
    }

    if (modo == M_INTERPOLAR) {
        if (auto* i = dynamic_cast<const InterpolacionExpr*>(e)) {
            if (!entorno) fallar("interpolación $ fuera de un cuerpo de macro", e->linea);
            ValorMacro valor = evaluar(i->expresion.get(), *entorno);
            ExprPtr resultado = aExpresion(valor, e->linea);
            // Todo lo interpolado con $ proviene del contexto de la llamada:
            // conserva sus nombres (es el equivalente a esc() implícito de
            // Julia para los argumentos interpolados).
            resultado->macroEscapado = true;
            return resultado;
        }
        // esc(expr) dentro de un quote: se evalúa en tiempo de compilación y
        // el fragmento resultante se inserta marcado como escapado, de modo
        // que la higiene no renombre sus identificadores.
        if (auto* llam = dynamic_cast<const Llamada*>(e)) {
            auto* id = dynamic_cast<const Identificador*>(llam->destino.get());
            if (id && id->nombre == "esc") {
                if (!entorno) fallar("esc() fuera de un cuerpo de macro", e->linea);
                if (llam->argumentos.size() != 1) fallar("esc() espera un argumento", e->linea);
                ValorMacro valor = evaluar(llam->argumentos[0].get(), *entorno);
                ExprPtr resultado = aExpresion(valor, e->linea);
                resultado->macroEscapado = true;
                return resultado;
            }
        }
    }

    auto copiaBase = [&](auto* n) {
        n->linea = e->linea;
        n->macroEscapado = e->macroEscapado;
    };

    // MacroInvocacion también se copia en modo M_COPIAR: es un nodo opaco
    // para el intérprete hasta que se expande explícitamente con M_EXPANDER
    // (necesario para @expandir_macro @otra_macro ...).
    if (auto* m = dynamic_cast<const MacroInvocacion*>(e)) {
        auto c = std::make_unique<MacroInvocacion>();
        copiaBase(c.get());
        c->nombre = m->nombre;
        for (const auto& a : m->argumentos)
            c->argumentos.push_back(copiarExpr(a.get(), modo, entorno));
        return c;
    }

    if (dynamic_cast<const LitNumero*>(e) || dynamic_cast<const LitCadena*>(e) ||
        dynamic_cast<const LitLogico*>(e) || dynamic_cast<const LitNulo*>(e) ||
        dynamic_cast<const Identificador*>(e) || dynamic_cast<const SimboloExpr*>(e) ||
        dynamic_cast<const VarArgs*>(e) || dynamic_cast<const AccesoEste*>(e)) {
        // Nodos sin hijos: copia directa (incluye línea y macroEscapado).
        if (auto* n = dynamic_cast<const LitNumero*>(e)) return std::make_unique<LitNumero>(*n);
        if (auto* n = dynamic_cast<const LitCadena*>(e)) return std::make_unique<LitCadena>(*n);
        if (auto* n = dynamic_cast<const LitLogico*>(e)) return std::make_unique<LitLogico>(*n);
        if (dynamic_cast<const LitNulo*>(e)) return std::make_unique<LitNulo>();
        if (auto* n = dynamic_cast<const Identificador*>(e)) return std::make_unique<Identificador>(*n);
        if (auto* n = dynamic_cast<const SimboloExpr*>(e)) return std::make_unique<SimboloExpr>(*n);
        if (dynamic_cast<const VarArgs*>(e)) return std::make_unique<VarArgs>();
        return std::make_unique<AccesoEste>();
    }

    if (auto* n = dynamic_cast<const Binaria*>(e)) {
        auto c = std::make_unique<Binaria>(); copiaBase(c.get());
        c->op = n->op;
        c->izq = copiarExpr(n->izq.get(), modo, entorno);
        c->der = copiarExpr(n->der.get(), modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Unaria*>(e)) {
        auto c = std::make_unique<Unaria>(); copiaBase(c.get());
        c->op = n->op;
        c->operando = copiarExpr(n->operando.get(), modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const PostOperador*>(e)) {
        auto c = std::make_unique<PostOperador>(); copiaBase(c.get());
        c->op = n->op;
        c->operando = copiarExpr(n->operando.get(), modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Ternaria*>(e)) {
        auto c = std::make_unique<Ternaria>(); copiaBase(c.get());
        c->condicion = copiarExpr(n->condicion.get(), modo, entorno);
        c->siCierto = copiarExpr(n->siCierto.get(), modo, entorno);
        c->siFalso = copiarExpr(n->siFalso.get(), modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const AccesoIndice*>(e)) {
        auto c = std::make_unique<AccesoIndice>(); copiaBase(c.get());
        c->objeto = copiarExpr(n->objeto.get(), modo, entorno);
        c->indice = copiarExpr(n->indice.get(), modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const AccesoMiembro*>(e)) {
        auto c = std::make_unique<AccesoMiembro>(); copiaBase(c.get());
        c->objeto = copiarExpr(n->objeto.get(), modo, entorno);
        c->miembro = n->miembro;
        return c;
    }
    if (auto* n = dynamic_cast<const Llamada*>(e)) {
        auto c = std::make_unique<Llamada>(); copiaBase(c.get());
        c->destino = copiarExpr(n->destino.get(), modo, entorno);
        c->tipoArgsExplicitos = n->tipoArgsExplicitos;
        for (const auto& a : n->argumentos)
            c->argumentos.push_back(copiarExpr(a.get(), modo, entorno));
        return c;
    }
    if (auto* n = dynamic_cast<const ListaLiteral*>(e)) {
        auto c = std::make_unique<ListaLiteral>(); copiaBase(c.get());
        for (const auto& a : n->elementos)
            c->elementos.push_back(copiarExpr(a.get(), modo, entorno));
        return c;
    }
    if (auto* n = dynamic_cast<const DiccionarioLiteral*>(e)) {
        auto c = std::make_unique<DiccionarioLiteral>(); copiaBase(c.get());
        for (const auto& p : n->pares)
            c->pares.push_back({copiarExpr(p.clave.get(), modo, entorno),
                                copiarExpr(p.valor.get(), modo, entorno)});
        return c;
    }
    if (auto* n = dynamic_cast<const FuncionAnonima*>(e)) {
        auto c = std::make_unique<FuncionAnonima>(); copiaBase(c.get());
        c->parametros = n->parametros;
        c->variadico = n->variadico;
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const NuevoExpr*>(e)) {
        auto c = std::make_unique<NuevoExpr>(); copiaBase(c.get());
        c->clase = n->clase; c->tipoArgs = n->tipoArgs;
        c->esArreglo = n->esArreglo; c->tipoElemento = n->tipoElemento;
        c->tipoElementoClase = n->tipoElementoClase;
        c->tamanoArreglo = copiarExpr(n->tamanoArreglo.get(), modo, entorno);
        for (const auto& a : n->argumentos) c->argumentos.push_back(copiarExpr(a.get(), modo, entorno));
        for (const auto& a : n->tamanosArreglo) c->tamanosArreglo.push_back(copiarExpr(a.get(), modo, entorno));
        return c;
    }
    if (auto* n = dynamic_cast<const EsExpr*>(e)) {
        auto c = std::make_unique<EsExpr>(); copiaBase(c.get());
        c->objeto = copiarExpr(n->objeto.get(), modo, entorno);
        c->clase = n->clase;
        return c;
    }
    if (auto* n = dynamic_cast<const InterpolacionExpr*>(e)) {
        auto c = std::make_unique<InterpolacionExpr>(); copiaBase(c.get());
        c->expresion = copiarExpr(n->expresion.get(), modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const CitaExpr*>(e)) {
        // Una cita anidada evalúa sus propias interpolaciones cuando se
        // evalúe a su vez, así que aquí se copia sin tocarlas.
        auto c = std::make_unique<CitaExpr>(); copiaBase(c.get());
        c->cuerpo = copiarLista(n->cuerpo, M_COPIAR, nullptr);
        return c;
    }
    if (auto* n = dynamic_cast<const BloqueMacroExpr*>(e)) {
        auto c = std::make_unique<BloqueMacroExpr>(); copiaBase(c.get());
        c->sentencias = copiarLista(n->sentencias, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const FragmentoExpr*>(e)) {
        auto c = std::make_unique<FragmentoExpr>(); copiaBase(c.get());
        c->expresion = copiarExpr(n->expresion.get(), modo, entorno);
        c->escapado = n->escapado;
        return c;
    }

    fallar("expresión no soportada dentro de una macro", e->linea);
}

SentPtr SistemaMacros::copiarSent(const Sentencia* s, int modo, Entorno* entorno) {
    if (!s) return nullptr;

    auto copiaBase = [&](auto* n) {
        n->linea = s->linea;
        n->macroEscapado = s->macroEscapado;
    };

    if (auto* n = dynamic_cast<const Asignacion*>(s)) {
        auto c = std::make_unique<Asignacion>(); copiaBase(c.get());
        c->esVar = n->esVar; c->esConst = n->esConst; c->exportado = n->exportado;
        c->esDefecto = n->esDefecto; c->esGlobal = n->esGlobal;
        c->tiposDestino = n->tiposDestino;
        c->tiposElementoDestino = n->tiposElementoDestino;
        c->tiposElementoClaseDestino = n->tiposElementoClaseDestino;
        c->tamanosDestinoArreglo = n->tamanosDestinoArreglo;
        for (const auto& d : n->destinos) c->destinos.push_back(copiarExpr(d.get(), modo, entorno));
        for (const auto& v : n->valores) c->valores.push_back(copiarExpr(v.get(), modo, entorno));
        return c;
    }
    if (auto* n = dynamic_cast<const ExprSentencia*>(s)) {
        auto c = std::make_unique<ExprSentencia>(); copiaBase(c.get());
        c->expr = copiarExpr(n->expr.get(), modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Retornar*>(s)) {
        auto c = std::make_unique<Retornar>(); copiaBase(c.get());
        for (const auto& v : n->valores) c->valores.push_back(copiarExpr(v.get(), modo, entorno));
        return c;
    }
    if (auto* n = dynamic_cast<const Si*>(s)) {
        auto c = std::make_unique<Si>(); copiaBase(c.get());
        c->condicion = copiarExpr(n->condicion.get(), modo, entorno);
        c->entonces = copiarLista(n->entonces, modo, entorno);
        for (const auto& r : n->osis)
            c->osis.push_back({copiarExpr(r.condicion.get(), modo, entorno),
                               copiarLista(r.cuerpo, modo, entorno)});
        c->tieneSino = n->tieneSino;
        c->sino = copiarLista(n->sino, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Elegir*>(s)) {
        auto c = std::make_unique<Elegir>(); copiaBase(c.get());
        c->opcion = copiarExpr(n->opcion.get(), modo, entorno);
        for (const auto& caso : n->casos)
            c->casos.push_back({copiarExpr(caso.valor.get(), modo, entorno),
                                copiarLista(caso.cuerpo, modo, entorno)});
        c->defecto = copiarLista(n->defecto, modo, entorno);
        c->tieneDefecto = n->tieneDefecto;
        return c;
    }
    if (auto* n = dynamic_cast<const Mientras*>(s)) {
        auto c = std::make_unique<Mientras>(); copiaBase(c.get());
        c->condicion = copiarExpr(n->condicion.get(), modo, entorno);
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Desde*>(s)) {
        auto c = std::make_unique<Desde>(); copiaBase(c.get());
        c->inicio = copiarSent(n->inicio.get(), modo, entorno);
        c->condicion = copiarExpr(n->condicion.get(), modo, entorno);
        c->incremento = copiarSent(n->incremento.get(), modo, entorno);
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Para*>(s)) {
        auto c = std::make_unique<Para>(); copiaBase(c.get());
        c->variable = n->variable;
        c->iterable = copiarExpr(n->iterable.get(), modo, entorno);
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Repetir*>(s)) {
        auto c = std::make_unique<Repetir>(); copiaBase(c.get());
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        c->condicionHasta = copiarExpr(n->condicionHasta.get(), modo, entorno);
        return c;
    }
    if (dynamic_cast<const Romper*>(s)) return std::make_unique<Romper>();
    if (auto* n = dynamic_cast<const FuncionDef*>(s)) {
        auto c = std::make_unique<FuncionDef>(); copiaBase(c.get());
        c->nombre = n->nombre; c->genericos = n->genericos; c->parametros = n->parametros;
        c->tipoRetorno = n->tipoRetorno; c->tipoRetornoClase = n->tipoRetornoClase;
        c->tipoRetornoArgs = n->tipoRetornoArgs;
        c->tipoRetornoElemento = n->tipoRetornoElemento;
        c->tipoRetornoElementoClase = n->tipoRetornoElementoClase;
        c->tamanosRetornoArreglo = n->tamanosRetornoArreglo;
        c->variadico = n->variadico; c->exportado = n->exportado;
        c->esDefecto = n->esDefecto; c->inseguro = n->inseguro; c->esGlobal = n->esGlobal;
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const MacroDef*>(s)) {
        auto c = std::make_unique<MacroDef>(); copiaBase(c.get());
        c->nombre = n->nombre; c->parametros = n->parametros; c->exportado = n->exportado;
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const InseguroBloque*>(s)) {
        auto c = std::make_unique<InseguroBloque>(); copiaBase(c.get());
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const HiloDef*>(s)) {
        auto c = std::make_unique<HiloDef>(); copiaBase(c.get());
        c->nombre = n->nombre;
        if (n->cuerpo) {
            auto fn = std::make_unique<FuncionAnonima>();
            fn->linea = n->cuerpo->linea;
            fn->variadico = n->cuerpo->variadico;
            fn->cuerpo = copiarLista(n->cuerpo->cuerpo, modo, entorno);
            fn->parametros = n->cuerpo->parametros;
            for (size_t i = 0; i < fn->parametros.size(); ++i)
                fn->parametros[i].valorInicial =
                    copiarExpr(n->cuerpo->parametros[i].valorInicial.get(), modo, entorno);
            c->cuerpo = std::move(fn);
        }
        return c;
    }
    if (auto* n = dynamic_cast<const Esperar*>(s)) {
        auto c = std::make_unique<Esperar>(); copiaBase(c.get());
        c->hilo = copiarExpr(n->hilo.get(), modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Bloquear*>(s)) {
        auto c = std::make_unique<Bloquear>(); copiaBase(c.get());
        c->cerrojo = copiarExpr(n->cerrojo.get(), modo, entorno);
        c->cuerpo = copiarLista(n->cuerpo, modo, entorno);
        return c;
    }

    // Declaraciones de POO: se copian campo por campo y se recorren los cuerpos
    // de métodos para expander macros que aparezcan dentro de ellos.
    auto copiarCampos = [&](const std::vector<CampoDef>& campos) {
        std::vector<CampoDef> resultado;
        for (const auto& campo : campos) {
            // CampoDef contiene un ExprPtr: no es copizable por valor.
            CampoDef copia;
            copia.nombre = campo.nombre;
            copia.tipoAnotado = campo.tipoAnotado;
            copia.tipoClase = campo.tipoClase;
            copia.tipoArgs = campo.tipoArgs;
            copia.tipoElemento = campo.tipoElemento;
            copia.tipoElementoClase = campo.tipoElementoClase;
            copia.tamanosArreglo = campo.tamanosArreglo;
            copia.acceso = campo.acceso;
            copia.esEstatico = campo.esEstatico;
            copia.linea = campo.linea;
            copia.valorDefecto = copiarExpr(campo.valorDefecto.get(), modo, entorno);
            resultado.push_back(std::move(copia));
        }
        return resultado;
    };
    auto copiarMetodos = [&](const std::vector<MetodoDef>& metodos) {
        std::vector<MetodoDef> resultado;
        for (const auto& metodo : metodos) {
            // MetodoDef contiene una ListaSent: no es copizable por valor.
            MetodoDef copia;
            copia.nombre = metodo.nombre;
            copia.parametros = metodo.parametros;
            copia.tipoRetorno = metodo.tipoRetorno;
            copia.tipoRetornoClase = metodo.tipoRetornoClase;
            copia.tipoRetornoArgs = metodo.tipoRetornoArgs;
            copia.tipoRetornoElemento = metodo.tipoRetornoElemento;
            copia.tipoRetornoElementoClase = metodo.tipoRetornoElementoClase;
            copia.tamanosRetornoArreglo = metodo.tamanosRetornoArreglo;
            copia.genericos = metodo.genericos;
            copia.acceso = metodo.acceso;
            copia.esEstatico = metodo.esEstatico;
            copia.esAbstracto = metodo.esAbstracto;
            copia.esSobreescritura = metodo.esSobreescritura;
            copia.esConstructor = metodo.esConstructor;
            copia.linea = metodo.linea;
            copia.cuerpo = copiarLista(metodo.cuerpo, modo, entorno);
            resultado.push_back(std::move(copia));
        }
        return resultado;
    };
    if (auto* n = dynamic_cast<const ClaseDef*>(s)) {
        auto c = std::make_unique<ClaseDef>(); copiaBase(c.get());
        c->nombre = n->nombre; c->genericos = n->genericos; c->padre = n->padre;
        c->interfaces = n->interfaces; c->esAbstracta = n->esAbstracta;
        c->campos = copiarCampos(n->campos); c->metodos = copiarMetodos(n->metodos);
        c->exportado = n->exportado; c->esDefecto = n->esDefecto;
        return c;
    }
    if (auto* n = dynamic_cast<const EstructuraDef*>(s)) {
        auto c = std::make_unique<EstructuraDef>(); copiaBase(c.get());
        c->nombre = n->nombre; c->genericos = n->genericos;
        c->campos = copiarCampos(n->campos); c->metodos = copiarMetodos(n->metodos);
        c->exportado = n->exportado; c->esDefecto = n->esDefecto;
        return c;
    }
    if (auto* n = dynamic_cast<const InterfazDef*>(s)) {
        auto c = std::make_unique<InterfazDef>(); copiaBase(c.get());
        c->nombre = n->nombre; c->genericos = n->genericos;
        c->metodos = copiarMetodos(n->metodos);
        c->exportado = n->exportado; c->esDefecto = n->esDefecto;
        return c;
    }
    if (auto* n = dynamic_cast<const LlamadaBase*>(s)) {
        auto c = std::make_unique<LlamadaBase>(); copiaBase(c.get());
        for (const auto& a : n->argumentos) c->argumentos.push_back(copiarExpr(a.get(), modo, entorno));
        return c;
    }
    if (auto* n = dynamic_cast<const Programa*>(s)) {
        auto c = std::make_unique<Programa>(); copiaBase(c.get());
        c->sentencias = copiarLista(n->sentencias, modo, entorno);
        return c;
    }
    if (auto* n = dynamic_cast<const Incluir*>(s)) return std::make_unique<Incluir>(*n);
    if (auto* n = dynamic_cast<const ImportarDecl*>(s)) return std::make_unique<ImportarDecl>(*n);
    if (auto* n = dynamic_cast<const ExportarDesde*>(s)) return std::make_unique<ExportarDesde>(*n);
    if (auto* n = dynamic_cast<const ExternoBloque*>(s)) return std::make_unique<ExternoBloque>(*n);

    fallar("sentencia no soportada dentro de una macro", s->linea);
}

ListaSent SistemaMacros::copiarLista(const ListaSent& lista, int modo, Entorno* entorno) {
    ListaSent resultado;
    resultado.reserve(lista.size());
    for (const auto& s : lista) resultado.push_back(copiarSent(s.get(), modo, entorno));
    return resultado;
}

// ---------------------------------------------------------------------------
// Conversión ValorMacro <-> AST y transformación del programa.
// ---------------------------------------------------------------------------

ExprPtr SistemaMacros::aExpresion(const ValorMacro& valor, int linea) {
    ExprPtr resultado;
    switch (valor.tipo) {
        case ValorMacro::Tipo::Numero: {
            auto n = std::make_unique<LitNumero>();
            n->valor = valor.numero;
            n->esEntero = std::floor(valor.numero) == valor.numero;
            n->lexema = valorATexto(valor, linea);
            resultado = std::move(n);
            break;
        }
        case ValorMacro::Tipo::Cadena: {
            auto n = std::make_unique<LitCadena>();
            n->valor = valor.texto;
            resultado = std::move(n);
            break;
        }
        case ValorMacro::Tipo::Logico: {
            auto n = std::make_unique<LitLogico>();
            n->valor = valor.logico;
            resultado = std::move(n);
            break;
        }
        case ValorMacro::Tipo::Nulo:
            resultado = std::make_unique<LitNulo>();
            break;
        case ValorMacro::Tipo::Simbolo: {
            // :nombre se convierte en identificador al insertarse en código;
            // el texto ":nombre" solo existe dentro del intérprete.
            auto n = std::make_unique<Identificador>();
            n->nombre = valor.texto;
            resultado = std::move(n);
            break;
        }
        case ValorMacro::Tipo::Expresion:
            if (!valor.expresion) fallar("expresión de macro vacía", linea);
            resultado = copiarExpr(valor.expresion.get(), M_COPIAR, nullptr);
            break;
        case ValorMacro::Tipo::Lista: {
            auto n = std::make_unique<ListaLiteral>();
            for (const auto& v : valor.lista) n->elementos.push_back(aExpresion(v, linea));
            resultado = std::move(n);
            break;
        }
        case ValorMacro::Tipo::Diccionario: {
            auto n = std::make_unique<DiccionarioLiteral>();
            for (const auto& p : valor.diccionario)
                n->pares.push_back({aExpresion(ValorMacro::cadena(p.first), linea),
                                    aExpresion(p.second, linea)});
            resultado = std::move(n);
            break;
        }
        case ValorMacro::Tipo::Funcion:
            fallar("una función no se puede insertar como expresión generada", linea);
    }
    resultado->linea = linea;
    if (valor.escapado) resultado->macroEscapado = true;
    return resultado;
}

SentPtr SistemaMacros::aSentencia(const ValorMacro& valor, int linea) {
    if (valor.tipo == ValorMacro::Tipo::Expresion && valor.expresion) {
        if (auto* s = dynamic_cast<const Sentencia*>(valor.expresion.get())) {
            SentPtr resultado = copiarSent(s, M_COPIAR, nullptr);
            resultado->linea = linea;
            if (valor.escapado) resultado->macroEscapado = true;
            return resultado;
        }
    }
    auto resultado = std::make_unique<ExprSentencia>();
    resultado->linea = linea;
    resultado->expr = aExpresion(valor, linea);
    resultado->macroEscapado = resultado->expr->macroEscapado;
    return resultado;
}

void SistemaMacros::agregarSentencias(std::vector<SentPtr>& destino,
                                      const ValorMacro& valor, int linea) {
    ListaSent temporal;
    if (valor.tipo == ValorMacro::Tipo::Expresion && valor.expresion) {
        if (auto* bloque = dynamic_cast<const BloqueMacroExpr*>(valor.expresion.get())) {
            temporal = copiarLista(bloque->sentencias, M_COPIAR, nullptr);
            if (!transformarLista(temporal))
                fallar("error al expander macros en el bloque generado", linea);
            for (auto& s : temporal) destino.push_back(std::move(s));
            return;
        }
    }
    temporal.push_back(aSentencia(valor, linea));
    if (!transformarLista(temporal))
        fallar("error al expander macros en el código generado", linea);
    for (auto& s : temporal) destino.push_back(std::move(s));
}

// --- Higiene: renombrar las variables introducidas por quote ---------------

void SistemaMacros::recogerLocales(const Expresion* e, std::set<std::string>& locales) {
    if (!e || e->macroEscapado) return;
    if (auto* n = dynamic_cast<const InterpolacionExpr*>(e)) {
        recogerLocales(n->expresion.get(), locales);
    } else if (auto* n = dynamic_cast<const Binaria*>(e)) {
        recogerLocales(n->izq.get(), locales); recogerLocales(n->der.get(), locales);
    } else if (auto* n = dynamic_cast<const Unaria*>(e)) {
        recogerLocales(n->operando.get(), locales);
    } else if (auto* n = dynamic_cast<const PostOperador*>(e)) {
        recogerLocales(n->operando.get(), locales);
    } else if (auto* n = dynamic_cast<const Ternaria*>(e)) {
        recogerLocales(n->condicion.get(), locales);
        recogerLocales(n->siCierto.get(), locales);
        recogerLocales(n->siFalso.get(), locales);
    } else if (auto* n = dynamic_cast<const AccesoIndice*>(e)) {
        recogerLocales(n->objeto.get(), locales); recogerLocales(n->indice.get(), locales);
    } else if (auto* n = dynamic_cast<const AccesoMiembro*>(e)) {
        recogerLocales(n->objeto.get(), locales);
    } else if (auto* n = dynamic_cast<const Llamada*>(e)) {
        recogerLocales(n->destino.get(), locales);
        for (const auto& a : n->argumentos) recogerLocales(a.get(), locales);
    } else if (auto* n = dynamic_cast<const ListaLiteral*>(e)) {
        for (const auto& a : n->elementos) recogerLocales(a.get(), locales);
    } else if (auto* n = dynamic_cast<const DiccionarioLiteral*>(e)) {
        for (const auto& p : n->pares) {
            recogerLocales(p.clave.get(), locales); recogerLocales(p.valor.get(), locales);
        }
    } else if (auto* n = dynamic_cast<const FuncionAnonima*>(e)) {
        for (const auto& p : n->parametros) locales.insert(p.nombre);
        for (const auto& s : n->cuerpo) recogerLocales(s.get(), locales);
    } else if (auto* n = dynamic_cast<const BloqueMacroExpr*>(e)) {
        for (const auto& s : n->sentencias) recogerLocales(s.get(), locales);
    }
    // CitaExpr y FragmentoExpr: no se recorren (pertenecen a otra expansión).
}

void SistemaMacros::recogerLocales(const Sentencia* s, std::set<std::string>& locales) {
    if (!s || s->macroEscapado) return;
    if (auto* n = dynamic_cast<const Asignacion*>(s)) {
        for (const auto& d : n->destinos)
            if (auto* id = dynamic_cast<const Identificador*>(d.get())) locales.insert(id->nombre);
        for (const auto& v : n->valores) recogerLocales(v.get(), locales);
    } else if (auto* n = dynamic_cast<const ExprSentencia*>(s)) {
        recogerLocales(n->expr.get(), locales);
    } else if (auto* n = dynamic_cast<const Retornar*>(s)) {
        for (const auto& v : n->valores) recogerLocales(v.get(), locales);
    } else if (auto* n = dynamic_cast<const Si*>(s)) {
        recogerLocales(n->condicion.get(), locales);
        for (const auto& x : n->entonces) recogerLocales(x.get(), locales);
        for (const auto& r : n->osis) {
            recogerLocales(r.condicion.get(), locales);
            for (const auto& x : r.cuerpo) recogerLocales(x.get(), locales);
        }
        for (const auto& x : n->sino) recogerLocales(x.get(), locales);
    } else if (auto* n = dynamic_cast<const Mientras*>(s)) {
        recogerLocales(n->condicion.get(), locales);
        for (const auto& x : n->cuerpo) recogerLocales(x.get(), locales);
    } else if (auto* n = dynamic_cast<const Desde*>(s)) {
        recogerLocales(n->inicio.get(), locales);
        recogerLocales(n->condicion.get(), locales);
        recogerLocales(n->incremento.get(), locales);
        for (const auto& x : n->cuerpo) recogerLocales(x.get(), locales);
    } else if (auto* n = dynamic_cast<const Para*>(s)) {
        locales.insert(n->variable);
        recogerLocales(n->iterable.get(), locales);
        for (const auto& x : n->cuerpo) recogerLocales(x.get(), locales);
    } else if (auto* n = dynamic_cast<const Repetir*>(s)) {
        for (const auto& x : n->cuerpo) recogerLocales(x.get(), locales);
        recogerLocales(n->condicionHasta.get(), locales);
    } else if (auto* n = dynamic_cast<const FuncionDef*>(s)) {
        locales.insert(n->nombre);
        for (const auto& p : n->parametros) locales.insert(p.nombre);
        for (const auto& x : n->cuerpo) recogerLocales(x.get(), locales);
    } else if (auto* n = dynamic_cast<const MacroDef*>(s)) {
        locales.insert(n->nombre);
        for (const auto& x : n->cuerpo) recogerLocales(x.get(), locales);
    } else if (auto* n = dynamic_cast<const InseguroBloque*>(s)) {
        for (const auto& x : n->cuerpo) recogerLocales(x.get(), locales);
    } else if (auto* n = dynamic_cast<const HiloDef*>(s)) {
        locales.insert(n->nombre);
        if (n->cuerpo) {
            for (const auto& p : n->cuerpo->parametros) locales.insert(p.nombre);
            for (const auto& x : n->cuerpo->cuerpo) recogerLocales(x.get(), locales);
        }
    } else if (auto* n = dynamic_cast<const Esperar*>(s)) {
        recogerLocales(n->hilo.get(), locales);
    } else if (auto* n = dynamic_cast<const Bloquear*>(s)) {
        recogerLocales(n->cerrojo.get(), locales);
        for (const auto& x : n->cuerpo) recogerLocales(x.get(), locales);
    } else if (auto* n = dynamic_cast<const LlamadaBase*>(s)) {
        for (const auto& a : n->argumentos) recogerLocales(a.get(), locales);
    }
}

// PLAN_HILOS: las capturas viven en un shared_ptr. renombrar() nunca
// reemplaza el nodo raíz (solo edita nombres dentro del árbol), así que es
// seguro prestar el puntero a la implementación de ExprPtr y liberar la
// propiedad temporal antes de devolverlo al shared_ptr original.
void SistemaMacros::renombrar(std::shared_ptr<Expresion>& e,
                              const std::map<std::string, std::string>& mapa) {
    if (!e) return;
    ExprPtr prestado(e.get());
    renombrar(prestado, mapa);
    prestado.release();
}

void SistemaMacros::renombrar(ExprPtr& e, const std::map<std::string, std::string>& mapa) {
    if (!e || e->macroEscapado) return;
    if (auto* n = dynamic_cast<Identificador*>(e.get())) {
        auto it = mapa.find(n->nombre);
        if (it != mapa.end()) n->nombre = it->second;
        return;
    }
    if (dynamic_cast<LitNumero*>(e.get()) || dynamic_cast<LitCadena*>(e.get()) ||
        dynamic_cast<LitLogico*>(e.get()) || dynamic_cast<LitNulo*>(e.get()) ||
        dynamic_cast<SimboloExpr*>(e.get()) || dynamic_cast<VarArgs*>(e.get()) ||
        dynamic_cast<AccesoEste*>(e.get())) return;

    if (auto* n = dynamic_cast<Binaria*>(e.get())) {
        renombrar(n->izq, mapa); renombrar(n->der, mapa);
    } else if (auto* n = dynamic_cast<Unaria*>(e.get())) {
        renombrar(n->operando, mapa);
    } else if (auto* n = dynamic_cast<PostOperador*>(e.get())) {
        renombrar(n->operando, mapa);
    } else if (auto* n = dynamic_cast<Ternaria*>(e.get())) {
        renombrar(n->condicion, mapa); renombrar(n->siCierto, mapa); renombrar(n->siFalso, mapa);
    } else if (auto* n = dynamic_cast<AccesoIndice*>(e.get())) {
        renombrar(n->objeto, mapa); renombrar(n->indice, mapa);
    } else if (auto* n = dynamic_cast<AccesoMiembro*>(e.get())) {
        renombrar(n->objeto, mapa);
    } else if (auto* n = dynamic_cast<Llamada*>(e.get())) {
        renombrar(n->destino, mapa);
        for (auto& a : n->argumentos) renombrar(a, mapa);
    } else if (auto* n = dynamic_cast<ListaLiteral*>(e.get())) {
        for (auto& a : n->elementos) renombrar(a, mapa);
    } else if (auto* n = dynamic_cast<DiccionarioLiteral*>(e.get())) {
        for (auto& p : n->pares) { renombrar(p.clave, mapa); renombrar(p.valor, mapa); }
    } else if (auto* n = dynamic_cast<FuncionAnonima*>(e.get())) {
        for (auto& s : n->cuerpo) renombrar(s, mapa);
    } else if (auto* n = dynamic_cast<InterpolacionExpr*>(e.get())) {
        renombrar(n->expresion, mapa);
    } else if (auto* n = dynamic_cast<BloqueMacroExpr*>(e.get())) {
        renombrarLista(n->sentencias, mapa);
    } else if (auto* n = dynamic_cast<FragmentoExpr*>(e.get())) {
        renombrar(n->expresion, mapa);
    }
}

void SistemaMacros::renombrar(SentPtr& s, const std::map<std::string, std::string>& mapa) {
    if (!s || s->macroEscapado) return;
    if (auto* n = dynamic_cast<Asignacion*>(s.get())) {
        for (auto& d : n->destinos) renombrar(d, mapa);
        for (auto& v : n->valores) renombrar(v, mapa);
    } else if (auto* n = dynamic_cast<ExprSentencia*>(s.get())) {
        renombrar(n->expr, mapa);
    } else if (auto* n = dynamic_cast<Retornar*>(s.get())) {
        for (auto& v : n->valores) renombrar(v, mapa);
    } else if (auto* n = dynamic_cast<Si*>(s.get())) {
        renombrar(n->condicion, mapa); renombrarLista(n->entonces, mapa);
        for (auto& r : n->osis) { renombrar(r.condicion, mapa); renombrarLista(r.cuerpo, mapa); }
        renombrarLista(n->sino, mapa);
    } else if (auto* n = dynamic_cast<Mientras*>(s.get())) {
        renombrar(n->condicion, mapa); renombrarLista(n->cuerpo, mapa);
    } else if (auto* n = dynamic_cast<Desde*>(s.get())) {
        renombrar(n->inicio, mapa); renombrar(n->condicion, mapa);
        renombrar(n->incremento, mapa); renombrarLista(n->cuerpo, mapa);
    } else if (auto* n = dynamic_cast<Para*>(s.get())) {
        auto it = mapa.find(n->variable);
        if (it != mapa.end()) n->variable = it->second;
        renombrar(n->iterable, mapa); renombrarLista(n->cuerpo, mapa);
    } else if (auto* n = dynamic_cast<Repetir*>(s.get())) {
        renombrarLista(n->cuerpo, mapa); renombrar(n->condicionHasta, mapa);
    } else if (auto* n = dynamic_cast<FuncionDef*>(s.get())) {
        auto it = mapa.find(n->nombre);
        if (it != mapa.end()) n->nombre = it->second;
        for (auto& x : n->cuerpo) renombrar(x, mapa);
    } else if (auto* n = dynamic_cast<InseguroBloque*>(s.get())) {
        renombrarLista(n->cuerpo, mapa);
    } else if (auto* n = dynamic_cast<HiloDef*>(s.get())) {
        auto it = mapa.find(n->nombre);
        if (it != mapa.end()) n->nombre = it->second;
        if (n->cuerpo) {
            for (auto& p : n->cuerpo->parametros) {
                auto itp = mapa.find(p.nombre);
                if (itp != mapa.end()) p.nombre = itp->second;
                renombrar(p.valorInicial, mapa);
            }
            renombrarLista(n->cuerpo->cuerpo, mapa);
        }
    } else if (auto* n = dynamic_cast<Esperar*>(s.get())) {
        renombrar(n->hilo, mapa);
    } else if (auto* n = dynamic_cast<Bloquear*>(s.get())) {
        renombrar(n->cerrojo, mapa);
        renombrarLista(n->cuerpo, mapa);
    } else if (auto* n = dynamic_cast<LlamadaBase*>(s.get())) {
        for (auto& a : n->argumentos) renombrar(a, mapa);
    }
}

void SistemaMacros::renombrarLista(ListaSent& lista, const std::map<std::string, std::string>& mapa) {
    for (auto& s : lista) renombrar(s, mapa);
}

// ---------------------------------------------------------------------------
// Intérprete del cuerpo de macro: values Latino (números, cadenas, listas,
// diccionarios, símbolos, funciones) y valores Expr (AST sin evaluar).
// ---------------------------------------------------------------------------

ValorMacro SistemaMacros::evaluarBinaria(const Binaria& b, Entorno& entorno) {
    ValorMacro izq = evaluar(b.izq.get(), entorno);
    ValorMacro der = evaluar(b.der.get(), entorno);
    const std::string& op = b.op;

    if (op == "==" || op == "!=") {
        bool iguales = false;
        if (izq.tipo == der.tipo) {
            switch (izq.tipo) {
                case ValorMacro::Tipo::Nulo: iguales = true; break;
                case ValorMacro::Tipo::Numero: iguales = izq.numero == der.numero; break;
                case ValorMacro::Tipo::Cadena:
                case ValorMacro::Tipo::Simbolo: iguales = izq.texto == der.texto; break;
                case ValorMacro::Tipo::Logico: iguales = izq.logico == der.logico; break;
                case ValorMacro::Tipo::Expresion:
                    iguales = ImpresorFuente::aTexto(*izq.expresion) ==
                              ImpresorFuente::aTexto(*der.expresion);
                    break;
                default: iguales = false; break;
            }
        }
        return ValorMacro::logicoV(op == "==" ? iguales : !iguales);
    }
    if (op == "<" || op == ">" || op == "<=" || op == ">=") {
        double a = 0, c = 0;
        if (izq.tipo == ValorMacro::Tipo::Numero) a = izq.numero;
        else if (izq.tipo == ValorMacro::Tipo::Cadena || izq.tipo == ValorMacro::Tipo::Simbolo)
            a = 0;  // las comparaciones de texto usan ==; aquí solo números
        else fallar("comparación no válida dentro de una macro", b.linea);
        if (der.tipo == ValorMacro::Tipo::Numero) c = der.numero;
        else fallar("comparación no válida dentro de una macro", b.linea);
        if (op == "<") return ValorMacro::logicoV(a < c);
        if (op == ">") return ValorMacro::logicoV(a > c);
        if (op == "<=") return ValorMacro::logicoV(a <= c);
        return ValorMacro::logicoV(a >= c);
    }
    if (op == "&&") return ValorMacro::logicoV(esVerdadero(izq) && esVerdadero(der));
    if (op == "||") return ValorMacro::logicoV(esVerdadero(izq) || esVerdadero(der));
    if (op == "..") return ValorMacro::cadena(valorATexto(izq, b.linea) + valorATexto(der, b.linea));
    if (op == "+" && izq.tipo == ValorMacro::Tipo::Cadena)
        return ValorMacro::cadena(valorATexto(izq, b.linea) + valorATexto(der, b.linea));

    if (izq.tipo != ValorMacro::Tipo::Numero || der.tipo != ValorMacro::Tipo::Numero)
        fallar("operador '" + op + "' requiere números dentro de una macro", b.linea);
    double a = izq.numero, c = der.numero;
    if (op == "+") return ValorMacro::numeroV(a + c);
    if (op == "-") return ValorMacro::numeroV(a - c);
    if (op == "*") return ValorMacro::numeroV(a * c);
    if (op == "/") {
        if (c == 0) fallar("división por cero en una macro", b.linea);
        return ValorMacro::numeroV(a / c);
    }
    if (op == "%") {
        if (c == 0) fallar("módulo por cero en una macro", b.linea);
        return ValorMacro::numeroV(std::fmod(a, c));
    }
    if (op == "^") return ValorMacro::numeroV(std::pow(a, c));
    fallar("operador no soportado en macros: '" + op + "'", b.linea);
}

ValorMacro SistemaMacros::construirExpr(const std::string& tipo,
                                        const std::vector<ValorMacro>& argumentos, int linea) {
    auto textoArgumento = [&](const ValorMacro& v) {
        if (v.tipo == ValorMacro::Tipo::Cadena || v.tipo == ValorMacro::Tipo::Simbolo)
            return v.texto;
        fallar("se esperaba un símbolo o cadena como argumento de Expr (tipo=" +
               nombreTipoValor(v) + " '" + valorATexto(v, linea) + "')", linea);
    };
    auto exprArgumento = [&](size_t i) -> ExprPtr {
        if (i >= argumentos.size()) fallar("faltan argumentos en Expr(:" + tipo + ")", linea);
        return aExpresion(argumentos[i], linea);
    };

    // Los nodos que son sentencias (asignación, si, mientras, retornar) se
    // envuelven en un BloqueMacroExpr: así `Expr(:si, ...)` sigue siendo un
    // ValorMacro::Expresion y `agregarSentencias` lo inserta como sentencia.
    auto envolver = [&](SentPtr sentencia) {
        auto bloque = std::make_unique<BloqueMacroExpr>();
        bloque->linea = linea;
        bloque->sentencias.push_back(std::move(sentencia));
        return std::shared_ptr<Expresion>(bloque.release());
    };

    std::shared_ptr<Expresion> resultado;
    // `argumentos` NO incluye el símbolo del tipo: `Expr()` lo separa antes
    // de llamar aquí, y las macros incluidas construyen nodos directamente.
    if (tipo == "numero" || tipo == "cadena" || tipo == "logico" || tipo == "nulo" ||
        tipo == "identificador" || tipo == "simbolo") {
        resultado = aExpresion(argumentos.empty() ? ValorMacro::nulo() : argumentos[0], linea);
        if (tipo == "simbolo") {
            auto s = std::make_unique<SimboloExpr>();
            s->nombre = textoArgumento(argumentos.empty() ? ValorMacro::cadena("") : argumentos[0]);
            s->linea = linea;
            resultado = std::move(s);
        }
    } else if (tipo == "llamada") {
        auto n = std::make_unique<Llamada>();
        n->linea = linea;
        n->destino = exprArgumento(0);
        for (size_t i = 1; i < argumentos.size(); i++)
            n->argumentos.push_back(aExpresion(argumentos[i], linea));
        resultado = std::move(n);
    } else if (tipo == "binaria" || tipo == "unaria") {
        std::string op = textoArgumento(argumentos.empty() ? ValorMacro::cadena("") : argumentos[0]);
        if (tipo == "binaria") {
            auto n = std::make_unique<Binaria>();
            n->linea = linea; n->op = op;
            n->izq = exprArgumento(1); n->der = exprArgumento(2);
            resultado = std::move(n);
        } else if (op == "!" || op == "no") {
            // El runtime no tiene negación lógica como operador unario
            // (`lat_negar` es negación numérica): se baja a `expr == falso`.
            auto n = std::make_unique<Binaria>();
            n->linea = linea; n->op = "==";
            n->izq = exprArgumento(1);
            auto falso = std::make_unique<LitLogico>();
            falso->valor = false;
            falso->linea = linea;
            n->der = std::move(falso);
            resultado = std::move(n);
        } else {
            auto n = std::make_unique<Unaria>();
            n->linea = linea; n->op = op;
            n->operando = exprArgumento(1);
            resultado = std::move(n);
        }
    } else if (tipo == "asignacion") {
        auto n = std::make_unique<Asignacion>();
        n->linea = linea;
        n->destinos.push_back(exprArgumento(0));
        n->valores.push_back(exprArgumento(1));
        if (!dynamic_cast<Identificador*>(n->destinos[0].get()) &&
            !dynamic_cast<AccesoIndice*>(n->destinos[0].get()) &&
            !dynamic_cast<AccesoMiembro*>(n->destinos[0].get()))
            fallar("destino de asignación no válido en Expr(:asignacion)", linea);
        resultado = envolver(std::move(n));
    } else if (tipo == "indice" || tipo == "miembro") {
        if (tipo == "indice") {
            auto n = std::make_unique<AccesoIndice>();
            n->linea = linea;
            n->objeto = exprArgumento(0); n->indice = exprArgumento(1);
            resultado = std::move(n);
        } else {
            auto n = std::make_unique<AccesoMiembro>();
            n->linea = linea;
            n->objeto = exprArgumento(0);
            n->miembro = textoArgumento(argumentos.size() > 1 ? argumentos[1] : ValorMacro::cadena(""));
            resultado = std::move(n);
        }
    } else if (tipo == "lista") {
        auto n = std::make_unique<ListaLiteral>();
        n->linea = linea;
        for (const auto& a : argumentos)
            n->elementos.push_back(aExpresion(a, linea));
        resultado = std::move(n);
    } else if (tipo == "bloque") {
        auto n = std::make_unique<BloqueMacroExpr>();
        n->linea = linea;
        for (const auto& a : argumentos)
            agregarSentencias(n->sentencias, a, linea);
        resultado = std::move(n);
    } else if (tipo == "macro") {
        auto n = std::make_unique<MacroInvocacion>();
        n->linea = linea;
        n->nombre = textoArgumento(argumentos.empty() ? ValorMacro::cadena("") : argumentos[0]);
        for (size_t i = 1; i < argumentos.size(); i++)
            n->argumentos.push_back(aExpresion(argumentos[i], linea));
        resultado = std::move(n);
    } else if (tipo == "retornar") {
        auto n = std::make_unique<Retornar>();
        n->linea = linea;
        for (const auto& a : argumentos)
            n->valores.push_back(aExpresion(a, linea));
        resultado = envolver(std::move(n));
    } else if (tipo == "si") {
        auto n = std::make_unique<Si>();
        n->linea = linea;
        n->condicion = exprArgumento(0);
        if (argumentos.size() > 1)
            agregarSentencias(n->entonces, argumentos[1], linea);
        resultado = envolver(std::move(n));
    } else if (tipo == "mientras") {
        auto n = std::make_unique<Mientras>();
        n->linea = linea;
        n->condicion = exprArgumento(0);
        if (argumentos.size() > 1)
            agregarSentencias(n->cuerpo, argumentos[1], linea);
        resultado = envolver(std::move(n));
    } else {
        fallar("tipo de Expr desconocido: ':" + tipo + "'", linea);
    }
    return ValorMacro::expr(std::move(resultado));
}

ValorMacro SistemaMacros::llamarFuncion(const ValorMacro& funcion,
                                        std::vector<ValorMacro> argumentos, int linea) {
    const auto& def = *funcion.funcion->definicion;
    if (argumentos.size() != def.parametros.size())
        fallar("la función de macro '" + def.nombre + "' espera " +
               std::to_string(def.parametros.size()) + " argumentos", linea);
    auto entornoHijo = std::make_shared<Entorno>(funcion.funcion->cierre);
    for (size_t i = 0; i < argumentos.size(); i++)
        entornoHijo->definir(def.parametros[i].nombre, argumentos[i]);
    ListaSent cuerpo = copiarLista(def.cuerpo, M_COPIAR, nullptr);
    Salida salida = ejecutarLista(cuerpo, *entornoHijo);
    if (salida.control == Salida::Control::Rompe)
        fallar("'romper' fuera de un bucle dentro de una macro", linea);
    return salida.valor;
}

ValorMacro SistemaMacros::evaluarLlamada(const Llamada& llamada, Entorno& entorno) {
    auto* id = dynamic_cast<const Identificador*>(llamada.destino.get());
    if (!id)
        fallar("solo se pueden llamar funciones o builtins de macros por su nombre", llamada.linea);

    const std::string& nombre = id->nombre;
    std::vector<ValorMacro> argumentos;
    for (const auto& a : llamada.argumentos) argumentos.push_back(evaluar(a.get(), entorno));

    if (nombre == "esc") {
        if (argumentos.size() != 1) fallar("esc() espera un argumento", llamada.linea);
        ValorMacro resultado = argumentos[0];
        resultado.escapado = true;
        return resultado;
    }
    if (nombre == "string" || nombre == "acadena") {
        std::string texto;
        for (const auto& a : argumentos) {
            // string(:simbolo) -> "simbolo", sin los dos puntos (como en Julia).
            texto += (a.tipo == ValorMacro::Tipo::Simbolo) ? a.texto
                                                           : valorATexto(a, llamada.linea);
        }
        return ValorMacro::cadena(texto);
    }
    if (nombre == "simbolo" || nombre == "symbol") {
        if (argumentos.size() != 1) fallar("simbolo() espera un argumento", llamada.linea);
        return ValorMacro::simbolo(valorATexto(argumentos[0], llamada.linea));
    }
    // `generar_simbolo` (alias `gensimb`; se conserva `gensym` por
    // compatibilidad) produce un nombre fresco para código generado.
    if (nombre == "generar_simbolo" || nombre == "gensimb" || nombre == "gensym") {
        std::string prefijo = argumentos.empty() ? "__gensimb" :
                              valorATexto(argumentos[0], llamada.linea);
        return ValorMacro::simbolo(prefijo + std::to_string(++contadorGensym));
    }
    if (nombre == "Expr" || nombre == "expr") {
        if (argumentos.empty()) fallar("Expr() necesita al menos el tipo", llamada.linea);
        // El primer argumento es el tipo (:llamada, :binaria, ...): se extrae
        // como texto de símbolo, sin los dos puntos que añade valorATexto().
        std::string tipo = (argumentos[0].tipo == ValorMacro::Tipo::Simbolo ||
                            argumentos[0].tipo == ValorMacro::Tipo::Cadena)
                               ? argumentos[0].texto
                               : valorATexto(argumentos[0], llamada.linea);
        std::vector<ValorMacro> resto(argumentos.begin() + 1, argumentos.end());
        return construirExpr(tipo, resto, llamada.linea);
    }
    if (nombre == "longitud") {
        if (argumentos.size() != 1) fallar("longitud() espera un argumento", llamada.linea);
        const auto& v = argumentos[0];
        if (v.tipo == ValorMacro::Tipo::Lista) return ValorMacro::numeroV((double)v.lista.size());
        if (v.tipo == ValorMacro::Tipo::Cadena) return ValorMacro::numeroV((double)v.texto.size());
        if (v.tipo == ValorMacro::Tipo::Diccionario) return ValorMacro::numeroV((double)v.diccionario.size());
        if (v.tipo == ValorMacro::Tipo::Expresion) {
            if (auto* c = dynamic_cast<const Llamada*>(v.expresion.get()))
                return ValorMacro::numeroV((double)c->argumentos.size());
            if (auto* l = dynamic_cast<const ListaLiteral*>(v.expresion.get()))
                return ValorMacro::numeroV((double)l->elementos.size());
            if (auto* b = dynamic_cast<const BloqueMacroExpr*>(v.expresion.get()))
                return ValorMacro::numeroV((double)b->sentencias.size());
        }
        fallar("longitud() no aplicable a este valor de macro", llamada.linea);
    }
    if (nombre == "tipo") {
        if (argumentos.size() != 1) fallar("tipo() espera un argumento", llamada.linea);
        return ValorMacro::simbolo(nombreTipoValor(argumentos[0]));
    }
    if (nombre == "escribir" || nombre == "imprimir" || nombre == "poner") {
        for (const auto& a : argumentos) std::cout << valorATexto(a, llamada.linea) << ' ';
        std::cout << '\n';
        return ValorMacro::nulo();
    }
    if (nombre == "rango") {
        if (argumentos.size() != 1 || argumentos[0].tipo != ValorMacro::Tipo::Numero)
            fallar("rango() espera un número", llamada.linea);
        std::vector<ValorMacro> valores;
        for (int i = 0; i < static_cast<int>(argumentos[0].numero); i++)
            valores.push_back(ValorMacro::numeroV(i));
        return ValorMacro::listaV(std::move(valores));
    }
    if (nombre == "error") {
        if (argumentos.empty()) fallar("error() en macro", llamada.linea);
        fallar(valorATexto(argumentos[0], llamada.linea), llamada.linea);
    }
    if (auto* funcion = entorno.buscar(nombre)) {
        if (funcion->tipo != ValorMacro::Tipo::Funcion)
            fallar("'" + nombre + "' no es una función dentro de la macro", llamada.linea);
        return llamarFuncion(*funcion, argumentos, llamada.linea);
    }
    fallar("función no definida dentro de la macro: '" + nombre + "'", llamada.linea);
}

ValorMacro SistemaMacros::evaluar(const Expresion* expresion, Entorno& entorno) {
    if (!expresion) return ValorMacro::nulo();

    if (auto* n = dynamic_cast<const LitNumero*>(expresion)) return ValorMacro::numeroV(n->valor);
    if (auto* n = dynamic_cast<const LitCadena*>(expresion)) return ValorMacro::cadena(n->valor);
    if (auto* n = dynamic_cast<const LitLogico*>(expresion)) return ValorMacro::logicoV(n->valor);
    if (dynamic_cast<const LitNulo*>(expresion)) return ValorMacro::nulo();
    if (auto* n = dynamic_cast<const SimboloExpr*>(expresion)) return ValorMacro::simbolo(n->nombre);
    if (auto* n = dynamic_cast<const Identificador*>(expresion)) {
        ValorMacro* valor = entorno.buscar(n->nombre);
        if (!valor) fallar("variable no declarada en la macro: '" + n->nombre + "'", n->linea);
        return *valor;
    }
    if (auto* n = dynamic_cast<const InterpolacionExpr*>(expresion))
        return evaluar(n->expresion.get(), entorno);
    if (auto* n = dynamic_cast<const Binaria*>(expresion)) return evaluarBinaria(*n, entorno);
    if (auto* n = dynamic_cast<const Unaria*>(expresion)) {
        ValorMacro operando = evaluar(n->operando.get(), entorno);
        if (n->op == "-") {
            if (operando.tipo != ValorMacro::Tipo::Numero)
                fallar("'-' requiere un número en una macro", n->linea);
            return ValorMacro::numeroV(-operando.numero);
        }
        if (n->op == "!" || n->op == "no")
            return ValorMacro::logicoV(!esVerdadero(operando));
        fallar("operador unario no soportado en macros: '" + n->op + "'", n->linea);
    }
    if (auto* n = dynamic_cast<const Ternaria*>(expresion)) {
        return esVerdadero(evaluar(n->condicion.get(), entorno))
                   ? evaluar(n->siCierto.get(), entorno)
                   : evaluar(n->siFalso.get(), entorno);
    }
    if (auto* n = dynamic_cast<const AccesoIndice*>(expresion)) {
        ValorMacro objeto = evaluar(n->objeto.get(), entorno);
        ValorMacro indice = evaluar(n->indice.get(), entorno);
        std::string clave = valorATexto(indice, n->linea);
        if (objeto.tipo == ValorMacro::Tipo::Lista) {
            if (indice.tipo != ValorMacro::Tipo::Numero) fallar("índice no numérico en macro", n->linea);
            size_t i = static_cast<size_t>(indice.numero);
            if (i >= objeto.lista.size()) fallar("índice fuera de rango en macro", n->linea);
            return objeto.lista[i];
        }
        if (objeto.tipo == ValorMacro::Tipo::Diccionario) {
            for (const auto& p : objeto.diccionario) if (p.first == clave) return p.second;
            return ValorMacro::nulo();
        }
        fallar("índice no aplicable a este valor de macro", n->linea);
    }
    if (auto* n = dynamic_cast<const AccesoMiembro*>(expresion)) {
        ValorMacro objeto = evaluar(n->objeto.get(), entorno);
        if (objeto.tipo == ValorMacro::Tipo::Diccionario) {
            for (const auto& p : objeto.diccionario) if (p.first == n->miembro) return p.second;
            return ValorMacro::nulo();
        }
        if (objeto.tipo == ValorMacro::Tipo::Expresion && objeto.expresion) {
            const Expresion* e = objeto.expresion.get();
            if (n->miembro == "tipo") {
                std::string clase = "expresion";
                if (dynamic_cast<const LitNumero*>(e)) clase = "numero";
                else if (dynamic_cast<const LitCadena*>(e)) clase = "cadena";
                else if (dynamic_cast<const LitLogico*>(e)) clase = "logico";
                else if (dynamic_cast<const LitNulo*>(e)) clase = "nulo";
                else if (dynamic_cast<const Identificador*>(e)) clase = "identificador";
                else if (dynamic_cast<const Llamada*>(e)) clase = "llamada";
                else if (dynamic_cast<const Binaria*>(e)) clase = "binaria";
                else if (dynamic_cast<const Asignacion*>(e)) clase = "asignacion";
                else if (dynamic_cast<const BloqueMacroExpr*>(e)) clase = "bloque";
                return ValorMacro::simbolo(clase);
            }
            if (n->miembro == "args") {
                std::vector<ValorMacro> args;
                auto agregar = [&](const ExprPtr& x) {
                    args.push_back(ValorMacro::expr(std::shared_ptr<Expresion>(copiarExpr(x.get(), M_COPIAR, nullptr).release())));
                };
                if (auto* c = dynamic_cast<const Llamada*>(e)) {
                    agregar(c->destino);
                    for (const auto& a : c->argumentos) agregar(a);
                } else if (auto* b = dynamic_cast<const Binaria*>(e)) {
                    agregar(b->izq); agregar(b->der);
                } else if (auto* l = dynamic_cast<const ListaLiteral*>(e)) {
                    for (const auto& a : l->elementos) agregar(a);
                }
                return ValorMacro::listaV(std::move(args));
            }
        }
        fallar("acceso a miembro no soportado en macros: '.'" + n->miembro, n->linea);
    }
    if (auto* n = dynamic_cast<const ListaLiteral*>(expresion)) {
        std::vector<ValorMacro> valores;
        for (const auto& e : n->elementos) valores.push_back(evaluar(e.get(), entorno));
        return ValorMacro::listaV(std::move(valores));
    }
    if (auto* n = dynamic_cast<const DiccionarioLiteral*>(expresion)) {
        std::vector<std::pair<std::string, ValorMacro>> pares;
        for (const auto& p : n->pares)
            pares.push_back({valorATexto(evaluar(p.clave.get(), entorno), n->linea),
                             evaluar(p.valor.get(), entorno)});
        return ValorMacro::dicV(std::move(pares));
    }
    if (auto* n = dynamic_cast<const Llamada*>(expresion)) return evaluarLlamada(*n, entorno);
    if (auto* n = dynamic_cast<const CitaExpr*>(expresion)) {
        auto bloque = std::make_unique<BloqueMacroExpr>();
        bloque->linea = n->linea;
        bloque->macroEscapado = n->macroEscapado;
        bloque->sentencias = copiarLista(n->cuerpo, M_INTERPOLAR, &entorno);

        // Higiene: las variables introducidas por el propio quote se renombran
        // de forma única; los subárboles marcados por esc() se respetan.
        std::set<std::string> locales;
        for (const auto& s : bloque->sentencias) recogerLocales(s.get(), locales);
        std::map<std::string, std::string> mapa;
        for (const auto& nombre : locales)
            mapa[nombre] = "__m" + std::to_string(++contadorGensym) + "_" + nombre;
        renombrarLista(bloque->sentencias, mapa);
        return ValorMacro::expr(std::shared_ptr<Expresion>(bloque.release()));
    }
    if (auto* n = dynamic_cast<const MacroInvocacion*>(expresion)) {
        std::vector<ValorMacro> argumentos;
        for (const auto& a : n->argumentos)
            argumentos.push_back(ValorMacro::expr(
                std::shared_ptr<Expresion>(copiarExpr(a.get(), M_COPIAR, nullptr).release())));
        ValorMacro resultado = invocarMacro(n->nombre, argumentos, n->linea);
        if (resultado.tipo != ValorMacro::Tipo::Expresion)
            resultado = ValorMacro::expr(std::shared_ptr<Expresion>(
                aExpresion(resultado, n->linea).release()));
        return resultado;
    }
    if (auto* n = dynamic_cast<const FuncionAnonima*>(expresion)) {
        auto def = std::make_shared<FuncionDef>();
        def->linea = n->linea;
        def->nombre = "<anonima>";
        def->parametros = n->parametros;
        def->variadico = n->variadico;
        def->cuerpo = copiarLista(n->cuerpo, M_COPIAR, nullptr);
        auto funcion = std::make_shared<ValorFuncionMacro>();
        funcion->definicion = def;
        funcion->cierre = entorno.shared_from_this();
        return ValorMacro::funcionV(funcion);
    }
    fallar("expresión no soportada dentro de una macro", expresion->linea);
}

// ---------------------------------------------------------------------------
// Ejecución de sentencias del cuerpo de macro.
// ---------------------------------------------------------------------------

SistemaMacros::Salida SistemaMacros::ejecutarLista(ListaSent& lista, Entorno& entorno) {
    Salida salida;
    for (auto& sentencia : lista) {
        salida = ejecutar(*sentencia, entorno);
        if (salida.control != Salida::Control::Normal) return salida;
    }
    return salida;
}

SistemaMacros::Salida SistemaMacros::ejecutar(Sentencia& sentencia, Entorno& entorno) {
    Salida salida;
    int linea = sentencia.linea;

    if (auto* n = dynamic_cast<Asignacion*>(&sentencia)) {
        std::vector<ValorMacro> valores;
        for (const auto& v : n->valores) valores.push_back(evaluar(v.get(), entorno));
        for (size_t i = 0; i < n->destinos.size(); i++) {
            const ValorMacro& valor = i < valores.size() ? valores[i] : ValorMacro::nulo();
            if (auto* id = dynamic_cast<Identificador*>(n->destinos[i].get())) {
                asignarEnEntorno(entorno, id->nombre, valor);
            } else if (auto* acceso = dynamic_cast<AccesoIndice*>(n->destinos[i].get())) {
                auto* objetoId = dynamic_cast<Identificador*>(acceso->objeto.get());
                if (!objetoId) fallar("índice de asignación no soportado en macro", linea);
                ValorMacro* objetivo = entorno.buscar(objetoId->nombre);
                if (!objetivo) fallar("variable no declarada en la macro: '" + objetoId->nombre + "'", linea);
                ValorMacro indice = evaluar(acceso->indice.get(), entorno);
                if (objetivo->tipo == ValorMacro::Tipo::Lista) {
                    size_t pos = static_cast<size_t>(indice.numero);
                    if (pos >= objetivo->lista.size()) fallar("índice fuera de rango en macro", linea);
                    objetivo->lista[pos] = valor;
                } else if (objetivo->tipo == ValorMacro::Tipo::Diccionario) {
                    std::string clave = valorATexto(indice, linea);
                    bool encontrada = false;
                    for (auto& p : objetivo->diccionario) {
                        if (p.first == clave) { p.second = valor; encontrada = true; break; }
                    }
                    if (!encontrada) objetivo->diccionario.push_back({clave, valor});
                } else fallar("índice no aplicable en macro", linea);
            } else fallar("destino de asignación no soportado en macro", linea);
        }
        salida.valor = valores.empty() ? ValorMacro::nulo() : valores.back();
    } else if (auto* n = dynamic_cast<ExprSentencia*>(&sentencia)) {
        salida.valor = evaluar(n->expr.get(), entorno);
    } else if (auto* n = dynamic_cast<Retornar*>(&sentencia)) {
        std::vector<ValorMacro> valores;
        for (const auto& v : n->valores) valores.push_back(evaluar(v.get(), entorno));
        if (valores.size() == 1) salida.valor = valores[0];
        else if (valores.size() > 1) salida.valor = ValorMacro::listaV(valores);
        salida.control = Salida::Control::Retorno;
    } else if (auto* n = dynamic_cast<Si*>(&sentencia)) {
        if (esVerdadero(evaluar(n->condicion.get(), entorno))) {
            salida = ejecutarLista(n->entonces, entorno);
        } else {
            bool tomada = false;
            for (const auto& rama : n->osis) {
                if (esVerdadero(evaluar(rama.condicion.get(), entorno))) {
                    salida = ejecutarLista(const_cast<ListaSent&>(rama.cuerpo), entorno);
                    tomada = true;
                    break;
                }
            }
            if (!tomada && n->tieneSino) salida = ejecutarLista(n->sino, entorno);
        }
    } else if (auto* n = dynamic_cast<Mientras*>(&sentencia)) {
        int iteraciones = 0;
        while (esVerdadero(evaluar(n->condicion.get(), entorno))) {
            if (++iteraciones > 100000) fallar("bucle infinito en macro", linea);
            salida = ejecutarLista(n->cuerpo, entorno);
            if (salida.control == Salida::Control::Retorno) return salida;
            if (salida.control == Salida::Control::Rompe) { salida.control = Salida::Control::Normal; break; }
        }
    } else if (auto* n = dynamic_cast<Desde*>(&sentencia)) {
        if (n->inicio) ejecutar(*n->inicio, entorno);
        int iteraciones = 0;
        while (esVerdadero(evaluar(n->condicion.get(), entorno))) {
            if (++iteraciones > 100000) fallar("bucle infinito en macro", linea);
            salida = ejecutarLista(n->cuerpo, entorno);
            if (salida.control == Salida::Control::Retorno) return salida;
            if (salida.control == Salida::Control::Rompe) { salida.control = Salida::Control::Normal; break; }
            if (n->incremento) ejecutar(*n->incremento, entorno);
        }
    } else if (auto* n = dynamic_cast<Para*>(&sentencia)) {
        ValorMacro iterable = evaluar(n->iterable.get(), entorno);
        std::vector<ValorMacro> elementos;
        if (iterable.tipo == ValorMacro::Tipo::Lista) {
            elementos = iterable.lista;
        } else if (iterable.tipo == ValorMacro::Tipo::Diccionario) {
            for (const auto& p : iterable.diccionario) elementos.push_back(ValorMacro::cadena(p.first));
        } else if (iterable.tipo == ValorMacro::Tipo::Expresion && iterable.expresion) {
            if (auto* l = dynamic_cast<const ListaLiteral*>(iterable.expresion.get())) {
                for (const auto& e : l->elementos) elementos.push_back(evaluar(e.get(), entorno));
            } else if (auto* b = dynamic_cast<const BloqueMacroExpr*>(iterable.expresion.get())) {
                for (const auto& s : b->sentencias) {
                    if (auto* es = dynamic_cast<const ExprSentencia*>(s.get()))
                        elementos.push_back(ValorMacro::expr(std::shared_ptr<Expresion>(
                            copiarExpr(es->expr.get(), M_COPIAR, nullptr).release())));
                }
            } else fallar("iterable no válido en un bucle de macro", linea);
        } else fallar("iterable no válido en un bucle de macro", linea);

        for (const auto& elemento : elementos) {
            asignarEnEntorno(entorno, n->variable, elemento);
            salida = ejecutarLista(n->cuerpo, entorno);
            if (salida.control == Salida::Control::Retorno) return salida;
            if (salida.control == Salida::Control::Rompe) { salida.control = Salida::Control::Normal; break; }
        }
    } else if (auto* n = dynamic_cast<Repetir*>(&sentencia)) {
        int iteraciones = 0;
        do {
            if (++iteraciones > 100000) fallar("bucle infinito en macro", linea);
            salida = ejecutarLista(n->cuerpo, entorno);
            if (salida.control == Salida::Control::Retorno) return salida;
            if (salida.control == Salida::Control::Rompe) { salida.control = Salida::Control::Normal; break; }
        } while (!esVerdadero(evaluar(n->condicionHasta.get(), entorno)));
    } else if (dynamic_cast<Romper*>(&sentencia)) {
        salida.control = Salida::Control::Rompe;
    } else if (auto* n = dynamic_cast<FuncionDef*>(&sentencia)) {
        SentPtr copia = copiarSent(n, M_COPIAR, nullptr);
        auto def = std::shared_ptr<FuncionDef>(static_cast<FuncionDef*>(copia.release()));
        auto funcion = std::make_shared<ValorFuncionMacro>();
        funcion->definicion = def;
        funcion->cierre = entorno.shared_from_this();
        entorno.definir(n->nombre, ValorMacro::funcionV(funcion));
    } else if (auto* n = dynamic_cast<InseguroBloque*>(&sentencia)) {
        salida = ejecutarLista(n->cuerpo, entorno);
    } else if (auto* n = dynamic_cast<Bloquear*>(&sentencia)) {
        salida = ejecutarLista(n->cuerpo, entorno);
    } else if (auto* n = dynamic_cast<Esperar*>(&sentencia)) {
        evaluar(n->hilo.get(), entorno);
    } else if (auto* n = dynamic_cast<HiloDef*>(&sentencia)) {
        fallar("'hilo' no se puede ejecutar dentro de una macro", n->linea);
    } else if (auto* n = dynamic_cast<Elegir*>(&sentencia)) {
        ValorMacro opcion = evaluar(n->opcion.get(), entorno);
        bool tomada = false;
        for (const auto& caso : n->casos) {
            ValorMacro valor = evaluar(caso.valor.get(), entorno);
            std::string a = valorATexto(opcion, linea), b = valorATexto(valor, linea);
            if (a == b) {
                salida = ejecutarLista(const_cast<ListaSent&>(caso.cuerpo), entorno);
                tomada = true;
                break;
            }
        }
        if (!tomada && n->tieneDefecto) salida = ejecutarLista(n->defecto, entorno);
    } else {
        fallar("sentencia no soportada dentro de una macro", linea);
    }
    return salida;
}

// ---------------------------------------------------------------------------
// Macros integradas, invocación y expansión.
// ---------------------------------------------------------------------------

ValorMacro SistemaMacros::macroIncluida(const std::string& nombre,
                                        const std::vector<ValorMacro>& argumentos, int linea) {
    auto valorExpr = [&](ExprPtr e) {
        ValorMacro v = ValorMacro::expr(std::shared_ptr<Expresion>(e.release()));
        v.expresion->linea = linea;
        return v;
    };
    auto id = [&](const std::string& n) {
        auto e = std::make_unique<Identificador>();
        e->nombre = n;
        return valorExpr(std::move(e));
    };
    auto concat = [&](std::vector<ValorMacro> piezas) {
        ValorMacro resultado = piezas.front();
        for (size_t i = 1; i < piezas.size(); i++)
            resultado = construirExpr("binaria", {ValorMacro::simbolo(".."), resultado, piezas[i]}, linea);
        return resultado;
    };
    auto acadena = [&](const ValorMacro& v) {
        return construirExpr("llamada", {ValorMacro::simbolo("acadena"), v}, linea);
    };

    // Nombres canónicos en español; se conservan los nombres en inglés como
    // alias para no romper programas ya escritos.
    std::vector<ValorMacro> sentencias;
    if (nombre == "mostrar" || nombre == "show") {
        if (argumentos.empty()) fallar("@mostrar necesita al menos una expresión", linea);
        for (const auto& argumento : argumentos) {
            std::string texto = valorATexto(argumento, linea);
            ValorMacro mensaje = concat({ValorMacro::cadena(texto + " = "), acadena(argumento)});
            sentencias.push_back(construirExpr(
                "llamada", {ValorMacro::simbolo("escribir"), mensaje}, linea));
        }
    } else if (nombre == "afirmar" || nombre == "asegurar" || nombre == "assert") {
        if (argumentos.size() != 1) fallar("@afirmar espera una condición", linea);
        ValorMacro condicion = construirExpr(
            "unaria", {ValorMacro::simbolo("!"), argumentos[0]}, linea);
        // Se usa el builtin `error` (escribe en stderr y termina con código 1)
        // para no exigir que el programa haya hecho `incluir "sis"`.
        auto salir = construirExpr("llamada",
            {ValorMacro::simbolo("error"),
             ValorMacro::cadena("afirmación falló: " + valorATexto(argumentos[0], linea))}, linea);
        ValorMacro cuerpo = construirExpr("bloque", {salir}, linea);
        sentencias.push_back(construirExpr("si", {condicion, cuerpo}, linea));
    } else if (nombre == "tiempo" || nombre == "time") {
        if (argumentos.size() != 1) fallar("@tiempo espera una expresión", linea);
        std::string sufijo = std::to_string(++contadorGensym);
        std::string t0 = "__tiempo_t0_" + sufijo;
        std::string resultado = "__tiempo_resultado_" + sufijo;
        std::string dt = "__tiempo_ms_" + sufijo;
        ValorMacro reloj = construirExpr("llamada", {ValorMacro::simbolo("tiempo_ns")}, linea);
        sentencias.push_back(construirExpr("asignacion", {id(t0), reloj}, linea));
        sentencias.push_back(construirExpr("asignacion", {id(resultado), argumentos[0]}, linea));
        ValorMacro reloj2 = construirExpr("llamada", {ValorMacro::simbolo("tiempo_ns")}, linea);
        ValorMacro diferencia = construirExpr(
            "binaria", {ValorMacro::simbolo("-"), reloj2, id(t0)}, linea);
        ValorMacro milisegundos = construirExpr(
            "binaria", {ValorMacro::simbolo("/"), diferencia, ValorMacro::numeroV(1000000.0)}, linea);
        sentencias.push_back(construirExpr("asignacion", {id(dt), milisegundos}, linea));
        ValorMacro mensaje = concat({
            ValorMacro::cadena(valorATexto(argumentos[0], linea) + " -> "),
            acadena(id(resultado)),
            ValorMacro::cadena(" ("),
            acadena(id(dt)),
            ValorMacro::cadena(" ms)")
        });
        sentencias.push_back(construirExpr(
            "llamada", {ValorMacro::simbolo("escribir"), mensaje}, linea));
        sentencias.push_back(id(resultado));
    } else {
        fallar("macro integrada desconocida: '@" + nombre + "'", linea);
    }
    return construirExpr("bloque", sentencias, linea);
}

ValorMacro SistemaMacros::ejecutarCuerpo(MacroDef& def, const std::vector<ValorMacro>& argumentos) {
    if (argumentos.size() != def.parametros.size())
        fallar("la macro '@" + def.nombre + "' espera " + std::to_string(def.parametros.size()) +
               " argumentos, se dieron " + std::to_string(argumentos.size()), def.linea);
    auto entorno = std::make_shared<Entorno>();
    for (size_t i = 0; i < argumentos.size(); i++)
        entorno->definir(def.parametros[i], argumentos[i]);
    Salida salida = ejecutarLista(def.cuerpo, *entorno);
    if (salida.control == Salida::Control::Rompe)
        fallar("'romper' fuera de un bucle en la macro '@" + def.nombre + "'", def.linea);
    return salida.valor;
}

ValorMacro SistemaMacros::invocarMacro(const std::string& nombre,
                                       std::vector<ValorMacro> argumentos, int linea) {
    if (profundidad > 64) fallar("expansión de macros demasiado profunda", linea);
    profundidad++;
    struct GuardarProfundidad {
        int& profundidad;
        ~GuardarProfundidad() { profundidad--; }
    } guard{profundidad};

    if (nombre == "expandir_macro" || nombre == "macroexpand") {
        if (argumentos.size() != 1) fallar("@expandir_macro espera una expresión", linea);
        ExprPtr original = aExpresion(argumentos[0], linea);
        ExprPtr expandido;
        if (auto* m = dynamic_cast<MacroInvocacion*>(original.get())) {
            // Invocación directa: permite inspeccionar macros que devuelven
            // bloques de varias sentencias (expandirInvocacion solo acepta
            // una expresión porque la expansión se inserta en una expresión).
            std::vector<ValorMacro> args;
            for (const auto& a : m->argumentos)
                args.push_back(ValorMacro::expr(std::shared_ptr<Expresion>(
                    copiarExpr(a.get(), M_COPIAR, nullptr).release())));
            expandido = aExpresion(invocarMacro(m->nombre, args, m->linea), m->linea);
        } else {
            expandido = copiarExpr(original.get(), M_EXPANDER, nullptr);
        }
        std::string texto = ImpresorFuente::aTexto(*expandido);
        while (!texto.empty() && (texto.back() == '\n' || texto.back() == '\r'))
            texto.pop_back();
        reporte << texto << '\n';
        ValorMacro nulo = ValorMacro::nulo();
        return ValorMacro::expr(std::shared_ptr<Expresion>(
            aExpresion(nulo, linea).release()));
    }
    if (nombre == "mostrar" || nombre == "show" ||
        nombre == "afirmar" || nombre == "asegurar" || nombre == "assert" ||
        nombre == "tiempo" || nombre == "time")
        return macroIncluida(nombre, argumentos, linea);

    auto it = definiciones.find(nombre);
    if (it == definiciones.end())
        fallar("macro no definida '@" + nombre + "'", linea);
    return ejecutarCuerpo(*it->second, argumentos);
}

ExprPtr SistemaMacros::expandirInvocacion(MacroInvocacion& invocacion) {
    std::vector<ValorMacro> argumentos;
    for (const auto& a : invocacion.argumentos)
        argumentos.push_back(ValorMacro::expr(std::shared_ptr<Expresion>(
            copiarExpr(a.get(), M_COPIAR, nullptr).release())));

    ValorMacro resultado = invocarMacro(invocacion.nombre, argumentos, invocacion.linea);
    if (resultado.tipo != ValorMacro::Tipo::Expresion)
        resultado = ValorMacro::expr(std::shared_ptr<Expresion>(
            aExpresion(resultado, invocacion.linea).release()));

    ExprPtr expandido = aExpresion(resultado, invocacion.linea);
    if (auto* bloque = dynamic_cast<BloqueMacroExpr*>(expandido.get())) {
        if (bloque->sentencias.empty()) return aExpresion(ValorMacro::nulo(), invocacion.linea);
        if (bloque->sentencias.size() == 1) {
            if (auto* simple = dynamic_cast<ExprSentencia*>(bloque->sentencias[0].get()))
                return copiarExpr(simple->expr.get(), M_EXPANDER, nullptr);
        }
        fallar("la macro '@" + invocacion.nombre +
               "' devolvió varias sentencias pero se usó como expresión", invocacion.linea);
    }
    return copiarExpr(expandido.get(), M_EXPANDER, nullptr);
}

bool SistemaMacros::transformarLista(ListaSent& lista) {
    ListaSent resultado;
    resultado.reserve(lista.size());
    for (auto& sentencia : lista) {
        if (auto* simple = dynamic_cast<ExprSentencia*>(sentencia.get())) {
            if (auto* invocacion = dynamic_cast<MacroInvocacion*>(simple->expr.get())) {
                std::vector<ValorMacro> argumentos;
                for (const auto& a : invocacion->argumentos)
                    argumentos.push_back(ValorMacro::expr(std::shared_ptr<Expresion>(
                        copiarExpr(a.get(), M_COPIAR, nullptr).release())));
                ValorMacro valor = invocarMacro(invocacion->nombre, argumentos, invocacion->linea);
                agregarSentencias(resultado, valor, invocacion->linea);
                continue;
            }
        }
        resultado.push_back(copiarSent(sentencia.get(), M_EXPANDER, nullptr));
    }
    lista = std::move(resultado);
    return true;
}

void SistemaMacros::recolectar(Programa& programa) {
    ListaSent restantes;
    for (auto& sentencia : programa.sentencias) {
        if (auto* macro = dynamic_cast<MacroDef*>(sentencia.get())) {
            auto poseedor = std::unique_ptr<MacroDef>(static_cast<MacroDef*>(sentencia.release()));
            if (definiciones.count(poseedor->nombre))
                fallar("macro duplicada '@" + poseedor->nombre + "'", poseedor->linea);
            definiciones[poseedor->nombre] = poseedor.get();
            definicionesPropias.push_back(std::move(poseedor));
        } else {
            restantes.push_back(std::move(sentencia));
        }
    }
    programa.sentencias = std::move(restantes);
}

bool SistemaMacros::expandir(Programa& programa) {
    return transformarLista(programa.sentencias);
}

}  // namespace

bool ExpandidorMacros::expandir(Programa& programa, std::ostream& reporte) {
    try {
        SistemaMacros sistema(reporte);
        sistema.recolectar(programa);
        return sistema.expandir(programa);
    } catch (const ErrorMacro& error) {
        std::cerr << "Error de macro" << (error.linea ? " en línea " + std::to_string(error.linea) : "")
                  << ": " << error.mensaje << std::endl;
        return false;
    }
}

bool ExpandidorMacros::expandir(Programa& programa) {
    std::ostringstream descartado;
    return expandir(programa, descartado);
}
