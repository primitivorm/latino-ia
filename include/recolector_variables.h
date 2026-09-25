// recolector_variables.h
//
// Recolección de variables locales asignadas dentro de un cuerpo de
// sentencias, descendiendo en bloques anidados (si/elegir/desde/mientras/
// repetir) pero nunca dentro de una FuncionDef anidada. Replica el hoisting
// total sin scoping por bloque de GeneradorC (cada variable asignada en
// cualquier punto de la función se declara al inicio, sin importar en qué
// rama). Compartida entre GeneradorC (src/compiler.cpp) y GeneradorLLVM
// (src/compiler_llvm.cpp, ver Fase L4 de input/PLAN_LLVM.md, Reto 4:
// "Hoisting total sin scoping por bloque" — es una decisión de *lenguaje*,
// no de *backend*, así que ambos generadores deben coincidir exactamente en
// qué variables recolectan).

#ifndef RECOLECTOR_VARIABLES_H
#define RECOLECTOR_VARIABLES_H

#include <set>
#include <string>

#include "ast.h"

// Agrega a 'destino' el nombre de cada variable asignada (destino de una
// Asignacion, incluido el 'inicio' de un Desde) dentro de 'cuerpo', luego
// quita de 'destino' los nombres presentes en 'excluir' (p.ej. los
// parámetros de la función/método, o 'este').
void recolectarVariables(const ListaSent& cuerpo, std::set<std::string>& destino,
                          const std::set<std::string>& excluir);

// Agrega a 'destino' el nombre de cada variable declarada "global"
// (Asignacion::esGlobal) dentro de 'cuerpo', con el mismo criterio de
// descenso que recolectarVariables (nunca dentro de una FuncionDef
// anidada). GeneradorC/GeneradorLLVM la usan sobre las sentencias de nivel
// superior del Programa para obtener el conjunto de nombres "global", y
// luego lo pasan como 'excluir' a recolectarVariables() al recolectar las
// locales de cada función/método -- así una asignación a ese nombre dentro
// del cuerpo no crea una variable local que lo sombree, sino que escribe la
// celda real de nivel superior (ver GeneradorC::genFuncion/genMetodo y
// GeneradorLLVM::genFuncion/genMetodo).
void recolectarGlobalesExplicitos(const ListaSent& cuerpo, std::set<std::string>& destino);

#endif  // RECOLECTOR_VARIABLES_H
