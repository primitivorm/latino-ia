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
| `input/` | Planes de trabajo (`PLAN_BASE.md`, `PLAN_LIBS.md`, `PLAN_POO.md`, `PLAN_LLVM.md`) |

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

**No usar `ctest -j` (paralelo):** `invocador_c.cpp`/`invocador_llvm.cpp`
escriben el objeto/`.c` temporal de cada compilación en una ruta fija
(no única por proceso), así que varios `latino.exe` concurrentes se pisan
el mismo archivo temporal entre sí y producen fallos aleatorios (hallazgo
real de la Fase L11 de `PLAN_LLVM.md`).

Suites de prueba disponibles:

| Suite | Qué cubre |
|---|---|
| `test_lexer` | Tokenización |
| `test_parser` | Árbol AST |
| `test_ast` | Nodos del AST |
| `test_semantico` | Análisis semántico |
| `test_codegen` | Generación de código C |
| `test_tipado` | Tipado gradual opcional (Fase 27) |
| `test_poo` | Parser/semántico/codegen de POO: clases, herencia, interfaces (Fase 28) |
| `test_poo_e2e` | Programas POO completos (Fase 28) |
| `test_e2e` | Programas completos en `ejemplos/` |
| `test_funciones_base` | Funciones built-in (`tipo`, `acadena`, etc.) |
| `test_incluir` | Sistema de módulos |
| `test_lib_cadena` | Librería `cadena` |
| `test_lib_lista` | Librería `lista` |
| `test_lib_dic` | Librería `dic` |
| `test_lib_mate` | Librería `mate` |
| `test_lib_sis` | Librería `sis` |
| `test_lib_archivo` | Librería `archivo` |
| `test_codegen_llvm` | ABI del backend LLVM (Fase L2 de `PLAN_LLVM.md`); solo se registra si `LATINO_LLVM_BACKEND` está habilitado |

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

## Backend LLVM (en desarrollo)

Plan completo en [input/PLAN_LLVM.md](input/PLAN_LLVM.md). Agrega un segundo
backend (`--backend=llvm`) que convive con el actual de C (`--backend=c`,
sigue siendo el predeterminado). Versión objetivo: **LLVM 18.x**, obtenido
con el mismo toolchain que compila `latino.exe` para evitar problemas de ABI
de C++ (API C++ nativa de LLVM, no la API-C):

| Plataforma | Cómo obtener LLVM 18.x |
|---|---|
| Windows | `.\install_llvm.ps1` (ver aviso de espacio en disco en README.md) |
| Linux | `apt install llvm-18-dev` (o el paquete equivalente de la distro) |
| macOS | `brew install llvm@18` (apuntar `CMAKE_PREFIX_PATH`) |

**Estado (fases de `PLAN_LLVM.md`):** L0-L1 completas (plumbing de build +
enlace end-to-end, verificado con LLVM real). L2 completa (mecanismo de
ABI): `tools/abi_probe.c` fuerza a Clang a emitir `declare` de todo el
runtime; el `add_custom_command` de `CMakeLists.txt` genera
`generated/runtime_abi.ll` en cada build; `RuntimeAbiLLVM`
(`include/runtime_abi_llvm.h` / `src/runtime_abi_llvm.cpp`) lo importa y
expone `%struct.LatValor` y las firmas reales del runtime a
`GeneradorLLVM`, en vez de reconstruirlas a mano. Hallazgo clave: en
Windows x64/MSVC, `LatValor` se pasa/retorna **por puntero** (`sret` +
`ptr`), nunca por valor — cualquier codegen futuro (L3+) debe modelar cada
valor Latino como un puntero a una celda `alloca %LatValor`, nunca como un
struct LLVM por registro. L3 completa (tipos/literales/expresión), L4
completa (variables locales, asignación, expresiones compuestas), L5
completa (control de flujo: si/desde/mientras/repetir/elegir/romper), L6
completa (funciones de usuario y variádica: prototipo adelantado para
recursión, `retornar`, llamadas a funciones de usuario), L7 completa
(llamadas a builtins/las 7 bibliotecas/despacho dinámico
`lat_obj_llamar_metodo`) y L8 completa (POO: `nuevo`/`es`/`este`/`base`,
métodos de instancia y estáticos con la firma empaquetada `(sret, nargs,
args)` — distinta de la de una función de usuario porque el número de
argumentos de una llamada dinámica solo se conoce en tiempo de ejecución) —
todas verificadas con LLVM real vía pruebas de codegen unitarias
(`tests/test_codegen_llvm.cpp`, subcadena de IR + `verifyModule`, 206
comprobaciones). L9 completa (driver AOT): `GeneradorLLVM::generar()` ya
recorre el `Programa` real (ya no el módulo "hola mundo" de L1) y arma un
`main` que llama a `lat_set_args`/`lat_abi_verificar` antes de traducir el
resto del programa; nueva bandera `--solo-ir`. Verificado con LLVM real: los
27 ejemplos de `ejemplos/*.lat` con anotación `#salida:` compilan y ejecutan
con `--backend llvm` produciendo salida **idéntica byte a byte** a
`--backend c` (adelanta el gate de paridad de salida de la Fase L12 para
estos ejemplos; la paridad exhaustiva de todas las construcciones del
lenguaje sigue siendo el gate formal de L12), y las 44 suites de CTest
existentes siguen pasando. L10 completa (modo `--jit` vía `llvm::orc::LLJIT`,
ver `ejecutarJit` en `src/invocador_llvm.cpp`): ejecuta el mismo
`llvm::Module` en memoria, sin `.obj`/`.exe` intermedio. Tres hallazgos
reales de Windows documentados en el plan: (1) `DynamicLibrarySearchGenerator
::GetForCurrentProcess()` no resuelve símbolos enlazados STATIC dentro de
`latino.exe` (MSVC no exporta nada de un `.exe`) — el runtime para el modo
JIT (`latino_runtime_estatico`, target CMake) es una biblioteca DINÁMICA con
`WINDOWS_EXPORT_ALL_SYMBOLS`, cargada explícitamente por ruta con `Load()`,
copiada junto a `latino.exe` en cada build; (2) el `argv` sintético que
recibe el `main` JIT-eado debe ser `static` (no de pila), porque
`lat_set_args` guarda el puntero tal cual en una global del runtime; (3) el
proceso podía crashear DESPUÉS de que el programa ya ejecutó y retornó
correctamente, durante el desenrollado normal de C++ al salir (probable
orden de descarga de la DLL) — mitigado con `fflush(nullptr)` +
`std::_Exit()` inmediatamente después de `ejecutarJit`. Verificado con los
27 ejemplos con `#salida:`: salida y código de salida idénticos a
`--backend c`, sin archivos intermedios, sin crashes; 44 suites de CTest
siguen pasando. L11 completa: la suite de CTest pasó de 44 a 80 pruebas —
`tests/test_e2e.cpp`/`tests/test_harness.h` aceptan un backend opcional
(`c`/`llvm`) y `tests/CMakeLists.txt` registra una variante `_llvm` de cada
prueba E2E y de cada suite de librería/POO/funciones base/módulos,
reutilizando los mismos binarios ya compilados. Las 80 pasan al 100%
corriendo en serie (ver nota de `ctest -j` arriba, hallazgo de esta fase).
L12 completa (paridad y decisión de default): corrida completa de las 80
pruebas de CTest en serie (100% verde, 2680.82 s reales) más una medición
dedicada de tiempo de compilación y tamaño de binario sobre los 27 ejemplos
con ambos backends (`build/medir_paridad_l12.ps1`, script ad-hoc de esta
fase) — backend C: 127.3 s totales / ~360.5 KB promedio por ejecutable;
backend LLVM: 120.8 s totales (~5% más rápido) / ~360.1 KB promedio. Los
tres criterios de paridad del plan (salida idéntica, suites de librería en
verde, sin regresión de tiempo) se cumplieron, así que `--backend llvm` pasó
a ser el default en `src/main.cpp`, condicionado a `#ifdef LATINO_CON_LLVM`
(sin ese backend en el build, el único disponible sigue siendo `c`). Hallazgo
de esta fase: `--solo-c` (emite el C intermedio, solo tiene sentido para ese
backend) dependía implícitamente del viejo default `c`; se agregó una
bandera `backendExplicito` para que `--solo-c` sin `--backend` explícito siga
usando `c` en vez del nuevo default `llvm`, preservando el uso documentado en
README/CLAUDE.md sin romper la simetría de errores cuando el usuario sí pide
`--backend llvm --solo-c`. L13 (retiro futuro de `GeneradorC`) sigue fuera de
alcance temporal, sin cambios.

## Ramas y PRs

- Rama principal de desarrollo: `dev`
- Convención de ramas: `fase-<N>-<descripcion-breve>`
- Los PRs se dirigen siempre a `dev`
