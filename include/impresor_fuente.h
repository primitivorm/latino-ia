// impresor_fuente.h
//
// Convierte un subárbol del AST de vuelta a código Latino legible. Se usa para
// `@expandir_macro` y para la función de macro `string(expr)`. A diferencia de
// ImpresorAST (volcado de depuración), la salida es parseable de nuevo.
// Las expresiones compuestas se imprimen entre paréntesis para no depender de
// una tabla de precedencias completa.

#ifndef IMPRESOR_FUENTE_H
#define IMPRESOR_FUENTE_H

#include <ostream>
#include <sstream>
#include <string>

#include "ast.h"

class ImpresorFuente {
public:
    explicit ImpresorFuente(std::ostream& salida);

    void imprimir(Nodo& nodo);

    static std::string aTexto(Nodo& nodo);

private:
    std::ostream& salida;
    int nivel = 0;

    void linea(const std::string& texto);
    void sangria();

    void imprimirExpresion(const Expresion* e);
    void imprimirSentencia(const Sentencia* s);
    void imprimirLista(const ListaSent& lista);
    void imprimirParametros(const std::vector<ParamFuncion>& parametros);
};

#endif  // IMPRESOR_FUENTE_H
