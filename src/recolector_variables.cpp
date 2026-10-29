// recolector_variables.cpp — ver recolector_variables.h

#include "recolector_variables.h"

#include <functional>

namespace {

using Filtro = std::function<bool(const Asignacion&)>;

void colectar(Sentencia* s, std::set<std::string>& out, const Filtro& filtro);

void colectarLista(const ListaSent& cuerpo, std::set<std::string>& out, const Filtro& filtro) {
    for (const auto& s : cuerpo)
        if (s) colectar(s.get(), out, filtro);
}

void colectar(Sentencia* s, std::set<std::string>& out, const Filtro& filtro) {
    if (auto* a = dynamic_cast<Asignacion*>(s)) {
        if (filtro(*a))
            for (auto& d : a->destinos)
                if (auto* id = dynamic_cast<Identificador*>(d.get()))
                    out.insert(id->nombre);
        return;
    }
    if (auto* si = dynamic_cast<Si*>(s)) {
        colectarLista(si->entonces, out, filtro);
        for (auto& r : si->osis) colectarLista(r.cuerpo, out, filtro);
        if (si->tieneSino) colectarLista(si->sino, out, filtro);
        return;
    }
    if (auto* el = dynamic_cast<Elegir*>(s)) {
        for (auto& c : el->casos) colectarLista(c.cuerpo, out, filtro);
        if (el->tieneDefecto) colectarLista(el->defecto, out, filtro);
        return;
    }
    if (auto* de = dynamic_cast<Desde*>(s)) {
        if (de->inicio) colectar(de->inicio.get(), out, filtro);
        colectarLista(de->cuerpo, out, filtro);
        return;
    }
    if (auto* pa = dynamic_cast<Para*>(s)) {
        Asignacion asignacionImplicita;
        if (filtro(asignacionImplicita))
            out.insert(pa->variable);
        colectarLista(pa->cuerpo, out, filtro);
        return;
    }
    if (auto* mi = dynamic_cast<Mientras*>(s)) {
        colectarLista(mi->cuerpo, out, filtro);
        return;
    }
    if (auto* re = dynamic_cast<Repetir*>(s)) {
        colectarLista(re->cuerpo, out, filtro);
        return;
    }
    // PLAN_FFI.md: "inseguro" es transparente para el hoisting -- una
    // variable asignada dentro de un bloque "inseguro" se declara igual que
    // si no estuviera envuelta en uno.
    if (auto* ib = dynamic_cast<InseguroBloque*>(s)) {
        colectarLista(ib->cuerpo, out, filtro);
        return;
    }
    // FuncionDef: no se desciende. ExprSentencia/Romper/Retornar: nada que declarar.
}

}  // namespace

void recolectarVariables(const ListaSent& cuerpo, std::set<std::string>& destino,
                         const std::set<std::string>& excluir) {
    colectarLista(cuerpo, destino, [](const Asignacion&) { return true; });
    for (const std::string& e : excluir)
        destino.erase(e);
}

void recolectarGlobalesExplicitos(const ListaSent& cuerpo, std::set<std::string>& destino) {
    for (const auto& s : cuerpo) {
        if (!s) continue;
        if (auto* a = dynamic_cast<Asignacion*>(s.get())) {
            if (a->esGlobal)
                for (auto& d : a->destinos)
                    if (auto* id = dynamic_cast<Identificador*>(d.get()))
                        destino.insert(id->nombre);
            continue;
        }
        if (auto* f = dynamic_cast<FuncionDef*>(s.get())) {
            if (f->esGlobal)
                destino.insert(f->nombre);
            continue;
        }
        if (auto* si = dynamic_cast<Si*>(s.get())) {
            recolectarGlobalesExplicitos(si->entonces, destino);
            for (auto& r : si->osis) recolectarGlobalesExplicitos(r.cuerpo, destino);
            if (si->tieneSino) recolectarGlobalesExplicitos(si->sino, destino);
            continue;
        }
        if (auto* el = dynamic_cast<Elegir*>(s.get())) {
            for (auto& c : el->casos) recolectarGlobalesExplicitos(c.cuerpo, destino);
            if (el->tieneDefecto) recolectarGlobalesExplicitos(el->defecto, destino);
            continue;
        }
        if (auto* de = dynamic_cast<Desde*>(s.get())) {
            if (de->inicio) {
                auto* a = dynamic_cast<Asignacion*>(de->inicio.get());
                if (a && a->esGlobal)
                    for (auto& d : a->destinos)
                        if (auto* id = dynamic_cast<Identificador*>(d.get()))
                            destino.insert(id->nombre);
            }
            recolectarGlobalesExplicitos(de->cuerpo, destino);
            continue;
        }
        if (auto* pa = dynamic_cast<Para*>(s.get())) {
            recolectarGlobalesExplicitos(pa->cuerpo, destino);
            continue;
        }
        if (auto* mi = dynamic_cast<Mientras*>(s.get())) {
            recolectarGlobalesExplicitos(mi->cuerpo, destino);
            continue;
        }
        if (auto* re = dynamic_cast<Repetir*>(s.get())) {
            recolectarGlobalesExplicitos(re->cuerpo, destino);
            continue;
        }
        if (auto* ib = dynamic_cast<InseguroBloque*>(s.get())) {
            recolectarGlobalesExplicitos(ib->cuerpo, destino);
        }
    }
}
