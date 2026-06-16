// ast.cpp
//
// La mayor parte del AST es cabecera (los nodos son agregados con `aceptar`
// en línea). Este archivo proporciona la unidad de traducción que ancla la
// vtable de la clase base `Nodo` (evita emitirla en cada .o que incluya ast.h).

#include "ast.h"

Nodo::~Nodo() = default;
