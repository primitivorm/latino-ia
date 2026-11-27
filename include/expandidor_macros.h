// expandidor_macros.h
//
// Fase de expansión de macros al estilo de Julia (PLAN_MACROS.md). Se ejecuta
// después del parseo (y de la expansión de `incluir`) y antes del análisis
// semántico: las macros son funciones que reciben expresiones sin evaluar y
// devuelven una nueva expresión, que sustituye a la invocación en el AST.
// Los nodos MacroDef se eliminan del programa: no existen en runtime.

#ifndef EXPANDIDOR_MACROS_H
#define EXPANDIDOR_MACROS_H

#include <iosfwd>
#include <memory>

#include "ast.h"

class ExpandidorMacros {
public:
    // Expandirá todas las invocaciones `@...` de `programa`. `reporte` recibe
    // el código generado por `@expandir_macro`. Devuelve false si hay errores
    // (ya reportados por stderr).
    static bool expandir(Programa& programa, std::ostream& reporte);

    // Igual, sin reporte (para pruebas y usos que no piden @expandir_macro).
    static bool expandir(Programa& programa);
};

#endif  // EXPANDIDOR_MACROS_H
