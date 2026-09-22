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
    : profundidadBucle(0), profundidadFuncion(0), profundidadVariadica(0),
      profundidadInseguro(0) {}

bool AnalizadorSemantico::analizar(Programa& programa) {
    ambitos.clear();
    clasesVariable.clear();
    funciones.clear();
    funcionesExternas.clear();
    tipos.clear();
    constantes.clear();
    errores.clear();
    profundidadBucle = profundidadFuncion = profundidadVariadica = profundidadInseguro = 0;

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
    clasesVariable.emplace_back();  // PLAN_POO.md (Reto 6): misma pila que ambitos
}

void AnalizadorSemantico::salirAmbito() {
    if (!ambitos.empty())
        ambitos.pop_back();
    if (!clasesVariable.empty())
        clasesVariable.pop_back();
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

void AnalizadorSemantico::registrarClaseVariable(const std::string& nombre, const std::string& clase) {
    if (clasesVariable.empty()) return;
    if (clase.empty())
        clasesVariable.back().erase(nombre);
    else
        clasesVariable.back()[nombre] = clase;
}

std::string AnalizadorSemantico::tipoClaseDeVariable(const std::string& nombre) const {
    for (auto it = clasesVariable.rbegin(); it != clasesVariable.rend(); ++it) {
        auto encontrado = it->find(nombre);
        if (encontrado != it->end()) return encontrado->second;
    }
    return "";
}

TipoAnotado AnalizadorSemantico::tipoDeVariable(const std::string& nombre) const {
    for (auto it = ambitos.rbegin(); it != ambitos.rend(); ++it) {
        auto encontrado = it->find(nombre);
        if (encontrado != it->end()) return encontrado->second;
    }
    return TipoAnotado::Ninguno;
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

// PLAN_FFI.md (F4): nombre de lexema de un TipoFFI, para mensajes de error
// (misma duplicación deliberada que nombreTipoAst en ast_impresor.cpp -- cada
// capa tiene su propia función de una sola dirección, enum -> string).
static std::string nombreTipoFFI(TipoFFI t) {
    switch (t) {
        case TipoFFI::Numero:    return "numero";
        case TipoFFI::Logico:    return "logico";
        case TipoFFI::Cadena:    return "cadena";
        case TipoFFI::Nulo:      return "nulo";
        case TipoFFI::Entero8:   return "entero8";
        case TipoFFI::Entero16:  return "entero16";
        case TipoFFI::Entero32:  return "entero32";
        case TipoFFI::Entero64:  return "entero64";
        case TipoFFI::Natural8:  return "natural8";
        case TipoFFI::Natural16: return "natural16";
        case TipoFFI::Natural32: return "natural32";
        case TipoFFI::Natural64: return "natural64";
        case TipoFFI::Puntero:   return "puntero";
    }
    return "";
}

// PLAN_FFI.md (F4): categoría amplia de un TipoFFI para el chequeo estático
// de un argumento en un sitio de llamada. Los distintos anchos de
// entero/natural son intercambiables entre sí y con "numero" para este
// chequeo -- el ancho/truncado real al tipo C exacto se aplica en el
// marshalling de F5/F6, no aquí (mismo motivo que lat_ffi_verificar_tipo,
// en runtime/latino.c, recibe un LatTipo y no un TipoFFI).
enum class CategoriaFFI { Numero, Logico, Cadena, Puntero, Nulo };

static CategoriaFFI categoriaDeTipoFFI(TipoFFI t) {
    switch (t) {
        case TipoFFI::Logico:  return CategoriaFFI::Logico;
        case TipoFFI::Cadena:  return CategoriaFFI::Cadena;
        case TipoFFI::Puntero: return CategoriaFFI::Puntero;
        case TipoFFI::Nulo:    return CategoriaFFI::Nulo;
        default:               return CategoriaFFI::Numero;  // numero + entero*/natural*
    }
}

const std::unordered_set<std::string>& tiposPrimitivosGenericos() {
    static const std::unordered_set<std::string> primitivos = {
        "numero", "cadena", "logico", "lista", "dic", "nulo"
    };
    return primitivos;
}
}  // namespace (anon)

// PLAN_GENERICOS.md: nombre de tipo concreto estático de una expresión, usado
// para inferir parámetros genéricos en un sitio de llamada. Cadena vacía si
// la expresión es dinámica (no se puede determinar en compilación) — misma
// filosofía de degradación gradual que tipoDelLiteral (Fase 27).
std::string AnalizadorSemantico::nombreConcretoDeExpr(Expresion* e) {
    if (!e) return "";
    if (dynamic_cast<LitNumero*>(e))          return "numero";
    if (dynamic_cast<LitCadena*>(e))          return "cadena";
    if (dynamic_cast<LitLogico*>(e))          return "logico";
    if (dynamic_cast<LitNulo*>(e))            return "nulo";
    if (dynamic_cast<ListaLiteral*>(e))       return "lista";
    if (dynamic_cast<DiccionarioLiteral*>(e)) return "dic";
    if (auto* nuevo = dynamic_cast<NuevoExpr*>(e)) return nuevo->clase;
    return "";
}

// PLAN_POO.md (Reto 6 / 4.10): nombre de clase estático de 'e' para control
// de acceso. Deliberadamente más limitado que nombreConcretoDeExpr: acá solo
// interesan tipos de OBJETO (nunca "numero"/"cadena"/... -- no tienen campos
// privados), y un Identificador SÍ se resuelve (vía tipoClaseDeVariable),
// porque el caso de uso típico de "objeto.campo" es una variable, no un
// "nuevo Clase()" inline en cada acceso.
std::string AnalizadorSemantico::nombreClaseEstaticaAcceso(Expresion* e) const {
    if (!e) return "";
    if (auto* nuevo = dynamic_cast<NuevoExpr*>(e)) return nuevo->clase;
    if (auto* id = dynamic_cast<Identificador*>(e)) return tipoClaseDeVariable(id->nombre);
    return "";
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
            // PLAN_FFI.md (F4): colisión con un nombre ya declarado en un
            // bloque "externo" -- se detecta acá para cubrir el caso en que
            // el "externo" aparece antes en el archivo; el caso inverso
            // (FuncionDef antes que externo) lo detecta el bucle de abajo.
            if (funcionesExternas.count(f->nombre)) {
                agregarError(f->linea, "'" + f->nombre + "' ya está declarada como función externa");
                continue;
            }
            if (funciones.count(f->nombre)) {
                agregarError(f->linea, "la función '" + f->nombre + "' ya está definida");
                continue;
            }
            InfoFuncion info;
            info.numParametros = f->parametros.size();
            info.variadico = f->variadico;
            info.linea = f->linea;
            info.genericos = f->genericos;
            info.tipoRetorno = f->tipoRetorno;
            info.tipoRetornoClase = f->tipoRetornoClase;
            for (const ParamFuncion& p : f->parametros) {
                info.parametrosTipo.push_back(p.tipo);
                info.parametrosClase.push_back(p.tipoClase);
            }
            funciones[f->nombre] = std::move(info);
        } else if (auto* ext = dynamic_cast<ExternoBloque*>(s.get())) {
            // PLAN_FFI.md (F4): registra cada firma "funcion nombre(...): tipo"
            // del bloque "externo" en su propia tabla, separada de `funciones`.
            for (const FuncionExterna& fe : ext->funciones) {
                if (funciones.count(fe.nombre)) {
                    agregarError(fe.linea, "'" + fe.nombre + "' ya está declarada como función Latino");
                    continue;
                }
                if (funcionesExternas.count(fe.nombre)) {
                    agregarError(fe.linea, "'" + fe.nombre + "' ya está declarada como función externa");
                    continue;
                }
                InfoFuncionExterna info;
                info.tipoRetorno = fe.tipoRetorno;
                info.linea = fe.linea;
                for (const ParamFFI& p : fe.parametros)
                    info.parametrosTipo.push_back(p.tipo);
                funcionesExternas[fe.nombre] = std::move(info);
            }
        }
    }
}

void AnalizadorSemantico::analizarBloque(ListaSent& cuerpo) {
    for (auto& s : cuerpo)
        if (s) s->aceptar(*this);
}

void AnalizadorSemantico::analizarMetodo(MetodoDef& metodo) {
    entrarGenericos(metodo.genericos);  // PLAN_GENERICOS.md: <T> propio del método, además del de la clase

    validarTipoObjeto(metodo.tipoRetorno, metodo.tipoRetornoClase, metodo.linea);

    entrarAmbito();

    std::unordered_set<std::string> vistos;
    for (const ParamFuncion& p : metodo.parametros) {
        if (!vistos.insert(p.nombre).second)
            agregarError(metodo.linea,
                         "parámetro duplicado '" + p.nombre + "' en el método '" + metodo.nombre + "'");
        validarTipoObjeto(p.tipo, p.tipoClase, metodo.linea);
        declararVariable(p.nombre, p.tipo, metodo.linea);
        // PLAN_POO.md (Reto 6): un parámetro "p: Perro" tiene tipo estático
        // conocido durante todo el cuerpo del método -- habilita el control
        // de acceso para "p.campo" sin necesitar ningún "nuevo" inline.
        if (p.tipo == TipoAnotado::Objeto) registrarClaseVariable(p.nombre, p.tipoClase);
    }

    ++profundidadFuncion;
    analizarBloque(metodo.cuerpo);
    --profundidadFuncion;

    salirAmbito();
    salirGenericos();
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
    if (tipo != TipoAnotado::Objeto) return;
    if (esGenericoActivo(clase)) return;  // PLAN_GENERICOS.md: "T" en su propio ámbito
    if (!estaTipoDefinido(clase))
        agregarError(linea, "tipo de objeto desconocido '" + clase + "'");
}

// ---------------------------------------------------------------------------
// Genéricos (PLAN_GENERICOS.md)
// ---------------------------------------------------------------------------
void AnalizadorSemantico::entrarGenericos(const std::vector<ParametroGenerico>& genericos) {
    std::unordered_map<std::string, ParametroGenerico> mapa;
    for (const ParametroGenerico& g : genericos) {
        for (const std::string& b : g.bounds) {
            if (!estaTipoDefinido(b))
                agregarError(g.linea, "restricción genérica desconocida '" + b +
                                          "' en el parámetro '" + g.nombre + "'");
            else if (obtenerTipo(b)->tipo != TipoInfoKind::Interfaz)
                agregarError(g.linea, "'" + b +
                                          "' no es una interfaz; las restricciones genéricas solo pueden ser interfaces");
        }
        mapa[g.nombre] = g;
    }
    genericosActivos.push_back(std::move(mapa));
}

void AnalizadorSemantico::salirGenericos() {
    if (!genericosActivos.empty())
        genericosActivos.pop_back();
}

bool AnalizadorSemantico::esGenericoActivo(const std::string& nombre) const {
    for (const auto& mapa : genericosActivos)
        if (mapa.count(nombre))
            return true;
    return false;
}

bool AnalizadorSemantico::tipoImplementaInterfaz(const std::string& nombreTipo,
                                                 const std::string& interfaz) const {
    const InfoTipo* t = obtenerTipo(nombreTipo);
    while (t) {
        for (const std::string& i : t->interfaces)
            if (i == interfaz) return true;
        if (t->padre.empty()) break;
        t = obtenerTipo(t->padre);
    }
    return false;
}

// PLAN_POO.md (Reto 6 / 4.10): control de acceso "mejor esfuerzo" en
// compilación. Busca 'miembro' en 'tipoObjeto' y su cadena de herencia; si
// no aparece en ningún nivel, no hace nada (esta función no valida
// existencia de miembros -- un miembro desconocido cae, como siempre, en el
// despacho dinámico de runtime, lat_obj_get/lat_obj_llamar_metodo). Si
// aparece, compara el modificador de acceso contra 'tipoActual' (la
// clase/estructura cuyo método se está analizando en este momento, "" si
// estamos fuera de cualquier método).
void AnalizadorSemantico::verificarAccesoMiembro(const std::string& tipoObjeto,
                                                 const std::string& miembro, int linea) {
    std::string declarante;
    ModificadorAcceso acceso = ModificadorAcceso::Publico;
    bool esMetodo = false;
    bool encontrado = false;

    for (std::string nivel = tipoObjeto; !nivel.empty();) {
        const InfoTipo* t = obtenerTipo(nivel);
        if (!t) break;
        auto itCampo = t->campos.find(miembro);
        if (itCampo != t->campos.end()) {
            acceso = itCampo->second;
            declarante = nivel;
            encontrado = true;
            break;
        }
        auto itMetodo = t->metodos.find(miembro);
        if (itMetodo != t->metodos.end()) {
            acceso = itMetodo->second.acceso;
            declarante = nivel;
            esMetodo = true;
            encontrado = true;
            break;
        }
        nivel = t->padre;
    }

    if (!encontrado || acceso == ModificadorAcceso::Publico) return;

    const char* nombreMiembro = esMetodo ? "método" : "campo";

    if (acceso == ModificadorAcceso::Privado) {
        if (tipoActual != declarante)
            agregarError(linea, std::string(nombreMiembro) + " privado '" + miembro +
                                     "' no accesible fuera de '" + declarante + "'");
        return;
    }

    // Protegido: 'tipoActual' debe ser 'declarante' o una subclase de él.
    bool permitido = false;
    for (std::string nivel = tipoActual; !nivel.empty();) {
        if (nivel == declarante) {
            permitido = true;
            break;
        }
        const InfoTipo* t = obtenerTipo(nivel);
        nivel = t ? t->padre : "";
    }
    if (!permitido)
        agregarError(linea, std::string(nombreMiembro) + " protegido '" + miembro +
                                 "' no accesible fuera de '" + declarante +
                                 "' o sus subclases");
}

void AnalizadorSemantico::validarBoundConcreto(const std::string& concreto,
                                               const ParametroGenerico& g, int linea) {
    if (g.bounds.empty() || concreto.empty()) return;  // sin bound, o tipo dinámico: sin chequeo (gradual)
    if (tiposPrimitivosGenericos().count(concreto)) {
        std::string listaBounds;
        for (const std::string& b : g.bounds)
            listaBounds += (listaBounds.empty() ? "" : " + ") + b;
        agregarError(linea, "los tipos primitivos no pueden satisfacer la restricción genérica '" +
                                 listaBounds + "' (parámetro '" + g.nombre + "')");
        return;
    }
    if (!estaTipoDefinido(concreto)) return;  // ya se reporta "tipo desconocido" en otro lado
    for (const std::string& bound : g.bounds) {
        if (!tipoImplementaInterfaz(concreto, bound))
            agregarError(linea, "'" + concreto + "' no implementa '" + bound +
                                     "', requerido por el parámetro genérico '" + g.nombre + "'");
    }
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
            info.genericos = c->genericos;
            info.linea = c->linea;

            std::unordered_set<std::string> nombresCampos;
            std::unordered_set<std::string> nombresMetodos;
            bool tieneConstructor = false;

            for (const CampoDef& campo : c->campos) {
                if (!nombresCampos.insert(campo.nombre).second)
                    agregarError(campo.linea,
                                 "campo duplicado '" + campo.nombre + "' en la clase '" + c->nombre + "'");
                info.campos[campo.nombre] = campo.acceso;
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
                infoMetodo.acceso = metodo.acceso;
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
            info.genericos = e->genericos;
            info.linea = e->linea;

            std::unordered_set<std::string> nombresCampos;
            std::unordered_set<std::string> nombresMetodos;
            bool tieneConstructor = false;

            for (const CampoDef& campo : e->campos) {
                if (!nombresCampos.insert(campo.nombre).second)
                    agregarError(campo.linea,
                                 "campo duplicado '" + campo.nombre + "' en la estructura '" + e->nombre + "'");
                info.campos[campo.nombre] = campo.acceso;
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
                infoMetodo.acceso = metodo.acceso;
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
            info.genericos = i->genericos;
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
                infoMetodo.acceso = metodo.acceso;
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

    // PLAN_POO.md (Reto 6 / 4.10): control de acceso "mejor esfuerzo". 'este.X'
    // siempre está permitido (se está, por definición, dentro de un método de
    // la propia clase o de una subclase) -- no hace falta ni siquiera resolver
    // un tipo estático para ese caso. Para el resto, solo se verifica cuando
    // el tipo estático del objeto se puede determinar (ver
    // nombreClaseEstaticaAcceso); si es dinámico, se degrada sin chequeo,
    // igual filosofía que el resto del tipado gradual.
    if (dynamic_cast<AccesoEste*>(n.objeto.get())) return;
    std::string tipoObjeto = nombreClaseEstaticaAcceso(n.objeto.get());
    if (!tipoObjeto.empty())
        verificarAccesoMiembro(tipoObjeto, n.miembro, n.linea);
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
            bool aridadOk = true;
            if (info.variadico) {
                if (nargs < info.numParametros) {
                    aridadOk = false;
                    agregarError(id->linea,
                                 "la función '" + nombre + "' requiere al menos " +
                                     std::to_string(info.numParametros) +
                                     " argumento(s), se pasaron " + std::to_string(nargs));
                }
            } else if (nargs != info.numParametros) {
                aridadOk = false;
                agregarError(id->linea,
                             "la función '" + nombre + "' espera " +
                                 std::to_string(info.numParametros) +
                                 " argumento(s), se pasaron " + std::to_string(nargs));
            }

            // PLAN_GENERICOS.md: inferencia + chequeo de bounds en el sitio de
            // llamada de una función genérica ("identidad(5)" / turbofish
            // "identidad::<numero>(5)"). No se intenta si la aridad ya falló.
            if (aridadOk && !info.genericos.empty()) {
                std::unordered_map<std::string, std::string> sustitucion;
                auto esNombreGenerico = [&](const std::string& nombreTipo) {
                    for (const ParametroGenerico& g : info.genericos)
                        if (g.nombre == nombreTipo) return true;
                    return false;
                };

                if (!n.tipoArgsExplicitos.empty()) {
                    // Turbofish: sin inferencia, mapeo directo por posición.
                    if (n.tipoArgsExplicitos.size() != info.genericos.size()) {
                        agregarError(id->linea,
                                     "'" + nombre + "' espera " + std::to_string(info.genericos.size()) +
                                         " argumento(s) de tipo genérico, se dieron " +
                                         std::to_string(n.tipoArgsExplicitos.size()));
                    } else {
                        for (size_t i = 0; i < info.genericos.size(); i++)
                            sustitucion[info.genericos[i].nombre] = n.tipoArgsExplicitos[i];
                    }
                } else {
                    for (size_t i = 0; i < info.parametrosTipo.size() && i < n.argumentos.size(); i++) {
                        if (info.parametrosTipo[i] != TipoAnotado::Objeto) continue;
                        const std::string& nombreParam = info.parametrosClase[i];
                        if (!esNombreGenerico(nombreParam)) continue;
                        std::string concreto = nombreConcretoDeExpr(n.argumentos[i].get());
                        if (concreto.empty()) {
                            // Argumento identificador ya anotado con un tipo primitivo
                            // (p.ej. "x: numero = 5"): la anotación de la variable
                            // también sirve para inferir. Los identificadores de tipo
                            // Objeto no se pueden usar aquí porque Asignacion no
                            // guarda el nombre de clase de una variable anotada como
                            // tipo de objeto (limitación preexistente a este plan).
                            if (auto* idArg = dynamic_cast<Identificador*>(n.argumentos[i].get())) {
                                TipoAnotado t = tipoDeVariable(idArg->nombre);
                                if (t != TipoAnotado::Ninguno && t != TipoAnotado::Objeto)
                                    concreto = nombreTipoAnotado(t);
                            }
                        }
                        if (concreto.empty()) continue;  // dinámico: no se puede inferir, se degrada (gradual)
                        auto itSust = sustitucion.find(nombreParam);
                        if (itSust == sustitucion.end())
                            sustitucion[nombreParam] = concreto;
                        else if (itSust->second != concreto)
                            agregarError(id->linea,
                                         "el tipo genérico '" + nombreParam + "' se infirió como '" +
                                             itSust->second + "' pero este argumento es de tipo '" +
                                             concreto + "'");
                    }
                    if (info.tipoRetorno == TipoAnotado::Objeto &&
                        esNombreGenerico(info.tipoRetornoClase) &&
                        !sustitucion.count(info.tipoRetornoClase)) {
                        agregarError(id->linea,
                                     "no se puede inferir el tipo genérico '" + info.tipoRetornoClase +
                                         "' de '" + nombre + "'; usá '" + nombre + "::<Tipo>(...)'");
                    }
                }

                for (const ParametroGenerico& g : info.genericos) {
                    auto itSust = sustitucion.find(g.nombre);
                    if (itSust != sustitucion.end())
                        validarBoundConcreto(itSust->second, g, id->linea);
                }
            }
        } else if (auto itExt = funcionesExternas.find(nombre); itExt != funcionesExternas.end()) {
            // PLAN_FFI.md (F4): llamada a una función "externo".
            const InfoFuncionExterna& info = itExt->second;

            if (profundidadInseguro == 0)
                agregarError(id->linea,
                             "llamada a función externa '" + nombre + "' fuera de un bloque 'inseguro'");

            size_t nargs = n.argumentos.size();
            if (nargs != info.parametrosTipo.size()) {
                agregarError(id->linea,
                             "número de argumentos incorrecto para la función externa '" + nombre +
                                 "': se esperaban " + std::to_string(info.parametrosTipo.size()) +
                                 ", se recibieron " + std::to_string(nargs));
            } else {
                for (size_t i = 0; i < nargs; i++) {
                    CategoriaFFI catEsperada = categoriaDeTipoFFI(info.parametrosTipo[i]);

                    TipoAnotado real = tipoDelLiteral(n.argumentos[i].get());
                    if (real == TipoAnotado::Ninguno) {
                        if (auto* idArg = dynamic_cast<Identificador*>(n.argumentos[i].get()))
                            real = tipoDeVariable(idArg->nombre);
                    }
                    if (real == TipoAnotado::Ninguno)
                        continue;  // dinámico: no se puede verificar en compilación (se difiere a runtime)

                    // "nulo" literal es un puntero nulo válido para un parámetro
                    // 'puntero' (ver ejemplo "MessageBoxA(nulo, ...)" de
                    // "Sintaxis propuesta" y hallazgo de F4 en PLAN_FFI.md).
                    if (real == TipoAnotado::Nulo && catEsperada == CategoriaFFI::Puntero)
                        continue;

                    bool compatible =
                        (catEsperada == CategoriaFFI::Numero && real == TipoAnotado::Numero) ||
                        (catEsperada == CategoriaFFI::Logico && real == TipoAnotado::Logico) ||
                        (catEsperada == CategoriaFFI::Cadena && real == TipoAnotado::Cadena) ||
                        (catEsperada == CategoriaFFI::Nulo && real == TipoAnotado::Nulo);
                    if (!compatible) {
                        agregarError(id->linea,
                                     "tipo incompatible: el argumento " + std::to_string(i + 1) +
                                         " de la función externa '" + nombre + "' espera '" +
                                         nombreTipoFFI(info.parametrosTipo[i]) +
                                         "' pero se pasó un valor de tipo '" +
                                         nombreTipoAnotado(real) + "'");
                    }
                }
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

    // PLAN_GENERICOS.md: "nuevo Pila<numero>()". Sin argumentos de tipo, se
    // permite igual (filosofía gradual: sin especificar, no hay chequeo de
    // bounds) siempre que la clase sea genérica.
    if (!n.tipoArgs.empty()) {
        if (n.tipoArgs.size() != tipo->genericos.size()) {
            agregarError(n.linea, "'" + n.clase + "' espera " +
                                       std::to_string(tipo->genericos.size()) +
                                       " argumento(s) de tipo genérico (<...>), se dieron " +
                                       std::to_string(n.tipoArgs.size()));
        } else {
            for (size_t i = 0; i < tipo->genericos.size(); i++)
                validarBoundConcreto(n.tipoArgs[i], tipo->genericos[i], n.linea);
        }
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

    entrarGenericos(n.genericos);  // PLAN_GENERICOS.md: <T> visible en campos y métodos

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

    salirGenericos();
}

void AnalizadorSemantico::visitar(EstructuraDef& n) {
    const InfoTipo* tipo = obtenerTipo(n.nombre);
    if (!tipo) return;

    entrarGenericos(n.genericos);  // PLAN_GENERICOS.md: <T> visible en campos y métodos

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

    salirGenericos();
}

void AnalizadorSemantico::visitar(InterfazDef& n) {
    entrarGenericos(n.genericos);  // PLAN_GENERICOS.md: <T> visible en las firmas de métodos
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
    salirGenericos();
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
            // PLAN_POO.md (Reto 6): rastreo "mejor esfuerzo" del tipo estático
            // de una variable a partir de su última asignación vista --
            // "p = nuevo Perro(...)" habilita el control de acceso para
            // "p.campo" más adelante en el mismo ámbito. Cualquier otra
            // asignación (incluida una anotación de tipo sin valor "nuevo")
            // borra el dato en vez de arriesgar un hint incorrecto.
            std::string claseValor;
            if (i < n.valores.size())
                if (auto* nuevo = dynamic_cast<NuevoExpr*>(n.valores[i].get()))
                    claseValor = nuevo->clase;
            registrarClaseVariable(id->nombre, claseValor);
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
    entrarGenericos(n.genericos);  // PLAN_GENERICOS.md
    validarTipoObjeto(n.tipoRetorno, n.tipoRetornoClase, n.linea);
    entrarAmbito();

    std::unordered_set<std::string> vistos;
    for (const ParamFuncion& p : n.parametros) {
        if (!vistos.insert(p.nombre).second)
            agregarError(n.linea, "parámetro duplicado '" + p.nombre + "' en la función '" +
                                      n.nombre + "'");
        validarTipoObjeto(p.tipo, p.tipoClase, n.linea);
        declararVariable(p.nombre, p.tipo, n.linea);
        if (p.tipo == TipoAnotado::Objeto) registrarClaseVariable(p.nombre, p.tipoClase);  // PLAN_POO.md (Reto 6)
    }

    ++profundidadFuncion;
    if (n.variadico) ++profundidadVariadica;
    if (n.inseguro) ++profundidadInseguro;  // PLAN_FFI.md (F4): "funcion inseguro nombre(...)"
    analizarBloque(n.cuerpo);
    if (n.inseguro) --profundidadInseguro;
    if (n.variadico) --profundidadVariadica;
    --profundidadFuncion;

    salirAmbito();
    salirGenericos();
}

// PLAN_FFI.md (F4): "inseguro ... fin" -- análogo a "unsafe { }" en Rust,
// habilita llamar funciones "externo" dentro de su cuerpo (y de cualquier
// bloque normal anidado adentro, igual que profundidadFuncion/
// profundidadVariadica).
void AnalizadorSemantico::visitar(InseguroBloque& n) {
    ++profundidadInseguro;
    analizarBloque(n.cuerpo);
    --profundidadInseguro;
}

void AnalizadorSemantico::visitar(Retornar& n) {
    if (profundidadFuncion == 0)
        agregarError(n.linea, "'retornar' fuera de una función");
    if (n.valor) n.valor->aceptar(*this);
}
