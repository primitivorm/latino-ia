# Plan de trabajo — Compilador del lenguaje Latino

## Contexto

`latino-ia` es la implementación de un **compilador para el lenguaje de programación Latino**, escrito en C++17 y construido con CMake (`generar-salida.ps1` genera la solución de Visual Studio 2022). La especificación completa del lenguaje está en [WORK.md](WORK.md): variables, constantes, operadores, estructuras de control (`si/sino/osi`, `elegir`, `desde`, `mientras`, `repetir-hasta`, `romper`), funciones (con argumentos variables), listas y diccionarios.

**Decisiones de diseño:**
- Modelo de ejecución: **compilador primero** (el intérprete queda fuera de alcance por ahora).
- Alcance: **solo el lenguaje** (sin componentes de IA por el momento).
- Estrategia de compilación: **transpilar Latino → C**, y delegar la generación del binario al compilador de C del sistema. Esto se alinea con la tabla de tipos de [WORK.md](WORK.md) que ya mapea cada tipo de Latino a un tipo de C.

El resultado esperado: poder escribir un archivo `.lat` con la sintaxis de WORK.md, ejecutar el compilador, obtener un `.c` y de ahí un ejecutable que produzca la salida correcta.

```
archivo .lat → Lexer → Parser (AST) → Análisis semántico → Generación de código C → compilador C → ejecutable
```

## Estado de avance

| Fase | Descripción | Estado |
| ---- | ----------- | ------ |
| 0 | Cimientos del proyecto | ✅ Completada (PR #1) |
| 1 | Lexer completo | ✅ Completada (PR #1) |
| 2 | Diseño del AST | ✅ Completada (PR #2) |
| 3 | Parser → AST | ✅ Completada (PR #3) |
| 4 | Análisis semántico | ✅ Completada (PR #4) |
| 5 | Generación de código C | ✅ Completada (PR #5) |
| 6 | Biblioteca de runtime en C | ✅ Completada (PR #5) |
| 7 | Driver / CLI | ✅ Completada (PR #6, #7) |
| 8 | Pruebas y ejemplos | ✅ Completada (PR #8) |
| 9 | Funciones base faltantes | ✅ Completada (PR #9) |


---

## Fases del plan

### Fase 0 — Cimientos del proyecto ✅
- [x] [README.md](README.md) con propósito, estrategia, estructura y cómo construir/ejecutar.
- [x] Carpeta [ejemplos/](ejemplos/) con programas `.lat` derivados de [WORK.md](WORK.md), cada uno con su salida esperada como comentario `#salida:`.
- [x] Carpeta [runtime/](runtime/) (placeholder) para la biblioteca C de soporte.
- [x] Convención de extensión de archivo (`.lat`).

### Fase 1 — Lexer completo ✅
Archivos: [src/lexer.cpp](src/lexer.cpp), [include/lexer.h](include/lexer.h).
- [x] Corregido el defecto de "carácter de más" (ahora se usa *lookahead* con `peekChar`).
- [x] `TokenType` ampliado (nuevo `FinDeLinea`).
- [x] Todos los operadores de WORK.md sección V: `+ - * / % ^`, `&& ||`, `..`, `++ --`, `== != < > <= >= ~=`, `=`, `.`, `?`, `:`, y el variádico `...`.
- [x] Delimitadores `( ) [ ] { } , ;`.
- [x] Cadenas con `"` y `'` (lexema sin comillas, escapes preservados).
- [x] Comentarios de línea `#` y `//`, y multilínea `/* */`.
- [x] Conteo de líneas correcto (incluso a través de comentarios multilínea).
- [x] Pruebas unitarias en [tests/test_lexer.cpp](tests/test_lexer.cpp), integradas con CTest.

### Fase 2 — Diseño del AST ✅ ([include/ast.h](include/ast.h), [src/ast.cpp](src/ast.cpp))
- [x] Jerarquía de nodos: programa, sentencias (asignación, asignación múltiple, `si/osi/sino`, `elegir/caso/defecto`, `desde`, `mientras`, `repetir/hasta`, `romper`, definición de función, `retornar`, expresión-sentencia) y expresiones (literales numérico/cadena/lógico/`nulo`, identificador, binaria, unaria, postoperador `++/--`, ternaria, acceso por índice `[]`, acceso a miembro `.`, llamada, literal de lista, literal de diccionario, `...`).
- [x] Patrón **Visitante** (`Visitante` con `aceptar`/`visitar`), para que análisis y generación de código sean visitantes.
- [x] [include/ast_impresor.h](include/ast_impresor.h) + [src/ast_impresor.cpp](src/ast_impresor.cpp): `ImpresorAST`, primer visitante (vuelca el AST; reutilizado en `--ast`).
- [x] Pruebas en [tests/test_ast.cpp](tests/test_ast.cpp).

### Fase 3 — Parser → AST ✅ ([src/parser.cpp](src/parser.cpp), [include/parser.h](include/parser.h))
- [x] **Descenso recursivo** que devuelve el AST (`unique_ptr<Programa>`).
- [x] Expresiones con **precedencia** tipo C/Lua: ternario, `||`, `&&`, igualdad (`== != ~=`), relacional (`< > <= >=`), concatenación `..`, aditivo, multiplicativo, potencia `^` (asoc. derecha), unario `-`, postfijos (`++ --`, `[]`, `.`, llamada).
- [x] Todas las sentencias de la sección VI de WORK.md, incluida la asignación múltiple (`a, b, c = 1, 2, 3`).
- [x] Funciones: `funcion`/`fun`, `retornar`/`ret`, parámetro variádico `...` y `[...]`.
- [x] `FinDeLinea` como terminador de sentencia; saltos ignorados dentro de `()`, `[]`, `{}` (listas/diccionarios multilínea).
- [x] Manejo de errores con línea y mensaje claro (sin `exit()` crudo; devuelve `nullptr`).
- [x] Pruebas en [tests/test_parser.cpp](tests/test_parser.cpp).

### Fase 4 — Análisis semántico ✅ ([include/analizador_semantico.h](include/analizador_semantico.h), [src/analizador_semantico.cpp](src/analizador_semantico.cpp))
- [x] Tabla de símbolos por ámbito (global/función) para resolver variables y funciones (con *hoisting* de definiciones).
- [x] Reglas de WORK.md: **constantes en mayúsculas** (no reasignables). Los identificadores inválidos (empezar por número o ser palabra reservada) ya los descarta el lexer.
- [x] Errores semánticos **acumulados** (sin abortar al primero), ordenados por línea: variable no declarada, reasignación de constante, `romper`/`retornar`/`...` fuera de contexto, función no definida, aridad incorrecta, redefinición de función, parámetro duplicado.
- [x] Sin chequeo de tipos (Latino es dinámico; los tipos se resuelven en el runtime, Fase 6).
- [x] Pruebas en [tests/test_semantico.cpp](tests/test_semantico.cpp).

### Fase 5 — Generación de código C ✅ ([src/compiler.cpp](src/compiler.cpp), [include/compiler.h](include/compiler.h))
> Las Fases 5 y 6 se hicieron juntas: el código C generado no tiene sentido (ni se puede verificar) sin el runtime al que llama.
- [x] `GeneradorC` recorre el AST y emite C. Cada variable se compila al tipo dinámico `LatValor`.
- [x] Control de flujo: `si`→`if/else if/else`, `desde`/`mientras`→`while`, `repetir/hasta`→`do/while` (condición negada), `elegir`→cadena `if/else if` con `lat_igual`, `romper`→`break`, `retornar`→`return`.
- [x] Funciones de usuario → funciones C que reciben/devuelven `LatValor` (con prototipos para *hoisting*). Variádicas: parámetros fijos + `lat_resto` (lista); las llamadas empaquetan los argumentos extra; `[...]` → `lat_resto`.
- [x] Listas/diccionarios → `lat_lista_de`/`lat_dic_de`; indexación y asignación por índice (incluida la negativa); ternario, concatenación, etc.
- [x] Pruebas en [tests/test_codegen.cpp](tests/test_codegen.cpp).

### Fase 6 — Biblioteca de runtime en C ✅ ([runtime/latino.h](runtime/latino.h), [runtime/latino.c](runtime/latino.c))
- [x] Tipo `LatValor` (unión etiquetada: `nulo`/lógico/`double`/cadena/lista/diccionario), acorde a la tabla de tipos de [WORK.md](WORK.md) sección IV.
- [x] Aritmética, relacionales, lógicos, concatenación `..`, indexación (negativa en listas) y acceso a diccionario por clave.
- [x] Funciones integradas: `escribir`, `imprimir`; conversión a cadena (`lat_a_cadena`) con formato `[a, b, c]` y `{clave: valor}`.
- [x] Estrategia de memoria simple **documentada** (sin liberación por ahora; suficiente para programas cortos).
- [x] Compilado como librería `latino_runtime` en CMake para verificar que sigue siendo válido.

### Fase 7 — Driver / CLI ✅ ([src/main.cpp](src/main.cpp), [include/invocador_c.h](include/invocador_c.h), [src/invocador_c.cpp](src/invocador_c.cpp))
- [x] Lee un archivo `.lat` pasado como argumento.
- [x] Orquesta: fuente → Lexer → Parser (AST) → Semántico → `GeneradorC` → escribe `.c` temporal → **invoca el compilador de C** → ejecutable.
- [x] **Invocación automática del compilador**: `config.h` generado por CMake ([src/config.h.in](src/config.h.in)) hornea la ruta del compilador, el estilo (`msvc`/`gnu`), `VsDevCmd.bat` y `LATINO_RUNTIME_DIR`. En MSVC se monta el entorno de VS vía un `.bat` temporal (con `-startdir=none`), así `latino prog.lat` produce el `.exe` desde **cualquier** shell. Fallback a `CC`/PATH (gcc/clang).
- [x] Banderas: `-o <salida>`, `--solo-c` (emite el C a `-o` si se indica, si no a stdout), `--ast`, `--runtime <dir>`.

### Fase 8 — Pruebas y ejemplos ✅
- [x] Pruebas unitarias del lexer ([tests/test_lexer.cpp](tests/test_lexer.cpp)).
- [x] Pruebas del AST ([tests/test_ast.cpp](tests/test_ast.cpp)).
- [x] Pruebas del parser ([tests/test_parser.cpp](tests/test_parser.cpp)).
- [x] Pruebas del análisis semántico ([tests/test_semantico.cpp](tests/test_semantico.cpp)).
- [x] Pruebas de generación de código C ([tests/test_codegen.cpp](tests/test_codegen.cpp)).
- [x] Extremo a extremo (manual): los 11 ejemplos de [ejemplos/](ejemplos/) generan C, compilan con el runtime y se ejecutan; las salidas de `hola`, `operadores`, `si`, `funciones`, `listas`, `diccionarios`, etc. coinciden con las documentadas (`#salida:`).
- [x] Automatizar el extremo a extremo dentro de CTest (driver portable de pruebas E2E en C++).

### Fase 9 — Funciones base faltantes ✅
- [x] Conversión de tipos: `acadena()`, `alogico()`, `anumero()`.
- [x] Entrada estándar: `leer()`.
- [x] Introspección de tipos: `tipo()`.
- [x] Salida con formato: `imprimirf()`.
- [x] Consola/pantalla y control: `limpiar()`, `error()`.
- [x] Soporte básico para palabras clave adicionales: `escribe`, `poner` (sinónimos de escribir/imprimir), e `incluir` (stub de runtime).
- [x] Pruebas E2E automatizadas para estas funciones.


---

## Archivos clave

| Componente | Archivos |
|---|---|
| Lexer | [src/lexer.cpp](src/lexer.cpp), [include/lexer.h](include/lexer.h) |
| AST | [include/ast.h](include/ast.h), [src/ast.cpp](src/ast.cpp), [include/ast_impresor.h](include/ast_impresor.h), [src/ast_impresor.cpp](src/ast_impresor.cpp) |
| Parser | [src/parser.cpp](src/parser.cpp), [include/parser.h](include/parser.h) |
| Análisis semántico | [include/analizador_semantico.h](include/analizador_semantico.h), [src/analizador_semantico.cpp](src/analizador_semantico.cpp) |
| Generación de código | [src/compiler.cpp](src/compiler.cpp), [include/compiler.h](include/compiler.h) |
| Runtime C | [runtime/latino.h](runtime/latino.h), [runtime/latino.c](runtime/latino.c) |
| Driver / invocación del compilador | [src/main.cpp](src/main.cpp), [include/invocador_c.h](include/invocador_c.h), [src/invocador_c.cpp](src/invocador_c.cpp), [src/config.h.in](src/config.h.in) |
| Pruebas | [tests/](tests/) (test_lexer, test_ast, test_parser, test_semantico, test_codegen), [tests/CMakeLists.txt](tests/CMakeLists.txt) |
| Build | [src/CMakeLists.txt](src/CMakeLists.txt), [CMakeLists.txt](CMakeLists.txt) |
| Especificación / fuente de verdad | [WORK.md](WORK.md) |

> Nota: [src/interpreter.cpp](src/interpreter.cpp) e [include/interpreter.h](include/interpreter.h) quedan **sin tocar** (intérprete diferido). Se conservan como base para una fase futura que compartiría el AST de la Fase 2.

---

## Verificación

1. **Construir:** `.\generar-salida.ps1` y luego `cmake --build build` (o `cmake --build build --config Debug`).
2. **Pruebas unitarias (CTest):** `ctest --test-dir build -C Debug --output-on-failure`.
   - ✅ 5 suites: lexer, AST, parser, análisis semántico y generación de código C.
3. **Extremo a extremo:** `latino <archivo.lat> -o <salida.exe>` produce el ejecutable directamente (desde cualquier shell, sin Developer prompt) y al correrlo la salida coincide con la documentada (`#salida:`). Los 11 ejemplos pasan.
4. **Regresión (futuro):** automatizar el paso 3 como suite (CTest) que, por cada ejemplo, lo compile con `latino`, lo ejecute y compare la salida. Ya es viable porque `latino` invoca el compilador por su cuenta.
