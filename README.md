# latino-ia

Compilador para el lenguaje de programación **Latino**, escrito en C++17.

La especificación del lenguaje está en [SINTAXIS.md](SINTAXIS.md).

## Estado actual — backend de C 100 % implementado ✅ · backend LLVM predeterminado ✅ (L13 pendiente, fuera de alcance temporal)

El compilador transpila código `.lat` a un ejecutable nativo, vía dos
backends de generación de código intercambiables: LLVM (`--backend=llvm`,
predeterminado en builds con `LATINO_LLVM_BACKEND` habilitado, ver
[Backend LLVM](#backend-llvm-en-desarrollo)) y C (`--backend=c`, usado por
defecto si el build no incluye LLVM). Todas las librerías estándar documentadas en
el [Manual-Latino](https://github.com/lenguaje-latino/Manual-Latino) están
implementadas y cubiertas por pruebas E2E, incluyendo tipado gradual
opcional, Programación Orientada a Objetos (clases, herencia, interfaces,
estructuras), genéricos al estilo de Rust (`<T>`, bounds, turbofish),
módulos (`exportar`/`importar`) y FFI con C al estilo de Rust (`externo`/
`inseguro`, ver [FFI con C](#ffi-con-c-al-estilo-de-rust)).

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
| 21–26 | 26 funciones nuevas en librerías estándar | #21, #22 |
| 27   | Tipado gradual opcional (`var`/`const`, anotaciones de tipo) | #24, #25 |
| 28   | Programación Orientada a Objetos (clases, herencia, interfaces, estructuras) | #26 |
| 29   | Backend LLVM — ver [Backend LLVM](#backend-llvm-en-desarrollo) (L0-L12 completas, L13 fuera de alcance temporal) | #27–#38 |
| 30   | Módulos: `exportar` / `importar` (M1-M6 completas, M7 diferida) | #39–#45 |
| 31   | Genéricos al estilo de Rust: `<T>`, bounds, `donde`, turbofish (G1-G8 completas) | #46–#47 |
| 32   | FFI con C al estilo de Rust: `externo` / `inseguro` — ver [FFI con C](#ffi-con-c-al-estilo-de-rust) (F1-F8 completas) | #49–#56 |

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
| `input/`          | Planes de trabajo ([PLAN_BASE.md](input/PLAN_BASE.md), [PLAN_LIBS.md](input/PLAN_LIBS.md), [PLAN_POO.md](input/PLAN_POO.md), [PLAN_TIPADO.md](input/PLAN_TIPADO.md), [PLAN_LLVM.md](input/PLAN_LLVM.md), [PLAN_MODULOS.md](input/PLAN_MODULOS.md), [PLAN_GENERICOS.md](input/PLAN_GENERICOS.md), [PLAN_FFI.md](input/PLAN_FFI.md)). |
| `SINTAXIS.md`     | Especificación completa del lenguaje (fuente de verdad). |

## Construir

Requisitos: CMake 3.12+ y Visual Studio 2022 (Windows) o GCC/Clang (Linux/macOS).

En Windows, `compilar_latino.ps1` automatiza todo el proceso: pregunta si se
quiere soporte para el backend LLVM y, si la respuesta es sí, instala LLVM
con `install_llvm.ps1` (ver [Backend LLVM](#backend-llvm-en-desarrollo)) antes
de configurar y compilar.

```powershell
.\compilar_latino.ps1
# ¿Deseas soporte para el backend LLVM (--backend=llvm)? (s/N)
#   N -> genera y compila en build/     (--backend=llvm no disponible)
#   s -> instala LLVM si hace falta, genera y compila en build-llvm/
```

El ejecutable resultante (`latino.exe`) queda en `<directorio de
build>\src\Release\`. También se puede configurar y compilar a mano:

```powershell
# Genera la solución de Visual Studio 2022 en build/
cmake -B build -G "Visual Studio 17 2022" -A x64

# Compila
cmake --build build --config Release
```

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

### Programación Orientada a Objetos

```latino
clase Animal
    publico nombre: cadena

    funcion Animal(nombre: cadena)
        este.nombre = nombre
    fin

    publico funcion hablar(): cadena
        retornar este.nombre .. " hace un sonido"
    fin
fin

clase Perro extiende Animal
    publico funcion hablar(): cadena sobreescribir
        retornar este.nombre .. " dice: ¡Guau!"
    fin
fin

p = nuevo Perro("Rex")
escribir(p.hablar())      # Rex dice: ¡Guau!
escribir(p es Animal)     # cierto
```

También hay soporte para `interfaz` (implementación múltiple) y
`estructura` (tipos valor). Detalle completo en
[input/PLAN_POO.md](input/PLAN_POO.md).

### Genéricos al estilo de Rust

```latino
funcion identidad<T>(x: T): T
    retornar x
fin

escribir(identidad(5))                # 5 — T inferido de "5"
escribir(identidad::<cadena>("hola"))  # hola — turbofish explícito

clase Pila<T>
    privado items: lista
    funcion Pila()
        este.items = []
    fin
    publico funcion apilar(valor: T)
        lista.agregar(este.items, valor)
    fin
fin
```

`<T>` en `funcion`/`clase`/`estructura`/`interfaz`, restricciones ("bounds")
con `:`/`+`/`donde`, y turbofish `::<...>` para instanciación explícita.
Sin monomorphización (erasure: `LatValor` ya es dinámico). Detalle completo
en [input/PLAN_GENERICOS.md](input/PLAN_GENERICOS.md).

### Módulos: `exportar` / `importar`

```latino
# geometria.lat
exportar const PI = 3.14159
exportar funcion area_circulo(r)
    retornar PI * r * r
fin
```

```latino
# principal.lat
importar { area_circulo } desde "geometria.lat"
escribir(area_circulo(2))   # 6.28318
```

Ámbito propio por archivo, al estilo de ES Modules/TypeScript — convive
con `incluir` (que sigue siendo el mecanismo para librerías estándar y
scripts sin necesidad de aislamiento). Detalle completo en
[input/PLAN_MODULOS.md](input/PLAN_MODULOS.md).

### FFI con C al estilo de Rust

```latino
externo
    funcion abs(n: entero32): entero32
    funcion strlen(s: cadena): entero64
fin

inseguro
    escribir(abs(-5))          # 5
    escribir(strlen("hola"))   # 4
fin
```

Un bloque `externo` declara la firma **real** de una función C (tipos con
ancho fijo, punteros opacos) y la llama directamente — resuelto en tiempo
de compilación por el linker del sistema, al estilo de `extern "C"` de
Rust. Toda llamada debe ocurrir dentro de `inseguro ... fin` (o una
`funcion inseguro`), análogo a `unsafe`. Distinto de `incluir "paquete"`
(carga dinámica en runtime con una firma ya empaquetada a Latino) — ambos
mecanismos conviven. Detalle completo en
[input/PLAN_FFI.md](input/PLAN_FFI.md) y en la sección XII de
[SINTAXIS.md](SINTAXIS.md).

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
- Unitarias para lexer, AST, parser, análisis semántico y generación de código
  (incluye tipado gradual, POO, genéricos y FFI: `test_tipado`, `test_poo`,
  `test_genericos`, `test_ffi`, `test_runtime_ffi`).
- E2E para cada programa de `ejemplos/` (compila, ejecuta y compara salida),
  incluyendo `test_poo_e2e`, `test_genericos_e2e`, `test_modulos_e2e`,
  `test_modulos_multi` y `test_ffi_e2e`.
- Suites de cobertura por librería: `test_lib_cadena`, `test_lib_lista`, `test_lib_dic`,
  `test_lib_mate`, `test_lib_sis`, `test_lib_archivo`, `test_lib_paquete`, `test_funciones_base`, `test_incluir`.
- `test_codegen_llvm`: mecanismo de ABI del backend LLVM (Fase L2) y, desde
  la Fase 32, codegen de FFI (solo se registra si el build tiene
  `LATINO_LLVM_BACKEND` habilitado).
- `test_modulos`: resolución unitaria de `exportar`/`importar`, sin pasar
  por el binario `latino` real.

## Backend LLVM (en desarrollo)

Segundo backend de generación de código basado en LLVM (`--backend=llvm`),
que convive con el backend de C (`--backend=c`, mantenido como fallback en
builds sin LLVM). Plan completo con las 13 fases (L0-L13) en
[input/PLAN_LLVM.md](input/PLAN_LLVM.md).

**Estado:** L0-L11 completas y verificadas con LLVM real (ver detalle fase
por fase en [CLAUDE.md](CLAUDE.md)). L12 completa: los 27 ejemplos de
`ejemplos/*.lat` producen salida idéntica byte a byte en ambos backends, las
9 suites de librería/POO/módulos pasan en su variante `--backend=llvm`, y no
hay regresión de tiempo de compilación (LLVM resultó ~5% más rápido que C
compilando esos 27 ejemplos) — por lo que `llvm` pasó a ser el backend
predeterminado en builds con `LATINO_LLVM_BACKEND` habilitado. L13 (retiro
de `GeneradorC`) queda fuera de alcance temporal, ver el plan.

**Versión objetivo: LLVM 18.x.** Este backend usa la API C++ nativa de LLVM
(`IRBuilder`), por lo que las bibliotecas de LLVM deben compilarse con el
mismo toolchain que compila `latino.exe` para evitar problemas de ABI de
C++. Instalación recomendada por plataforma:

| Plataforma | Cómo obtener LLVM 18.x |
|---|---|
| Windows | `.\install_llvm.ps1` (clona/arranca vcpkg si hace falta e instala `llvm[core,clang,target-x86]:x64-windows-release`; el triplet `-release` evita compilar también la variante Debug) |
| Linux | Paquete `llvm-18-dev` de la distro (ej. `apt install llvm-18-dev`) |
| macOS | `brew install llvm@18` (Homebrew no lo pone en el `PATH` por defecto; hay que apuntar `CMAKE_PREFIX_PATH`) |

No se recomienda usar binarios de LLVM prebuilt de fuentes genéricas
(distintas al toolchain del proyecto) — ver "Retos técnicos" en
[input/PLAN_LLVM.md](input/PLAN_LLVM.md) para el razonamiento completo.

**Aviso de espacio en disco (Windows/vcpkg):** el feature set completo por
defecto del puerto `llvm` de vcpkg (`clang,default-targets,enable-bindings,
enable-terminfo,enable-zlib,enable-zstd,lld,tools`) construyendo además la
variante Debug puede consumir más de 100 GB en `vcpkg/buildtrees` antes de
fallar por falta de espacio. El comando de arriba (features acotadas +
triplet `-release`) usó ~24 GB y tardó ~2.1 h en esta máquina.

En Windows, `.\compilar_latino.ps1` hace los tres pasos (instalar LLVM,
configurar y compilar) en uno solo — ver [Construir](#construir). También se
puede hacer a mano, tras instalar LLVM con `install_llvm.ps1`:
```powershell
cmake -B build-llvm -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-release
cmake --build build-llvm --config Release
```
(`CMAKE_TOOLCHAIN_FILE` debe fijarse en la primera configuración de un
directorio de build — no se puede inyectar en un `build/` ya configurado sin
LLVM, de ahí el directorio separado `build-llvm`.)
