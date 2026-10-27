---
title: 'Rangos estilo Python en Latino'
type: 'feature'
created: '2026-10-27'
status: 'done'
review_loop_iteration: 0
baseline_commit: '09c82c240ec76f0151f2d9f5f770d14703a7cce6'
context:
  - '{project-root}/SINTAXIS.md'
  - '{project-root}/CLAUDE.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Latino no ofrece el objeto `rango` ni la iteración `para ... en ...`; por ello no se pueden expresar rangos Python con límites exclusivos, pasos negativos ni convertirlos a listas.

**Approach:** Añadir `rango(inicio, fin, salto)` como valor lazy del runtime, la sentencia `para variable en expresion` y la conversión `lista.a_lista`. La misma semántica debe funcionar en los backends C y LLVM.

## Boundaries & Constraints

**Always:** `rango(fin)` equivale a inicio 0 y salto 1; `rango(inicio, fin)` usa salto 1; el límite final es exclusivo; un salto negativo cuenta hacia atrás. Aceptar de uno a tres argumentos numéricos enteros finitos y rechazar un salto cero, entradas inválidas o aridad incorrecta con error explícito. El objeto `rango` debe ser lazy; `lista.a_lista` materializa una lista. `tipo(rango(...))` devuelve `"rango"`. Mantener el comportamiento de `romper`, la gestión de memoria y el ABI real de LLVM.

**Ask First:** Ninguna.

**Never:** No cambiar `desde(...)`, no convertir `rango` en lista eager, no ampliar `para ... en` a listas, cadenas, diccionarios u otros iterables, ni añadir slicing u otras operaciones de `range` que no aparecen en este alcance.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Argumento único | Iterar `rango(5)` | Visita `0, 1, 2, 3, 4`; excluye 5 | Ninguno |
| Inicio, fin y paso | Iterar `rango(2, 7)` y `rango(1, 10, 2)` | `2..6` y `1, 3, 5, 7, 9` | Ninguno |
| Paso negativo | Iterar `rango(5, 0, -1)` | Visita `5, 4, 3, 2, 1`; excluye 0 | Ninguno |
| Conversión | `lista.a_lista(rango(0, 10, 2))` | Lista `[0, 2, 4, 6, 8]`; `tipo(rango(...))` es `"rango"` | Fallos de validación/asignación reportados |
| Entrada inválida | Paso cero, argumento fraccionario o aridad fuera de 1–3 | No inicia una iteración inválida | Error claro y código de salida distinto de cero |

</frozen-after-approval>

## Code Map

- `include/ast.h`, `include/parser.h`, `src/parser.cpp` — nodos de sentencia y parser; `para` ya es palabra reservada, `en` no lo es.
- `include/ast_impresor.h`, `src/ast_impresor.cpp`, `src/recolector_variables.cpp` — impresión del AST y recolección de variables locales en bucles.
- `include/analizador_semantico.h`, `src/analizador_semantico.cpp` — recorrido semántico y contexto de `romper`.
- `runtime/latino.h`, `runtime/latino.c` — tags/valores, ciclo de vida, identificación y constructores del runtime.
- `runtime/libs/lista.h`, `runtime/libs/lista.c` — API estándar `lista.*`; actualmente no existe `a_lista`.
- `src/compiler.cpp`, `src/compiler_llvm.cpp`, `tools/abi_probe.c` — generación C/LLVM e importación de ABI de funciones runtime.
- `tests/test_parser.cpp`, `tests/test_semantico.cpp`, `tests/test_codegen.cpp`, `tests/test_codegen_llvm.cpp`, `tests/test_lib_lista.cpp`, `tests/test_funciones_anonimas_e2e.cpp`, `ejemplos/rango.lat` — pruebas de sintaxis, código generado, conversión y E2E. Los ejemplos se registran automáticamente en CTest para C y LLVM cuando está habilitado.
- `SINTAXIS.md`, `CLAUDE.md` — referencia del lenguaje y catálogo de suites.

## Tasks & Acceptance

**Execution:**
- [x] `include/ast.h`, `include/parser.h`, `src/parser.cpp`, `src/ast_impresor.cpp`, `include/ast_impresor.h` — modelar y parsear `para <id> en <expr> ... fin`, aceptando `:` final opcional como en los ejemplos, y hacer visible el nodo en `--ast`.
- [x] `include/analizador_semantico.h`, `src/analizador_semantico.cpp`, `src/recolector_variables.cpp` — analizar el iterable y cuerpo como bucle, registrar la variable bajo las reglas actuales y validar `romper`.
- [x] `runtime/latino.h`, `runtime/latino.c` — implementar un valor lazy `rango`, sus argumentos/validaciones, tipo `"rango"` y ciclo de vida seguro.
- [x] `runtime/libs/lista.h`, `runtime/libs/lista.c`, `tools/abi_probe.c` — añadir `lista.a_lista` y exponer al ABI LLVM las funciones runtime usadas, con errores explícitos ante entradas inválidas o fallo de materialización.
- [x] `src/compiler.cpp`, `src/compiler_llvm.cpp` — mapear la llamada `rango` a su constructor runtime y generar iteración con fin exclusivo y pasos positivos/negativos, conservando el flujo de `romper` y el ABI LLVM `sret`/punteros.
- [x] `tests/test_parser.cpp`, `tests/test_semantico.cpp`, `tests/test_codegen.cpp`, `tests/test_codegen_llvm.cpp`, `tests/test_lib_lista.cpp`, `ejemplos/rango.lat` — cubrir límites, pasos, lista, errores y ejecución/paridad de ambos backends.
- [x] `SINTAXIS.md`, `CLAUDE.md` — documentar sintaxis, semántica y cobertura E2E.

**Acceptance Criteria:**
- Given `para i en rango(5)`, `rango(2, 7)`, `rango(1, 10, 2)` o `rango(5, 0, -1)`, when se ejecuta en C y LLVM, then se visitan exactamente los valores descritos y ambos backends producen salida idéntica.
- Given un rango, when se llama `lista.a_lista` o `tipo`, then se materializa la lista esperada o se devuelve `"rango"` sin convertir el rango antes de esa llamada.
- Given paso cero, argumentos no enteros/no numéricos o aridad inválida, when se construye el rango, then el runtime informa el error y no entra en un bucle infinito ni devuelve una lista aparentemente válida.
- Given `romper` dentro de `para`, when se compila y ejecuta con ambos backends, then se sale del bucle sin ejecutar otra iteración y el módulo LLVM verifica correctamente.

## Spec Change Log

## Design Notes

Representar el rango como campos inline de `LatValor` evita reservar memoria por cada iteración y mantiene lazy la secuencia; solo `lista.a_lista` materializa sus elementos. La materialización vuelve a comprobar el límite exclusivo en cada valor porque la aritmética double puede redondear un paso hasta el límite. Los nombres de helpers runtime usados desde LLVM deben añadirse al ABI probe; los valores `LatValor` siguen almacenándose/pasándose según el ABI importado, nunca como structs LLVM por registro.

## Verification

**Commands:**
- `cmake --build build --config Debug` — compilación del backend C y pruebas.
- `cmake --build build-llvm --config Release` — compilación del backend LLVM.
- `ctest --test-dir build -C Debug -R "^(test_parser|test_semantico|test_codegen|test_runtime_rango.*|test_rango_errores_e2e|test_lib_lista|test_funciones_anonimas_e2e|e2e_rango)$" --output-on-failure` — 16 pruebas dirigidas C correctas.
- `ctest --test-dir build-llvm -C Release -R "^(test_parser|test_semantico|test_codegen|test_codegen_llvm|test_runtime_rango.*|test_rango_errores_e2e(_llvm)?|test_lib_lista(_llvm)?|test_funciones_anonimas_e2e(_llvm)?|e2e_rango(_llvm)?)$" --output-on-failure` — 21 pruebas dirigidas C/LLVM correctas; ejecutar en serie.

## Suggested Review Order

**Sintaxis y semántica del bucle**

- El parser introduce `para ... en ...` sin alterar el `desde` existente.
  [`parser.cpp:462`](../../src/parser.cpp#L462)

- El AST lleva una variable y un iterable hasta los visitantes.
  [`ast.h:489`](../../include/ast.h#L489)

**Representación lazy y conversión**

- El constructor valida enteros y guarda el rango inline, evitando fugas por iteración.
  [`latino.c:239`](../../runtime/latino.c#L239)

- La conversión a lista revalida el límite exclusivo ante redondeo de double.
  [`lista.c:30`](../../runtime/libs/lista.c#L30)

**Generación C y LLVM**

- El backend C traduce cada elemento y mantiene el control de `romper`.
  [`compiler.cpp:911`](../../src/compiler.cpp#L911)

- LLVM usa celdas ABI importadas y recolecta anónimas dentro del nuevo bucle.
  [`compiler_llvm.cpp:1083`](../../src/compiler_llvm.cpp#L1083)

**Cobertura de comportamiento**

- Las pruebas runtime incluyen pasos negativos, validación y límite redondeado.
  [`test_runtime_rango.cpp:25`](../../tests/test_runtime_rango.cpp#L25)

- E2E confirma que las funciones anónimas dentro de `para` siguen disponibles en LLVM.
  [`test_funciones_anonimas_e2e.cpp:72`](../../tests/test_funciones_anonimas_e2e.cpp#L72)

- El ejemplo reúne las formas solicitadas y su salida esperada.
  [`rango.lat:1`](../../ejemplos/rango.lat#L1)
