// ast_impresor.h
//
// Visitante que vuelca el AST como texto indentado. Útil para depurar el
// parser (Fase 3) y como base de la bandera --ast del compilador (Fase 7).

#ifndef AST_IMPRESOR_H
#define AST_IMPRESOR_H

#include <ostream>
#include "ast.h"

class ImpresorAST : public Visitante {
public:
    explicit ImpresorAST(std::ostream& salida);

    // Imprime el subárbol cuya raíz es `nodo`.
    void imprimir(Nodo& nodo);

    // Expresiones
    void visitar(LitNumero&) override;
    void visitar(LitCadena&) override;
    void visitar(LitLogico&) override;
    void visitar(LitNulo&) override;
    void visitar(Identificador&) override;
    void visitar(Binaria&) override;
    void visitar(Unaria&) override;
    void visitar(PostOperador&) override;
    void visitar(Ternaria&) override;
    void visitar(AccesoIndice&) override;
    void visitar(AccesoMiembro&) override;
    void visitar(Llamada&) override;
    void visitar(ListaLiteral&) override;
    void visitar(DiccionarioLiteral&) override;
    void visitar(VarArgs&) override;

    // Sentencias
    void visitar(Programa&) override;
    void visitar(Asignacion&) override;
    void visitar(ExprSentencia&) override;
    void visitar(Si&) override;
    void visitar(Elegir&) override;
    void visitar(Desde&) override;
    void visitar(Mientras&) override;
    void visitar(Repetir&) override;
    void visitar(Romper&) override;
    void visitar(FuncionDef&) override;
    void visitar(Retornar&) override;

private:
    std::ostream& salida;
    int nivel;

    void linea(const std::string& texto);  // imprime con la sangría actual
    void hijo(Nodo& nodo);                  // visita un hijo con +1 de sangría
    void hijos(ListaSent& lista);
};

#endif  // AST_IMPRESOR_H
