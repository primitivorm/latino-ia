// compiler.cpp — generación de código C (Fase 5). Ver compiler.h.

#include "compiler.h"

#include <cstdio>

// ---------------------------------------------------------------------------
// Recolección de variables locales (descendiendo en bloques, no en funciones)
// ---------------------------------------------------------------------------
namespace {

void colectar(Sentencia* s, std::set<std::string>& out);

void colectarLista(const ListaSent& cuerpo, std::set<std::string>& out) {
    for (const auto& s : cuerpo)
        if (s) colectar(s.get(), out);
}

void colectar(Sentencia* s, std::set<std::string>& out) {
    if (auto* a = dynamic_cast<Asignacion*>(s)) {
        for (auto& d : a->destinos)
            if (auto* id = dynamic_cast<Identificador*>(d.get()))
                out.insert(id->nombre);
        return;
    }
    if (auto* si = dynamic_cast<Si*>(s)) {
        colectarLista(si->entonces, out);
        for (auto& r : si->osis) colectarLista(r.cuerpo, out);
        if (si->tieneSino) colectarLista(si->sino, out);
        return;
    }
    if (auto* el = dynamic_cast<Elegir*>(s)) {
        for (auto& c : el->casos) colectarLista(c.cuerpo, out);
        if (el->tieneDefecto) colectarLista(el->defecto, out);
        return;
    }
    if (auto* de = dynamic_cast<Desde*>(s)) {
        if (de->inicio) colectar(de->inicio.get(), out);
        colectarLista(de->cuerpo, out);
        return;
    }
    if (auto* mi = dynamic_cast<Mientras*>(s)) {
        colectarLista(mi->cuerpo, out);
        return;
    }
    if (auto* re = dynamic_cast<Repetir*>(s)) {
        colectarLista(re->cuerpo, out);
        return;
    }
    // FuncionDef: no se desciende. ExprSentencia/Romper/Retornar: nada que declarar.
}

const char* fnBinaria(const std::string& op) {
    if (op == "+")  return "lat_sumar";
    if (op == "-")  return "lat_restar";
    if (op == "*")  return "lat_multiplicar";
    if (op == "/")  return "lat_dividir";
    if (op == "%")  return "lat_modulo";
    if (op == "^")  return "lat_potencia";
    if (op == "..") return "lat_concatenar";
    if (op == "==") return "lat_igual";
    if (op == "!=") return "lat_distinto";
    if (op == "<")  return "lat_menor";
    if (op == ">")  return "lat_mayor";
    if (op == "<=") return "lat_menor_igual";
    if (op == ">=") return "lat_mayor_igual";
    if (op == "&&") return "lat_y";
    if (op == "||") return "lat_o";
    if (op == "~=") return "lat_coincide";
    return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------
std::string GeneradorC::varC(const std::string& nombre) { return "v_" + nombre; }
std::string GeneradorC::funC(const std::string& nombre) { return "lat_fn_" + nombre; }

std::string GeneradorC::escaparCadena(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '\\': r += "\\\\"; break;
            case '"':  r += "\\\""; break;
            case '\n': r += "\\n"; break;
            case '\t': r += "\\t"; break;
            case '\r': r += "\\r"; break;
            default:   r += c; break;
        }
    }
    return r;
}

void GeneradorC::emitir(const std::string& linea) {
    salida << std::string(indentacion * 4, ' ') << linea << "\n";
}

std::string GeneradorC::nuevoTemp() {
    return "_t" + std::to_string(contadorTemp++);
}

void GeneradorC::recolectarFunciones(Programa& programa) {
    for (auto& s : programa.sentencias)
        if (auto* f = dynamic_cast<FuncionDef*>(s.get()))
            funciones[f->nombre] = InfoFuncion{f->parametros.size(), f->variadico};
}

void GeneradorC::recolectarVariables(const ListaSent& cuerpo,
                                     std::set<std::string>& destino,
                                     const std::set<std::string>& excluir) {
    colectarLista(cuerpo, destino);
    for (const std::string& e : excluir)
        destino.erase(e);
}

// ---------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------
std::string GeneradorC::genExpr(Expresion* e) {
    if (!e) return "lat_nulo()";

    if (auto* n = dynamic_cast<LitNumero*>(e)) {
        std::string num = n->lexema.empty() ? std::to_string(n->valor) : n->lexema;
        return "lat_numero(" + num + ")";
    }
    if (auto* n = dynamic_cast<LitCadena*>(e)) {
        return "lat_cadena(\"" + escaparCadena(n->valor) + "\")";
    }
    if (auto* n = dynamic_cast<LitLogico*>(e)) {
        return n->valor ? "lat_logico(1)" : "lat_logico(0)";
    }
    if (dynamic_cast<LitNulo*>(e)) {
        return "lat_nulo()";
    }
    if (auto* n = dynamic_cast<Identificador*>(e)) {
        return varC(n->nombre);
    }
    if (dynamic_cast<VarArgs*>(e)) {
        return "lat_resto";
    }
    if (auto* n = dynamic_cast<Binaria*>(e)) {
        const char* fn = fnBinaria(n->op);
        if (!fn) return "lat_nulo()";
        return std::string(fn) + "(" + genExpr(n->izq.get()) + ", " +
               genExpr(n->der.get()) + ")";
    }
    if (auto* n = dynamic_cast<Unaria*>(e)) {
        return "lat_negar(" + genExpr(n->operando.get()) + ")";
    }
    if (auto* n = dynamic_cast<PostOperador*>(e)) {
        // i++ / i--  ->  (v_i = lat_sumar(v_i, lat_numero(1)))
        std::string lv = genExpr(n->operando.get());
        std::string fn = (n->op == "++") ? "lat_sumar" : "lat_restar";
        return "(" + lv + " = " + fn + "(" + lv + ", lat_numero(1)))";
    }
    if (auto* n = dynamic_cast<Ternaria*>(e)) {
        return "(lat_es_verdadero(" + genExpr(n->condicion.get()) + ") ? " +
               genExpr(n->siCierto.get()) + " : " + genExpr(n->siFalso.get()) + ")";
    }
    if (auto* n = dynamic_cast<AccesoIndice*>(e)) {
        return "lat_obtener_indice(" + genExpr(n->objeto.get()) + ", " +
               genExpr(n->indice.get()) + ")";
    }
    if (auto* n = dynamic_cast<AccesoMiembro*>(e)) {
        return "lat_obtener_indice(" + genExpr(n->objeto.get()) +
               ", lat_cadena(\"" + escaparCadena(n->miembro) + "\"))";
    }
    if (auto* n = dynamic_cast<Llamada*>(e)) {
        return genLlamada(n);
    }
    if (auto* n = dynamic_cast<ListaLiteral*>(e)) {
        // "[...]" significa la lista de argumentos variádicos (lat_resto), no
        // una lista que la contiene.
        if (n->elementos.size() == 1 && dynamic_cast<VarArgs*>(n->elementos[0].get()))
            return "lat_resto";
        std::string s = "lat_lista_de(" + std::to_string(n->elementos.size());
        for (auto& el : n->elementos)
            s += ", " + genExpr(el.get());
        s += ")";
        return s;
    }
    if (auto* n = dynamic_cast<DiccionarioLiteral*>(e)) {
        std::string s = "lat_dic_de(" + std::to_string(n->pares.size());
        for (auto& par : n->pares)
            s += ", " + genExpr(par.clave.get()) + ", " + genExpr(par.valor.get());
        s += ")";
        return s;
    }
    return "lat_nulo()";
}

std::string GeneradorC::genLlamada(Llamada* ll) {
    if (auto* id = dynamic_cast<Identificador*>(ll->destino.get())) {
        const std::string& nombre = id->nombre;

        if (nombre == "escribir" || nombre == "imprimir" || nombre == "escribe" || nombre == "poner") {
            std::string fn = (nombre == "imprimir") ? "lat_imprimir" : "lat_escribir";
            std::string arg =
                ll->argumentos.empty() ? "lat_nulo()" : genExpr(ll->argumentos[0].get());
            return fn + "(" + arg + ")";
        }
        if (nombre == "acadena" || nombre == "alogico" || nombre == "anumero" || nombre == "tipo" || nombre == "error" || nombre == "incluir") {
            std::string arg =
                ll->argumentos.empty() ? "lat_nulo()" : genExpr(ll->argumentos[0].get());
            return "lat_" + nombre + "(" + arg + ")";
        }
        if (nombre == "leer" || nombre == "limpiar") {
            return "lat_" + nombre + "()";
        }
        if (nombre == "imprimirf") {
            std::string s = "lat_imprimirf(" + std::to_string(ll->argumentos.size());
            for (auto& arg : ll->argumentos) {
                s += ", " + genExpr(arg.get());
            }
            s += ")";
            return s;
        }

        auto it = funciones.find(nombre);
        if (it != funciones.end()) {
            const InfoFuncion& info = it->second;
            size_t nfix = info.numParametros;
            size_t nargs = ll->argumentos.size();
            std::string s = funC(nombre) + "(";
            for (size_t i = 0; i < nfix; i++) {
                if (i) s += ", ";
                s += (i < nargs) ? genExpr(ll->argumentos[i].get()) : "lat_nulo()";
            }
            if (info.variadico) {
                if (nfix) s += ", ";
                size_t resto = (nargs > nfix) ? nargs - nfix : 0;
                s += "lat_lista_de(" + std::to_string(resto);
                for (size_t i = nfix; i < nargs; i++)
                    s += ", " + genExpr(ll->argumentos[i].get());
                s += ")";
            }
            s += ")";
            return s;
        }
        return "lat_nulo() /* llamada no soportada: " + nombre + " */";
    }
    // Llamada de librería: cadena.xxx(args), lista.xxx(args), etc.
    if (auto* am = dynamic_cast<AccesoMiembro*>(ll->destino.get())) {
        if (auto* obj = dynamic_cast<Identificador*>(am->objeto.get())) {
            static const char* LIBS[] = {
                "cadena", "lista", "dic", "mate", "sis", "archivo", "paquete", nullptr
            };
            bool esLib = false;
            for (int i = 0; LIBS[i]; i++)
                if (obj->nombre == LIBS[i]) { esLib = true; break; }

            if (esLib) {
                libsUsadas.insert(obj->nombre);
                const std::string& lib = obj->nombre;
                const std::string& fn  = am->miembro;

                // cadena.formato es variádica (primer arg = fmt, resto = valores)
                if (lib == "cadena" && fn == "formato") {
                    std::string s = "lat_cadena_formato(" +
                                    std::to_string(ll->argumentos.size());
                    for (auto& arg : ll->argumentos)
                        s += ", " + genExpr(arg.get());
                    s += ")";
                    return s;
                }

                // Resto de funciones de librería: args fijos
                std::string nombre_c = "lat_" + lib + "_" + fn;
                std::string s = nombre_c + "(";
                for (size_t i = 0; i < ll->argumentos.size(); i++) {
                    if (i) s += ", ";
                    s += genExpr(ll->argumentos[i].get());
                }
                s += ")";
                return s;
            }

            // Despacho dinámico sobre variable que contiene un módulo cargado.
            // milib.fn(args)  →  lat_paquete_llamar(v_milib, "fn", nargs, arg0, ...)
            {
                libsUsadas.insert("paquete");
                const std::string& varname = obj->nombre;
                const std::string& fnname  = am->miembro;
                std::string s = "lat_paquete_llamar(" + varC(varname) +
                                ", lat_cadena(\"" + escaparCadena(fnname) + "\"), " +
                                std::to_string(ll->argumentos.size());
                for (auto& arg : ll->argumentos)
                    s += ", " + genExpr(arg.get());
                s += ")";
                return s;
            }
        }
    }

    return "lat_nulo() /* llamada dinamica no soportada */";
}

// ---------------------------------------------------------------------------
// Sentencias
// ---------------------------------------------------------------------------
std::string GeneradorC::genAsignacionDestino(Expresion* destino,
                                             const std::string& valorC) {
    if (auto* id = dynamic_cast<Identificador*>(destino)) {
        return varC(id->nombre) + " = " + valorC + ";";
    }
    if (auto* ai = dynamic_cast<AccesoIndice*>(destino)) {
        return "lat_asignar_indice(" + genExpr(ai->objeto.get()) + ", " +
               genExpr(ai->indice.get()) + ", " + valorC + ");";
    }
    if (auto* am = dynamic_cast<AccesoMiembro*>(destino)) {
        return "lat_asignar_indice(" + genExpr(am->objeto.get()) + ", lat_cadena(\"" +
               escaparCadena(am->miembro) + "\"), " + valorC + ");";
    }
    return "/* destino de asignacion no soportado */;";
}

void GeneradorC::genSentencia(Sentencia* s) {
    // incluir "nombre" — registra la librería para que el preámbulo emita #include.
    // El archivo .lat ya fue expandido por main.cpp antes de llegar aquí.
    if (auto* inc = dynamic_cast<Incluir*>(s)) {
        const std::string& mod = inc->modulo;
        // Solo agregar a libsUsadas si no termina en ".lat"
        if (mod.size() < 4 || mod.substr(mod.size() - 4) != ".lat")
            libsUsadas.insert(mod);
        return;
    }
    if (auto* a = dynamic_cast<Asignacion*>(s)) {
        if (a->destinos.size() == 1 && a->valores.size() == 1) {
            emitir(genAsignacionDestino(a->destinos[0].get(),
                                        genExpr(a->valores[0].get())));
            return;
        }
        std::vector<std::string> temps;
        for (auto& v : a->valores) {
            std::string t = nuevoTemp();
            emitir("LatValor " + t + " = " + genExpr(v.get()) + ";");
            temps.push_back(t);
        }
        for (size_t i = 0; i < a->destinos.size(); i++) {
            std::string rhs = (i < temps.size()) ? temps[i] : "lat_nulo()";
            emitir(genAsignacionDestino(a->destinos[i].get(), rhs));
        }
        return;
    }
    if (auto* es = dynamic_cast<ExprSentencia*>(s)) {
        emitir(genExpr(es->expr.get()) + ";");
        return;
    }
    if (auto* si = dynamic_cast<Si*>(s)) {
        emitir("if (lat_es_verdadero(" + genExpr(si->condicion.get()) + ")) {");
        ++indentacion; genBloque(si->entonces); --indentacion;
        for (auto& r : si->osis) {
            emitir("} else if (lat_es_verdadero(" + genExpr(r.condicion.get()) + ")) {");
            ++indentacion; genBloque(r.cuerpo); --indentacion;
        }
        if (si->tieneSino) {
            emitir("} else {");
            ++indentacion; genBloque(si->sino); --indentacion;
        }
        emitir("}");
        return;
    }
    if (auto* el = dynamic_cast<Elegir*>(s)) {
        std::string sw = nuevoTemp();
        emitir("LatValor " + sw + " = " + genExpr(el->opcion.get()) + ";");
        bool primero = true;
        for (auto& c : el->casos) {
            std::string cond =
                "lat_es_verdadero(lat_igual(" + sw + ", " + genExpr(c.valor.get()) + "))";
            emitir((primero ? std::string("if (") : std::string("} else if (")) + cond +
                   ") {");
            primero = false;
            ++indentacion; genBloque(c.cuerpo); --indentacion;
        }
        if (el->tieneDefecto) {
            if (primero) {
                emitir("(void)" + sw + ";");
                emitir("{");
                ++indentacion; genBloque(el->defecto); --indentacion;
                emitir("}");
            } else {
                emitir("} else {");
                ++indentacion; genBloque(el->defecto); --indentacion;
                emitir("}");
            }
        } else if (!primero) {
            emitir("}");
        } else {
            emitir("(void)" + sw + ";");
        }
        return;
    }
    if (auto* de = dynamic_cast<Desde*>(s)) {
        genSentencia(de->inicio.get());
        emitir("while (lat_es_verdadero(" + genExpr(de->condicion.get()) + ")) {");
        ++indentacion;
        genBloque(de->cuerpo);
        genSentencia(de->incremento.get());
        --indentacion;
        emitir("}");
        return;
    }
    if (auto* mi = dynamic_cast<Mientras*>(s)) {
        emitir("while (lat_es_verdadero(" + genExpr(mi->condicion.get()) + ")) {");
        ++indentacion; genBloque(mi->cuerpo); --indentacion;
        emitir("}");
        return;
    }
    if (auto* re = dynamic_cast<Repetir*>(s)) {
        emitir("do {");
        ++indentacion; genBloque(re->cuerpo); --indentacion;
        emitir("} while (!lat_es_verdadero(" + genExpr(re->condicionHasta.get()) + "));");
        return;
    }
    if (dynamic_cast<Romper*>(s)) {
        emitir("break;");
        return;
    }
    if (auto* rt = dynamic_cast<Retornar*>(s)) {
        emitir("return " + (rt->valor ? genExpr(rt->valor.get()) : "lat_nulo()") + ";");
        return;
    }
    // FuncionDef u otros: no se emiten dentro de un bloque.
}

void GeneradorC::genBloque(const ListaSent& cuerpo) {
    for (const auto& s : cuerpo)
        if (s) genSentencia(s.get());
}

void GeneradorC::genFuncion(FuncionDef* f) {
    std::string params;
    for (size_t i = 0; i < f->parametros.size(); i++) {
        if (i) params += ", ";
        params += "LatValor " + varC(f->parametros[i]);
    }
    if (f->variadico) {
        if (!f->parametros.empty()) params += ", ";
        params += "LatValor lat_resto";
    }
    if (params.empty()) params = "void";

    emitir("static LatValor " + funC(f->nombre) + "(" + params + ") {");
    ++indentacion;

    std::set<std::string> excluir(f->parametros.begin(), f->parametros.end());
    std::set<std::string> vars;
    recolectarVariables(f->cuerpo, vars, excluir);
    for (const std::string& v : vars)
        emitir("LatValor " + varC(v) + " = lat_nulo();");

    genBloque(f->cuerpo);
    emitir("return lat_nulo();");

    --indentacion;
    emitir("}");
    emitir("");
}

// ---------------------------------------------------------------------------
// Programa completo — generación en dos fases:
//   1. cuerpo (funciones + main) → detecta libsUsadas vía genLlamada
//   2. preámbulo (#include) + cuerpo → salida final
// ---------------------------------------------------------------------------
void GeneradorC::generarCuerpo(Programa& programa) {
    // Prototipos de las funciones de usuario.
    bool hayFunciones = false;
    for (auto& s : programa.sentencias) {
        if (auto* f = dynamic_cast<FuncionDef*>(s.get())) {
            std::string tipos;
            for (size_t i = 0; i < f->parametros.size(); i++) {
                if (i) tipos += ", ";
                tipos += "LatValor";
            }
            if (f->variadico) {
                if (!f->parametros.empty()) tipos += ", ";
                tipos += "LatValor";
            }
            if (tipos.empty()) tipos = "void";
            emitir("static LatValor " + funC(f->nombre) + "(" + tipos + ");");
            hayFunciones = true;
        }
    }
    if (hayFunciones) emitir("");

    // Definiciones de las funciones de usuario.
    for (auto& s : programa.sentencias)
        if (auto* f = dynamic_cast<FuncionDef*>(s.get()))
            genFuncion(f);

    // main: el resto de las sentencias de nivel superior.
    emitir("int main(void) {");
    ++indentacion;

    std::set<std::string> vars;
    recolectarVariables(programa.sentencias, vars, {});
    for (const std::string& v : vars)
        emitir("LatValor " + varC(v) + " = lat_nulo();");

    for (auto& s : programa.sentencias)
        if (!dynamic_cast<FuncionDef*>(s.get()))
            genSentencia(s.get());  // genSentencia ignora Incluir internamente

    emitir("return 0;");
    --indentacion;
    emitir("}");
}

std::string GeneradorC::generar(Programa& programa) {
    salida.str("");
    salida.clear();
    indentacion = 0;
    contadorTemp = 0;
    funciones.clear();
    libsUsadas.clear();

    recolectarFunciones(programa);

    // Fase 1: generar el cuerpo para detectar qué librerías se usan.
    generarCuerpo(programa);
    std::string cuerpo = salida.str();

    // Fase 2: preámbulo con los #include correctos, seguido del cuerpo.
    salida.str("");
    salida.clear();
    emitir("/* Generado por el compilador de Latino */");
    emitir("#include \"latino.h\"");
    for (const std::string& lib : libsUsadas)
        emitir("#include \"libs/" + lib + ".h\"");
    emitir("");
    salida << cuerpo;

    return salida.str();
}
