// analizador_semantico.cpp

#include "analizador_semantico.h"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace {
// Una constante es un identificador cuyas letras son todas mayúsculas
// (p.ej. PI, G). Debe tener al menos una letra mayúscula.
bool esMayusculas(const std::string& nombre) {
    bool hayMayuscula = false;
    for (unsigned char c : nombre) {
        if (std::isalpha(c)) {
            if (std::islower(c)) return false;
            hayMayuscula = true;
        }
    }
    return hayMayuscula;
}
}  // namespace

AnalizadorSemantico::AnalizadorSemantico()
    : profundidadBucle(0), profundidadFuncion(0), profundidadVariadica(0) {}

bool AnalizadorSemantico::analizar(Programa& programa) {
    ambitos.clear();
    funciones.clear();
    constantes.clear();
    errores.clear();
    profundidadBucle = profundidadFuncion = profundidadVariadica = 0;

    programa.aceptar(*this);

    // Reporta los errores ordenados por línea.
    std::stable_sort(errores.begin(), errores.end(),
                     [](const ErrorSemantico& a, const ErrorSemantico& b) {
                         return a.linea < b.linea;
                     });
    for (const ErrorSemantico& e : errores)
        std::cerr << "Error semántico en línea " << e.linea << ": " << e.mensaje
                  << std::endl;

    return errores.empty();
}

// ---------------------------------------------------------------------------
// Utilidades de ámbito y símbolos
// ---------------------------------------------------------------------------
void AnalizadorSemantico::entrarAmbito() {
    ambitos.emplace_back();
}

void AnalizadorSemantico::salirAmbito() {
    if (!ambitos.empty())
        ambitos.pop_back();
}

void AnalizadorSemantico::declararVariable(const std::string& nombre, int linea) {
    if (esMayusculas(nombre)) {
        if (constantes.count(nombre)) {
            agregarError(linea, "no se puede reasignar la constante '" + nombre + "'");
            return;
        }
        constantes.insert(nombre);
    }
    if (!ambitos.empty())
        ambitos.back().insert(nombre);
}

bool AnalizadorSemantico::estaDeclarada(const std::string& nombre) const {
    for (const auto& ambito : ambitos)
        if (ambito.count(nombre))
            return true;
    return false;
}

void AnalizadorSemantico::usarIdentificador(const std::string& nombre, int linea) {
    if (estaDeclarada(nombre)) return;
    if (funciones.count(nombre)) return;  // nombre de función usado como valor
    if (esIncorporada(nombre)) return;
    // Nota: los namespaces de librería (cadena, lista, etc.) se filtran en
    // visitar(AccesoMiembro&), NO aquí, para que `lista[0]` sin declarar siga
    // reportando error.
    agregarError(linea, "variable no declarada '" + nombre + "'");
}

bool AnalizadorSemantico::esIncorporada(const std::string& nombre) const {
    return nombre == "escribir" || nombre == "imprimir" || nombre == "escribe" || nombre == "poner" ||
           nombre == "acadena" || nombre == "alogico" || nombre == "anumero" ||
           nombre == "leer" || nombre == "tipo" || nombre == "imprimirf" ||
           nombre == "limpiar" || nombre == "error";
}

bool AnalizadorSemantico::esLibreria(const std::string& nombre) const {
    return nombre == "cadena" || nombre == "lista" || nombre == "dic" ||
           nombre == "mate"   || nombre == "sis"   || nombre == "archivo" ||
           nombre == "paquete";
}

void AnalizadorSemantico::recolectarFunciones(Programa& programa) {
    for (auto& s : programa.sentencias) {
        if (auto* f = dynamic_cast<FuncionDef*>(s.get())) {
            if (funciones.count(f->nombre)) {
                agregarError(f->linea, "la función '" + f->nombre + "' ya está definida");
                continue;
            }
            funciones[f->nombre] =
                InfoFuncion{f->parametros.size(), f->variadico, f->linea};
        }
    }
}

void AnalizadorSemantico::analizarBloque(ListaSent& cuerpo) {
    for (auto& s : cuerpo)
        if (s) s->aceptar(*this);
}

void AnalizadorSemantico::agregarError(int linea, const std::string& mensaje) {
    errores.push_back(ErrorSemantico{linea, mensaje});
}

// ---------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------
void AnalizadorSemantico::visitar(LitNumero&) {}
void AnalizadorSemantico::visitar(LitCadena&) {}
void AnalizadorSemantico::visitar(LitLogico&) {}
void AnalizadorSemantico::visitar(LitNulo&) {}

void AnalizadorSemantico::visitar(Identificador& n) {
    usarIdentificador(n.nombre, n.linea);
}

void AnalizadorSemantico::visitar(Binaria& n) {
    if (n.izq) n.izq->aceptar(*this);
    if (n.der) n.der->aceptar(*this);
}

void AnalizadorSemantico::visitar(Unaria& n) {
    if (n.operando) n.operando->aceptar(*this);
}

void AnalizadorSemantico::visitar(PostOperador& n) {
    if (n.operando) n.operando->aceptar(*this);
}

void AnalizadorSemantico::visitar(Ternaria& n) {
    if (n.condicion) n.condicion->aceptar(*this);
    if (n.siCierto) n.siCierto->aceptar(*this);
    if (n.siFalso) n.siFalso->aceptar(*this);
}

void AnalizadorSemantico::visitar(AccesoIndice& n) {
    if (n.objeto) n.objeto->aceptar(*this);
    if (n.indice) n.indice->aceptar(*this);
}

void AnalizadorSemantico::visitar(AccesoMiembro& n) {
    if (!n.objeto) return;
    // Si el objeto es el nombre de una librería, no es una variable; omitir el chequeo.
    if (auto* id = dynamic_cast<Identificador*>(n.objeto.get()))
        if (esLibreria(id->nombre)) return;
    n.objeto->aceptar(*this);
}

void AnalizadorSemantico::visitar(Llamada& n) {
    for (auto& a : n.argumentos)
        if (a) a->aceptar(*this);

    if (auto* id = dynamic_cast<Identificador*>(n.destino.get())) {
        const std::string& nombre = id->nombre;
        auto it = funciones.find(nombre);
        if (it != funciones.end()) {
            const InfoFuncion& info = it->second;
            size_t nargs = n.argumentos.size();
            if (info.variadico) {
                if (nargs < info.numParametros)
                    agregarError(id->linea,
                                 "la función '" + nombre + "' requiere al menos " +
                                     std::to_string(info.numParametros) +
                                     " argumento(s), se pasaron " + std::to_string(nargs));
            } else if (nargs != info.numParametros) {
                agregarError(id->linea,
                             "la función '" + nombre + "' espera " +
                                 std::to_string(info.numParametros) +
                                 " argumento(s), se pasaron " + std::to_string(nargs));
            }
        } else if (esIncorporada(nombre)) {
            // función incorporada: aridad variable, no se comprueba
        } else if (estaDeclarada(nombre)) {
            // variable usada como función (llamada dinámica): permitido
        } else {
            agregarError(id->linea, "función no definida '" + nombre + "'");
        }
    } else if (n.destino) {
        n.destino->aceptar(*this);
    }
}

void AnalizadorSemantico::visitar(ListaLiteral& n) {
    for (auto& e : n.elementos)
        if (e) e->aceptar(*this);
}

void AnalizadorSemantico::visitar(DiccionarioLiteral& n) {
    for (auto& par : n.pares) {
        if (par.clave) par.clave->aceptar(*this);
        if (par.valor) par.valor->aceptar(*this);
    }
}

void AnalizadorSemantico::visitar(VarArgs& n) {
    if (profundidadVariadica == 0)
        agregarError(n.linea, "'...' sólo es válido dentro de una función variádica");
}

// ---------------------------------------------------------------------------
// Sentencias
// ---------------------------------------------------------------------------
void AnalizadorSemantico::visitar(Incluir&) {}

void AnalizadorSemantico::visitar(Programa& n) {
    entrarAmbito();
    recolectarFunciones(n);
    for (auto& s : n.sentencias)
        if (s) s->aceptar(*this);
    salirAmbito();
}

void AnalizadorSemantico::visitar(Asignacion& n) {
    // Primero los valores (lecturas), luego se declaran los destinos.
    for (auto& v : n.valores)
        if (v) v->aceptar(*this);

    for (auto& d : n.destinos) {
        if (auto* id = dynamic_cast<Identificador*>(d.get())) {
            declararVariable(id->nombre, id->linea);
        } else if (d) {
            // destino tipo numeros[0] u obj.campo: se valida el objeto base.
            d->aceptar(*this);
        }
    }
}

void AnalizadorSemantico::visitar(ExprSentencia& n) {
    if (n.expr) n.expr->aceptar(*this);
}

void AnalizadorSemantico::visitar(Si& n) {
    if (n.condicion) n.condicion->aceptar(*this);
    analizarBloque(n.entonces);
    for (auto& rama : n.osis) {
        if (rama.condicion) rama.condicion->aceptar(*this);
        analizarBloque(rama.cuerpo);
    }
    if (n.tieneSino)
        analizarBloque(n.sino);
}

void AnalizadorSemantico::visitar(Elegir& n) {
    if (n.opcion) n.opcion->aceptar(*this);
    for (auto& caso : n.casos) {
        if (caso.valor) caso.valor->aceptar(*this);
        analizarBloque(caso.cuerpo);
    }
    if (n.tieneDefecto)
        analizarBloque(n.defecto);
}

void AnalizadorSemantico::visitar(Desde& n) {
    if (n.inicio) n.inicio->aceptar(*this);
    if (n.condicion) n.condicion->aceptar(*this);
    if (n.incremento) n.incremento->aceptar(*this);
    ++profundidadBucle;
    analizarBloque(n.cuerpo);
    --profundidadBucle;
}

void AnalizadorSemantico::visitar(Mientras& n) {
    if (n.condicion) n.condicion->aceptar(*this);
    ++profundidadBucle;
    analizarBloque(n.cuerpo);
    --profundidadBucle;
}

void AnalizadorSemantico::visitar(Repetir& n) {
    ++profundidadBucle;
    analizarBloque(n.cuerpo);
    --profundidadBucle;
    if (n.condicionHasta) n.condicionHasta->aceptar(*this);
}

void AnalizadorSemantico::visitar(Romper& n) {
    if (profundidadBucle == 0)
        agregarError(n.linea, "'romper' fuera de un bucle");
}

void AnalizadorSemantico::visitar(FuncionDef& n) {
    entrarAmbito();

    std::unordered_set<std::string> vistos;
    for (const std::string& p : n.parametros) {
        if (!vistos.insert(p).second)
            agregarError(n.linea, "parámetro duplicado '" + p + "' en la función '" +
                                      n.nombre + "'");
        declararVariable(p, n.linea);
    }

    ++profundidadFuncion;
    if (n.variadico) ++profundidadVariadica;
    analizarBloque(n.cuerpo);
    if (n.variadico) --profundidadVariadica;
    --profundidadFuncion;

    salirAmbito();
}

void AnalizadorSemantico::visitar(Retornar& n) {
    if (profundidadFuncion == 0)
        agregarError(n.linea, "'retornar' fuera de una función");
    if (n.valor) n.valor->aceptar(*this);
}
