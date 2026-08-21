# latino-ia

Compilador para el lenguaje de programación **Latino**, escrito en C++17.

La especificación del lenguaje está en [SINTAXIS.md](SINTAXIS.md).

## Estado actual — backend de C 100 % implementado ✅ · backend LLVM en desarrollo 🚧

El compilador transpila código `.lat` → C → ejecutable nativo (backend
`--backend=c`, predeterminado). Todas las librerías estándar documentadas en
el [Manual-Latino](https://github.com/lenguaje-latino/Manual-Latino) están
implementadas y cubiertas por pruebas E2E, incluyendo tipado gradual
opcional y Programación Orientada a Objetos (clases, herencia, interfaces,
estructuras). Un segundo backend basado en LLVM está en desarrollo activo —
ver [Backend LLVM](#backend-llvm-en-desarrollo) más abajo.

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
| 29   | Backend LLVM — en desarrollo, ver abajo (L0-L1 y L2 completas) | #27, #28 |

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
| `input/`          | Planes de trabajo ([PLAN_BASE.md](input/PLAN_BASE.md), [PLAN_LIBS.md](input/PLAN_LIBS.md), [PLAN_POO.md](input/PLAN_POO.md), [PLAN_LLVM.md](input/PLAN_LLVM.md)). |
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
  (incluye tipado gradual y POO: `test_tipado`, `test_poo`).
- E2E para cada programa de `ejemplos/` (compila, ejecuta y compara salida),
  incluyendo `test_poo_e2e`.
- Suites de cobertura por librería: `test_lib_cadena`, `test_lib_lista`, `test_lib_dic`,
  `test_lib_mate`, `test_lib_sis`, `test_lib_archivo`, `test_funciones_base`, `test_incluir`.
- `test_codegen_llvm`: mecanismo de ABI del backend LLVM (Fase L2, solo se
  registra si el build tiene `LATINO_LLVM_BACKEND` habilitado).

## Backend LLVM (en desarrollo)

Segundo backend de generación de código basado en LLVM (`--backend=llvm`),
que convive con el backend actual de C (`--backend=c`, que sigue siendo el
predeterminado). Plan completo con las 13 fases (L0-L13) en
[input/PLAN_LLVM.md](input/PLAN_LLVM.md).

**Estado:** L0 y L1 completas y verificadas con LLVM real (plumbing de
build + enlace: `latino --backend llvm ejemplos/hola.lat` produce un
ejecutable funcional). L2 completa y verificada: el layout real de
`LatValor` y las firmas del runtime se derivan de IR generado por Clang
(`tools/abi_probe.c` → `generated/runtime_abi.ll`) en vez de reconstruirse a
mano — necesario porque, en Windows x64/MSVC, `LatValor` se pasa/retorna por
puntero, no por valor. Los recorridos reales del AST (literales, control de
flujo, funciones, FFI, POO) llegan en las fases L3-L8, todavía no
implementadas.

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

Para configurar el proyecto con esa instalación:
```powershell
cmake -B build-llvm -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-release
cmake --build build-llvm --config Release
```
(`CMAKE_TOOLCHAIN_FILE` debe fijarse en la primera configuración de un
directorio de build — no se puede inyectar en un `build/` ya configurado sin
LLVM, de ahí el directorio separado `build-llvm`.)
