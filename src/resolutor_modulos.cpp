// resolutor_modulos.cpp
//
// Ver resolutor_modulos.h y PLAN_MODULOS.md (M3: resolución de un solo
// módulo; M4: resolución multi-módulo con "importar { ... } desde ...";
// M5: "importar * como ns desde ..." e "importar Nombre desde ..." con
// "exportar por defecto").

#include "resolutor_modulos.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "lexer.h"
#include "parser.h"

namespace fs = std::filesystem;

namespace {

// --- Recolección de nombres locales dentro de un cuerpo de función/método -
//
// Latino no tiene ámbito de bloque (ver AnalizadorSemantico::analizarBloque):
// toda asignación dentro de una función, sin importar cuán anidada esté en
// si/desde/mientras/repetir/elegir, declara una variable local a la función
// completa (no hay 'global' implementado, ver PLAN_MODULOS.md). Por eso esta
// recolección "levanta" (hoisting) todos los destinos de asignación del
// cuerpo entero antes de decidir qué identificadores están sombreados,
// deteniéndose en el límite de una función/método/tipo anidado (que tiene su
// propio ámbito, resuelto por separado cuando el recorrido llegue a él).
void recolectarLocales(const ListaSent& cuerpo, std::unordered_set<std::string>& out);

void recolectarLocalesDeSentencia(const Sentencia* s, std::unordered_set<std::string>& out) {
    if (!s) return;

    if (auto* a = dynamic_cast<const Asignacion*>(s)) {
        for (auto& d : a->destinos)
            if (auto* id = dynamic_cast<const Identificador*>(d.get()))
                out.insert(id->nombre);
    } else if (auto* si = dynamic_cast<const Si*>(s)) {
        recolectarLocales(si->entonces, out);
        for (auto& rama : si->osis)
            recolectarLocales(rama.cuerpo, out);
        recolectarLocales(si->sino, out);
    } else if (auto* el = dynamic_cast<const Elegir*>(s)) {
        for (auto& c : el->casos)
            recolectarLocales(c.cuerpo, out);
        recolectarLocales(el->defecto, out);
    } else if (auto* d = dynamic_cast<const Desde*>(s)) {
        recolectarLocalesDeSentencia(d->inicio.get(), out);
        recolectarLocales(d->cuerpo, out);
    } else if (auto* m = dynamic_cast<const Mientras*>(s)) {
        recolectarLocales(m->cuerpo, out);
    } else if (auto* r = dynamic_cast<const Repetir*>(s)) {
        recolectarLocales(r->cuerpo, out);
    }
    // FuncionDef/ClaseDef/EstructuraDef/InterfazDef anidados (si el lenguaje
    // llegara a admitirlos): ámbito propio, no se hereda hacia arriba.
}

void recolectarLocales(const ListaSent& cuerpo, std::unordered_set<std::string>& out) {
    for (auto& s : cuerpo)
        recolectarLocalesDeSentencia(s.get(), out);
}

// --- M5: import de espacio de nombres ("importar * como ns desde ...") -----
//
// Un namespace importado no crea bindings de nombre por nombre en
// renombres_: en cambio, "ns.X" (un AccesoMiembro cuyo objeto es el
// Identificador "ns") se reescribe directamente al nombre interno de "X" en
// la tabla de exportación del módulo referenciado. rutaMostrada es la ruta
// tal como se escribió en el "importar", usada solo para mensajes de error.
struct NamespaceImport {
    const ResolutorModulos::TablaExportacion* tabla;
    std::string rutaMostrada;
};

// --- Reescritura de referencias internas -----------------------------------
//
// Recorre todo el árbol reescribiendo:
//  - Identificador: solo si no está sombreado por un parámetro/variable local
//    de la función/método que lo contiene (ver recolectarLocales arriba).
//  - Nombres de tipo por string (NuevoExpr::clase, EsExpr::clase,
//    ClaseDef::padre/interfaces, CampoDef::tipoClase,
//    FuncionDef/MetodoDef::tipoRetornoClase, ParamFuncion::tipoClase): estos
//    nunca están sombreados por variables locales (son un espacio de nombres
//    distinto), así que se reescriben con una simple búsqueda en la tabla.
//  - "ns.X" (ver NamespaceImport arriba): reemplaza el nodo AccesoMiembro
//    completo por un Identificador con el nombre interno de X. Requiere que
//    los métodos que recorren un ExprPtr propio (no solo un Expresion&) usen
//    visitarExpr en vez de "->aceptar(*this)" directamente, porque
//    reemplazar el nodo exige reasignar el unique_ptr del padre.
class ReescritorReferencias : public Visitante {
public:
    explicit ReescritorReferencias(const std::unordered_map<std::string, std::string>& renombres,
                                    std::unordered_map<std::string, NamespaceImport> namespaces = {})
        : renombres_(renombres), namespaces_(std::move(namespaces)) {}

    bool tuvoError() const { return error_; }

    void visitar(Programa& n) override {
        for (auto& s : n.sentencias)
            if (s) s->aceptar(*this);
    }

    // --- Expresiones sin hijos ---
    void visitar(LitNumero&) override {}
    void visitar(LitCadena&) override {}
    void visitar(LitLogico&) override {}
    void visitar(LitNulo&) override {}
    void visitar(VarArgs&) override {}
    void visitar(AccesoEste&) override {}

    void visitar(Identificador& n) override { renombrarUso(n.nombre); }

    void visitar(Binaria& n) override {
        visitarExpr(n.izq);
        visitarExpr(n.der);
    }
    void visitar(Unaria& n) override { visitarExpr(n.operando); }
    void visitar(PostOperador& n) override { visitarExpr(n.operando); }
    void visitar(Ternaria& n) override {
        visitarExpr(n.condicion);
        visitarExpr(n.siCierto);
        visitarExpr(n.siFalso);
    }
    void visitar(AccesoIndice& n) override {
        visitarExpr(n.objeto);
        visitarExpr(n.indice);
    }
    // n.miembro es un nombre de campo/método por-instancia, no un nombre de
    // nivel superior: nunca se reescribe. El caso "ns.X" se resuelve antes de
    // llegar acá, en visitarExpr del padre que contiene este AccesoMiembro.
    void visitar(AccesoMiembro& n) override { visitarExpr(n.objeto); }
    void visitar(Llamada& n) override {
        visitarExpr(n.destino);
        for (auto& a : n.argumentos)
            visitarExpr(a);
    }
    void visitar(ListaLiteral& n) override {
        for (auto& e : n.elementos)
            visitarExpr(e);
    }
    void visitar(DiccionarioLiteral& n) override {
        for (auto& p : n.pares) {
            visitarExpr(p.clave);
            visitarExpr(p.valor);
        }
    }
    void visitar(NuevoExpr& n) override {
        renombrarTipo(n.clase);
        for (auto& a : n.argumentos)
            visitarExpr(a);
    }
    void visitar(EsExpr& n) override {
        visitarExpr(n.objeto);
        renombrarTipo(n.clase);
    }

    // --- Sentencias ---
    void visitar(Incluir&) override {}
    void visitar(ImportarDecl&) override {}
    void visitar(ExportarDesde&) override {}

    void visitar(Asignacion& n) override {
        for (auto& v : n.valores)
            visitarExpr(v);
        for (auto& d : n.destinos) {
            if (dynamic_cast<Identificador*>(d.get()))
                continue;  // declaración/escritura de variable: no es un "uso" a reescribir
            visitarExpr(d);
        }
    }
    void visitar(ExprSentencia& n) override { visitarExpr(n.expr); }
    void visitar(Si& n) override {
        visitarExpr(n.condicion);
        visitarBloque(n.entonces);
        for (auto& rama : n.osis) {
            visitarExpr(rama.condicion);
            visitarBloque(rama.cuerpo);
        }
        visitarBloque(n.sino);
    }
    void visitar(Elegir& n) override {
        visitarExpr(n.opcion);
        for (auto& c : n.casos) {
            visitarExpr(c.valor);
            visitarBloque(c.cuerpo);
        }
        visitarBloque(n.defecto);
    }
    void visitar(Desde& n) override {
        if (n.inicio) n.inicio->aceptar(*this);
        visitarExpr(n.condicion);
        if (n.incremento) n.incremento->aceptar(*this);
        visitarBloque(n.cuerpo);
    }
    void visitar(Mientras& n) override {
        visitarExpr(n.condicion);
        visitarBloque(n.cuerpo);
    }
    void visitar(Repetir& n) override {
        visitarBloque(n.cuerpo);
        visitarExpr(n.condicionHasta);
    }
    void visitar(Romper&) override {}
    void visitar(Retornar& n) override { visitarExpr(n.valor); }
    void visitar(LlamadaBase& n) override {
        for (auto& a : n.argumentos)
            visitarExpr(a);
    }

    void visitar(FuncionDef& n) override {
        visitarCuerpoFuncion(n.parametros, n.tipoRetorno, n.tipoRetornoClase, n.cuerpo);
    }

    void visitar(ClaseDef& n) override {
        renombrarTipo(n.padre);
        for (auto& iface : n.interfaces)
            renombrarTipo(iface);
        for (auto& campo : n.campos)
            visitarCampo(campo);
        for (auto& metodo : n.metodos)
            visitarMetodo(metodo);
    }
    void visitar(EstructuraDef& n) override {
        for (auto& campo : n.campos)
            visitarCampo(campo);
        for (auto& metodo : n.metodos)
            visitarMetodo(metodo);
    }
    void visitar(InterfazDef& n) override {
        for (auto& metodo : n.metodos)
            visitarMetodo(metodo);
    }

private:
    const std::unordered_map<std::string, std::string>& renombres_;
    std::unordered_map<std::string, NamespaceImport> namespaces_;
    mutable bool error_ = false;
    // Pila de ámbitos de función/método activos (nombres de parámetros +
    // locales "levantados"). Latino no anida funciones, así que en la
    // práctica nunca crece más allá de profundidad 1, pero se maneja como
    // pila por generalidad (y porque los métodos de una clase se visitan
    // dentro del recorrido general, no en un nivel "especial").
    std::vector<std::unordered_set<std::string>> sombreado_;

    bool estaSombreado(const std::string& nombre) const {
        for (const auto& ambito : sombreado_)
            if (ambito.count(nombre))
                return true;
        return false;
    }

    void reportarError(const std::string& mensaje) const {
        std::cerr << "Error: " << mensaje << "\n";
        error_ = true;
    }

    void renombrarUso(std::string& nombre) const {
        if (estaSombreado(nombre)) return;
        if (namespaces_.count(nombre)) {
            reportarError("'" + nombre + "' no es un import de espacio de nombres");
            return;
        }
        auto it = renombres_.find(nombre);
        if (it != renombres_.end()) nombre = it->second;
    }

    // Nombres de tipo (clases/estructuras/interfaces): espacio de nombres
    // distinto al de variables, nunca sombreado por un parámetro/local.
    void renombrarTipo(std::string& nombre) const {
        if (nombre.empty()) return;
        auto it = renombres_.find(nombre);
        if (it != renombres_.end()) nombre = it->second;
    }

    // Recorre una expresión propia de un padre (una ranura ExprPtr, no solo
    // el Expresion apuntado), permitiendo reemplazar el nodo completo cuando
    // resulta ser "ns.X" (ver NamespaceImport arriba). Todo recorrido de un
    // campo ExprPtr en esta clase debe pasar por acá en vez de llamar
    // "->aceptar(*this)" directamente, para que el reemplazo alcance
    // cualquier profundidad del árbol.
    void visitarExpr(ExprPtr& e) {
        if (!e) return;
        if (auto* am = dynamic_cast<AccesoMiembro*>(e.get())) {
            if (auto* id = dynamic_cast<Identificador*>(am->objeto.get())) {
                if (!estaSombreado(id->nombre)) {
                    auto itNs = namespaces_.find(id->nombre);
                    if (itNs != namespaces_.end()) {
                        auto itMiembro = itNs->second.tabla->find(am->miembro);
                        if (itMiembro == itNs->second.tabla->end()) {
                            reportarError("'" + id->nombre + "." + am->miembro +
                                          "' no esta exportado por '" +
                                          itNs->second.rutaMostrada + "'");
                            return;
                        }
                        auto nuevo = std::make_unique<Identificador>();
                        nuevo->nombre = itMiembro->second;
                        nuevo->linea = e->linea;
                        e = std::move(nuevo);
                        return;
                    }
                }
            }
        }
        e->aceptar(*this);
    }

    void visitarBloque(ListaSent& cuerpo) {
        for (auto& s : cuerpo)
            if (s) s->aceptar(*this);
    }

    void visitarCuerpoFuncion(std::vector<ParamFuncion>& parametros, TipoAnotado tipoRetorno,
                               std::string& tipoRetornoClase, ListaSent& cuerpo) {
        std::unordered_set<std::string> locales;
        for (auto& p : parametros)
            locales.insert(p.nombre);
        recolectarLocales(cuerpo, locales);
        sombreado_.push_back(std::move(locales));

        for (auto& p : parametros)
            if (p.tipo == TipoAnotado::Objeto) renombrarTipo(p.tipoClase);
        if (tipoRetorno == TipoAnotado::Objeto) renombrarTipo(tipoRetornoClase);

        visitarBloque(cuerpo);

        sombreado_.pop_back();
    }

    void visitarMetodo(MetodoDef& m) {
        visitarCuerpoFuncion(m.parametros, m.tipoRetorno, m.tipoRetornoClase, m.cuerpo);
    }

    void visitarCampo(CampoDef& campo) {
        if (campo.tipoAnotado == TipoAnotado::Objeto)
            renombrarTipo(campo.tipoClase);
        visitarExpr(campo.valorDefecto);
    }
};

// --- Manglado de las declaraciones propias de un módulo --------------------
//
// Extraído de lo que antes era el cuerpo de resolverModuloUnico (M3), sin la
// pasada final de ReescritorReferencias: M4 necesita fusionar estos
// renombres con los que produce cada "importar" ANTES de reescribir
// referencias, para que una sola pasada de ReescritorReferencias vea el
// mapa completo (mismo respeto de sombreado para ambos orígenes de
// renombre). resolverModuloUnico (sin imports) sigue siendo, en esencia,
// "manglar + reescribir" en un solo paso.
struct DeclaracionesPropias {
    std::unordered_map<std::string, std::string> renombres;
    ResolutorModulos::TablaExportacion tabla;
};

DeclaracionesPropias manglarDeclaracionesPropias(Programa& programa, const std::string& rutaCanonica) {
    DeclaracionesPropias resultado;
    auto& renombres = resultado.renombres;
    auto& tabla = resultado.tabla;
    const std::string slug = ResolutorModulos::slugDesdeRuta(rutaCanonica);

    auto manglar = [&](const std::string& nombre) {
        return "__mod_" + slug + "__" + nombre;
    };
    // Registra el renombre y, si corresponde, la entrada de la tabla de
    // exportación (clave especial "__defecto__" para "exportar por defecto",
    // ver PLAN_MODULOS.md).
    auto registrar = [&](const std::string& nombreOriginal, const std::string& nombreNuevo,
                          bool exportado, bool esDefecto) {
        renombres[nombreOriginal] = nombreNuevo;
        if (esDefecto) tabla["__defecto__"] = nombreNuevo;
        else if (exportado) tabla[nombreOriginal] = nombreNuevo;
    };

    for (auto& s : programa.sentencias) {
        Sentencia* sp = s.get();
        if (auto* f = dynamic_cast<FuncionDef*>(sp)) {
            std::string nuevo = manglar(f->nombre);
            registrar(f->nombre, nuevo, f->exportado, f->esDefecto);
            f->nombre = nuevo;
            f->exportado = f->esDefecto = false;  // paso 8: declaración "desnuda"
        } else if (auto* c = dynamic_cast<ClaseDef*>(sp)) {
            std::string nuevo = manglar(c->nombre);
            registrar(c->nombre, nuevo, c->exportado, c->esDefecto);
            c->nombre = nuevo;
            c->exportado = c->esDefecto = false;
            // AnalizadorSemantico ubica al constructor buscando, dentro del
            // mapa de métodos del tipo, la clave igual al nombre (ya
            // renombrado) de la clase (ver "metodos.find(n.clase)" /
            // "metodos.find(actual->padre)" en visitar(NuevoExpr&)/
            // visitar(LlamadaBase&)) -- no usa el flag esConstructor para
            // esa búsqueda puntual, así que el nombre del método
            // constructor debe seguir coincidiendo con el nombre (nuevo)
            // de su clase.
            for (auto& m : c->metodos)
                if (m.esConstructor) m.nombre = nuevo;
        } else if (auto* e = dynamic_cast<EstructuraDef*>(sp)) {
            std::string nuevo = manglar(e->nombre);
            registrar(e->nombre, nuevo, e->exportado, e->esDefecto);
            e->nombre = nuevo;
            e->exportado = e->esDefecto = false;
            for (auto& m : e->metodos)
                if (m.esConstructor) m.nombre = nuevo;
        } else if (auto* i = dynamic_cast<InterfazDef*>(sp)) {
            std::string nuevo = manglar(i->nombre);
            registrar(i->nombre, nuevo, i->exportado, i->esDefecto);
            i->nombre = nuevo;
            i->exportado = i->esDefecto = false;
        } else if (auto* a = dynamic_cast<Asignacion*>(sp)) {
            for (auto& destino : a->destinos) {
                auto* id = dynamic_cast<Identificador*>(destino.get());
                if (!id) continue;
                std::string nuevo = manglar(id->nombre);
                registrar(id->nombre, nuevo, a->exportado, a->esDefecto);
                id->nombre = nuevo;
            }
            a->exportado = a->esDefecto = false;
        }
    }

    return resultado;
}

// --- M4: resolución multi-módulo (DFS con memoización) ----------------------
//
// Recorre el grafo de módulos referenciado por "importar"/"exportar ...
// desde" a partir del archivo de entrada. Cada módulo se identifica por su
// ruta canónica (absoluta, normalizada léxicamente); se parsea y procesa una
// sola vez (memoización en estados_), en orden de dependencias (DFS
// post-order: un módulo se agrega a dependencias_ recién cuando todos los
// módulos que él mismo importa ya fueron agregados). Un módulo que sigue "en
// proceso" (todavía en pila_) al ser revisitado es un import circular.
class ResolutorProyecto {
public:
    // Devuelve nullptr en error (ya reportado por stderr).
    std::unique_ptr<Programa> resolver(std::unique_ptr<Programa> entrada,
                                        const std::string& rutaEntrada) {
        std::string rutaEntradaCanon = normalizar(rutaEntrada);

        estados_[rutaEntradaCanon].enProceso = true;
        pila_.push_back(rutaEntradaCanon);

        ResolutorModulos::TablaExportacion tablaEntrada = procesarModulo(*entrada, rutaEntradaCanon);

        pila_.pop_back();
        estados_[rutaEntradaCanon].enProceso = false;

        if (!ok_) return nullptr;

        estados_[rutaEntradaCanon].listo = true;
        estados_[rutaEntradaCanon].exportaciones = std::move(tablaEntrada);

        auto resultado = std::make_unique<Programa>();
        resultado->sentencias = std::move(dependencias_);
        for (auto& s : entrada->sentencias)
            resultado->sentencias.push_back(std::move(s));
        return resultado;
    }

private:
    struct EstadoModulo {
        bool enProceso = false;
        bool listo = false;
        ResolutorModulos::TablaExportacion exportaciones;
    };

    std::unordered_map<std::string, EstadoModulo> estados_;
    std::vector<std::string> pila_;  // rutas canónicas en el DFS activo (mensaje de ciclo)
    ListaSent dependencias_;         // sentencias de módulos ya resueltos, en orden de dependencia
    bool ok_ = true;

    static std::string normalizar(const std::string& ruta) {
        return fs::path(ruta).lexically_normal().generic_string();
    }

    // Mangla las declaraciones propias de `programa`, resuelve sus
    // "importar"/"exportar ... desde" (fusionando esos renombres con los
    // propios) y reescribe TODAS las referencias en una sola pasada. Elimina
    // del árbol los nodos ya resueltos (paso 8 del algoritmo). Devuelve la
    // tabla de exportación propia (sin entradas de re-export, ver M7). En
    // error, ok_ pasa a false (el valor de retorno se descarta).
    ResolutorModulos::TablaExportacion procesarModulo(Programa& programa,
                                                        const std::string& rutaCanonica) {
        DeclaracionesPropias propias = manglarDeclaracionesPropias(programa, rutaCanonica);
        fs::path dirBase = fs::path(rutaCanonica).parent_path();
        std::unordered_map<std::string, NamespaceImport> namespaces;

        for (auto& s : programa.sentencias) {
            if (auto* imp = dynamic_cast<ImportarDecl*>(s.get())) {
                const ResolutorModulos::TablaExportacion* tablaDestino =
                    resolverDependencia(dirBase, imp->ruta);
                if (!tablaDestino) {
                    ok_ = false;
                    continue;
                }
                if (imp->tipo == TipoImportar::Nombrado) {
                    for (auto& ni : imp->nombres) {
                        auto it = tablaDestino->find(ni.origen);
                        if (it == tablaDestino->end()) {
                            std::cerr << "Error: el modulo '" << imp->ruta << "' no exporta '"
                                      << ni.origen << "'\n";
                            ok_ = false;
                            continue;
                        }
                        propias.renombres[ni.alias] = it->second;
                    }
                } else if (imp->tipo == TipoImportar::Espacio) {
                    namespaces[imp->aliasEspacio] = NamespaceImport{tablaDestino, imp->ruta};
                } else {  // TipoImportar::PorDefecto
                    auto it = tablaDestino->find("__defecto__");
                    if (it == tablaDestino->end()) {
                        std::cerr << "Error: el modulo '" << imp->ruta
                                  << "' no tiene 'exportar por defecto'\n";
                        ok_ = false;
                        continue;
                    }
                    propias.renombres[imp->nombreLocal] = it->second;
                }
            } else if (dynamic_cast<ExportarDesde*>(s.get())) {
                std::cerr << "Error: 'exportar { ... } desde \"...\"' (re-export) todavia no "
                             "esta implementado (PLAN_MODULOS.md, M7).\n";
                ok_ = false;
            }
        }

        if (!ok_) return propias.tabla;

        if (!propias.renombres.empty() || !namespaces.empty()) {
            ReescritorReferencias reescritor(propias.renombres, namespaces);
            reescritor.visitar(programa);
            if (reescritor.tuvoError()) ok_ = false;
        }

        if (!ok_) return propias.tabla;

        ListaSent limpio;
        for (auto& s : programa.sentencias) {
            if (dynamic_cast<ImportarDecl*>(s.get()) || dynamic_cast<ExportarDesde*>(s.get()))
                continue;
            limpio.push_back(std::move(s));
        }
        programa.sentencias = std::move(limpio);

        return propias.tabla;
    }

    // Resuelve (memoizado, con detección de ciclos) el módulo referenciado
    // por `rutaImport` (relativa a `dirBase`). Devuelve un puntero a su
    // tabla de exportación ya calculada, o nullptr en error (ya reportado).
    const ResolutorModulos::TablaExportacion* resolverDependencia(const fs::path& dirBase,
                                                                    const std::string& rutaImport) {
        std::string rutaCanonica = normalizar((dirBase / rutaImport).string());

        auto it = estados_.find(rutaCanonica);
        if (it != estados_.end()) {
            if (it->second.enProceso) {
                std::cerr << "Error: import circular detectado: ";
                bool desde = false;
                for (auto& r : pila_) {
                    if (r == rutaCanonica) desde = true;
                    if (desde) std::cerr << fs::path(r).filename().string() << " -> ";
                }
                std::cerr << fs::path(rutaCanonica).filename().string() << "\n";
                return nullptr;
            }
            return &it->second.exportaciones;
        }

        std::ifstream archivo(rutaCanonica, std::ios::binary);
        if (!archivo) {
            std::cerr << "Error: no se pudo abrir el modulo importado: '" << rutaImport << "'\n";
            return nullptr;
        }
        std::stringstream ss;
        ss << archivo.rdbuf();
        std::string fuente = ss.str();

        Lexer lexer(fuente);
        Parser parser(lexer);
        std::unique_ptr<Programa> programa = parser.parse();
        if (!programa) {
            std::cerr << "Error de sintaxis en el modulo importado: '" << rutaImport << "'\n";
            return nullptr;
        }

        if (!ResolutorModulos::participaDeModulos(*programa)) {
            std::cerr << "Error: '" << rutaImport << "' no usa 'exportar' -- "
                         "querias 'incluir \"" << rutaImport << "\"'?\n";
            return nullptr;
        }

        estados_[rutaCanonica].enProceso = true;
        pila_.push_back(rutaCanonica);

        ResolutorModulos::TablaExportacion tabla = procesarModulo(*programa, rutaCanonica);

        pila_.pop_back();
        EstadoModulo& estado = estados_[rutaCanonica];
        estado.enProceso = false;

        if (!ok_) return nullptr;

        for (auto& s : programa->sentencias)
            dependencias_.push_back(std::move(s));

        estado.listo = true;
        estado.exportaciones = std::move(tabla);
        return &estado.exportaciones;
    }
};

}  // namespace

std::string ResolutorModulos::slugDesdeRuta(const std::string& ruta) {
    std::string slug;
    slug.reserve(ruta.size());
    for (char c : ruta)
        slug += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    return slug;
}

bool ResolutorModulos::participaDeModulos(const Programa& programa) {
    for (auto& s : programa.sentencias) {
        Sentencia* sp = s.get();
        if (!sp) continue;
        if (dynamic_cast<ImportarDecl*>(sp) || dynamic_cast<ExportarDesde*>(sp))
            return true;
        if (auto* f = dynamic_cast<FuncionDef*>(sp)) { if (f->exportado) return true; }
        else if (auto* c = dynamic_cast<ClaseDef*>(sp)) { if (c->exportado) return true; }
        else if (auto* e = dynamic_cast<EstructuraDef*>(sp)) { if (e->exportado) return true; }
        else if (auto* i = dynamic_cast<InterfazDef*>(sp)) { if (i->exportado) return true; }
        else if (auto* a = dynamic_cast<Asignacion*>(sp)) { if (a->exportado) return true; }
    }
    return false;
}

ResolutorModulos::TablaExportacion ResolutorModulos::resolverModuloUnico(
    Programa& programa, const std::string& rutaCanonica) {
    DeclaracionesPropias propias = manglarDeclaracionesPropias(programa, rutaCanonica);

    if (!propias.renombres.empty()) {
        ReescritorReferencias reescritor(propias.renombres);
        reescritor.visitar(programa);
    }

    return propias.tabla;
}

std::unique_ptr<Programa> ResolutorModulos::resolverProyecto(std::unique_ptr<Programa> entrada,
                                                               const std::string& rutaEntrada) {
    if (!entrada) return nullptr;

    bool tieneImports = false;
    for (auto& s : entrada->sentencias) {
        if (dynamic_cast<ImportarDecl*>(s.get()) || dynamic_cast<ExportarDesde*>(s.get())) {
            tieneImports = true;
            break;
        }
    }

    if (!tieneImports) {
        resolverModuloUnico(*entrada, rutaEntrada);
        return entrada;
    }

    ResolutorProyecto proyecto;
    return proyecto.resolver(std::move(entrada), rutaEntrada);
}
