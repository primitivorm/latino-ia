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
    tipos.clear();
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

void AnalizadorSemantico::declararVariable(const std::string& nombre,
                                           TipoAnotado tipo, int linea, bool esConst) {
    if (constantes.count(nombre)) {
        agregarError(linea, "no se puede reasignar la constante '" + nombre + "'");
        return;
    }
    if (esConst || esMayusculas(nombre)) {
        constantes.insert(nombre);
    }
    if (!ambitos.empty())
        ambitos.back()[nombre] = tipo;
}

bool AnalizadorSemantico::estaDeclarada(const std::string& nombre) const {
    for (const auto& ambito : ambitos)
        if (ambito.count(nombre))
            return true;
    return false;
}

namespace {
static std::string nombreTipoAnotado(TipoAnotado t) {
    switch (t) {
        case TipoAnotado::Numero: return "numero";
        case TipoAnotado::Cadena: return "cadena";
        case TipoAnotado::Logico: return "logico";
        case TipoAnotado::Lista:  return "lista";
        case TipoAnotado::Dic:    return "dic";
        case TipoAnotado::Nulo:   return "nulo";
        default: return "desconocido";
    }
}

// Devuelve el tipo anotado correspondiente a un nodo literal puro.
// Devuelve Ninguno para expresiones dinámicas (no se puede verificar en compilación).
static TipoAnotado tipoDelLiteral(Expresion* e) {
    if (dynamic_cast<LitNumero*>(e))          return TipoAnotado::Numero;
    if (dynamic_cast<LitCadena*>(e))          return TipoAnotado::Cadena;
    if (dynamic_cast<LitLogico*>(e))          return TipoAnotado::Logico;
    if (dynamic_cast<LitNulo*>(e))            return TipoAnotado::Nulo;
    if (dynamic_cast<ListaLiteral*>(e))       return TipoAnotado::Lista;
    if (dynamic_cast<DiccionarioLiteral*>(e)) return TipoAnotado::Dic;
    return TipoAnotado::Ninguno;
}
}  // namespace (anon)

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

void AnalizadorSemantico::analizarMetodo(MetodoDef& metodo) {
    validarTipoObjeto(metodo.tipoRetorno, metodo.tipoRetornoClase, metodo.linea);

    entrarAmbito();

    std::unordered_set<std::string> vistos;
    for (const ParamFuncion& p : metodo.parametros) {
        if (!vistos.insert(p.nombre).second)
            agregarError(metodo.linea,
                         "parámetro duplicado '" + p.nombre + "' en el método '" + metodo.nombre + "'");
        validarTipoObjeto(p.tipo, p.tipoClase, metodo.linea);
        declararVariable(p.nombre, p.tipo, metodo.linea);
    }

    ++profundidadFuncion;
    analizarBloque(metodo.cuerpo);
    --profundidadFuncion;

    salirAmbito();
}

void AnalizadorSemantico::agregarError(int linea, const std::string& mensaje) {
    errores.push_back(ErrorSemantico{linea, mensaje});
}

bool AnalizadorSemantico::estaTipoDefinido(const std::string& nombre) const {
    return tipos.count(nombre) > 0;
}

const AnalizadorSemantico::InfoTipo* AnalizadorSemantico::obtenerTipo(
    const std::string& nombre) const {
    auto it = tipos.find(nombre);
    return it != tipos.end() ? &it->second : nullptr;
}

void AnalizadorSemantico::validarTipoObjeto(TipoAnotado tipo,
                                            const std::string& clase,
                                            int linea) {
    if (tipo == TipoAnotado::Objeto && !estaTipoDefinido(clase))
        agregarError(linea, "tipo de objeto desconocido '" + clase + "'");
}

void AnalizadorSemantico::recolectarTipos(Programa& programa) {
    tipos.clear();

    for (auto& s : programa.sentencias) {
        if (auto* c = dynamic_cast<ClaseDef*>(s.get())) {
            if (tipos.count(c->nombre)) {
                agregarError(c->linea, "el tipo '" + c->nombre + "' ya está definido");
                continue;
            }

            InfoTipo info;
            info.tipo = TipoInfoKind::Clase;
            info.esAbstracta = c->esAbstracta;
            info.padre = c->padre;
            info.interfaces = c->interfaces;
            info.linea = c->linea;

            std::unordered_set<std::string> nombresCampos;
            std::unordered_set<std::string> nombresMetodos;
            bool tieneConstructor = false;

            for (const CampoDef& campo : c->campos) {
                if (!nombresCampos.insert(campo.nombre).second)
                    agregarError(campo.linea,
                                 "campo duplicado '" + campo.nombre + "' en la clase '" + c->nombre + "'");
                info.campos.insert(campo.nombre);
            }

            for (const MetodoDef& metodo : c->metodos) {
                if (!nombresMetodos.insert(metodo.nombre).second) {
                    agregarError(metodo.linea,
                                 "método duplicado '" + metodo.nombre + "' en la clase '" + c->nombre + "'");
                    continue;
                }
                if (metodo.esConstructor) {
                    if (tieneConstructor)
                        agregarError(metodo.linea,
                                     "constructor duplicado en la clase '" + c->nombre + "'");
                    tieneConstructor = true;
                }

                InfoMetodo infoMetodo;
                infoMetodo.nombre = metodo.nombre;
                infoMetodo.tipoRetorno = metodo.tipoRetorno;
                infoMetodo.tipoRetornoClase = metodo.tipoRetornoClase;
                infoMetodo.esConstructor = metodo.esConstructor;
                infoMetodo.esAbstracto = metodo.esAbstracto;
                infoMetodo.esEstatico = metodo.esEstatico;
                infoMetodo.esSobreescritura = metodo.esSobreescritura;
                infoMetodo.linea = metodo.linea;
                for (const ParamFuncion& parametro : metodo.parametros) {
                    infoMetodo.parametros.push_back(parametro.tipo);
                    infoMetodo.parametrosClase.push_back(parametro.tipoClase);
                }

                info.metodos[metodo.nombre] = std::move(infoMetodo);
            }

            tipos[c->nombre] = std::move(info);
        } else if (auto* e = dynamic_cast<EstructuraDef*>(s.get())) {
            if (tipos.count(e->nombre)) {
                agregarError(e->linea, "el tipo '" + e->nombre + "' ya está definido");
                continue;
            }

            InfoTipo info;
            info.tipo = TipoInfoKind::Estructura;
            info.esAbstracta = false;
            info.linea = e->linea;

            std::unordered_set<std::string> nombresCampos;
            std::unordered_set<std::string> nombresMetodos;
            bool tieneConstructor = false;

            for (const CampoDef& campo : e->campos) {
                if (!nombresCampos.insert(campo.nombre).second)
                    agregarError(campo.linea,
                                 "campo duplicado '" + campo.nombre + "' en la estructura '" + e->nombre + "'");
                info.campos.insert(campo.nombre);
            }

            for (const MetodoDef& metodo : e->metodos) {
                if (!nombresMetodos.insert(metodo.nombre).second) {
                    agregarError(metodo.linea,
                                 "método duplicado '" + metodo.nombre + "' en la estructura '" + e->nombre + "'");
                    continue;
                }
                if (metodo.esConstructor) {
                    if (tieneConstructor)
                        agregarError(metodo.linea,
                                     "constructor duplicado en la estructura '" + e->nombre + "'");
                    tieneConstructor = true;
                }

                InfoMetodo infoMetodo;
                infoMetodo.nombre = metodo.nombre;
                infoMetodo.tipoRetorno = metodo.tipoRetorno;
                infoMetodo.tipoRetornoClase = metodo.tipoRetornoClase;
                infoMetodo.esConstructor = metodo.esConstructor;
                infoMetodo.esAbstracto = metodo.esAbstracto;
                infoMetodo.esEstatico = metodo.esEstatico;
                infoMetodo.esSobreescritura = metodo.esSobreescritura;
                infoMetodo.linea = metodo.linea;
                for (const ParamFuncion& parametro : metodo.parametros) {
                    infoMetodo.parametros.push_back(parametro.tipo);
                    infoMetodo.parametrosClase.push_back(parametro.tipoClase);
                }

                info.metodos[metodo.nombre] = std::move(infoMetodo);
            }

            tipos[e->nombre] = std::move(info);
        } else if (auto* i = dynamic_cast<InterfazDef*>(s.get())) {
            if (tipos.count(i->nombre)) {
                agregarError(i->linea, "el tipo '" + i->nombre + "' ya está definido");
                continue;
            }

            InfoTipo info;
            info.tipo = TipoInfoKind::Interfaz;
            info.esAbstracta = true;
            info.linea = i->linea;

            std::unordered_set<std::string> nombresMetodos;
            for (const MetodoDef& metodo : i->metodos) {
                if (!nombresMetodos.insert(metodo.nombre).second) {
                    agregarError(metodo.linea,
                                 "método duplicado '" + metodo.nombre + "' en la interfaz '" + i->nombre + "'");
                    continue;
                }

                InfoMetodo infoMetodo;
                infoMetodo.nombre = metodo.nombre;
                infoMetodo.tipoRetorno = metodo.tipoRetorno;
                infoMetodo.tipoRetornoClase = metodo.tipoRetornoClase;
                infoMetodo.esConstructor = false;
                infoMetodo.esAbstracto = true;
                infoMetodo.esEstatico = metodo.esEstatico;
                infoMetodo.esSobreescritura = false;
                infoMetodo.linea = metodo.linea;
                for (const ParamFuncion& parametro : metodo.parametros) {
                    infoMetodo.parametros.push_back(parametro.tipo);
                    infoMetodo.parametrosClase.push_back(parametro.tipoClase);
                }

                info.metodos[metodo.nombre] = std::move(infoMetodo);
            }

            tipos[i->nombre] = std::move(info);
        }
    }
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
    // Si el objeto es el nombre de una librería o de un tipo POO (acceso a un
    // miembro estático), no es una variable; omitir el chequeo.
    if (auto* id = dynamic_cast<Identificador*>(n.objeto.get()))
        if (esLibreria(id->nombre) || estaTipoDefinido(id->nombre)) return;
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

void AnalizadorSemantico::visitar(NuevoExpr& n) {
    for (auto& a : n.argumentos)
        if (a) a->aceptar(*this);

    const InfoTipo* tipo = obtenerTipo(n.clase);
    if (!tipo) {
        agregarError(n.linea, "tipo de objeto desconocido '" + n.clase + "'");
        return;
    }
    if (tipo->tipo == TipoInfoKind::Interfaz) {
        agregarError(n.linea, "no se puede instanciar la interfaz '" + n.clase + "'");
        return;
    }
    if (tipo->esAbstracta) {
        agregarError(n.linea, "no se puede instanciar la clase abstracta '" + n.clase + "'");
        return;
    }

    auto it = tipo->metodos.find(n.clase);
    if (it != tipo->metodos.end()) {
        if (it->second.parametros.size() != n.argumentos.size()) {
            agregarError(n.linea,
                         "constructor de '" + n.clase + "' espera " +
                             std::to_string(it->second.parametros.size()) +
                             " argumento(s), se pasaron " + std::to_string(n.argumentos.size()));
        }
    } else if (!n.argumentos.empty()) {
        agregarError(n.linea,
                     "la clase/estructura '" + n.clase + "' no tiene constructor que reciba " +
                         std::to_string(n.argumentos.size()) + " argumento(s)");
    }
}

void AnalizadorSemantico::visitar(EsExpr& n) {
    if (n.objeto) n.objeto->aceptar(*this);
    if (!estaTipoDefinido(n.clase))
        agregarError(n.linea, "tipo desconocido '" + n.clase + "' en la expresión 'es'");
}

void AnalizadorSemantico::visitar(AccesoEste& n) {
    if (!enClase || !enMetodoInstancia)
        agregarError(n.linea, "'este' sólo puede usarse dentro de un método de instancia");
}

void AnalizadorSemantico::visitar(LlamadaBase& n) {
    for (auto& a : n.argumentos)
        if (a) a->aceptar(*this);

    if (!enConstructor) {
        agregarError(n.linea, "'base' sólo puede llamarse dentro de un constructor");
        return;
    }
    const InfoTipo* actual = obtenerTipo(tipoActual);
    if (!actual) return;
    if (actual->padre.empty()) {
        agregarError(n.linea, "la clase '" + tipoActual + "' no tiene clase base");
        return;
    }
    const InfoTipo* padre = obtenerTipo(actual->padre);
    if (!padre) return;

    auto it = padre->metodos.find(actual->padre);
    if (it != padre->metodos.end()) {
        if (it->second.parametros.size() != n.argumentos.size()) {
            agregarError(n.linea,
                         "constructor de la clase base '" + actual->padre + "' espera " +
                             std::to_string(it->second.parametros.size()) +
                             " argumento(s), se pasaron " + std::to_string(n.argumentos.size()));
        }
    } else if (!n.argumentos.empty()) {
        agregarError(n.linea,
                     "la clase base '" + actual->padre + "' no tiene constructor que reciba " +
                         std::to_string(n.argumentos.size()) + " argumento(s)");
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

void AnalizadorSemantico::visitar(ClaseDef& n) {
    const InfoTipo* tipo = obtenerTipo(n.nombre);
    if (!tipo) return;

    if (!n.padre.empty()) {
        if (n.padre == n.nombre)
            agregarError(n.linea, "una clase no puede heredar de sí misma");
        else if (!estaTipoDefinido(n.padre))
            agregarError(n.linea, "tipo base desconocido '" + n.padre + "'");
        else if (obtenerTipo(n.padre)->tipo != TipoInfoKind::Clase)
            agregarError(n.linea, "la clase base de '" + n.nombre + "' debe ser otra clase");
    }

    for (const std::string& interfaz : n.interfaces) {
        if (!estaTipoDefinido(interfaz))
            agregarError(n.linea, "interfaz desconocida '" + interfaz + "'");
        else if (obtenerTipo(interfaz)->tipo != TipoInfoKind::Interfaz)
            agregarError(n.linea, "'" + interfaz + "' no es una interfaz");
    }

    const InfoTipo* padre = n.padre.empty() ? nullptr : obtenerTipo(n.padre);
    for (const MetodoDef& metodo : n.metodos) {
        if (metodo.esAbstracto) {
            if (!n.esAbstracta)
                agregarError(metodo.linea,
                             "la clase '" + n.nombre + "' no puede contener métodos abstractos sin declarar 'abstracto'");
        }
        if (metodo.esSobreescritura) {
            if (n.padre.empty()) {
                agregarError(metodo.linea,
                             "el método '" + metodo.nombre + "' declara 'sobreescribir' pero '" + n.nombre + "' no hereda de ninguna clase");
            } else if (padre) {
                auto it = padre->metodos.find(metodo.nombre);
                if (it == padre->metodos.end())
                    agregarError(metodo.linea,
                                 "el método '" + metodo.nombre + "' declara 'sobreescribir' pero no existe en la clase base '" + n.padre + "'");
                else if (it->second.parametros.size() != metodo.parametros.size())
                    agregarError(metodo.linea,
                                 "el método '" + metodo.nombre + "' no coincide con la firma del método de la clase base '" + n.padre + "'");
            }
        }
    }
    if (padre && !n.esAbstracta) {
        for (const auto& [nombre, infoMetodo] : padre->metodos) {
            if (infoMetodo.esAbstracto) {
                auto it = tipo->metodos.find(nombre);
                if (it == tipo->metodos.end() ||
                    it->second.parametros.size() != infoMetodo.parametros.size()) {
                    agregarError(n.linea,
                                 "la clase '" + n.nombre + "' debe implementar el método abstracto '" + nombre +
                                     "' de la clase base '" + n.padre + "' o declararse abstracto");
                }
            }
        }
    }

    for (const std::string& interfaz : n.interfaces) {
        const InfoTipo* infoInterfaz = obtenerTipo(interfaz);
        if (!infoInterfaz || infoInterfaz->tipo != TipoInfoKind::Interfaz)
            continue;
        if (!n.esAbstracta) {
            for (const auto& [nombre, metodoInterface] : infoInterfaz->metodos) {
                auto it = tipo->metodos.find(nombre);
                if (it == tipo->metodos.end() ||
                    it->second.parametros.size() != metodoInterface.parametros.size()) {
                    agregarError(n.linea,
                                 "la clase '" + n.nombre + "' no implementa el método '" + nombre +
                                     "' de la interfaz '" + interfaz + "'");
                }
            }
        }
    }

    for (const CampoDef& campo : n.campos) {
        validarTipoObjeto(campo.tipoAnotado, campo.tipoClase, campo.linea);
        if (campo.valorDefecto) campo.valorDefecto->aceptar(*this);
    }

    bool anteriorEnClase = enClase;
    bool anteriorEnMetodoInstancia = enMetodoInstancia;
    bool anteriorEnConstructor = enConstructor;
    std::string anteriorTipoActual = tipoActual;

    enClase = true;
    tipoActual = n.nombre;
    for (MetodoDef& metodo : n.metodos) {
        enMetodoInstancia = !metodo.esEstatico;
        enConstructor = metodo.esConstructor;
        analizarMetodo(metodo);
    }

    enClase = anteriorEnClase;
    enMetodoInstancia = anteriorEnMetodoInstancia;
    enConstructor = anteriorEnConstructor;
    tipoActual = anteriorTipoActual;
}

void AnalizadorSemantico::visitar(EstructuraDef& n) {
    const InfoTipo* tipo = obtenerTipo(n.nombre);
    if (!tipo) return;

    for (const CampoDef& campo : n.campos) {
        validarTipoObjeto(campo.tipoAnotado, campo.tipoClase, campo.linea);
        if (campo.valorDefecto) campo.valorDefecto->aceptar(*this);
    }

    bool anteriorEnClase = enClase;
    bool anteriorEnMetodoInstancia = enMetodoInstancia;
    bool anteriorEnConstructor = enConstructor;
    std::string anteriorTipoActual = tipoActual;

    enClase = true;
    tipoActual = n.nombre;
    for (MetodoDef& metodo : n.metodos) {
        if (metodo.esSobreescritura)
            agregarError(metodo.linea,
                         "las estructuras no pueden declarar métodos con 'sobreescribir'");
        if (metodo.esAbstracto)
            agregarError(metodo.linea,
                         "las estructuras no pueden declarar métodos abstractos");
        enMetodoInstancia = !metodo.esEstatico;
        enConstructor = metodo.esConstructor;
        analizarMetodo(metodo);
    }

    enClase = anteriorEnClase;
    enMetodoInstancia = anteriorEnMetodoInstancia;
    enConstructor = anteriorEnConstructor;
    tipoActual = anteriorTipoActual;
}

void AnalizadorSemantico::visitar(InterfazDef& n) {
    for (const MetodoDef& metodo : n.metodos) {
        if (!metodo.esAbstracto)
            agregarError(metodo.linea,
                         "los métodos de una interfaz deben ser abstractos");
        if (!metodo.cuerpo.empty())
            agregarError(metodo.linea,
                         "los métodos de una interfaz no pueden tener implementación");
        validarTipoObjeto(metodo.tipoRetorno, metodo.tipoRetornoClase, metodo.linea);
        for (const ParamFuncion& parametro : metodo.parametros)
            validarTipoObjeto(parametro.tipo, parametro.tipoClase, metodo.linea);
    }
}

void AnalizadorSemantico::visitar(Programa& n) {
    entrarAmbito();
    recolectarFunciones(n);
    recolectarTipos(n);
    for (auto& s : n.sentencias)
        if (s) s->aceptar(*this);
    salirAmbito();
}

void AnalizadorSemantico::visitar(Asignacion& n) {
    // Primero los valores (lecturas), luego se declaran los destinos.
    for (auto& v : n.valores)
        if (v) v->aceptar(*this);

    for (size_t i = 0; i < n.destinos.size(); i++) {
        TipoAnotado tipo = (i < n.tiposDestino.size())
                               ? n.tiposDestino[i]
                               : TipoAnotado::Ninguno;

        if (auto* id = dynamic_cast<Identificador*>(n.destinos[i].get())) {
            // Verificación estática: si hay anotación y el valor es un literal,
            // detectar la incompatibilidad en compilación.
            if (tipo != TipoAnotado::Ninguno && i < n.valores.size() && n.valores[i]) {
                TipoAnotado real = tipoDelLiteral(n.valores[i].get());
                if (real != TipoAnotado::Ninguno && real != tipo)
                    agregarError(id->linea,
                                 "tipo incompatible: se declaró '" +
                                     nombreTipoAnotado(tipo) +
                                     "' pero el valor es '" +
                                     nombreTipoAnotado(real) + "'");
            }
            declararVariable(id->nombre, tipo, id->linea, n.esConst);
        } else if (n.destinos[i]) {
            // destino tipo numeros[0] u obj.campo: se valida el objeto base.
            n.destinos[i]->aceptar(*this);
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
    validarTipoObjeto(n.tipoRetorno, n.tipoRetornoClase, n.linea);
    entrarAmbito();

    std::unordered_set<std::string> vistos;
    for (const ParamFuncion& p : n.parametros) {
        if (!vistos.insert(p.nombre).second)
            agregarError(n.linea, "parámetro duplicado '" + p.nombre + "' en la función '" +
                                      n.nombre + "'");
        validarTipoObjeto(p.tipo, p.tipoClase, n.linea);
        declararVariable(p.nombre, p.tipo, n.linea);
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
