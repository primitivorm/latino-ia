// impresor_fuente.cpp — AST -> código Latino legible (PLAN_MACROS.md).

#include "impresor_fuente.h"

#include <iomanip>
#include <sstream>

namespace {

std::string comillas(const std::string& valor) {
    std::string salida = "\"";
    for (char c : valor) {
        if (c == '\\' || c == '"') salida += '\\';
        salida += c;
    }
    salida += '"';
    return salida;
}

const char* nombreTipo(TipoAnotado tipo) {
    switch (tipo) {
        case TipoAnotado::Numero: return "numero";
        case TipoAnotado::Cadena: return "cadena";
        case TipoAnotado::Logico: return "logico";
        case TipoAnotado::Lista: return "lista";
        case TipoAnotado::Dic: return "dic";
        case TipoAnotado::Nulo: return "nulo";
        case TipoAnotado::Objeto: return "objeto";
        case TipoAnotado::Arreglo: return "arreglo";
        default: return "";
    }
}

std::string numero(double valor, bool entero) {
    std::ostringstream os;
    if (entero) os << static_cast<long long>(valor);
    else os << std::setprecision(15) << valor;
    return os.str();
}

}  // namespace

ImpresorFuente::ImpresorFuente(std::ostream& salida) : salida(salida) {}

std::string ImpresorFuente::aTexto(Nodo& nodo) {
    std::ostringstream os;
    ImpresorFuente impresor(os);
    impresor.imprimir(nodo);
    return os.str();
}

void ImpresorFuente::sangria() {
    for (int i = 0; i < nivel; i++) salida << "    ";
}

void ImpresorFuente::linea(const std::string& texto) {
    sangria();
    salida << texto << '\n';
}

void ImpresorFuente::imprimir(Nodo& nodo) {
    if (auto* p = dynamic_cast<Programa*>(&nodo)) {
        imprimirLista(p->sentencias);
    } else if (auto* e = dynamic_cast<Expresion*>(&nodo)) {
        imprimirExpresion(e);
    } else if (auto* s = dynamic_cast<Sentencia*>(&nodo)) {
        imprimirSentencia(s);
    }
}

void ImpresorFuente::imprimirLista(const ListaSent& lista) {
    for (const auto& s : lista) imprimirSentencia(s.get());
}

void ImpresorFuente::imprimirParametros(const std::vector<ParamFuncion>& parametros) {
    salida << '(';
    for (size_t i = 0; i < parametros.size(); i++) {
        if (i) salida << ", ";
        salida << parametros[i].nombre;
        if (parametros[i].tipo != TipoAnotado::Ninguno)
            salida << ": " << nombreTipo(parametros[i].tipo);
    }
    salida << ')';
}

void ImpresorFuente::imprimirExpresion(const Expresion* e) {
    if (!e) { salida << "nulo"; return; }

    if (auto* n = dynamic_cast<const LitNumero*>(e)) {
        salida << numero(n->valor, n->esEntero);
    } else if (dynamic_cast<const LitCadena*>(e)) {
        salida << comillas(dynamic_cast<const LitCadena*>(e)->valor);
    } else if (auto* n = dynamic_cast<const LitLogico*>(e)) {
        salida << (n->valor ? "cierto" : "falso");
    } else if (dynamic_cast<const LitNulo*>(e)) {
        salida << "nulo";
    } else if (auto* n = dynamic_cast<const Identificador*>(e)) {
        salida << n->nombre;
    } else if (auto* n = dynamic_cast<const SimboloExpr*>(e)) {
        salida << ':' << n->nombre;
    } else if (auto* n = dynamic_cast<const Binaria*>(e)) {
        salida << '(';
        imprimirExpresion(n->izq.get());
        salida << ' ' << n->op << ' ';
        imprimirExpresion(n->der.get());
        salida << ')';
    } else if (auto* n = dynamic_cast<const Unaria*>(e)) {
        salida << '(' << n->op;
        imprimirExpresion(n->operando.get());
        salida << ')';
    } else if (auto* n = dynamic_cast<const PostOperador*>(e)) {
        salida << '(';
        imprimirExpresion(n->operando.get());
        salida << n->op << ')';
    } else if (auto* n = dynamic_cast<const Ternaria*>(e)) {
        salida << '(';
        imprimirExpresion(n->condicion.get());
        salida << " ? ";
        imprimirExpresion(n->siCierto.get());
        salida << " : ";
        imprimirExpresion(n->siFalso.get());
        salida << ')';
    } else if (auto* n = dynamic_cast<const AccesoIndice*>(e)) {
        imprimirExpresion(n->objeto.get());
        salida << '[';
        imprimirExpresion(n->indice.get());
        salida << ']';
    } else if (auto* n = dynamic_cast<const AccesoMiembro*>(e)) {
        imprimirExpresion(n->objeto.get());
        salida << '.' << n->miembro;
    } else if (auto* n = dynamic_cast<const Llamada*>(e)) {
        imprimirExpresion(n->destino.get());
        if (!n->tipoArgsExplicitos.empty()) {
            salida << "::<";
            for (size_t i = 0; i < n->tipoArgsExplicitos.size(); i++) {
                if (i) salida << ", ";
                salida << n->tipoArgsExplicitos[i];
            }
            salida << '>';
        }
        salida << '(';
        for (size_t i = 0; i < n->argumentos.size(); i++) {
            if (i) salida << ", ";
            imprimirExpresion(n->argumentos[i].get());
        }
        salida << ')';
    } else if (auto* n = dynamic_cast<const ListaLiteral*>(e)) {
        salida << '[';
        for (size_t i = 0; i < n->elementos.size(); i++) {
            if (i) salida << ", ";
            imprimirExpresion(n->elementos[i].get());
        }
        salida << ']';
    } else if (auto* n = dynamic_cast<const DiccionarioLiteral*>(e)) {
        salida << '{';
        for (size_t i = 0; i < n->pares.size(); i++) {
            if (i) salida << ", ";
            imprimirExpresion(n->pares[i].clave.get());
            salida << ": ";
            imprimirExpresion(n->pares[i].valor.get());
        }
        salida << '}';
    } else if (dynamic_cast<const VarArgs*>(e)) {
        salida << "...";
    } else if (auto* n = dynamic_cast<const FuncionAnonima*>(e)) {
        salida << "fun";
        imprimirParametros(n->parametros);
        salida << '\n';
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const NuevoExpr*>(e)) {
        salida << "nuevo " << n->clase;
        if (!n->tipoArgs.empty()) {
            salida << '<';
            for (size_t i = 0; i < n->tipoArgs.size(); i++) {
                if (i) salida << ", ";
                salida << n->tipoArgs[i];
            }
            salida << '>';
        }
        salida << '(';
        for (size_t i = 0; i < n->argumentos.size(); i++) {
            if (i) salida << ", ";
            imprimirExpresion(n->argumentos[i].get());
        }
        salida << ')';
    } else if (auto* n = dynamic_cast<const EsExpr*>(e)) {
        imprimirExpresion(n->objeto.get());
        salida << " es " << n->clase;
    } else if (dynamic_cast<const AccesoEste*>(e)) {
        salida << "este";
    } else if (auto* n = dynamic_cast<const InterpolacionExpr*>(e)) {
        salida << '$';
        imprimirExpresion(n->expresion.get());
    } else if (auto* n = dynamic_cast<const CitaExpr*>(e)) {
        salida << "quote\n";
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const BloqueMacroExpr*>(e)) {
        salida << "{\n";
        nivel++;
        imprimirLista(n->sentencias);
        nivel--;
        linea("}");
    } else if (auto* n = dynamic_cast<const FragmentoExpr*>(e)) {
        imprimirExpresion(n->expresion.get());
    } else if (auto* n = dynamic_cast<const MacroInvocacion*>(e)) {
        salida << '@' << n->nombre;
        for (const auto& a : n->argumentos) {
            salida << ' ';
            imprimirExpresion(a.get());
        }
    } else {
        salida << "/*expresion*/";
    }
}

void ImpresorFuente::imprimirSentencia(const Sentencia* s) {
    if (!s) return;

    if (auto* n = dynamic_cast<const Asignacion*>(s)) {
        std::string prefijo = n->esGlobal ? "global " : (n->esVar ? "var " : (n->esConst ? "const " : ""));
        if (n->exportado) prefijo = "exportar " + prefijo;
        for (size_t i = 0; i < n->destinos.size(); i++) {
            if (i) salida << ", ";
            if (i == 0) salida << prefijo;
            imprimirExpresion(n->destinos[i].get());
            if (i < n->tiposDestino.size() && n->tiposDestino[i] != TipoAnotado::Ninguno) {
                salida << ": " << nombreTipo(n->tiposDestino[i]);
            }
        }
        salida << " = ";
        for (size_t i = 0; i < n->valores.size(); i++) {
            if (i) salida << ", ";
            imprimirExpresion(n->valores[i].get());
        }
        salida << '\n';
    } else if (auto* n = dynamic_cast<const ExprSentencia*>(s)) {
        salida << "";
        imprimirExpresion(n->expr.get());
        salida << '\n';
    } else if (auto* n = dynamic_cast<const Retornar*>(s)) {
        salida << "retornar ";
        for (size_t i = 0; i < n->valores.size(); i++) {
            if (i) salida << ", ";
            imprimirExpresion(n->valores[i].get());
        }
        salida << '\n';
    } else if (auto* n = dynamic_cast<const Si*>(s)) {
        salida << "si ";
        imprimirExpresion(n->condicion.get());
        salida << '\n';
        nivel++;
        imprimirLista(n->entonces);
        nivel--;
        for (const auto& rama : n->osis) {
            linea("osi " + ImpresorFuente::aTexto(*const_cast<Expresion*>(rama.condicion.get())));
            nivel++;
            imprimirLista(rama.cuerpo);
            nivel--;
        }
        if (n->tieneSino) {
            linea("sino");
            nivel++;
            imprimirLista(n->sino);
            nivel--;
        }
        linea("fin");
    } else if (auto* n = dynamic_cast<const Mientras*>(s)) {
        salida << "mientras ";
        imprimirExpresion(n->condicion.get());
        salida << '\n';
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const Desde*>(s)) {
        std::string inicio = n->inicio ? aTexto(*n->inicio) : "";
        while (!inicio.empty() && (inicio.back() == '\n' || inicio.back() == '\r'))
            inicio.pop_back();
        std::string incremento = n->incremento ? aTexto(*n->incremento) : "";
        while (!incremento.empty() && (incremento.back() == '\n' || incremento.back() == '\r'))
            incremento.pop_back();
        linea("desde " + inicio + "; " +
              aTexto(*const_cast<Expresion*>(n->condicion.get())) + "; " + incremento);
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const Para*>(s)) {
        salida << "para " << n->variable << " en ";
        imprimirExpresion(n->iterable.get());
        salida << '\n';
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const Repetir*>(s)) {
        linea("repetir");
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        salida << "hasta ";
        imprimirExpresion(n->condicionHasta.get());
        salida << '\n';
    } else if (dynamic_cast<const Romper*>(s)) {
        linea("romper");
    } else if (auto* n = dynamic_cast<const FuncionDef*>(s)) {
        salida << "funcion " << n->nombre;
        imprimirParametros(n->parametros);
        salida << '\n';
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const MacroDef*>(s)) {
        salida << "macro " << n->nombre << '(';
        for (size_t i = 0; i < n->parametros.size(); i++) {
            if (i) salida << ", ";
            salida << n->parametros[i];
        }
        salida << ")\n";
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const InseguroBloque*>(s)) {
        linea("inseguro");
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const HiloDef*>(s)) {
        salida << "hilo " << n->nombre << '(';
        if (n->cuerpo) {
            for (size_t i = 0; i < n->cuerpo->parametros.size(); i++) {
                if (i) salida << ", ";
                salida << n->cuerpo->parametros[i].nombre;
            }
        }
        salida << ")\n";
        nivel++;
        if (n->cuerpo) imprimirLista(n->cuerpo->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const Esperar*>(s)) {
        salida << "esperar ";
        imprimirExpresion(n->hilo.get());
        salida << '\n';
    } else if (auto* n = dynamic_cast<const Bloquear*>(s)) {
        salida << "bloquear(";
        imprimirExpresion(n->cerrojo.get());
        salida << ")\n";
        nivel++;
        imprimirLista(n->cuerpo);
        nivel--;
        linea("fin");
    } else if (auto* n = dynamic_cast<const Incluir*>(s)) {
        linea("incluir \"" + n->modulo + "\"");
    } else if (auto* n = dynamic_cast<const LlamadaBase*>(s)) {
        salida << "base(";
        for (size_t i = 0; i < n->argumentos.size(); i++) {
            if (i) salida << ", ";
            imprimirExpresion(n->argumentos[i].get());
        }
        salida << ")\n";
    } else if (auto* n = dynamic_cast<const Elegir*>(s)) {
        salida << "elegir ";
        imprimirExpresion(n->opcion.get());
        salida << '\n';
        for (const auto& c : n->casos) {
            salida << "caso ";
            imprimirExpresion(c.valor.get());
            salida << ":\n";
            nivel++;
            imprimirLista(c.cuerpo);
            nivel--;
        }
        if (n->tieneDefecto) {
            linea("defecto:");
            nivel++;
            imprimirLista(n->defecto);
            nivel--;
        }
        linea("fin");
    } else {
        salida << "/*sentencia*/\n";
    }
}
