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

En Windows, `compilar_latino.ps1` automatiza todo el proceso: pregunta si se
quiere soporte para el backend LLVM y, si la respuesta es sí, instala LLVM
con `install_llvm.ps1` (ver sección "Backend LLVM") antes de configurar y
compilar.

```powershell
.\compilar_latino.ps1
# ¿Deseas soporte para el backend LLVM (--backend=llvm)? (s/N)
#   N -> genera y compila en build/     (--backend=llvm no disponible)
#   s -> instala LLVM si hace falta, genera y compila en build-llvm/
```

También se puede configurar y compilar a mano:

```powershell
# Genera la solución de Visual Studio 2022
cmake -B build -G "Visual Studio 17 2022" -A x64

# Compila en modo Release
cmake --build build --config Release
```

El ejecutable resultante (`latino.exe`) queda en `<directorio de
build>\src\Release\`.

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
| `test_genericos` | Parser/semántico/codegen de genéricos: `<T>`, bounds, `donde`, turbofish (Fase 31) |
| `test_genericos_e2e` | Programas completos con genéricos, incluida interacción con módulos (Fase 31) |
| `test_e2e` | Programas completos en `ejemplos/` |
| `test_funciones_base` | Funciones built-in (`tipo`, `acadena`, etc.) |
| `test_incluir` | Sistema de inclusión textual (`incluir "archivo.lat"`) |
| `test_modulos` | Resolución unitaria de `exportar`/`importar` (Fase 30) |
| `test_modulos_e2e` | Programas completos de un solo módulo con `exportar` (Fase 30) |
| `test_modulos_multi` | Programas multi-archivo con `importar` (Fase 30) |
| `test_lib_cadena` | Librería `cadena` |
| `test_lib_lista` | Librería `lista` |
| `test_lib_dic` | Librería `dic` |
| `test_lib_mate` | Librería `mate` |
| `test_lib_sis` | Librería `sis` |
| `test_lib_archivo` | Librería `archivo` |
| `test_codegen_llvm` | ABI del backend LLVM (Fase L2 de `PLAN_LLVM.md`) y, desde la Fase F6, codegen de FFI (`PLAN_FFI.md`); solo se registra si `LATINO_LLVM_BACKEND` está habilitado |
| `test_runtime_ffi` | Runtime de FFI con C: `LAT_PUNTERO`/`lat_ffi_verificar_tipo` (Fase F3 de `PLAN_FFI.md`) |
| `test_ffi` | Parser/semántico/codegen (backend C) de `externo`/`inseguro` (`PLAN_FFI.md`) |
| `test_ffi_e2e` | Programas completos con `externo`/`inseguro` llamando símbolos ya enlazados por defecto (Fase F7 de `PLAN_FFI.md`) |

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
| Windows | `.\compilar_latino.ps1` (pregunta e instala automáticamente) o `.\install_llvm.ps1` manualmente (ver aviso de espacio en disco en README.md) |
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

## Módulos: `exportar` / `importar` (en desarrollo)

Plan completo en [input/PLAN_MODULOS.md](input/PLAN_MODULOS.md). Agrega un
segundo mecanismo de organización de código, además de `incluir` (que no
cambia y sigue siendo el indicado para librerías estándar y scripts sin
necesidad de aislamiento): un archivo `.lat` que usa `exportar`/`importar`
se comporta como un módulo con ámbito propio, al estilo ES Modules/
TypeScript (ver sección X de [SINTAXIS.md](SINTAXIS.md)). La resolución
ocurre en tiempo de compilación, antes del análisis semántico
(`ResolutorModulos`, `include/resolutor_modulos.h` /
`src/resolutor_modulos.cpp`): reescribe el AST a un único `Programa` plano
con nombres de nivel superior manglados (`__mod_<slug>__<nombre>`), así que
ni `AnalizadorSemantico` ni `GeneradorC` ni `GeneradorLLVM` necesitan saber
que existieron módulos — la misma estrategia de "transformar el AST una
sola vez antes de la bifurcación de backend" que ya usó `PLAN_LLVM.md`.

**Estado (fases de `PLAN_MODULOS.md`):** M1-M6 completas. M1 (lexer/AST):
tres palabras reservadas nuevas (`exportar`, `importar`, `como`); nodos
`ImportarDecl`/`ExportarDesde` y campos `exportado`/`esDefecto`. M2
(parser): las tres formas de `importar` (nombrado con alias, `* como ns`,
por defecto) y `exportar` como prefijo de declaración o `exportar por
defecto`. M3 (resolución de un solo módulo, sin `importar`): mangling de
declaraciones de nivel superior y reescritura de referencias internas
(llamadas, `nuevo`, `es`, `base`, tipos por nombre de clase) respetando
sombreado de parámetros/locales. M4 (import nombrado + alias entre
archivos): DFS con memoización por ruta canónica, tabla de exportación por
módulo, detección de import circular. M5 (import de namespace y por
defecto): `importar * como ns` reescribe cada `ns.X` a su nombre interno
(o error si `X` no está exportado o `ns` no es un import de namespace);
`importar Nombre desde "ruta"` resuelve contra la clave especial
`"__defecto__"`. M6 (interacción con POO): herencia/interfaces/tipos
anotados que referencian un nombre importado ya funcionaban desde M4 sin
cambios; se agregó soporte para nombre de tipo calificado por namespace
(`nuevo ns.Clase(...)`, `es ns.Clase`, `extiende ns.Clase`, `implementa
ns.Iface`, tipo de campo/parámetro/retorno) vía
`Parser::parseNombreTipoCalificado()`. `runtime/latino.c`,
`runtime/libs/*`, `GeneradorC`, `GeneradorLLVM` y `AnalizadorSemantico` no
cambiaron en ninguna fase — solo reciben un `Programa` ya con nombres
únicos. M7 (re-export/barril, `exportar { X } desde "otro.lat"`) queda
diferida indefinidamente: la sintaxis se parsea desde M2 pero su
resolución no está implementada (sin efecto útil si se usa hoy más allá de
lo que M4 ya reporta explícitamente). Suites de prueba:
`tests/test_modulos.cpp` (unitarias sobre el AST resuelto),
`tests/test_modulos_e2e.cpp` y `tests/test_modulos_multi.cpp` (E2E vía
`latino` real, backends `c` y `llvm`).

## Genéricos al estilo de Rust (en desarrollo)

Plan completo en [input/PLAN_GENERICOS.md](input/PLAN_GENERICOS.md). Agrega
parámetros de tipo entre `<>` a `funcion`/`clase`/`estructura`/`interfaz`
(`clase Pila<T>`, `funcion identidad<T>(x: T): T`), restricciones ("bounds")
de interfaz con `:`/`+` y cláusula `donde`, y el operador turbofish
`::<...>` para instanciación explícita en posición de expresión (ver
sección XI de [SINTAXIS.md](SINTAXIS.md)). A diferencia de Rust, no hay
monomorphización: como `LatValor` ya es una unión tagueada dinámica, un
parámetro `T` se compila exactamente igual que uno sin anotar (erasure) —
`GeneradorC`/`GeneradorLLVM` no tienen ningún concepto nuevo de "tipo
genérico", solo evitan emitir el chequeo de runtime (`lat_verificar_tipo`)
para un parámetro cuyo nombre de tipo coincide con un `<T>` del
`FuncionDef`/`MetodoDef` (o de la `ClaseDef`/`EstructuraDef` envolvente,
para un método). Los bounds son estáticos únicamente: se verifican en el
analizador semántico contra el registro de tipos de POO (`implementa`)
solo cuando el tipo concreto se conoce en compilación (literal,
`nuevo Clase(...)`, variable anotada, o turbofish); no existe ni se agregó
un verificador de "implementa interfaz X" en runtime.

**Estado:** fases G1-G7 completas (sin PR/merge todavía). G1 (lexer):
palabra reservada `donde`, operador `::`. G2 (AST): `ParametroGenerico`
(`nombre` + `bounds`); campo `genericos` en `ClaseDef`/`EstructuraDef`/
`InterfazDef`/`FuncionDef`/`MetodoDef`; campo `tipoArgs` junto a cada
`tipoClase` existente; `tipoArgs` en `NuevoExpr`; `tipoArgsExplicitos`
(turbofish) en `Llamada`. G3/G4 (parser): `<...>` solo se intenta leer
como lista de tipos en cinco posiciones gramaticales fijas (nunca en
posición de expresión) — tras el nombre en declaraciones, en posición de
anotación de tipo, tras `nuevo Clase`, y tras `::` en una llamada — por lo
que no hay ambigüedad con el operador de comparación `<`/`>` sin
backtracking; `cerrarAngulo()` además separa `>=` en `>` + `=` cuando el
lexer los junta sin espacio (`Pila<numero>=...`). `extiende`/`implementa`/
`es` aceptan y descartan `<...>` tras el nombre (sustitución de tipo en la
base/interfaz queda para una fase futura). G5 (semántico): pila
`genericosActivos` (nombre → `ParametroGenerico`) empujada al entrar a una
declaración genérica, consultada por `validarTipoObjeto` antes que el
registro real de tipos; inferencia de parámetros genéricos en un sitio de
llamada por unificación simple contra el tipo del literal del argumento o
la anotación de una variable ya declarada (sin leer el AST más allá de
eso — no hay flow analysis); turbofish cuando la inferencia no alcanza
(p.ej. `T` solo aparece en el retorno); chequeo de bounds vía
`tipoImplementaInterfaz` (recorre la cadena `padre`). G6 (codegen):
hallazgo real de esta fase — la erasure NO era automática como asumía el
plan original: `GeneradorC`/`GeneradorLLVM` emitían igual
`lat_verificar_tipo(..., LAT_OBJETO, ...)` para cualquier parámetro
`TipoAnotado::Objeto`, generic o no, rompiendo en runtime cualquier
función genérica (`identidad(5)` fallaba el chequeo de tipo porque `5` no
es `LAT_OBJETO`); se corrigió construyendo, en `genFuncion`/`genMetodo` de
ambos backends, el conjunto de nombres genéricos activos (propios más los
de la clase/estructura envolvente) y saltando la emisión del chequeo para
esos nombres. G7 (módulos): verificado empíricamente sin cambios en
`resolutor_modulos.cpp` — `exportar clase Pila<T>` + `importar { Pila }
desde "..."` + `nuevo Pila<numero>()` compila y ejecuta correctamente.
Verificado solo con el backend `c` en esta máquina de desarrollo (LLVM
18.1 no está instalado aquí — `LATINO_LLVM_BACKEND` cae a `OFF` en
configuración pese al valor cacheado; el fix de G6 en
`compiler_llvm.cpp` replica el mismo patrón ya probado en `compiler.cpp`
pero queda pendiente de verificación con LLVM real). G8 completa: suite de
pruebas `tests/test_genericos.cpp` (parser + semántico + codegen, 59
comprobaciones), `tests/test_genericos_e2e.cpp` (6 `CasoTest` de un solo
archivo — inferencia, turbofish, bounds satisfechos, cláusula `donde`,
`Pila<T>`, `Par<A, B>` — más 1 `CasoTestMulti` que confirma
`exportar clase Pila<T>` + `importar { Pila } desde "..."` de punta a
punta vía el binario `latino` real, mismo patrón de
`test_modulos_multi.cpp`) y `ejemplos/genericos.lat` (E2E vía `test_e2e`).
Todo verificado solo con backend `c` (ver nota de LLVM arriba). Con esto
el plan v1 (fases G1-G8) queda completo; `nuevo Pila<Pila<numero>>()`
(anidado) e `implementa Contenedor<T>` con sustitución de tipo real siguen
fuera de alcance de v1 (ver "Fuera de alcance" del plan).

**Corrección posterior (2026-09-22):** la nota de arriba sobre "LLVM 18.1
no está instalado aquí" era incorrecta — se basaba solo en que `build/`
(el directorio en uso) no tiene LLVM configurado, sin revisar si existía
un directorio de build alternativo. Esta máquina sí tiene una instalación
real de LLVM 18.1.x vía vcpkg, accesible desde `build-llvm/` (ya
configurado con `LATINO_LLVM_BACKEND=ON`). Reconstruyendo ahí
(`cmake --build build-llvm --config Release`) y corriendo `ctest -C
Release`, tanto el fix de erasure de G6 como la interacción con módulos de
G7 quedan verificados con LLVM real: `test_genericos_e2e_llvm` y
`ejemplos/genericos_llvm` (vía `test_e2e`) pasan.

**Segunda corrección posterior (2026-09-22):** G7 se dio por verificado
solo con el caso "clase genérica exportada/importada sin bounds"
(`Pila<T>`), pero el propio G7 ya había anticipado el riesgo que faltaba
probar: "si `tipoArgs` almacenara nombres de tipo que también deban
manglearse cuando refieren a una clase importada — `Pila<OtraClaseDelModulo>`,
corregir en esta fase, no en G5". Ese caso (y su análogo con `bounds`,
`T: Comparable` importada de otro archivo) nunca se probó y de hecho
estaba roto: `ResolutorModulos` no reescribía `ParametroGenerico::bounds`
ni `tipoArgs`/`tipoArgsExplicitos`/`tipoRetornoArgs`. Corregido junto con
un hallazgo idéntico en `PLAN_MODULOS.md` (ver su propia sección de
Estado) — detalle completo en `PLAN_MODULOS.md`, "Corrección posterior".

## FFI con C al estilo de Rust (en desarrollo)

Plan completo en [input/PLAN_FFI.md](input/PLAN_FFI.md). Agrega `externo`,
un segundo mecanismo para consumir código C nativo, distinto de
`incluir "paquete"` (`runtime/libs/paquete.c`: carga dinámica en runtime
con una firma ya empaquetada a Latino): un bloque `externo` declara la
firma **real** de una función C (tipos con ancho fijo, punteros opacos,
ver sección XII de [SINTAXIS.md](SINTAXIS.md)) y se resuelve en tiempo de
**compilación**, al estilo de `extern "C"` de Rust. Toda llamada a una
función `externo` debe ocurrir dentro de un bloque `inseguro ... fin` (o
una función `funcion inseguro nombre(...)`) — análogo a `unsafe`: un
chequeo puramente estático, sin ningún efecto en runtime. `paquete` y
`externo` conviven indefinidamente, cada uno resuelve un problema
distinto.

**Estado:** fases F1-F7 completas; F8 (esta entrada) cierra el plan v1.
F1 (lexer/AST): palabras reservadas `externo`/`enlazar`/`inseguro`; enum
`TipoFFI` (`numero`/`cadena`/`logico`/`nulo` reutilizados del tipado
gradual + `entero8..64`/`natural8..64`/`puntero` nuevos); nodos
`ExternoBloque`/`InseguroBloque`; campo `inseguro` en `FuncionDef`. F2
(parser): `parseExterno()`/`parseFuncionExterna()`/`parseInseguro()`, con
`enlazar` opcional y tipo de retorno opcional (`TipoFFI::Nulo` por
defecto, equivalente a omitir `: void` en C — un `: nulo` explícito no se
puede escribir, porque `nulo` lexa como palabra reservada, no
identificador, la misma restricción sistémica que ya afecta a cualquier
anotación de tipo con nombre reservado, sin relación con FFI). F3
(runtime): `LAT_PUNTERO` nuevo en `LatTipo`/`LatValor` (`void*` sin
ref-conteo); constructor `lat_puntero`; `lat_ffi_verificar_tipo` (chequeo
dinámico, mismo patrón que `lat_verificar_tipo`); `p == nulo`/`p != nulo`
funcionan vía `son_iguales` — `p es nulo` no, porque `es` exige un
`Identificador` a su derecha y `nulo` es palabra reservada. F4
(semántico): tabla `funcionesExternas` separada de `funciones`; pila/
contador `inseguroActivo` que valida cada `Llamada` a un nombre externo;
chequeo de aridad exacta (sin variádicas) y de tipo estático cuando el
argumento es un literal o una variable ya anotada, degradado a runtime en
el resto de los casos; excepción explícita para el literal `nulo` hacia
un parámetro `puntero`. F5 (codegen backend C): declaraciones
`extern <firma C real>` + `#pragma comment(lib,...)` bajo `#ifdef
_MSC_VER` en el preámbulo; marshalling real de cada argumento
(`lat_ffi_verificar_tipo(...).como.X`, con un temporal + ternario para el
caso `nulo -> NULL` de un `puntero`) y del retorno (constructor de
runtime correspondiente); `invocador_c.cpp` agrega `-l<lib>` para GNU/
Clang. Hallazgo de esta fase: `InseguroBloque` no tenía caso en
`GeneradorC::genSentencia`/`recolectarVariables` — se agregó. F6 (codegen
backend LLVM): mismo marshalling, pero con GEP+load reales sobre
`%struct.LatValor` (nunca `.como.X`, eso es sintaxis de C) y, para el caso
`nulo -> puntero NULL`, una bifurcación real de basic blocks + `PHINode`
(nunca un `select`, que evaluaría ambos lados incondicionalmente);
`declararExterno` declara el símbolo nativo con su firma LLVM primitiva
real, nunca la firma empaquetada `(sret, ptr...)` de una función de
usuario/runtime. `tools/abi_probe.c` tuvo que agregar
`lat_puntero`/`lat_ffi_verificar_tipo` (F3 los dejó fuera, así que
`RuntimeAbiLLVM` no podía resolverlos); `ejecutarMsvc` (`invocador_c.cpp`)
ganó un parámetro `bibliotecasEnlazar` para agregar `<lib>.lib`
explícitamente, porque un objeto emitido por LLVM no tiene ningún
`#pragma comment` textual que embeber como el backend C. F7 (pruebas):
`tests/test_ffi.cpp` (codegen backend C, 17 comprobaciones), una sección
nueva en `tests/test_codegen_llvm.cpp` (codegen backend LLVM, 6 pruebas —
requirió hacer público `GeneradorLLVM::recolectarExterno`, igual que ya lo
son `recolectarTipos`/`declararFuncion`) y `tests/test_ffi_e2e.cpp` (4
`CasoTest` reales contra el binario `latino`: `abs`/`strlen`, `malloc`/
`free` con verificación de puntero nulo, chequeo dinámico de tipo sin
anotación estática, retorno `logico`) — todos sin `enlazar` (el caso con
`enlazar`/`MessageBoxA` abriría un diálogo real que bloquearía la
ejecución automática; su cobertura vive en `test_ffi.cpp`, verificando
solo el C generado). Verificado solo con el backend `c` en esta máquina de
desarrollo (LLVM 18.1 no está instalado aquí, mismo motivo que en
`PLAN_LLVM.md`/`PLAN_GENERICOS.md`); el código de `GeneradorLLVM` de F6/F7
replica patrones ya probados con LLVM real de fases anteriores, pero
queda pendiente de esa verificación. Con esto el plan v1 (fases F1-F8)
queda completo; bindgen automático, structs/uniones C por valor,
callbacks nativos, convenciones de llamada no nativas, unificar `externo`
con `paquete`, un `LAT_ENTERO64` sin pérdida de precisión y `link_name`
quedan fuera de alcance de v1 (ver "Fuera de alcance" del plan).

**Corrección posterior (2026-09-22):** igual que la nota de Genéricos más
arriba, "LLVM 18.1 no está instalado aquí" era incorrecto —
`build-llvm/` (segundo directorio de build, `LATINO_LLVM_BACKEND=ON`) ya
tiene una instalación real de LLVM 18.1.x vía vcpkg. Reconstruyendo ahí,
`test_codegen_llvm` (con la sección de F6/F7) pasa sus 233 comprobaciones
y `test_ffi_e2e` con `--backend llvm` pasa sus 4 casos de punta a punta —
el código de `GeneradorLLVM` de F6/F7 queda verificado con LLVM real, sin
cambios de código.

## Ramas y PRs

- Rama principal de desarrollo: `dev`
- Convención de ramas: `fase-<N>-<descripcion-breve>`
- Los PRs se dirigen siempre a `dev`
