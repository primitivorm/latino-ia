---
title: 'Funciones anónimas con backend LLVM'
type: 'feature'
created: '2026-10-26'
status: 'in-review'
review_loop_iteration: 0
baseline_commit: '454ef98'
context:
  - '{project-root}/SINTAXIS.md'
  - '{project-root}/CLAUDE.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Las funciones anónimas `fun (...) ... fin` compilan y funcionan con el backend C, pero LLVM omite su expresión y no puede invocar un valor función. El programa puede generar un ejecutable exitoso sin ejecutar esas expresiones.

**Approach:** Implementar en LLVM la generación de funciones anónimas y la invocación indirecta de valores `LAT_FUNCION`, conservando la representación uniforme de `LatValor` por puntero y el ABI LLVM real de Windows/MSVC. Cubrir expresiones anónimas anidadas en el AST y llamadas directas o a través de variables con pruebas unitarias y E2E.

## Boundaries & Constraints

**Always:** Mantener paridad funcional con el backend C para las construcciones de funciones anónimas ya aceptadas por el parser y el analizador; verificar el módulo con LLVM; mantener compatibilidad con AOT y JIT y con el ABI de `LatValor` (sret/punteros); propagar fallos de generación en vez de emitir éxito silencioso.

**Ask First:** Ninguna.

**Never:** No cambiar sintaxis ni semántica del backend C; no añadir captura de variables locales/closures si no forma parte del comportamiento actual; no invocar funciones LLVM mediante la convención C de retorno de `LatValor` por valor; no ampliar la tarea a bibliotecas, módulos o funciones nombradas.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Valor almacenado | Asignar `fun(x) retornar x + 1 fin` a `f`; ejecutar `f(41)` | LLVM imprime `42` | Fallo de parseo/codegen debe reportarse; no generar resultado silenciosamente vacío |
| Sin retorno explícito / parámetros faltantes | Anónima llamada con menos argumentos que parámetros | Comportamiento equivalente al backend C: argumentos faltantes y retorno implícito son `nulo` | El verificador LLVM debe aceptar el módulo |
| Anónimas anidadas | Función anónima dentro del cuerpo o expresión de otra | Ambas se recolectan y generan; el programa ejecuta | No dejar símbolos anónimos sin declarar |

</frozen-after-approval>

## Code Map

- `src/compiler_llvm.cpp` — traducción de expresiones y llamadas, funciones y armado del módulo LLVM.
- `include/compiler_llvm.h` — estado y declaraciones privadas del generador LLVM.
- `runtime/latino.h` / `runtime/latino.c` — representación e invocación de valores función; adaptar invocación a ABI LLVM si hace falta.
- `tools/abi_probe.c` — símbolos del runtime que se exponen al ABI generado para LLVM.
- `tests/test_codegen_llvm.cpp` — pruebas de IR y `verifyModule`.
- `tests/test_harness.h`, `tests/CMakeLists.txt` — ejecutar casos E2E con el backend LLVM.
- `src/compiler.cpp` — comportamiento C de referencia para recolectar/generar anónimas e invocarlas.

## Tasks & Acceptance

**Execution:**
- [x] `runtime/latino.h`, `runtime/latino.c`, `tools/abi_probe.c` — proporcionar una ruta de invocación LLVM segura por ABI para `LAT_FUNCION` y exponerla a `RuntimeAbiLLVM`.
- [x] `include/compiler_llvm.h`, `src/compiler_llvm.cpp` — recolectar anónimas incluidas en expresiones/cuerpos, declararlas y generar sus cuerpos con parámetros, locales, `retornar` y retorno implícito; crear el valor función y despachar llamadas a identificadores no nombrados como funciones estáticas.
- [x] `src/compiler.cpp` — emitir prototipos para anónimas recolectadas, permitir llamadas anidadas y empaquetar el resto variádico como lista según la sintaxis documentada.
- [x] `tests/test_codegen_llvm.cpp` — verificar generación de función/valor/llamada y ejecutar `verifyModule`.
- [x] `tests/test_funciones_anonimas_e2e.cpp`, `tests/CMakeLists.txt`, `CLAUDE.md` — añadir casos E2E para ambos backends (incluidos parámetros variádicos) y documentar la suite.

**Acceptance Criteria:**
- Given un programa con una anónima asignada a variable y llamada con argumento, when se compila y ejecuta con `--backend llvm`, then imprime el mismo resultado que el backend C.
- Given una anónima anidada o en una expresión compuesta, when el generador recorre el programa, then cada nodo se recolecta y genera una sola vez y el módulo pasa `llvm::verifyModule`.
- Given parámetros omitidos o retorno implícito, when se ejecuta la anónima en LLVM, then el resultado coincide con las reglas existentes del backend C.
- Given compilación AOT o JIT, when el programa invoca una función anónima, then la llamada respeta el ABI de LLVM y no depende de la convención incompatible de función C.

## Spec Change Log

## Design Notes

Las funciones anónimas del backend C se emiten como funciones de nivel superior con firma variádica empaquetada (`nargs`, arreglo de `LatValor`). En LLVM, esa firma necesita un retorno `sret` y argumentos empaquetados, igual que los métodos dinámicos. La invocación indirecta debe usar una entrada de runtime con firma LLVM compatible; el puntero almacenado en `LAT_FUNCION` no puede llamarse como `LatFnModulo` de retorno por valor en Windows x64/MSVC.

## Verification

**Commands:**
- `ctest --test-dir build-llvm -C Release -R "^test_codegen_llvm$" --output-on-failure` — suite unitaria LLVM verde.
- `ctest --test-dir build-llvm -C Release -R "^test_funciones_anonimas_e2e(_llvm)?$" --output-on-failure` — casos E2E para ambos backends verdes.
- `.\build-llvm\src\Release\latino.exe .\build-llvm\tests\llvm\anonima_variadica.lat --backend llvm --runtime .\runtime --jit` — imprime `7`, `9` y `0`; todos los casos E2E se verifican también con `--jit`.
