# latino-ia

Compilador para el lenguaje de programación **Latino**, escrito en C++17.

La especificación del lenguaje está en [SINTAXIS.md](SINTAXIS.md).

## Estado actual — 100 % implementado ✅

El compilador transpila código `.lat` → C → ejecutable nativo. Todas las
librerías estándar documentadas en el [Manual-Latino](https://github.com/lenguaje-latino/Manual-Latino)
están implementadas y cubiertas por pruebas E2E.

| Fase | Descripción | PR |
|------|-------------|-----|
| 0–1  | Cimientos + Lexer completo | #1 |
| 2    | Diseño del AST | #2 |
| 3    | Parser → AST | #3 |
| 4    | Análisis semántico | #4 |
| 5–6  | Generación de código C + runtime | #5 |
| 7    | Driver / CLI | #6, #7 |
| 8    | Pruebas y ejemplos E2E | #8 |
| 9    | Funciones base (`tipo`, `acadena`, `anumero`, `alogico`, `imprimirf`, …) | #9 |
| 10   | Librería `cadena` (27 funciones) | #10 |
| 11   | Librería `lista` (13 funciones) | #11 |
| 12   | Librería `dic` (5 funciones) | #12 |
| 13   | Librería `mate` (35 funciones) | #13 |
| 14   | Librería `sis` (9 funciones) | #14 |
| 15   | Librería `archivo` (9 funciones) | #15 |
| 16   | Librería `paquete` | #16 |
| 17   | Sistema de módulos (`incluir`) | #17 |
| 18   | Operador RegEx (`~=`) | #18 |
| 19   | Gestión de memoria por conteo de referencias | #19 |
| 20   | Suites de prueba de cobertura completa | #20 |

## Estrategia

```
archivo.lat → Lexer → Parser (AST) → Análisis semántico → Generación de código C → compilador C → ejecutable
```

El código C generado enlaza con `runtime/latino.c` y las librerías de
`runtime/libs/`, que implementan el sistema de tipos dinámicos
(`LatValor`: nulo, lógico, número, cadena, lista, diccionario).

## Estructura del proyecto

| Carpeta / archivo | Contenido |
|-------------------|-----------|
| `src/`            | Compilador: lexer, parser, AST, análisis semántico, generador C, driver CLI. |
| `include/`        | Cabeceras del compilador. |
| `runtime/latino.c / .h` | Runtime base: tipos, aritmética, IO, memoria ref-contada. |
| `runtime/libs/`   | Librerías estándar en C: `cadena`, `lista`, `dic`, `mate`, `sis`, `archivo`, `paquete`. |
| `ejemplos/`       | Programas `.lat` de ejemplo con anotaciones `#salida:` para pruebas E2E. |
| `tests/`          | Suites de prueba unitarias y E2E (CTest). |
| `input/`          | Plan de trabajo ([PLAN_BASE.md](input/PLAN_BASE.md), [PLAN_LIBS.md](input/PLAN_LIBS.md)). |
| `SINTAXIS.md`     | Especificación completa del lenguaje (fuente de verdad). |

## Construir

Requisitos: CMake 3.12+ y Visual Studio 2022 (Windows) o GCC/Clang (Linux/macOS).

```powershell
# Genera la solución de Visual Studio 2022 en build/
.\generar-salida.ps1

# Compila
cmake --build build --config Release
```

El ejecutable resultante se llama `latino`.

## Uso

```powershell
# Compilar un programa Latino a ejecutable
latino ejemplos/hola.lat -o hola.exe --runtime runtime

# Emitir solo el código C generado (sin compilar)
latino ejemplos/hola.lat --solo-c

# Volcar el AST (depuración)
latino ejemplos/hola.lat --ast
```

## Lenguaje soportado

### Tipos de datos

```latino
n   = 42          # número (double)
s   = "hola"      # cadena
b   = cierto      # lógico (cierto / falso)
x   = nulo        # nulo
lst = [1, 2, 3]   # lista
d   = {"a": 1}    # diccionario
```

### Estructuras de control

```latino
si n > 0
    escribir("positivo")
osi n == 0
    escribir("cero")
sino
    escribir("negativo")
fin

desde i = 1 hasta 5
    escribir(i)
fin

mientras b
    b = falso
fin

repetir
    n = n - 1
hasta n == 0

elegir x
    caso 1: escribir("uno")
    caso 2: escribir("dos")
    defecto: escribir("otro")
fin
```

### Funciones

```latino
funcion suma(a, b)
    retornar a + b
fin

escribir(suma(3, 4))   # 7
```

### Librerías estándar

```latino
incluir "cadena"
incluir "lista"
incluir "mate"
incluir "sis"
incluir "archivo"
incluir "dic"

escribir(cadena.mayusculas("hola"))       # HOLA
escribir(cadena.longitud("Latino"))       # 6
escribir(lista.longitud([1, 2, 3]))       # 3
escribir(mate.raiz(16))                   # 4
escribir(sis.operativo())                 # "windows" | "linux" | "macos"
archivo.escribir("out.txt", "contenido")
```

### Operadores

```latino
# Aritméticos: + - * / % ^
# Relacionales: == != < > <= >=
# Lógicos:      && ||
# Concatenación: ..
# RegEx:         ~=   (cierto si la cadena coincide con el patrón)
# Ternario:      cond ? a : b
```

## Pruebas

```powershell
cd build
ctest --output-on-failure
```

Las pruebas incluyen:
- Unitarias para lexer, AST, parser, análisis semántico y generación de código.
- E2E para cada programa de `ejemplos/` (compila, ejecuta y compara salida).
- Suites de cobertura por librería: `test_lib_cadena`, `test_lib_lista`, `test_lib_dic`,
  `test_lib_mate`, `test_lib_sis`, `test_lib_archivo`, `test_funciones_base`, `test_incluir`.
