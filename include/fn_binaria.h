// fn_binaria.h
//
// Tabla operador-binario -> nombre de función del runtime (lat_sumar,
// lat_igual, ...). Compartida entre GeneradorC (src/compiler.cpp) y
// GeneradorLLVM (src/compiler_llvm.cpp, ver Fase L4 de input/PLAN_LLVM.md)
// para que ambos backends mapeen los operadores de `Binaria::op` a la misma
// función sin mantener dos copias de la tabla.

#ifndef FN_BINARIA_H
#define FN_BINARIA_H

#include <string>

// Devuelve el nombre de la función lat_* que implementa el operador binario
// 'op' (p.ej. "+" -> "lat_sumar"), o nullptr si 'op' no es un operador
// binario conocido.
const char* fnBinaria(const std::string& op);

#endif  // FN_BINARIA_H
