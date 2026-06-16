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
| 0 | Cimientos del proyecto | ✅ Completada |
| 1 | Lexer completo | ✅ Completada |
| 2 | Diseño del AST | ⬜ Pendiente |
| 3 | Parser → AST | ⬜ Pendiente |
| 4 | Análisis semántico | ⬜ Pendiente |
| 5 | Generación de código C | ⬜ Pendiente |
| 6 | Biblioteca de runtime en C | ⬜ Pendiente |
| 7 | Driver / CLI | ⬜ Pendiente |
| 8 | Pruebas y ejemplos | 🟡 En curso (pruebas del lexer ya integradas con CTest) |

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

### Fase 2 — Diseño del AST (nuevos `include/ast.h`, `src/ast.cpp`)
- Jerarquía de nodos: programa, sentencias (asignación, asignación múltiple, `si/osi/sino`, `elegir/caso/defecto`, `desde`, `mientras`, `repetir/hasta`, `romper`, definición de función, `retornar`, llamada/expresión-sentencia) y expresiones (literales numérico/cadena/lógico/`nulo`, identificador, binaria, unaria, ternaria, acceso por índice `[]`, acceso a miembro `.`, llamada a función, literal de lista, literal de diccionario).
- Recomendado: jerarquía de clases con patrón **Visitor**, para que el generador de código sea un visitante.

### Fase 3 — Parser → AST ([src/parser.cpp](src/parser.cpp), [include/parser.h](include/parser.h))
- Reescribir como **descenso recursivo** que devuelve un AST (no `void`).
- Expresiones con **precedencia de operadores** correcta (tipo C/Python/Lua): ternario, `||`, `&&`, relacionales, `..`, aditivos, multiplicativos, potencia `^`, unarios, postfijos (`++ --`, `[]`, `.`, llamada), primarios.
- Todas las sentencias de la sección VI de WORK.md, incluida la asignación múltiple (`a, b, c = 1, 2, 3`) con relleno `nulo`/descarte.
- Funciones: `funcion`/`fun`, `retornar`/`ret`, parámetro variádico `...` y `[...]`.
- Usar los tokens `FinDeLinea` como terminadores de sentencia (tolerar saltos al inicio del archivo y dentro de listas/diccionarios multilínea).
- Manejo de errores con línea y mensaje claro (reemplazar los `exit(EXIT_FAILURE)` crudos).

### Fase 4 — Análisis semántico
- Tabla de símbolos por ámbito (global/función) para resolver variables y funciones.
- Validar reglas de WORK.md: identificadores válidos (no empiezan por número, no palabra reservada), **constantes en mayúsculas**.
- Reportar errores semánticos (variable no declarada, redefinición) sin abortar de golpe.
- Como Latino es dinámicamente tipado, el chequeo de tipos es mínimo; los tipos se resuelven en tiempo de ejecución vía el runtime (Fase 6).

### Fase 5 — Generación de código C ([src/compiler.cpp](src/compiler.cpp), [include/compiler.h](include/compiler.h))
- Implementar la generación como un **visitante del AST** que emite C.
- Cada variable se compila al tipo dinámico `Valor` (ver Fase 6), no a tipos C nativos directos.
- Traducir control de flujo (`si`→`if`, `desde`→`for`, `mientras`→`while`, `repetir/hasta`→`do/while`, `elegir`→`switch`/cadena `if/else`), funciones a funciones C que reciben/devuelven `Valor`, listas y diccionarios a constructores del runtime.
- Emitir el `.c` resultante (incluyendo `runtime/latino.h`).

### Fase 6 — Biblioteca de runtime en C (nuevo `runtime/latino.h`, `runtime/latino.c`)
- Tipo `Valor` (unión etiquetada: lógico/`double`/cadena/lista/diccionario/`nulo`), acorde a la tabla de tipos de [WORK.md](WORK.md) sección IV.
- Operaciones aritméticas, lógicas, relacionales, concatenación `..`, indexación (incluida la negativa de listas), acceso a diccionario por clave.
- Funciones integradas: `escribir`, `imprimir`, etc.
- Gestión de memoria de listas/diccionarios/cadenas (estrategia simple documentada).

### Fase 7 — Driver / CLI ([src/main.cpp](src/main.cpp))
- Reemplazar la cadena codificada por lectura de un archivo `.lat` pasado como argumento.
- Orquestar: leer fuente → Lexer → Parser (AST) → (Semántico) → Compiler (emite `.c`) → invocar el compilador C del sistema.
- Banderas: `-o salida`, `--solo-c` (emitir solo el `.c`), `--ast` (volcar el AST).
- Actualizar [src/CMakeLists.txt](src/CMakeLists.txt) con los nuevos archivos.

### Fase 8 — Pruebas y ejemplos 🟡
- [x] Pruebas unitarias del lexer integradas con CTest ([tests/test_lexer.cpp](tests/test_lexer.cpp), [tests/CMakeLists.txt](tests/CMakeLists.txt)).
- [ ] Pruebas del parser (volcar y comparar el AST de cada ejemplo).
- [ ] Pruebas de extremo a extremo: compilar y ejecutar cada archivo de [ejemplos/](ejemplos/) y comparar su salida contra la documentada (`#salida:`) en [WORK.md](WORK.md).

---

## Archivos clave

| Componente | Archivos |
|---|---|
| Lexer | [src/lexer.cpp](src/lexer.cpp), [include/lexer.h](include/lexer.h) |
| AST (nuevo) | `include/ast.h`, `src/ast.cpp` |
| Parser | [src/parser.cpp](src/parser.cpp), [include/parser.h](include/parser.h) |
| Generación de código | [src/compiler.cpp](src/compiler.cpp), [include/compiler.h](include/compiler.h) |
| Runtime C (nuevo) | `runtime/latino.h`, `runtime/latino.c` |
| Driver | [src/main.cpp](src/main.cpp) |
| Pruebas | [tests/test_lexer.cpp](tests/test_lexer.cpp), [tests/CMakeLists.txt](tests/CMakeLists.txt) |
| Build | [src/CMakeLists.txt](src/CMakeLists.txt), [CMakeLists.txt](CMakeLists.txt) |
| Especificación / fuente de verdad | [WORK.md](WORK.md) |

> Nota: [src/interpreter.cpp](src/interpreter.cpp) e [include/interpreter.h](include/interpreter.h) quedan **sin tocar** (intérprete diferido). Se conservan como base para una fase futura que compartiría el AST de la Fase 2.

---

## Verificación

1. **Construir:** `.\generar-salida.ps1` y luego `cmake --build build` (o `cmake --build build --config Debug`).
2. **Pruebas unitarias (CTest):** `ctest --test-dir build -C Debug --output-on-failure`.
   - Lexer: ✅ implementado en [tests/test_lexer.cpp](tests/test_lexer.cpp).
   - Parser (futuro): volcar el AST (`--ast`) de cada ejemplo y comprobar su estructura.
3. **Prueba de extremo a extremo (futuro):** por cada archivo en [ejemplos/](ejemplos/), ejecutar `latino ejemplo.lat -o ejemplo.exe`, correr el ejecutable y comparar su salida con la documentada (`#salida:`) en [WORK.md](WORK.md). Empezar por `escribir("hola mundo")` y avanzar característica por característica.
4. **Regresión:** integrar el paso 3 como suite automatizada (CTest) que falle si alguna salida no coincide.
