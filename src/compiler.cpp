// compiler.cpp — generación de código C (Fase 5). Ver compiler.h.

#include "compiler.h"

#include <cstdio>
#include <functional>

#include "fn_binaria.h"

namespace {

const char* tipoALatTipo(TipoAnotado t) {
    switch (t) {
        case TipoAnotado::Numero: return "LAT_NUMERO";
        case TipoAnotado::Cadena: return "LAT_CADENA";
        case TipoAnotado::Logico: return "LAT_LOGICO";
        case TipoAnotado::Lista:  return "LAT_LISTA";
        case TipoAnotado::Dic:    return "LAT_DICCIONARIO";
        case TipoAnotado::Nulo:   return "LAT_NULO";
        case TipoAnotado::Objeto: return "LAT_OBJETO";
        default: return nullptr;
    }
}

// PLAN_FFI.md (F5): tipo C real de un TipoFFI, para la declaración "extern"
// del símbolo nativo y para el cast al ancho exacto en el marshalling de
// argumentos/retorno (ver tabla de tipos FFI del plan).
const char* tipoFFIaC(TipoFFI t) {
    switch (t) {
        case TipoFFI::Numero:    return "double";
        case TipoFFI::Logico:    return "int";
        case TipoFFI::Cadena:    return "const char*";
        case TipoFFI::Nulo:      return "void";
        case TipoFFI::Entero8:   return "int8_t";
        case TipoFFI::Entero16:  return "int16_t";
        case TipoFFI::Entero32:  return "int32_t";
        case TipoFFI::Entero64:  return "int64_t";
        case TipoFFI::Natural8:  return "uint8_t";
        case TipoFFI::Natural16: return "uint16_t";
        case TipoFFI::Natural32: return "uint32_t";
        case TipoFFI::Natural64: return "uint64_t";
        case TipoFFI::Puntero:   return "void*";
    }
    return "void";
}

// PLAN_FFI.md (F5): LatTipo que le corresponde a un TipoFFI para
// lat_ffi_verificar_tipo -- los ocho anchos de entero/natural comparten
// LAT_NUMERO (mismo motivo que categoriaDeTipoFFI en analizador_semantico.cpp:
// el ancho exacto se aplica en el cast a tipoFFIaC, no en la verificación).
const char* tipoFFIaLatTipo(TipoFFI t) {
    switch (t) {
        case TipoFFI::Logico:  return "LAT_LOGICO";
        case TipoFFI::Cadena:  return "LAT_CADENA";
        case TipoFFI::Puntero: return "LAT_PUNTERO";
        case TipoFFI::Nulo:    return "LAT_NULO";
        default:               return "LAT_NUMERO";  // numero + entero*/natural*
    }
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

// PLAN_FFI.md (F5): registra cada firma de un bloque "externo" de nivel
// superior (mismo alcance que recolectarFunciones/recolectarTipos: no
// desciende dentro de bloques anidados, igual que un FuncionDef anidado
// tampoco se registra) y las bibliotecas nombradas por "enlazar".
void GeneradorC::recolectarExterno(Programa& programa) {
    for (auto& s : programa.sentencias) {
        if (auto* ext = dynamic_cast<ExternoBloque*>(s.get())) {
            if (!ext->enlazar.empty()) bibliotecasEnlazar.insert(ext->enlazar);
            for (const FuncionExterna& fe : ext->funciones) {
                InfoFuncionExterna info;
                info.tipoRetorno = fe.tipoRetorno;
                for (const ParamFFI& p : fe.parametros)
                    info.parametrosTipo.push_back(p.tipo);
                funcionesExternas[fe.nombre] = std::move(info);
            }
        }
    }
}

void GeneradorC::recolectarTipos(Programa& programa) {
    clases.clear();
    estructuras.clear();
    interfaces.clear();
    for (auto& s : programa.sentencias) {
        if (auto* c = dynamic_cast<ClaseDef*>(s.get()))
            clases[c->nombre] = c;
        else if (auto* e = dynamic_cast<EstructuraDef*>(s.get()))
            estructuras[e->nombre] = e;
        else if (auto* i = dynamic_cast<InterfazDef*>(s.get()))
            interfaces[i->nombre] = i;
    }
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
    if (auto* n = dynamic_cast<NuevoExpr*>(e)) {
        std::string obj = nuevoTemp();

        auto itC = clases.find(n->clase);
        if (itC != clases.end()) {
            ClaseDef* c = itC->second;

            // Cadena de ascendencia (de la clase base a la derivada), para que
            // lat_obj_es_instancia reconozca a los ancestros tras la herencia.
            std::vector<std::string> cadena;
            for (ClaseDef* t = c; t != nullptr;) {
                cadena.push_back(t->nombre);
                if (t->padre.empty()) break;
                auto itPadreTipo = clases.find(t->padre);
                t = (itPadreTipo != clases.end()) ? itPadreTipo->second : nullptr;
            }
            emitir("LatValor " + obj + " = lat_obj_nuevo(\"" + escaparCadena(cadena.back()) + "\");");
            for (size_t i = cadena.size() - 1; i-- > 0;)
                emitir("lat_obj_set_clase(" + obj + ", \"" + escaparCadena(cadena[i]) + "\");");

            std::function<void(ClaseDef*)> registrarTipo = [&](ClaseDef* tipo) {
                if (!tipo) return;
                if (!tipo->padre.empty()) {
                    auto itPadre = clases.find(tipo->padre);
                    if (itPadre != clases.end()) registrarTipo(itPadre->second);
                }
                for (const CampoDef& campo : tipo->campos) {
                    if (campo.valorDefecto) {
                        std::string valor = genExpr(campo.valorDefecto.get());
                        emitir("lat_obj_set(" + obj + ", lat_cadena(\"" + escaparCadena(campo.nombre) + "\"), " + valor + ");");
                    }
                }
                for (const MetodoDef& metodo : tipo->metodos) {
                    if (metodo.esConstructor || metodo.esEstatico || metodo.esAbstracto) continue;
                    emitir("lat_obj_set_metodo(" + obj + ", \"" + escaparCadena(metodo.nombre) + "\", lat_funcion_nueva(" + funC(tipo->nombre + "_" + metodo.nombre) + "));" );
                }
            };
            registrarTipo(c);

            bool tieneCtor = false;
            for (const MetodoDef& metodo : c->metodos) {
                if (metodo.esConstructor) {
                    tieneCtor = true;
                    break;
                }
            }
            if (tieneCtor) {
                std::string argsArr = nuevoTemp();
                size_t nargs = n->argumentos.size() + 1;
                emitir("LatValor " + argsArr + "[" + std::to_string(nargs) + "]; ");
                emitir(argsArr + "[0] = " + obj + ";");
                for (size_t i = 0; i < n->argumentos.size(); i++) {
                    std::string arg = genExpr(n->argumentos[i].get());
                    emitir(argsArr + "[" + std::to_string(i + 1) + "] = " + arg + ";");
                }
                emitir(funC(n->clase + "_" + n->clase) + "(" + std::to_string(nargs) + ", " + argsArr + ");");
            }
        } else {
            emitir("LatValor " + obj + " = lat_obj_nuevo(\"" + escaparCadena(n->clase) + "\");");
            auto itE = estructuras.find(n->clase);
            if (itE != estructuras.end()) {
                EstructuraDef* e = itE->second;
                for (const CampoDef& campo : e->campos) {
                    if (campo.valorDefecto) {
                        std::string valor = genExpr(campo.valorDefecto.get());
                        emitir("lat_obj_set(" + obj + ", lat_cadena(\"" + escaparCadena(campo.nombre) + "\"), " + valor + ");");
                    }
                }
                for (const MetodoDef& metodo : e->metodos) {
                    if (metodo.esConstructor || metodo.esEstatico || metodo.esAbstracto) continue;
                    emitir("lat_obj_set_metodo(" + obj + ", \"" + escaparCadena(metodo.nombre) + "\", lat_funcion_nueva(" + funC(e->nombre + "_" + metodo.nombre) + "));" );
                }

                bool tieneCtor = false;
                for (const MetodoDef& metodo : e->metodos) {
                    if (metodo.esConstructor) {
                        tieneCtor = true;
                        break;
                    }
                }
                if (tieneCtor) {
                    std::string argsArr = nuevoTemp();
                    size_t nargs = n->argumentos.size() + 1;
                    emitir("LatValor " + argsArr + "[" + std::to_string(nargs) + "]; ");
                    emitir(argsArr + "[0] = " + obj + ";");
                    for (size_t i = 0; i < n->argumentos.size(); i++) {
                        std::string arg = genExpr(n->argumentos[i].get());
                        emitir(argsArr + "[" + std::to_string(i + 1) + "] = " + arg + ";");
                    }
                    emitir(funC(e->nombre + "_" + e->nombre) + "(" + std::to_string(nargs) + ", " + argsArr + ");");
                }
            }
        }
        return obj;
    }
    if (auto* n = dynamic_cast<EsExpr*>(e)) {
        return "lat_logico(lat_obj_es_instancia(" + genExpr(n->objeto.get()) + ", \"" + escaparCadena(n->clase) + "\"))";
    }
    if (auto* n = dynamic_cast<AccesoEste*>(e)) {
        return "este";
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

        // PLAN_FFI.md (F5): llamada a una función "externo". La aridad/tipo
        // ya se validaron estáticamente cuando fue posible en el análisis
        // semántico (F4); acá se genera el marshalling real.
        auto itExt = funcionesExternas.find(nombre);
        if (itExt != funcionesExternas.end())
            return genLlamadaExterna(nombre, itExt->second, ll);

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

                // Hallazgo de la auditoría de PLAN_LIBS.md: paquete.llamar(modulo,
                // nombre, nargs, arg0, ...) es la única función de librería cuyo
                // tercer parámetro fijo NO es un LatValor real en C --
                // lat_paquete_llamar(LatValor, LatValor, int nargs, ...) recibe
                // "nargs" como un int crudo, escrito por el usuario en el sitio de
                // llamada (no derivado de contar argumentos, a diferencia de
                // cadena.formato arriba). El camino genérico de "args fijos" de
                // abajo pasaba ese argumento tal cual (un LatValor), rompiendo la
                // compilación C con un error de conversión de tipo -- nunca
                // funcionó llamado así, solo "milib.fn(args)" (que pasa por
                // lat_obj_llamar_metodo, con su propio nargs literal) funcionaba.
                if (lib == "paquete" && fn == "llamar" && ll->argumentos.size() >= 3) {
                    std::string s = "lat_paquete_llamar(" + genExpr(ll->argumentos[0].get()) +
                                    ", " + genExpr(ll->argumentos[1].get()) + ", (int)(" +
                                    genExpr(ll->argumentos[2].get()) + ").como.numero";
                    for (size_t i = 3; i < ll->argumentos.size(); i++)
                        s += ", " + genExpr(ll->argumentos[i].get());
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

            // Método estático: NombreClase.metodo(args...) / NombreEstructura.metodo(args...)
            std::string tipoDeclarante;
            {
                auto itC = clases.find(obj->nombre);
                for (ClaseDef* t = (itC != clases.end()) ? itC->second : nullptr; t;) {
                    bool encontrado = false;
                    for (const MetodoDef& m : t->metodos)
                        if (m.nombre == am->miembro && m.esEstatico) { encontrado = true; break; }
                    if (encontrado) { tipoDeclarante = t->nombre; break; }
                    if (t->padre.empty()) break;
                    auto itPadre = clases.find(t->padre);
                    t = (itPadre != clases.end()) ? itPadre->second : nullptr;
                }
                if (tipoDeclarante.empty()) {
                    auto itE = estructuras.find(obj->nombre);
                    if (itE != estructuras.end()) {
                        for (const MetodoDef& m : itE->second->metodos)
                            if (m.nombre == am->miembro && m.esEstatico) { tipoDeclarante = itE->second->nombre; break; }
                    }
                }
            }
            if (!tipoDeclarante.empty()) {
                size_t nargs = ll->argumentos.size();
                std::string argsArr;
                if (nargs > 0) {
                    argsArr = nuevoTemp();
                    emitir("LatValor " + argsArr + "[" + std::to_string(nargs) + "];");
                    for (size_t i = 0; i < nargs; i++)
                        emitir(argsArr + "[" + std::to_string(i) + "] = " + genExpr(ll->argumentos[i].get()) + ";");
                }
                return funC(tipoDeclarante + "_" + am->miembro) + "(" + std::to_string(nargs) +
                       ", " + (nargs > 0 ? argsArr : "NULL") + ")";
            }
        }

        // Llamada a método de objeto, módulo dinámico o valor invocable.
        {
            std::string s = "lat_obj_llamar_metodo(" + genExpr(am->objeto.get()) +
                            ", \"" + escaparCadena(am->miembro) + "\", " +
                            std::to_string(ll->argumentos.size());
            for (auto& arg : ll->argumentos)
                s += ", " + genExpr(arg.get());
            s += ")";
            return s;
        }
    }

    return "lat_nulo() /* llamada dinamica no soportada */";
}

// PLAN_FFI.md (F5): extrae el valor C primitivo de un argumento LatValor,
// con el chequeo dinámico lat_ffi_verificar_tipo (mismo espíritu que
// lat_verificar_tipo para un parámetro anotado de una función Latino normal:
// se emite siempre, sin importar si F4 ya lo verificó estáticamente).
std::string GeneradorC::genArgumentoFFI(Expresion* arg, TipoFFI tipo,
                                        const std::string& nombreFn, int indice) {
    std::string val = genExpr(arg);  // arg == nullptr -> "lat_nulo()" (ver genExpr)

    if (tipo == TipoFFI::Puntero) {
        // Hallazgo de F4 (ver PLAN_FFI.md): el literal "nulo" es un puntero
        // nulo válido, pero LAT_NULO != LAT_PUNTERO para
        // lat_ffi_verificar_tipo -- hay que desviarlo a NULL antes de pasar
        // por la verificación dinámica. Se usa un temporal porque 'val'
        // puede tener efectos de lado (p.ej. otra llamada) y no debe
        // evaluarse dos veces.
        std::string t = nuevoTemp();
        emitir("LatValor " + t + " = " + val + ";");
        return "(" + t + ".tipo == LAT_NULO ? NULL : lat_ffi_verificar_tipo(" + t +
               ", LAT_PUNTERO, \"" + escaparCadena(nombreFn) + "\", " + std::to_string(indice) +
               ").como.puntero)";
    }

    std::string verificado = "lat_ffi_verificar_tipo(" + val + ", " + tipoFFIaLatTipo(tipo) +
                             ", \"" + escaparCadena(nombreFn) + "\", " + std::to_string(indice) + ")";
    switch (tipo) {
        case TipoFFI::Logico: return verificado + ".como.logico";
        case TipoFFI::Cadena: return verificado + ".como.cadena";
        case TipoFFI::Numero: return verificado + ".como.numero";
        default:
            // Entero8/16/32/64, Natural8/16/32/64: LAT_NUMERO comparte
            // storage double; el ancho C exacto se aplica con el cast.
            return "(" + std::string(tipoFFIaC(tipo)) + ")" + verificado + ".como.numero";
    }
}

// PLAN_FFI.md (F5): llamada real al símbolo nativo (ya declarado "extern"
// en el preámbulo por generar()) y empaquetado del resultado en un LatValor.
std::string GeneradorC::genLlamadaExterna(const std::string& nombre,
                                          const InfoFuncionExterna& info, Llamada* ll) {
    std::string args;
    for (size_t i = 0; i < info.parametrosTipo.size(); i++) {
        if (i) args += ", ";
        Expresion* arg = (i < ll->argumentos.size()) ? ll->argumentos[i].get() : nullptr;
        args += genArgumentoFFI(arg, info.parametrosTipo[i], nombre, static_cast<int>(i + 1));
    }
    std::string llamadaC = nombre + "(" + args + ")";

    switch (info.tipoRetorno) {
        case TipoFFI::Nulo:
            // Retorno void: la coma permite descartar el resultado (que no
            // existe) y devolver un LatValor nulo en la misma expresión.
            return "(" + llamadaC + ", lat_nulo())";
        case TipoFFI::Logico:
            return "lat_logico((int)(" + llamadaC + "))";
        case TipoFFI::Cadena:
            return "lat_cadena(" + llamadaC + ")";
        case TipoFFI::Puntero:
            return "lat_puntero((void*)(" + llamadaC + "))";
        default:
            // Numero + Entero*/Natural*: LAT_NUMERO es double (ver riesgo de
            // precisión en enteros de 64 bits, documentado en PLAN_FFI.md).
            return "lat_numero((double)(" + llamadaC + "))";
    }
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
            TipoAnotado tipo = a->tiposDestino.empty()
                                   ? TipoAnotado::Ninguno
                                   : a->tiposDestino[0];
            std::string val = genExpr(a->valores[0].get());
            if (tipo != TipoAnotado::Ninguno) {
                const char* ct = tipoALatTipo(tipo);
                std::string nombreVar;
                if (auto* id = dynamic_cast<Identificador*>(a->destinos[0].get()))
                    nombreVar = id->nombre;
                val = "lat_verificar_tipo(" + val + ", " + ct +
                      ", \"" + nombreVar + "\", " + std::to_string(a->linea) + ")";
            }
            emitir(genAsignacionDestino(a->destinos[0].get(), val));
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
    if (auto* b = dynamic_cast<LlamadaBase*>(s)) {
        std::string argsArr = nuevoTemp();
        size_t nargs = b->argumentos.size() + 1;
        emitir("LatValor " + argsArr + "[" + std::to_string(nargs) + "]; ");
        emitir(argsArr + "[0] = este;");
        for (size_t i = 0; i < b->argumentos.size(); i++) {
            emitir(argsArr + "[" + std::to_string(i + 1) + "] = " + genExpr(b->argumentos[i].get()) + ";");
        }
        bool padreTieneCtor = false;
        if (!actualPadre.empty()) {
            auto itPadre = clases.find(actualPadre);
            if (itPadre != clases.end())
                for (const MetodoDef& m : itPadre->second->metodos)
                    if (m.esConstructor) { padreTieneCtor = true; break; }
        }
        if (!padreTieneCtor) {
            emitir("/* base() sin constructor de clase base que ejecutar */");
        } else {
            emitir(funC(actualPadre + "_" + actualPadre) + "(" + std::to_string(nargs) + ", " + argsArr + ");");
        }
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
    // PLAN_FFI.md (F5): "inseguro" es puramente un marcador estático (F4 ya
    // verificó que las llamadas dentro son válidas) -- no genera código
    // propio, solo emite su cuerpo tal cual.
    if (auto* ib = dynamic_cast<InseguroBloque*>(s)) {
        genBloque(ib->cuerpo);
        return;
    }
    // FuncionDef, ExternoBloque u otros: no se emiten dentro de un bloque
    // (ExternoBloque ya se recolectó en recolectarExterno/generar(), igual
    // que un FuncionDef anidado tampoco se registra).
}

void GeneradorC::genBloque(const ListaSent& cuerpo) {
    for (const auto& s : cuerpo)
        if (s) genSentencia(s.get());
}

void GeneradorC::genFuncion(FuncionDef* f) {
    std::string params;
    for (size_t i = 0; i < f->parametros.size(); i++) {
        if (i) params += ", ";
        params += "LatValor " + varC(f->parametros[i].nombre);
    }
    if (f->variadico) {
        if (!f->parametros.empty()) params += ", ";
        params += "LatValor lat_resto";
    }
    if (params.empty()) params = "void";

    emitir("static LatValor " + funC(f->nombre) + "(" + params + ") {");
    ++indentacion;

    std::set<std::string> excluir;
    for (const auto& p : f->parametros) excluir.insert(p.nombre);

    std::set<std::string> vars;
    recolectarVariables(f->cuerpo, vars, excluir);
    for (const std::string& v : vars)
        emitir("LatValor " + varC(v) + " = lat_nulo();");

    // PLAN_GENERICOS.md: un parámetro de tipo "T" se compila exactamente
    // como si no tuviera anotación (erasure) — nunca se emite un chequeo de
    // runtime para él, sea cual sea el bound declarado (los bounds son
    // solo estáticos, ver el plan).
    std::set<std::string> genericosFn;
    for (const auto& g : f->genericos) genericosFn.insert(g.nombre);

    // Chequeos de tipo de parámetros anotados.
    for (const auto& p : f->parametros) {
        if (p.tipo == TipoAnotado::Objeto && genericosFn.count(p.tipoClase)) continue;
        if (p.tipo != TipoAnotado::Ninguno) {
            const char* ct = tipoALatTipo(p.tipo);
            emitir("lat_verificar_tipo(" + varC(p.nombre) + ", " + ct +
                   ", \"" + p.nombre + "\", " + std::to_string(f->linea) + ");");
        }
    }

    genBloque(f->cuerpo);
    emitir("return lat_nulo();");

    --indentacion;
    emitir("}");
    emitir("");
}

void GeneradorC::genMetodo(const std::string& claseNombre, MetodoDef* metodo,
                           const std::string& padreNombre) {
    if (metodo->esAbstracto) return;

    emitir("static LatValor " + funC(claseNombre + "_" + metodo->nombre) + "(int nargs, LatValor* args) {");
    ++indentacion;

    bool instancia = !metodo->esEstatico;
    if (instancia) {
        emitir("LatValor este = (nargs > 0) ? args[0] : lat_nulo();");
    }

    size_t offset = instancia ? 1 : 0;
    for (size_t i = 0; i < metodo->parametros.size(); i++) {
        const std::string& nombre = metodo->parametros[i].nombre;
        std::string idx = std::to_string(i + offset);
        std::string expr = "(nargs > " + idx + ") ? args[" + idx + "] : lat_nulo()";
        emitir("LatValor " + varC(nombre) + " = " + expr + ";");
    }

    std::set<std::string> excluir;
    if (instancia) excluir.insert("este");
    for (const auto& p : metodo->parametros) excluir.insert(p.nombre);

    std::set<std::string> vars;
    recolectarVariables(metodo->cuerpo, vars, excluir);
    for (const std::string& v : vars)
        emitir("LatValor " + varC(v) + " = lat_nulo();");

    // PLAN_GENERICOS.md: erasure — ver comentario equivalente en genFuncion().
    // Un parámetro tipado con el nombre de un genérico del método o de la
    // clase/estructura envolvente nunca recibe un chequeo de runtime.
    std::set<std::string> genericosMetodo;
    for (const auto& g : metodo->genericos) genericosMetodo.insert(g.nombre);
    if (auto itClase = clases.find(claseNombre); itClase != clases.end())
        for (const auto& g : itClase->second->genericos) genericosMetodo.insert(g.nombre);
    if (auto itEstructura = estructuras.find(claseNombre); itEstructura != estructuras.end())
        for (const auto& g : itEstructura->second->genericos) genericosMetodo.insert(g.nombre);

    for (const auto& p : metodo->parametros) {
        if (p.tipo == TipoAnotado::Objeto && genericosMetodo.count(p.tipoClase)) continue;
        if (p.tipo != TipoAnotado::Ninguno) {
            const char* ct = tipoALatTipo(p.tipo);
            emitir("lat_verificar_tipo(" + varC(p.nombre) + ", " + ct +
                   ", \"" + p.nombre + "\", " + std::to_string(metodo->linea) + ");");
        }
    }

    if (!metodo->cuerpo.empty()) {
        std::string anteriorClase = actualClase;
        std::string anteriorPadre = actualPadre;
        actualClase = claseNombre;
        actualPadre = padreNombre;
        genBloque(metodo->cuerpo);
        actualClase = anteriorClase;
        actualPadre = anteriorPadre;
    }
    emitir("return lat_nulo();");

    --indentacion;
    emitir("}");
    emitir("");
}

void GeneradorC::genClase(ClaseDef* c) {
    std::string anteriorClase = actualClase;
    std::string anteriorPadre = actualPadre;
    actualClase = c->nombre;
    actualPadre = c->padre;
    for (MetodoDef& metodo : c->metodos)
        genMetodo(c->nombre, &metodo, c->padre);
    actualClase = anteriorClase;
    actualPadre = anteriorPadre;
}

void GeneradorC::genEstructura(EstructuraDef* e) {
    std::string anteriorClase = actualClase;
    std::string anteriorPadre = actualPadre;
    actualClase = e->nombre;
    actualPadre.clear();
    for (MetodoDef& metodo : e->metodos)
        genMetodo(e->nombre, &metodo, "");
    actualClase = anteriorClase;
    actualPadre = anteriorPadre;
}

void GeneradorC::genInterfaz(InterfazDef* i) {
    (void)i;
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

    // Prototipos de métodos de clases y estructuras.
    for (auto& [nombre, c] : clases) {
        for (const MetodoDef& m : c->metodos) {
            emitir("static LatValor " + funC(nombre + "_" + m.nombre) + "(int nargs, LatValor* args);");
            hayFunciones = true;
        }
    }
    for (auto& [nombre, e] : estructuras) {
        for (const MetodoDef& m : e->metodos) {
            emitir("static LatValor " + funC(nombre + "_" + m.nombre) + "(int nargs, LatValor* args);");
            hayFunciones = true;
        }
    }
    if (hayFunciones) emitir("");

    // Hallazgo real (auditoría de input/): una variable/constante de nivel
    // superior (p.ej. "const PI = 3.14159") se declaraba como LOCAL de
    // main() -- invisible para cualquier función definida a nivel superior
    // que la referenciara ("identificador no declarado" al compilar el C
    // generado; nunca se detectó porque ningún ejemplo/prueba existente
    // ejercitaba este patrón). Se declaran acá como globales reales de C,
    // ANTES de las definiciones de función/método, para que puedan verlas.
    // Sin inicializador explícito: static-storage se zero-inicializa por el
    // propio estándar de C, y LAT_NULO es el primer valor de LatTipo (0), así
    // que el resultado es exactamente lat_nulo() sin necesidad de llamarlo.
    // main() sigue siendo quien las asigna con su valor real, en el mismo
    // orden de sentencias que antes -- solo cambia dónde vive el storage.
    std::set<std::string> vars;
    recolectarVariables(programa.sentencias, vars, {});
    for (const std::string& v : vars)
        emitir("static LatValor " + varC(v) + ";");
    if (!vars.empty()) emitir("");

    // Definiciones de las funciones de usuario.
    for (auto& s : programa.sentencias)
        if (auto* f = dynamic_cast<FuncionDef*>(s.get()))
            genFuncion(f);

    // Definiciones de métodos de clases y estructuras.
    for (auto& [nombre, c] : clases)
        genClase(c);
    for (auto& [nombre, e] : estructuras)
        genEstructura(e);

    // main: el resto de las sentencias de nivel superior.
    emitir("int main(int argc, char *argv[]) {");
    ++indentacion;
    emitir("lat_set_args(argc, argv);");

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
    funcionesExternas.clear();
    bibliotecasEnlazar.clear();
    libsUsadas.clear();

    recolectarFunciones(programa);
    recolectarExterno(programa);
    recolectarTipos(programa);

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
    if (!funcionesExternas.empty())
        emitir("#include <stdint.h>");
    emitir("");

    // PLAN_FFI.md (F5): por cada bloque "externo enlazar \"lib\"", biblioteca
    // de importación adicional. #pragma comment solo lo entiende MSVC; en
    // GNU/Clang la agrega invocador_c.cpp como "-l<lib>" (ver
    // bibliotecasEnlazadas()).
    for (const std::string& lib : bibliotecasEnlazar) {
        emitir("#ifdef _MSC_VER");
        emitir("#pragma comment(lib, \"" + lib + ".lib\")");
        emitir("#endif");
    }

    // Declaraciones "extern" con la firma C real de cada función "externo"
    // (no la firma empaquetada LatValor de una función Latino normal).
    for (auto& s : programa.sentencias) {
        if (auto* ext = dynamic_cast<ExternoBloque*>(s.get())) {
            for (const FuncionExterna& fe : ext->funciones) {
                std::string params;
                for (size_t i = 0; i < fe.parametros.size(); i++) {
                    if (i) params += ", ";
                    params += tipoFFIaC(fe.parametros[i].tipo);
                }
                if (params.empty()) params = "void";
                emitir(std::string("extern ") + tipoFFIaC(fe.tipoRetorno) + " " + fe.nombre +
                       "(" + params + ");");
            }
        }
    }
    if (!funcionesExternas.empty()) emitir("");

    salida << cuerpo;

    return salida.str();
}
