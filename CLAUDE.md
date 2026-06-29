# CLAUDE.md — Guía para Claude Code en latino-ia

## Qué es este proyecto

Compilador para el lenguaje de programación **Latino**, escrito en C++17.
Latino es un lenguaje en español que transpila `.lat → C → ejecutable nativo`.

La especificación completa del lenguaje vive en [SINTAXIS.md](SINTAXIS.md).

## Pipeline de compilación

```
archivo.lat → Lexer → Parser (AST) → Análisis semántico → Generación C → compilador C → ejecutable
```

El código C generado enlaza con `runtime/latino.c` y las librerías de `runtime/libs/`.

## Estructura del proyecto

| Carpeta / archivo | Contenido |
|---|---|
| `src/` | Compilador: `lexer.cpp`, `parser.cpp`, `ast.cpp`, `analizador_semantico.cpp`, `compiler.cpp`, `main.cpp` |
| `include/` | Cabeceras del compilador |
| `runtime/latino.c/.h` | Runtime base: tipos dinámicos, aritmética, IO, memoria ref-contada |
| `runtime/libs/` | Librerías estándar en C: `cadena`, `lista`, `dic`, `mate`, `sis`, `archivo`, `paquete` |
| `ejemplos/` | Programas `.lat` con anotación `#salida:` para pruebas E2E |
| `tests/` | Suites de prueba unitarias y E2E (CTest) |
| `input/` | Planes de trabajo (`PLAN_BASE.md`, `PLAN_LIBS.md`) |

## Cómo construir

Requisitos: CMake 3.12+ y Visual Studio 2022 (Windows) o GCC/Clang (Linux/macOS).

```powershell
# Genera la solución de Visual Studio 2022
.\generar-salida.ps1

# Compila en modo Release
cmake --build build --config Release
```

El ejecutable resultante se llama `latino`.

## Cómo ejecutar pruebas

```powershell
cd build
ctest --output-on-failure
```

Suites de prueba disponibles:

| Suite | Qué cubre |
|---|---|
| `test_lexer` | Tokenización |
| `test_parser` | Árbol AST |
| `test_ast` | Nodos del AST |
| `test_semantico` | Análisis semántico |
| `test_codegen` | Generación de código C |
| `test_e2e` | Programas completos en `ejemplos/` |
| `test_funciones_base` | Funciones built-in (`tipo`, `acadena`, etc.) |
| `test_incluir` | Sistema de módulos |
| `test_lib_cadena` | Librería `cadena` |
| `test_lib_lista` | Librería `lista` |
| `test_lib_dic` | Librería `dic` |
| `test_lib_mate` | Librería `mate` |
| `test_lib_sis` | Librería `sis` |
| `test_lib_archivo` | Librería `archivo` |

## Convenciones del código

### Nomenclatura de funciones de librería

El compilador mapea `lib.funcion(args)` → `lat_lib_funcion(args)` en C.

- Prefijo: `lat_` seguido del nombre de la librería y la función, todo en minúsculas y separado por `_`.
- Ejemplo: `dic.elementos(d)` → `lat_dic_elementos(LatValor d)`
- **Todos los nombres de funciones de librería deben estar en español.**

### Sistema de tipos en runtime

El tipo central es `LatValor` (unión tagueada):

```c
typedef struct { LatTipo tipo; union { ... } v; } LatValor;
```

Tipos: `LAT_NULO`, `LAT_LOGICO`, `LAT_NUMERO`, `LAT_CADENA`, `LAT_LISTA`, `LAT_DIC`.

### Agregar una función a una librería

1. Declarar en `runtime/libs/<lib>.h` como `LatValor lat_<lib>_<fn>(LatValor ...);`
2. Implementar en `runtime/libs/<lib>.c`
3. El compilador (`src/compiler.cpp`) ya mapea automáticamente `lib.fn` → `lat_lib_fn`
4. Agregar casos de prueba en `tests/test_lib_<lib>.cpp`

### Pruebas E2E en `ejemplos/`

Cada archivo `.lat` puede tener una anotación al final:

```latino
# salida: texto esperado
```

El runner de pruebas compila, ejecuta y compara la salida con ese valor.

## Uso del CLI

```powershell
# Compilar a ejecutable
latino ejemplos/hola.lat -o hola.exe --runtime runtime

# Emitir solo el código C generado
latino ejemplos/hola.lat --solo-c

# Volcar el AST para depuración
latino ejemplos/hola.lat --ast
```

## Referencia rápida del lenguaje

```latino
# Tipos
n = 42          # número (double)
s = "hola"      # cadena
b = cierto      # lógico
x = nulo
lst = [1, 2, 3]
d = {"clave": "valor"}

# Control de flujo
si / osi / sino / fin
desde i = 1 hasta 5 / fin
mientras cond / fin
repetir / hasta cond
elegir x / caso N: / defecto: / fin

# Funciones
funcion nombre(a, b)
    retornar a + b
fin

# Librerías
incluir "cadena"
cadena.mayusculas("hola")   # "HOLA"

# Operadores especiales
".."   # concatenación de cadenas
"~="   # coincidencia con RegEx
"?"    # ternario: cond ? a : b
```

## Ramas y PRs

- Rama principal de desarrollo: `dev`
- Convención de ramas: `fase-<N>-<descripcion-breve>`
- Los PRs se dirigen siempre a `dev`
