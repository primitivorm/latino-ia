// resolutor_modulos.cpp
//
// Ver resolutor_modulos.h y PLAN_MODULOS.md (M3: resolución de un solo
// módulo, sin "importar" todavía).

#include "resolutor_modulos.h"

#include <cctype>
#include <unordered_set>

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
class ReescritorReferencias : public Visitante {
public:
    explicit ReescritorReferencias(const std::unordered_map<std::string, std::string>& renombres)
        : renombres_(renombres) {}

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
        if (n.izq) n.izq->aceptar(*this);
        if (n.der) n.der->aceptar(*this);
    }
    void visitar(Unaria& n) override { if (n.operando) n.operando->aceptar(*this); }
    void visitar(PostOperador& n) override { if (n.operando) n.operando->aceptar(*this); }
    void visitar(Ternaria& n) override {
        if (n.condicion) n.condicion->aceptar(*this);
        if (n.siCierto) n.siCierto->aceptar(*this);
        if (n.siFalso) n.siFalso->aceptar(*this);
    }
    void visitar(AccesoIndice& n) override {
        if (n.objeto) n.objeto->aceptar(*this);
        if (n.indice) n.indice->aceptar(*this);
    }
    // n.miembro es un nombre de campo/método por-instancia, no un nombre de
    // nivel superior: nunca se reescribe.
    void visitar(AccesoMiembro& n) override { if (n.objeto) n.objeto->aceptar(*this); }
    void visitar(Llamada& n) override {
        if (n.destino) n.destino->aceptar(*this);
        for (auto& a : n.argumentos)
            if (a) a->aceptar(*this);
    }
    void visitar(ListaLiteral& n) override {
        for (auto& e : n.elementos)
            if (e) e->aceptar(*this);
    }
    void visitar(DiccionarioLiteral& n) override {
        for (auto& p : n.pares) {
            if (p.clave) p.clave->aceptar(*this);
            if (p.valor) p.valor->aceptar(*this);
        }
    }
    void visitar(NuevoExpr& n) override {
        renombrarTipo(n.clase);
        for (auto& a : n.argumentos)
            if (a) a->aceptar(*this);
    }
    void visitar(EsExpr& n) override {
        if (n.objeto) n.objeto->aceptar(*this);
        renombrarTipo(n.clase);
    }

    // --- Sentencias ---
    void visitar(Incluir&) override {}
    void visitar(ImportarDecl&) override {}
    void visitar(ExportarDesde&) override {}

    void visitar(Asignacion& n) override {
        for (auto& v : n.valores)
            if (v) v->aceptar(*this);
        for (auto& d : n.destinos) {
            if (dynamic_cast<Identificador*>(d.get()))
                continue;  // declaración/escritura de variable: no es un "uso" a reescribir
            if (d) d->aceptar(*this);
        }
    }
    void visitar(ExprSentencia& n) override { if (n.expr) n.expr->aceptar(*this); }
    void visitar(Si& n) override {
        if (n.condicion) n.condicion->aceptar(*this);
        visitarBloque(n.entonces);
        for (auto& rama : n.osis) {
            if (rama.condicion) rama.condicion->aceptar(*this);
            visitarBloque(rama.cuerpo);
        }
        visitarBloque(n.sino);
    }
    void visitar(Elegir& n) override {
        if (n.opcion) n.opcion->aceptar(*this);
        for (auto& c : n.casos) {
            if (c.valor) c.valor->aceptar(*this);
            visitarBloque(c.cuerpo);
        }
        visitarBloque(n.defecto);
    }
    void visitar(Desde& n) override {
        if (n.inicio) n.inicio->aceptar(*this);
        if (n.condicion) n.condicion->aceptar(*this);
        if (n.incremento) n.incremento->aceptar(*this);
        visitarBloque(n.cuerpo);
    }
    void visitar(Mientras& n) override {
        if (n.condicion) n.condicion->aceptar(*this);
        visitarBloque(n.cuerpo);
    }
    void visitar(Repetir& n) override {
        visitarBloque(n.cuerpo);
        if (n.condicionHasta) n.condicionHasta->aceptar(*this);
    }
    void visitar(Romper&) override {}
    void visitar(Retornar& n) override { if (n.valor) n.valor->aceptar(*this); }
    void visitar(LlamadaBase& n) override {
        for (auto& a : n.argumentos)
            if (a) a->aceptar(*this);
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

    void renombrarUso(std::string& nombre) const {
        if (estaSombreado(nombre)) return;
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
        if (campo.valorDefecto)
            campo.valorDefecto->aceptar(*this);
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
    const std::string slug = slugDesdeRuta(rutaCanonica);
    std::unordered_map<std::string, std::string> renombres;
    TablaExportacion tabla;

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

    if (!renombres.empty()) {
        ReescritorReferencias reescritor(renombres);
        reescritor.visitar(programa);
    }

    return tabla;
}
