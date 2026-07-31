// ast_impresor.cpp

#include "ast_impresor.h"

#include <cstdio>
#include <string>

ImpresorAST::ImpresorAST(std::ostream& salida) : salida(salida), nivel(0) {}

void ImpresorAST::imprimir(Nodo& nodo) {
    nodo.aceptar(*this);
}

void ImpresorAST::linea(const std::string& texto) {
    for (int i = 0; i < nivel; ++i)
        salida << "  ";
    salida << texto << "\n";
}

void ImpresorAST::hijo(Nodo& nodo) {
    ++nivel;
    nodo.aceptar(*this);
    --nivel;
}

void ImpresorAST::hijos(ListaSent& lista) {
    ++nivel;
    for (auto& s : lista)
        s->aceptar(*this);
    --nivel;
}

// Formatea un literal numérico sin decimales innecesarios.
static std::string numeroATexto(double valor, bool esEntero) {
    if (esEntero) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(valor));
        return buf;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", valor);
    return buf;
}

// --- Expresiones -----------------------------------------------------------

void ImpresorAST::visitar(LitNumero& n) {
    linea("Numero " + numeroATexto(n.valor, n.esEntero));
}

void ImpresorAST::visitar(LitCadena& n) {
    linea("Cadena '" + n.valor + "'");
}

void ImpresorAST::visitar(LitLogico& n) {
    linea(std::string("Logico ") + (n.valor ? "cierto" : "falso"));
}

void ImpresorAST::visitar(LitNulo&) {
    linea("Nulo");
}

void ImpresorAST::visitar(Identificador& n) {
    linea("Identificador '" + n.nombre + "'");
}

void ImpresorAST::visitar(Binaria& n) {
    linea("Binaria '" + n.op + "'");
    if (n.izq) hijo(*n.izq);
    if (n.der) hijo(*n.der);
}

void ImpresorAST::visitar(Unaria& n) {
    linea("Unaria '" + n.op + "'");
    if (n.operando) hijo(*n.operando);
}

void ImpresorAST::visitar(PostOperador& n) {
    linea("PostOperador '" + n.op + "'");
    if (n.operando) hijo(*n.operando);
}

void ImpresorAST::visitar(Ternaria& n) {
    linea("Ternaria");
    if (n.condicion) hijo(*n.condicion);
    if (n.siCierto) hijo(*n.siCierto);
    if (n.siFalso) hijo(*n.siFalso);
}

void ImpresorAST::visitar(AccesoIndice& n) {
    linea("AccesoIndice");
    if (n.objeto) hijo(*n.objeto);
    if (n.indice) hijo(*n.indice);
}

void ImpresorAST::visitar(AccesoMiembro& n) {
    linea("AccesoMiembro '." + n.miembro + "'");
    if (n.objeto) hijo(*n.objeto);
}

void ImpresorAST::visitar(Llamada& n) {
    linea("Llamada");
    if (n.destino) hijo(*n.destino);
    for (auto& a : n.argumentos)
        if (a) hijo(*a);
}

void ImpresorAST::visitar(ListaLiteral& n) {
    linea("Lista (" + std::to_string(n.elementos.size()) + ")");
    for (auto& e : n.elementos)
        if (e) hijo(*e);
}

void ImpresorAST::visitar(DiccionarioLiteral& n) {
    linea("Diccionario (" + std::to_string(n.pares.size()) + ")");
    ++nivel;
    for (auto& p : n.pares) {
        linea("Par");
        if (p.clave) hijo(*p.clave);
        if (p.valor) hijo(*p.valor);
    }
    --nivel;
}

void ImpresorAST::visitar(VarArgs&) {
    linea("VarArgs ...");
}

// --- Sentencias ------------------------------------------------------------

void ImpresorAST::visitar(Programa& n) {
    linea("Programa");
    hijos(n.sentencias);
}

void ImpresorAST::visitar(Incluir& n) {
    linea("Incluir \"" + n.modulo + "\"");
}

void ImpresorAST::visitar(Asignacion& n) {
    std::string extra = "";
    if (n.esVar) extra = " [var]";
    else if (n.esConst) extra = " [const]";
    linea("Asignacion (" + std::to_string(n.destinos.size()) + " = " +
          std::to_string(n.valores.size()) + ")" + extra);
    ++nivel;
    linea("destinos:");
    for (auto& d : n.destinos)
        if (d) hijo(*d);
    linea("valores:");
    for (auto& v : n.valores)
        if (v) hijo(*v);
    --nivel;
}

void ImpresorAST::visitar(ExprSentencia& n) {
    linea("ExprSentencia");
    if (n.expr) hijo(*n.expr);
}

void ImpresorAST::visitar(Si& n) {
    linea("Si");
    if (n.condicion) hijo(*n.condicion);
    linea("entonces:");
    hijos(n.entonces);
    for (auto& rama : n.osis) {
        linea("osi:");
        if (rama.condicion) hijo(*rama.condicion);
        hijos(rama.cuerpo);
    }
    if (n.tieneSino) {
        linea("sino:");
        hijos(n.sino);
    }
}

void ImpresorAST::visitar(Elegir& n) {
    linea("Elegir");
    if (n.opcion) hijo(*n.opcion);
    for (auto& c : n.casos) {
        linea("caso:");
        if (c.valor) hijo(*c.valor);
        hijos(c.cuerpo);
    }
    if (n.tieneDefecto) {
        linea("defecto:");
        hijos(n.defecto);
    }
}

void ImpresorAST::visitar(Desde& n) {
    linea("Desde");
    if (n.inicio) hijo(*n.inicio);
    if (n.condicion) hijo(*n.condicion);
    if (n.incremento) hijo(*n.incremento);
    linea("cuerpo:");
    hijos(n.cuerpo);
}

void ImpresorAST::visitar(Mientras& n) {
    linea("Mientras");
    if (n.condicion) hijo(*n.condicion);
    linea("cuerpo:");
    hijos(n.cuerpo);
}

void ImpresorAST::visitar(Repetir& n) {
    linea("Repetir");
    linea("cuerpo:");
    hijos(n.cuerpo);
    linea("hasta:");
    if (n.condicionHasta) hijo(*n.condicionHasta);
}

void ImpresorAST::visitar(Romper&) {
    linea("Romper");
}

static std::string nombreTipoAst(TipoAnotado t) {
    switch (t) {
        case TipoAnotado::Numero: return "numero";
        case TipoAnotado::Cadena: return "cadena";
        case TipoAnotado::Logico: return "logico";
        case TipoAnotado::Lista:  return "lista";
        case TipoAnotado::Dic:    return "dic";
        case TipoAnotado::Nulo:   return "nulo";
        default: return "";
    }
}

void ImpresorAST::visitar(FuncionDef& n) {
    std::string firma = "Funcion '" + n.nombre + "' (";
    for (size_t i = 0; i < n.parametros.size(); ++i) {
        if (i) firma += ", ";
        firma += n.parametros[i].nombre;
        if (n.parametros[i].tipo != TipoAnotado::Ninguno)
            firma += ":" + nombreTipoAst(n.parametros[i].tipo);
    }
    if (n.variadico)
        firma += (n.parametros.empty() ? "..." : ", ...");
    firma += ")";
    if (n.tipoRetorno != TipoAnotado::Ninguno)
        firma += " -> " + nombreTipoAst(n.tipoRetorno);
    linea(firma);
    linea("cuerpo:");
    hijos(n.cuerpo);
}

void ImpresorAST::visitar(Retornar& n) {
    linea("Retornar");
    if (n.valor) hijo(*n.valor);
}
