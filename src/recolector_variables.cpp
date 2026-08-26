// recolector_variables.cpp — ver recolector_variables.h

#include "recolector_variables.h"

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

}  // namespace

void recolectarVariables(const ListaSent& cuerpo, std::set<std::string>& destino,
                         const std::set<std::string>& excluir) {
    colectarLista(cuerpo, destino);
    for (const std::string& e : excluir)
        destino.erase(e);
}
