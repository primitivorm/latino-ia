# Plan de trabajo — Migración del backend de Latino a LLVM

## Contexto

Latino transpila hoy `.lat → C → compilador de C del sistema → ejecutable` (ver
[CLAUDE.md](../CLAUDE.md) y [PLAN_BASE.md](PLAN_BASE.md)). El generador
(`GeneradorC`, [src/compiler.cpp](../src/compiler.cpp)) recorre el AST con
cascadas de `dynamic_cast` y emite **texto C** que llama a un runtime dinámico
en C ([runtime/latino.h](../runtime/latino.h)/[.c](../runtime/latino.c) +
`runtime/libs/*`). Ese texto se compila con `cl.exe`/`gcc`/`clang`
(`src/invocador_c.cpp`) para producir el binario final.

Este plan reemplaza esa etapa de generación por un backend que emite código
nativo vía **LLVM**, manteniendo intacto todo lo demás (lexer, parser, AST,
análisis semántico, runtime en C, librerías estándar). El objetivo no es
"reescribir Latino" — es sustituir un backend por otro, de la forma más segura
posible, reutilizando al máximo lo que ya funciona y está probado.

### Motivación

- Evitar que el paso final de cada compilación dependa de invocar un
  compilador de C completo (`cl.exe`/`gcc`) sobre texto generado; en su lugar,
  `latino.exe` mismo produce código máquina (objeto nativo) en el propio
  proceso, usando la API de LLVM.
- Abrir la puerta a optimizaciones sobre el código *del usuario* (inlining,
  pases de LLVM) y a un futuro modo de ejecución interactivo (JIT).
- Sentar las bases para trabajo futuro de especialización de tipos (el
  cuello de botella real de rendimiento en Latino — el *boxing* dinámico de
  `LatValor` — queda fuera de alcance de este plan, ver "Fuera de alcance").

### Decisiones de diseño

1. **Se reemplaza SOLO el backend de codegen.** `runtime/latino.c` y
   `runtime/libs/*.c` no cambian una línea; se siguen compilando con el
   compilador de C del sistema y se enlazan como hoy. El nuevo generador
   (`GeneradorLLVM`) trata las ~146 funciones `lat_*` del runtime exactamente
   como las trata hoy `GeneradorC`: como una frontera FFI estable a la que se
   hacen llamadas, nunca reimplementada en IR.
2. **Ambos backends convivirán indefinidamente**, seleccionables con
   `--backend=c|llvm` (decisión del usuario). `GeneradorC` no se retira como
   parte de este plan.
3. **API de LLVM: C++ nativa** (`llvm::IRBuilder`, `llvm::Module`,
   `llvm::Function`, decisión del usuario), no la API-C de LLVM. Esto es más
   ergonómico pero introduce un riesgo real de compatibilidad de ABI de C++
   entre `latino.exe` y las bibliotecas de LLVM enlazadas — ver
   "Retos técnicos § ABI de C++ de LLVM" para la mitigación obligatoria.
4. **Alcance AOT + JIT.** Además de emitir un ejecutable (como hoy), este plan
   agrega un modo de ejecución JIT vía LLVM ORC (`--jit`), que reutiliza el
   mismo `llvm::Module` que construye el backend AOT.
5. **Retrocompatibilidad total**: con `--backend=c` (default hasta que LLVM
   alcance paridad), el comportamiento de `latino` es idéntico al actual. Los
   22 ejemplos de `ejemplos/*.lat` y toda la suite de CTest existente no
   cambian.
6. **Fuera de alcance de este plan** (candidatos a planes futuros
   independientes, para no inflar el riesgo de esta migración):
   - Conteo de referencias real (ARC) — hoy `GeneradorC` nunca emite
     `lat_valor_retener`/`lat_valor_liberar`; `GeneradorLLVM` replica ese
     mismo comportamiento (memoria no liberada, aceptable para programas
     cortos, documentado ya en el runtime).
   - Vtablas estáticas / despacho no dinámico para POO — se mantiene el
     modelo actual de diccionarios (`lat_obj_llamar_metodo`).
   - Especialización de tipos / unboxing de `LatValor`.
   - Retiro de `GeneradorC` (`PLAN_RETIRO_BACKEND_C.md`, plan de seguimiento
     futuro, no antes de que `--backend=llvm` haya sido el *default* sin
     regresiones durante un ciclo de uso real).

## Arquitectura

```
archivo.lat → Lexer → Parser (AST) → Análisis semántico
                                          │
                         ┌────────────────┴────────────────┐
                         │ --backend=c (default)           │ --backend=llvm
                         ▼                                  ▼
                   GeneradorC (texto C)              GeneradorLLVM (llvm::Module)
                         │                                  │
                         ▼                       ┌──────────┴──────────┐
              invocador_c.cpp                     │ --jit               │ (AOT, default)
        (cl.exe/VsDevCmd o cc/gcc/clang)          ▼                     ▼
                         │                LLJIT (ORC) ejecuta     TargetMachine emite
                         │                en el propio proceso    .obj/.o en proceso
                         │                       │                      │
                         └───────────┬───────────┘                     ▼
                                     │                     invocador_c.cpp (extendido:
                                     ▼                     acepta .c y/o .obj mixtos)
                              runtime/latino.c + runtime/libs/*.c
                          (compilado por el toolchain de C del sistema,
                           enlazado o resuelto en el proceso JIT)
```

Los dos backends comparten AST, análisis semántico y runtime. Solo cambia qué
recorre el AST y qué produce.

### Decisión 1 — API de LLVM y toolchain (riesgo de ABI de C++)

LLVM ofrece dos superficies: la API-C (`llvm-c/*.h`, funciones `extern "C"`
sobre punteros opacos, con compromiso de estabilidad de fuente entre
versiones) y la API C++ nativa (`IRBuilder`, `Module`, etc., más ergonómica
pero **sin garantía de ABI estable** entre versiones de LLVM ni entre
toolchains distintos).

Por decisión del usuario, este plan usa la **API C++ nativa**. Esto exige una
mitigación explícita, porque si `latino.exe` (compilado con `cl.exe`/VS2022 en
Windows, o `gcc`/`clang` en Linux/macOS) enlaza bibliotecas de LLVM compiladas
con un toolchain distinto (versión de MSVC distinta, ABI de STL distinta,
configuración de excepciones/RTTI distinta), el resultado son crashes o
corrupción silenciosa difíciles de diagnosticar — no un error de enlace claro.

**Mitigación obligatoria (documentar en README/CLAUDE.md):**

| Plataforma | Cómo obtener LLVM | Por qué |
|---|---|---|
| Windows | **vcpkg**, vía [`install_llvm.ps1`](../install_llvm.ps1) (`llvm[core,clang,target-x86]:x64-windows-release`), con el mismo triplet/toolset MSVC que ya usa el proyecto vía `VsDevCmd.bat`/`cl.exe` | vcpkg compila LLVM con el toolchain de Visual Studio detectado, garantizando ABI de C++ coincidente con `latino.exe` |
| Linux | Paquete `llvm-<N>-dev` de la distro (ej. `apt install llvm-18-dev`) | Compilado con el mismo GCC/Clang de sistema que compilará `latino.exe` |
| macOS | `brew install llvm` | Igual razonamiento; requiere apuntar `CMAKE_PREFIX_PATH` porque Homebrew no lo pone en el PATH por defecto |

> **Aviso de espacio en disco (Windows/vcpkg)**: el feature set por defecto
> del puerto `llvm` (`clang,default-targets,enable-bindings,enable-terminfo,
> enable-zlib,enable-zstd,lld,tools`) construyendo además la variante Debug
> puede consumir más de 100 GB en `vcpkg/buildtrees` antes de fallar por
> falta de espacio (ocurrió al ejecutar esta fase: `vcpkg install
> llvm:x64-windows` falló tras 3.8 h con "No space left on device"). La
> combinación de arriba (features acotadas a `core,clang,target-x86` +
> triplet `x64-windows-release`, que solo construye Release) usó ~24 GB y
> tardó ~2.1 h.

- Fijar una **versión concreta de LLVM: 18.x** (18.1.6 al momento de
  ejecutar esta fase — el registro de vcpkg ya no ofrecía 17.x como versión
  por defecto; ver nota de reevaluación abajo, exactamente el caso que
  anticipaba).
- `find_package(LLVM 18.1 CONFIG REQUIRED)` en CMake — nunca "cualquier versión
  ≥ X" sin acotar.
- Nueva opción de CMake `LATINO_LLVM_BACKEND` (`ON` si `find_package(LLVM)`
  tiene éxito; si no, se compila igual pero sin `--backend=llvm` disponible,
  con un `message(WARNING ...)` claro).
- **No vendorizar LLVM** en el repo (cientos de MB–GB compilados); se trata
  como dependencia externa, igual que hoy se trata el compilador de C.

### Decisión 2 — ABI de `LatValor`

`LatValor` es un struct-por-valor de 16 bytes con una unión interna, pasado y
retornado **por valor** en las ~146 funciones del runtime. La clasificación
ABI de agregados por valor difiere entre Windows x64 y SysV x86-64, y
adivinarla a mano en el generador LLVM es exactamente el tipo de bug que
"funciona por casualidad" en una plataforma y corrompe memoria en otra.

**Mecanismo: no derivar el ABI a mano — extraerlo de Clang**, la misma
familia de herramientas que compilará `runtime/latino.c`:

1. Nuevo `tools/abi_probe.c`: incluye `runtime/latino.h` y todos los
   `runtime/libs/*.h`, declara una función identidad
   `LatValor lat_valor_layout_probe(LatValor v) { return v; }` para forzar a
   Clang a materializar el layout completo.
2. Nuevo paso de build (`add_custom_command` en CMake) ejecuta
   `clang -S -emit-llvm -target <triple-real> tools/abi_probe.c -o
   generated/runtime_abi.ll`, usando el triple real del toolchain de destino
   (en Windows, `x86_64-pc-windows-msvc`, no el triple genérico de MinGW).
3. `GeneradorLLVM` **importa** `runtime_abi.ll` (parseándolo con
   `llvm::parseIRFile` y usando `llvm::Linker::linkModules` o extrayendo el
   `StructType`/las declaraciones de función de ahí) en vez de construir
   `%LatValor` y las firmas a mano con `StructType::create`/`FunctionType::get`.
   Así la clasificación ABI la decide siempre Clang — la misma autoridad que
   compila el runtime real — y los dos lados no pueden discrepar.
4. **Defensa adicional en runtime**: nueva función `lat_abi_verificar()` en
   `runtime/latino.c`/`.h` con aserciones sobre `sizeof(LatValor)`,
   `_Alignof(LatValor)` y `offsetof(LatValor, tipo)`; se invoca al inicio de
   todo `main` generado (junto a `lat_set_args`, que ya se llama ahí hoy). Si
   el struct cambia sin regenerar `runtime_abi.ll`, el programa falla con un
   mensaje claro en vez de corromper memoria en silencio.
5. Este mecanismo requiere Clang **solo en la máquina que compila
   `latino.exe`** (o en CI), nunca en la máquina del usuario final que
   compila un `.lat`. Si no hay Clang disponible en build-time, se puede
   versionar `runtime_abi.ll` como artefacto regenerable (a decidir en la
   Fase L2 según conveniencia del entorno de build del proyecto).

### Decisión 3 — De LLVM IR a ejecutable (AOT)

Usar `llvm::TargetMachine::addPassesToEmitFile` para emitir el objeto nativo
(`.obj` en Windows/COFF, `.o` en ELF/Mach-O) **en el propio proceso de
`latino.exe`**, sin depender de tener `clang` instalado en la máquina del
usuario final (evita duplicar procesos externos y el roundtrip por texto
`.ll`). El objeto resultante se pasa a la infraestructura de enlace que
**ya existe** en `src/invocador_c.cpp` (localización de `VsDevCmd`/`cl.exe` en
Windows, `cc`/`gcc`/`clang` en otros sistemas), extendida para aceptar una
lista mixta de entradas (`.c` de usuario **o** `.obj`/`.o` ya compilado).

Pasos previos obligatorios antes de emitir: `LLVMInitializeNativeTarget()`,
`LLVMInitializeNativeAsmPrinter()`, y **`llvm::verifyModule()`** — nunca emitir
sin verificar primero (un módulo inválido produce errores opacos del backend
de codegen o, peor, un objeto corrupto que el linker rechaza sin explicación
útil).

### Decisión 4 — Modo JIT (LLVM ORC)

Nueva bandera `--jit`: ejecuta el `.lat` directamente en memoria vía
`llvm::orc::LLJIT`, sin pasar por objeto + enlazador. Reutiliza el **mismo**
`llvm::Module` que construye `GeneradorLLVM` para AOT — el JIT es un
consumidor alternativo del módulo, no un generador distinto.

El runtime debe ser resoluble desde el proceso JIT. Opción recomendada:
enlazar `runtime/latino.c` + `runtime/libs/*.c` **estáticamente dentro del
propio `latino.exe`** (nuevo target CMake `latino_runtime_estatico`), de forma
que sus símbolos ya estén presentes en el proceso; registrar
`llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess()` en el
`LLJIT` para que ORC los resuelva por nombre. Alternativa (si enlazar el
runtime estáticamente en `latino.exe` es indeseable): compilar el runtime como
biblioteca dinámica (`latino_runtime.dll`/`.so`) y usar
`llvm::sys::DynamicLibrary::LoadLibraryPermanently` antes de crear el `LLJIT`.

## Fases

| Fase | Descripción |
|---|---|
| L0 | Preparación (sin código): versión de LLVM, documentación de instalación |
| L1 | Integración de LLVM en el build + smoke test "hola mundo" |
| L2 | Mecanismo de ABI (`abi_probe.c`, `runtime_abi.ll`, `lat_abi_verificar`) |
| L3 | Tipos, literales y esqueleto de expresión |
| L4 | Variables locales, asignación, expresiones compuestas |
| L5 | Control de flujo (si/desde/mientras/repetir/elegir/romper) |
| L6 | Funciones de usuario y variádica |
| L7 | Llamadas a runtime y librerías (FFI) |
| L8 | POO: clases, estructuras, interfaces, herencia, `este`/`nuevo`/`es`/`base` |
| L9 | Driver AOT: flag `--backend`, extensión de `invocador_c.cpp`, `--solo-ir` |
| L10 | Modo JIT vía LLVM ORC (`--jit`) |
| L11 | Tests (unitarios de codegen LLVM + E2E comparativo contra backend C) |
| L12 | Paridad y decisión de default |
| L13 | *(fuera de alcance temporal)* Retiro de `GeneradorC` — plan de seguimiento |

### Fase L0 — Preparación ✅

- [x] Fijar LLVM 18.x como versión objetivo.
- [x] Documentar instalación por plataforma (tabla de la Decisión 1) en
  [README.md](../README.md) y [CLAUDE.md](../CLAUDE.md).
- Sin cambios de código.

### Fase L1 — Integración de LLVM en el build ✅ verificada con LLVM real

**Archivos:** `CMakeLists.txt`, `src/CMakeLists.txt`, `include/compiler_llvm.h`
(nuevo), `src/compiler_llvm.cpp` (nuevo), `include/invocador_llvm.h` (nuevo),
`src/invocador_llvm.cpp` (nuevo), `src/main.cpp` (flag `--backend=c|llvm`).

- [x] `find_package(LLVM 18.1 CONFIG QUIET)` condicionado a la opción
  `LATINO_LLVM_BACKEND` (`ON` por defecto; se apaga sola con un `WARNING` si
  no se encuentra LLVM, para no romper el build del backend C en máquinas
  sin LLVM instalado).
- [x] `GeneradorLLVM` mínimo (`compiler_llvm.cpp`): emite un módulo
  hardcodeado con un `main` que llama a `puts()` de la libc — **no** a
  `lat_escribir`/`lat_cadena` del runtime, deliberadamente, para no
  necesitar todavía la ABI de `LatValor` (eso es la Fase L2). Sin tocar el
  AST real; solo valida el plumbing de compilación + enlace end-to-end.
- [x] `invocador_llvm.cpp`: inicializa targets nativos, crea
  `TargetMachine`, emite `.obj`/`.o` vía `addPassesToEmitFile`, y reutiliza
  `compilarAEjecutable` (`invocador_c.cpp`) sin modificarlo — cl.exe/gcc ya
  aceptan un objeto donde hoy se les pasa un `.c` (confirmado al leer
  `invocador_c.cpp`: el argumento se inserta en la línea de comandos sin
  lógica dependiente de la extensión). El refactor "lista mixta de
  entradas" previsto originalmente para la Fase L9 no hizo falta por esto
  mismo — se deja anotado como hallazgo, no como trabajo pendiente.
- [x] `main.cpp`: nuevo flag `--backend c|llvm`; con `llvm` y sin
  `LATINO_CON_LLVM` definido (LLVM no encontrado en el build), imprime un
  error claro en vez de fallar en tiempo de compilación.

**LLVM instalado y build verificado de punta a punta** (LLVM 18.1.6 vía
vcpkg, triplet `x64-windows-release`, features `core,clang,target-x86`).
Hallazgos reales encontrados al compilar contra LLVM de verdad (los dos que
ya se anticipaban en los comentarios del código, más uno nuevo):

1. **`find_package(LLVM 18 CONFIG)` con solo el major FALLA.** El archivo de
   compatibilidad de versión de LLVM exige coincidencia de major**.minor**,
   no solo major — `find_package` reporta "considered but not accepted"
   incluso con la versión correcta instalada (18.1.6). Corregido pidiendo
   `find_package(LLVM 18.1 CONFIG)` explícitamente. **Implicación para
   fases futuras**: cualquier bump de versión de LLVM en este proyecto debe
   actualizar el pin a `major.minor`, no solo `major`.
2. **El enum de tipo de archivo cambió de nombre**: no es `llvm::CGFT_ObjectFile`
   (API vieja) sino `llvm::CodeGenFileType::ObjectFile` (`enum class` en
   `llvm/Support/CodeGen.h`) en LLVM 18.x. Corregido en `invocador_llvm.cpp`.
3. **Nuevo, no anticipado — bloqueo de archivo en Windows**: dejar el
   `llvm::raw_fd_ostream` del objeto emitido abierto (sin destruir/cerrar)
   mientras se invoca a `cl.exe`/`link.exe` para el enlace hace que el
   linker falle con `LNK1104: no se puede abrir el archivo`, porque Windows
   no permite que otro proceso abra un archivo que el propio `latino.exe`
   todavía tiene abierto para escritura. Corregido envolviendo el
   `raw_fd_ostream` y el `PassManager` en un bloque `{ }` para forzar su
   destrucción (y el cierre del archivo) antes de llamar a
   `compilarAEjecutable`. También se cambió la extensión del objeto temporal
   de `.o` a `.obj` para evitar una advertencia cosmética de `cl.exe`
   (D9024) sobre una extensión no reconocida.
4. `getDefaultTargetTriple` en `llvm/TargetParser/Host.h` (no
   `llvm/Support/Host.h`) resultó ser la ubicación correcta para LLVM 18.x
   — el comentario de precaución original acertó con esta.

**Instalación usada para verificar** (automatizada luego en
[`install_llvm.ps1`](../install_llvm.ps1), ver también README.md/CLAUDE.md):
`vcpkg install "llvm[core,clang,target-x86]:x64-windows-release"` (~24 GB,
~2.1 h). El primer intento con `vcpkg install llvm:x64-windows` (features
por defecto completas + variante Debug) agotó ~105 GB de disco antes de
fallar — ver el aviso de espacio en disco en README.md. `install_llvm.ps1`
incluye una comprobación de espacio libre (`-MinFreeSpaceGB`, 60 GB por
defecto) para detectar este problema antes de empezar en vez de después de
horas de build.

**Criterio de aceptación — cumplido:** `latino --backend llvm
ejemplos/hola.lat -o hola_llvm.exe --runtime runtime` compila sin errores y
`hola_llvm.exe` imprime `hola LLVM`. Confirmado además que `--backend=c`
(default) sigue produciendo el mismo resultado que antes en el mismo build
con LLVM habilitado (`hola mundo`), y que la suite CTest completa (43/43)
sigue pasando con `LATINO_LLVM_BACKEND=OFF` (build sin LLVM instalado).

### Fase L2 — Mecanismo de ABI ✅ verificada con LLVM real

**Archivos:** `tools/abi_probe.c` (nuevo), `CMakeLists.txt` (nuevo
`add_custom_command` + `find_program(clang)`), `src/CMakeLists.txt`
(dependencia de `latino` sobre el target `latino_runtime_abi`, componente
`irreader`), `src/config.h.in`/`config.h` (nueva macro
`LATINO_RUNTIME_ABI_LL`), `runtime/latino.h`/`.c` (agregada
`lat_abi_verificar()`), `include/runtime_abi_llvm.h` + `src/runtime_abi_llvm.cpp`
(nuevo, clase `RuntimeAbiLLVM`), `tests/test_codegen_llvm.cpp` (nuevo),
`tests/CMakeLists.txt` (registro condicional bajo `LATINO_LLVM_BACKEND`).

- [x] Implementado el mecanismo descrito en la Decisión 2 completo.
- [x] `tools/abi_probe.c` incluye `latino.h` + los 7 headers de
  `runtime/libs/` y referencia (sin llamar, `(void)fn;`) las 185 funciones
  `lat_*` declaradas ahí, para forzar a Clang a emitir un `declare` de cada
  una — un `#include` por sí solo **no** genera IR para símbolos nunca
  referenciados (hallazgo real, no anticipado explícitamente en el plan
  original).
- [x] `CMakeLists.txt` (raíz): dentro del bloque que ya localizaba LLVM,
  `find_program(LATINO_CLANG_EXECUTABLE clang HINTS ${LLVM_TOOLS_BINARY_DIR})`
  — el mismo Clang que trae el feature `clang` del puerto de vcpkg (ver
  `install_llvm.ps1`, Fase L0/L1). Si no se encuentra, apaga
  `LATINO_LLVM_BACKEND` con un `WARNING`, igual que cuando LLVM mismo no
  aparece. Un `add_custom_command` invoca
  `clang -S -emit-llvm -target x86_64-pc-windows-msvc -I<runtime> abi_probe.c
  -o <build>/generated/runtime_abi.ll` (target fijo en Windows/MSVC per
  Decisión 2; en otras plataformas se usa el triple por defecto del
  Clang/GCC de sistema), envuelto en `add_custom_target(latino_runtime_abi)`
  del que depende el target `latino`.
- [x] `RuntimeAbiLLVM` (`runtime_abi_llvm.h`/`.cpp`) parsea
  `generated/runtime_abi.ll` con `llvm::parseIRFile` en el mismo
  `LLVMContext` que use el módulo destino — los tipos LLVM son propiedad del
  `LLVMContext`, no del `Module`, así que `tipoLatValor()` (el
  `%struct.LatValor` real, tal como Clang lo clasificó) y las
  `llvm::FunctionType*` de `declarar()` se reutilizan directamente sin
  reconstruirlos a mano. `declarar()` también copia `AttributeList` y
  `CallingConv` del origen — necesario porque el atributo `sret` del
  retorno indirecto de `LatValor` no es parte del `FunctionType`, es un
  atributo aparte, y sin copiarlo el módulo sigue siendo IR válido pero
  pierde la anotación que documenta la clasificación ABI real.
- [x] `lat_abi_verificar(tam, alineacion, offset_tipo)` agregada al runtime:
  compara `sizeof/alignof/offsetof(LatValor, tipo)` reales (según el
  compilador de C que construyó `latino.c`) contra los valores que
  `GeneradorLLVM` habrá derivado de `runtime_abi.ll`; términa el programa
  con un mensaje claro si no coinciden. Queda lista para invocarse desde el
  `main` generado a partir de la Fase L3+ (aún no hay recorrido de AST real
  en `GeneradorLLVM` que la use).

**Hallazgo real (no anticipado en el plan original) — ABI real observada para
Windows x64/MSVC:** para `LatValor lat_sumar(LatValor a, LatValor b)`, Clang
NO pasa los parámetros por valor como un tipo struct de LLVM ni usa el
atributo `byval` — genera `declare void @lat_sumar(ptr sret(%struct.LatValor)
align 8, ptr noundef, ptr noundef)`: tanto el retorno como **ambos
parámetros** de tipo `LatValor` se pasan como punteros simples (el ABI de
paso-por-referencia ya está resuelto en el IR que emite el frontend de
Clang, no queda ninguna clasificación adicional para que un backend genérico
de LLVM decida). Los escalares (`double`, `int`, `char*`) sí se pasan
directamente sin indirección (ver `lat_numero(double n)`). Esto confirma en
la práctica el riesgo que motivó toda la Decisión 2: construir estas firmas
a mano fácilmente habría asumido "pasar `%struct.LatValor` por valor
directamente", que es exactamente incorrecto para este target — de ahí que
importar el `FunctionType` real de Clang (en vez de reconstruirlo) sea la
única vía segura. Implicación directa para la Fase L3+: toda "LatValue SSA"
del generador debe modelarse como un puntero a una celda de memoria
(`alloca %LatValor`), nunca como un valor LLVM de tipo struct por registro —
es el único modelo consistente con esta ABI en ambas direcciones (llamar al
runtime y, más adelante, generar los métodos/funciones de usuario con la
misma convención).

**Criterio de aceptación — cumplido:** `tests/test_codegen_llvm.cpp`
construye un módulo que declara `lat_numero`/`lat_sumar` vía `RuntimeAbiLLVM`
y una función trivial (`sumar_5_y_3`) que llama a `lat_numero(5)`,
`lat_numero(3)`, `lat_sumar(...)`, lee el campo `numero` del resultado por
`CreateStructGEP` sobre el `%struct.LatValor` importado, y lo retorna como
`i32`; `llvm::verifyModule` pasa sin errores. Además de la comparación de
`sizeof`, el test compara alineación (`alignof`) y el offset del campo
`tipo` (`offsetof`) entre lo que ve el compilador de C++ del proyecto y lo
que derivó Clang — las tres coinciden. Verificación extra manual (fuera del
test, para confirmar corrección semántica real y no solo validez sintáctica
de IR): un `.ll` equivalente escrito a mano, compilado con `llc
-filetype=obj` y enlazado con `runtime/latino.c` + `runtime/libs/*.c` vía
`cl.exe`, produce un ejecutable que imprime `resultado=8` — confirma que la
convención de llamada generada es *ejecutablemente* correcta, no solo válida
para el verificador de LLVM. Suite completa (`ctest -C Release`) sigue
pasando con `LATINO_LLVM_BACKEND=ON` (43 suites existentes + la nueva
`test_codegen_llvm`).

### Fase L3 — Tipos, literales y esqueleto de expresión ✅ verificada con LLVM real

**Archivos:** `include/compiler_llvm.h` / `src/compiler_llvm.cpp` (nuevo método
`GeneradorLLVM::genExpr`), `tests/test_codegen_llvm.cpp` (ampliado con las
pruebas de esta fase; ya existía desde la Fase L2), `tests/CMakeLists.txt`
(agrega `src/ast.cpp` y `src/compiler_llvm.cpp` a las fuentes del target).

- [x] `LitNumero`/`LitCadena`/`LitLogico`/`LitNulo` → `CreateCall` a
  `lat_numero`/`lat_cadena`/`lat_logico`/`lat_nulo` (declaradas vía
  `RuntimeAbiLLVM::declarar`, Fase L2), mapeo 1:1 con lo que hoy hace
  `GeneradorC::genExpr`.
- [x] `Identificador` → devuelve directamente el puntero de su celda ya
  declarada (tabla `nombre → llvm::Value*` que arma quien llama; la
  infraestructura real de declaración de variables llega en la Fase L4).
- [x] Arnés de test ampliado en `test_codegen_llvm.cpp`: `Module::print` a
  `std::string` + comprobación de subcadenas (equivalente a `contiene(...)`
  de `test_codegen.cpp`), más `verifyModule` en cada caso.

**Decisión de esta fase — representación uniforme por puntero (ajusta la
redacción original "`Identificador` → `CreateLoad`"):** el texto original de
esta fase asumía que un identificador se lee con `CreateLoad` (produciendo
un valor `%struct.LatValor` de primera clase, "por registro"). Se optó en
cambio por que **toda** "LatValue" que maneja `GeneradorLLVM` sea siempre un
`llvm::Value*` que apunta a una celda `%struct.LatValor` -- nunca un valor
cargado por registro -- y que `genExpr` de un identificador devuelva el
puntero de su celda tal cual, sin `CreateLoad`. Motivo: el hallazgo de ABI
de la Fase L2 (en Windows x64/MSVC, `LatValor` se pasa/retorna **siempre**
por puntero al cruzar hacia una función del runtime real) y el hecho de que
las funciones/métodos de usuario de la Fase L8 usarán la misma firma
uniforme por puntero (`%LatValor* @lat_fn_<Clase>_<metodo>(i32, %LatValor*)`)
hacen que cargar a un valor por registro solo para volver a escribirlo a una
celda temporal antes de cada llamada al runtime sea trabajo puro sin
beneficio -- mantener el puntero de punta a punta es más simple y evita esa
ida y vuelta en cada expresión.

**Criterio de aceptación — cumplido:** cinco pruebas nuevas
(`prueba_l3_lit_numero`, `_lit_cadena`, `_lit_logico`, `_lit_nulo`,
`_identificador`) construyen cada una un módulo aislado, invocan
`GeneradorLLVM::genExpr` sobre un nodo de AST construido directamente (sin
pasar por el parser), verifican por subcadena de IR que se emitió la llamada
esperada (`call void @lat_numero(`, el literal `c"hola\00"` incrustado,
`i32 1` para `cierto`, etc.) y que `llvm::verifyModule` pasa. La prueba de
`Identificador` confirma explícitamente que se devuelve el mismo puntero de
la celda ya declarada, sin `load` de por medio. Suite completa (`ctest -C
Release`) sigue pasando con `LATINO_LLVM_BACKEND=ON` (44 suites, sin
regresiones); `test_codegen_llvm` pasa con 26 comprobaciones (las 3 de L2 +
las nuevas de L3). Además, `latino --backend llvm ejemplos/hola.lat`
(todavía el esqueleto "hola mundo" de la Fase L1, sin tocar) sigue
funcionando de punta a punta -- confirma que construir `RuntimeAbiLLVM`
dentro del constructor de `GeneradorLLVM` (nuevo en esta fase) no rompe el
camino existente.

### Fase L4 — Variables locales, asignación, expresiones compuestas ✅ verificada con LLVM real

**Archivos:** `src/compiler_llvm.cpp`/`include/compiler_llvm.h` (ampliados),
nuevo `include/recolector_variables.h` + `src/recolector_variables.cpp`
(extraídos del `namespace` anónimo de `src/compiler.cpp`, función
`colectar()`/`colectarLista()`; el método `GeneradorC::recolectarVariables`
ahora es un `#include` a la función libre), nuevo `include/fn_binaria.h` +
`src/fn_binaria.cpp` (misma extracción para la tabla operador→función que
antes vivía en el `namespace` anónimo de `compiler.cpp`), `include/compiler.h`
(quita el método `recolectarVariables`, ya no hace falta — las llamadas sin
calificar del cuerpo de la clase resuelven directo a la función libre),
`src/CMakeLists.txt`/`tests/CMakeLists.txt` (agregan los dos archivos nuevos a
`latino` y a todos los targets de prueba que ya enlazaban `compiler.cpp` o
`compiler_llvm.cpp`), `tests/test_codegen_llvm.cpp` (ampliado con 48 pruebas
nuevas).

- [x] `Binaria`/`Unaria`/`PostOperador`/`Ternaria`/`AccesoIndice`/
  `AccesoMiembro`/`ListaLiteral`/`DiccionarioLiteral`/`VarArgs` → `genExpr`
  traduce los ocho a llamadas al runtime, reutilizando `fnBinaria` (ahora
  compartida con `GeneradorC`). Un operador binario desconocido devuelve
  `nullptr` (nunca ocurre con AST producido por el parser real; el caso
  existe solo por paridad defensiva con `GeneradorC`, que en ese caso cae a
  `lat_nulo()` — aquí se prefirió `nullptr`, consistente con el resto de
  `genExpr` para nodos no soportados, ya que no hay texto C que emitir "de
  todas formas").
- [x] `Ternaria` **no** evalúa ambos lados y elige: emite basic blocks reales
  (`tern_cierto`/`tern_falso`/`tern_fin` con `CreateCondBr`) igual que el
  operador `?:` de C que ya emite `GeneradorC` — evaluar ambos lados
  ejecutaría efectos secundarios del lado no tomado, un cambio de semántica
  observable. Es el primer uso de basic blocks múltiples en `GeneradorLLVM`
  (antes de la Fase L5 formal de control de flujo), porque el propio
  operador ternario lo exige.
- [x] `PostOperador` (`i++`/`i--`) descubrió un caso de aliasing no
  mencionado explícitamente en el texto original del plan: `i++` lee y
  escribe la misma celda. Pasar esa celda como `sret` de `lat_sumar` junto
  con ella misma como operando de entrada sería aliasing entre el puntero de
  salida y uno de entrada. Solución: calcular en una celda temporal
  (`post_tmp`) y copiar (`load`+`store` de `%struct.LatValor`, no una
  llamada) a la celda de la variable — replica exactamente lo que ya hace C
  de forma implícita (`v_i = lat_sumar(v_i, lat_numero(1))` usa un temporal
  oculto del propio lenguaje C para el valor de retorno antes de la
  asignación). Devuelve la celda de la variable (ya actualizada), no el
  temporal — consistente con la representación uniforme por puntero de la
  Fase L3.
- [x] `AccesoMiembro` (`objeto.miembro`) y la asignación a `AccesoMiembro`
  construyen la clave como `lat_cadena("miembro")` y delegan en
  `lat_obtener_indice`/`lat_asignar_indice` — mismo mapeo 1:1 que
  `GeneradorC::genExpr`/`genAsignacionDestino`.
- [x] `ListaLiteral`/`DiccionarioLiteral` llaman a las funciones **variádicas
  reales** del runtime (`lat_lista_de`/`lat_dic_de`, `declare ... (ptr sret,
  i64, ...)` en `runtime_abi.ll`) en vez de un empaquetado alternativo:
  `RuntimeAbiLLVM::declarar` ya trae el `FunctionType` con `isVarArg=true`
  desde Clang, y `IRBuilder::CreateCall` sobre una función variádica acepta
  argumentos extra sin más — no hace falta ningún manejo especial de
  `va_arg` ni de convención de varargs en el llamador (ver también Reto 5,
  Fase L6, para el caso simétrico de *declarar* una función variádica de
  usuario). Cada elemento se pasa como el puntero de su celda, igual que
  cualquier otro argumento `LatValor` — nunca el struct por valor — porque
  esa es la clasificación ABI real que Clang ya fijó para estas funciones en
  la Fase L2 (16 bytes > 8 → indirecto, sin distinción entre parámetro fijo
  y variádico en el ABI de Windows x64/MSVC).
- [x] El caso especial `[...]` (un único elemento `VarArgs` dentro de
  `ListaLiteral`, que significa "la lista de argumentos variádicos", no una
  lista que la contiene) se resuelve igual que `GeneradorC`: `VarArgs` busca
  la celda `"lat_resto"` en `variables` (la Fase L6 la poblará con el
  parámetro variádico real de la función).
- [x] `declararLocales(nombres, entryBuilder, modulo)` (nuevo método público):
  declara un `AllocaInst` de `%struct.LatValor` por nombre e inmediatamente
  lo inicializa con `lat_nulo()`, **en el entry block** vía el
  `entryBuilder` que pasa quien llama (Reto 3: separar el builder de
  `alloca`s del que avanza por basic blocks de control de flujo, para que
  `mem2reg` pueda promover a SSA). Recibe un `std::set<std::string>` ya
  resuelto por `recolectarVariables` — no conoce parámetros ni "este"; fusionar
  esa tabla con la de parámetros es responsabilidad de quien arme la función
  completa (Fase L6).
- [x] `genAsignacion(asign, builder, modulo, variables)` (nuevo método
  público): asignación simple (1 destino/1 valor, con `lat_verificar_tipo`
  interpuesto si hay anotación de tipo del tipado gradual — Fase 27) y
  múltiple (`a, b = 1, 2`), evaluando **todos** los valores a celdas
  temporales antes de asignar cualquier destino — igual que los temporales
  `_tN` de `GeneradorC`, necesario para que `a, b = b, a` intercambie en vez
  de pisarse. `genAsignacionDestino` (privado) traduce el lvalue:
  `Identificador` es una copia de struct dentro del módulo (`load`+`store`,
  nunca una llamada — no cruza la frontera FFI así que no aplica el ABI
  importado); `AccesoIndice`/`AccesoMiembro` llaman a `lat_asignar_indice`.
- [x] Extracción de `recolectarVariables`/`fnBinaria` a código compartido:
  ambas ya se usaban en `GeneradorC` con exactamente el comportamiento que
  necesitaba `GeneradorLLVM` (mismo hoisting total sin scoping por bloque,
  Reto 4 — es una decisión de *lenguaje*, no de *backend*, así que ambos
  generadores deben coincidir en qué variables recolectan; mismo mapeo
  operador→función). Extraerlas evita mantener dos copias que podrían
  divergir silenciosamente entre backends.
- [x] `tipoAnotadoALatTipo` (helper nuevo, anónimo en `compiler_llvm.cpp`)
  mapea `TipoAnotado` a los valores reales del enum `LatTipo` incluyendo
  `runtime/latino.h` directamente (`extern "C"`) en vez de re-derivar los
  valores a mano como texto (que es lo que hace `GeneradorC::tipoALatTipo`,
  ya que ahí basta con emitir el nombre de la macro para que el compilador
  de C la resuelva) — mismo principio de "una sola fuente de verdad" que la
  Decisión 2 aplicó al layout de `LatValor`.

**Criterio de aceptación — cumplido:** 48 pruebas nuevas en
`tests/test_codegen_llvm.cpp` (todas por subcadena de IR + `verifyModule`,
igual que las de la Fase L3) cubren cada nodo nuevo, el caso `[...]` →
`lat_resto`, un operador binario desconocido → `nullptr`, `declararLocales`
(incluye una prueba end-to-end con `recolectarVariables` real sobre un
`ListaSent` construido a mano), asignación simple, tipada (verifica el valor
entero exacto de `LAT_NUMERO` y el número de línea pasados a
`lat_verificar_tipo`) y múltiple (verifica que se crean dos celdas
temporales distintas antes de asignar). Suite completa (`ctest -C Release`):
`test_codegen_llvm` pasa con 74 comprobaciones (26 de L2-L3 + 48 nuevas);
`test_codegen`/`test_tipado`/`test_poo` (que ahora enlazan
`fn_binaria.cpp`/`recolector_variables.cpp` en vez de la copia interna de
`compiler.cpp`) siguen pasando sin cambios de comportamiento — confirma que
la extracción a código compartido no alteró `GeneradorC`.

### Fase L5 — Control de flujo

**Archivos:** `src/compiler_llvm.cpp`, `tests/test_codegen_llvm.cpp`.

- `Si`/`osi`/`sino` → basic blocks `then`/`elseifN`/`else`/`merge` con
  `CreateCondBr`/`CreateBr`.
- `Mientras`/`Desde`/`Repetir` → bloques `header`/`body`/`latch`/`exit`
  estándar; `Desde` traduce `inicio`/`incremento` reutilizando la traducción
  de sentencias ya existente para esas sub-sentencias.
- `Elegir` → cadena de comparaciones (`lat_igual` + `lat_es_verdadero`), **no**
  `SwitchInst` nativo de LLVM: los valores de caso en Latino son expresiones
  dinámicas evaluadas en runtime, no enteros constantes conocidos en tiempo de
  compilación — usar `switch` cambiaría la semántica (y el orden de
  evaluación de efectos secundarios).
- `Romper` → pila de "bloques de salida de bucle" en `GeneradorLLVM` (estado
  nuevo, no existe en `GeneradorC` porque ahí `break;` de C ya resuelve esto).
- Portar a `test_codegen_llvm.cpp` los casos de control de flujo equivalentes
  de `tests/test_codegen.cpp`.

### Fase L6 — Funciones de usuario y variádica

**Archivos:** `src/compiler_llvm.cpp`.

- `FuncionDef` → `Function::Create` con prototipo adelantado (paridad con los
  prototipos que hoy emite `GeneradorC::generarCuerpo` para permitir
  recursión/uso antes de definición).
- Variádica (`...`/`lat_resto`): **no usar varargs nativos de LLVM**; el
  llamador empaqueta los argumentos extra en una lista (`lat_lista_de`) antes
  de invocar, exactamente como ya hace `GeneradorC::genLlamada` — evita tener
  que manejar `va_arg`/intrínsecos de LLVM, que sería mucho más frágil sin
  aportar nada (Latino ya modela "el resto" como una lista en tiempo de
  ejecución).
- `Retornar`/`VarArgs`.

**Criterio de aceptación:** ejemplo E2E con función recursiva (factorial o
fibonacci) produce la misma salida con `--backend=llvm` que con
`--backend=c`.

### Fase L7 — Llamadas a runtime y librerías (FFI)

**Archivos:** `src/compiler_llvm.cpp`.

- Equivalente de `GeneradorC::genLlamada`: built-ins (`escribir`, `imprimir`,
  `leer`, `limpiar`, `acadena`, `tipo`, `error`, `incluir`), `imprimirf`
  (variádica → empaquetado en lista, igual que L6), y las 7 librerías
  (`cadena`, `lista`, `dic`, `mate`, `sis`, `archivo`, `paquete`) vía
  `AccesoMiembro`.
- A diferencia de `GeneradorC` (que detecta `libsUsadas` dinámicamente para
  emitir `#include` selectivos), `GeneradorLLVM` puede `declare` **todas**
  las funciones del runtime desde el arranque (ya disponibles vía
  `runtime_abi.ll` de la Fase L2) sin costo real — `invocador_c.cpp` ya
  enlaza incondicionalmente todos los `.c` de `runtime/libs/`.
- Métodos estáticos de clase/estructura (`NombreClase.metodo(...)`): mismo
  patrón de "empaquetar argumentos en arreglo + llamar función uniforme".

**Criterio de aceptación:** test dedicado que carga un módulo dinámico con
`paquete.cargar`/invoca una función exportada desde un programa compilado con
`--backend=llvm`, confirmando que la ABI derivada en L2 es compatible con
módulos externos compilados por separado.

### Fase L8 — POO

**Archivos:** `src/compiler_llvm.cpp`.

- Traducir 1:1 `GeneradorC::genClase`/`genMetodo`/`genEstructura`/
  `NuevoExpr`/`EsExpr`/`AccesoEste`/`LlamadaBase`, manteniendo el **mismo
  modelo dinámico basado en diccionarios** (`lat_obj_nuevo`,
  `lat_obj_set_metodo`, `lat_obj_llamar_metodo`, `lat_obj_es_instancia`,
  `lat_obj_set_clase` para la cadena de ascendencia). LLVM no introduce
  vtablas estáticas ni despacho por puntero a función en esta fase —
  deliberado, ver "Fuera de alcance".
- Métodos y constructores mantienen la firma uniforme
  `%LatValor @lat_fn_<Clase>_<metodo>(i32 %nargs, %LatValor* %args)`; `este`
  sigue siendo `args[0]` vía `CreateGEP`+`CreateLoad`.
- Mismo esquema de nombres `lat_fn_<Clase>_<metodo>` /
  `lat_fn_<Clase>_<Clase>` (constructor) que ya usa `GeneradorC`. Un módulo
  LLVM exige nombres de función globalmente únicos (como C) — verificar
  explícitamente en esta fase si una clase y una estructura pueden compartir
  nombre hoy sin que el analizador semántico lo detecte (ambas usan el mismo
  prefijo `lat_fn_`, así que colisionarían igual en el backend C actual; si
  no está prohibido, añadir la verificación en `analizador_semantico.cpp`
  como parte de esta fase, no solo en el backend LLVM).

### Fase L9 — Driver AOT

**Archivos:** `src/main.cpp`, `src/invocador_c.cpp`/`include/invocador_c.h`,
`src/invocador_llvm.cpp`/`include/invocador_llvm.h`.

- `main.cpp`: nuevo flag `--backend=c|llvm` (default `c`), despacho a
  `GeneradorC::generar` o `GeneradorLLVM::generar`. El resto del pipeline
  (`--ast`, expansión de `incluir "*.lat"`, análisis semántico) es
  independiente del backend y no cambia.
- `invocador_c.cpp`: refactor para aceptar una lista de *entradas* mixta
  (fuente `.c` de usuario **o** objeto `.obj`/`.o` ya compilado por LLVM) en
  la misma línea de comandos de `cl`/`cc`, sin cambiar la lógica de
  localización de `VsDevCmd`/`cl.exe`/env var `CC`.
- Nueva bandera `--solo-ir` (análoga a `--solo-c`): vuelca el `.ll` textual
  para depuración, sin compilar.

**Criterio de aceptación:** los 22 ejemplos de `ejemplos/*.lat` compilan y
ejecutan con `--backend=llvm` (aún puede haber diffs de salida frente a
`--backend=c` en construcciones no cubiertas hasta L8 — el gate de paridad
real es la Fase L12).

### Fase L10 — Modo JIT (LLVM ORC)

**Archivos:** `src/main.cpp` (flag `--jit`), `src/invocador_llvm.cpp`
(función nueva `ejecutarJit`), `CMakeLists.txt` (target
`latino_runtime_estatico`, ver Decisión 4).

- Implementar la ruta descrita en la Decisión 4: `llvm::orc::LLJIT` +
  `DynamicLibrarySearchGenerator::GetForCurrentProcess()`, resolviendo
  `lat_fn_main` (o el símbolo del `main` generado) y ejecutándolo en proceso.
- Reutiliza el mismo `llvm::Module` de `GeneradorLLVM` — no hay codegen nuevo
  en esta fase, solo un consumidor alternativo del módulo ya construido en
  L3–L8.

**Criterio de aceptación:** `latino --jit ejemplos/hola.lat` imprime la
salida esperada sin generar ningún archivo `.obj`/`.exe` intermedio en disco.

### Fase L11 — Tests

**Archivos:** `tests/test_codegen_llvm.cpp` (nuevo, poblado progresivamente en
L3–L8), `tests/test_e2e_llvm.cpp` (nuevo) o parametrización de
`tests/test_e2e.cpp`, `tests/CMakeLists.txt`.

- `test_codegen_llvm.cpp`: smoke tests estructurales (subcadena de IR +
  `verifyModule`), espejo liviano de `tests/test_codegen.cpp` — no se
  comparan fragmentos de texto C, se comparan fragmentos de IR (más sensible
  a la versión de LLVM, por eso solo subcadenas clave, no el módulo completo).
- E2E comparativo: correr los mismos `ejemplos/*.lat` con ambos backends y
  diffear la **salida de ejecución** (no solo "no crashea").
- Extender el arnés de las suites `test_lib_*`/`test_poo_e2e`/
  `test_funciones_base`/`test_incluir` (`tests/test_harness.h`) para aceptar
  qué backend usar, y correrlas también contra `--backend=llvm`.
- Todo lo anterior registrado en `tests/CMakeLists.txt` bajo
  `if(LATINO_LLVM_BACKEND)` — las máquinas sin LLVM instalado deben poder
  seguir corriendo la suite completa del backend C sin fallos por ausencia
  de LLVM.

### Fase L12 — Paridad y decisión de default

- Ejecutar toda la suite con ambos backends; medir tiempo de compilación
  end-to-end y tiempo/tamaño de los binarios resultantes.
- Criterio objetivo de "paridad" para considerar cambiar el default de
  `--backend` a `llvm`:
  1. Los 22 ejemplos de `ejemplos/*.lat` producen salida idéntica en ambos
     backends.
  2. Todas las suites de librería (`test_lib_*`, `test_poo_e2e`,
     `test_incluir`, `test_funciones_base`) pasan también contra
     `--backend=llvm`.
  3. Sin regresión de tiempo de compilación end-to-end mayor a lo razonable
     frente al backend C.
- Si se cumple, cambiar el default en `main.cpp`; si no, documentar los
  gaps restantes como fases de seguimiento.

### Fase L13 — *(fuera de alcance temporal)* Retiro de `GeneradorC`

No se ejecuta como parte de este plan. Se documenta aquí solo como
referencia: una vez `--backend=llvm` sea el default sin regresiones
reportadas durante un ciclo de uso real, un plan de seguimiento
(`PLAN_RETIRO_BACKEND_C.md`) podría evaluar si retirar `GeneradorC` (archivo
único de 841 líneas, barato de mantener) aporta valor real o si conviene
dejarlo indefinidamente como referencia/fallback de bajo costo — de forma
similar a cómo el proyecto ya mantiene `src/interpreter.cpp` sin usarlo
activamente.

## Retos técnicos detallados

### Reto 1: ABI de `LatValor` por valor en ~150 fronteras de función

**Problema**: cada llamada al runtime pasa/retorna un struct de 16 bytes por
valor; la clasificación ABI (registros vs. paso indirecto) difiere entre
Windows x64 y SysV x86-64, y un mismatch entre lo que emite `GeneradorLLVM` y
lo que espera el `.c` compilado corrompe memoria de forma silenciosa.

**Solución**: mecanismo de sondeo vía Clang descrito en la Decisión 2 — nunca
declarar el tipo/las firmas a mano. Implementar y verificar esto (Fase L2)
**antes** de escribir cualquier codegen real (Fase L3+).

### Reto 2: ABI de C++ entre `latino.exe` y las bibliotecas de LLVM

**Problema**: al usar la API C++ nativa de LLVM (decisión del usuario), un
mismatch de toolchain/STL/ABI entre `latino.exe` y `libLLVM*` produce
crashes o corrupción silenciosa, no errores de enlace claros.

**Solución**: obtener LLVM exclusivamente por los canales de la tabla de la
Decisión 1 (vcpkg en Windows con el mismo triplet MSVC, paquetes de distro en
Linux/macOS), fijar versión concreta (18.x), y documentar explícitamente que
mezclar un LLVM prebuilt genérico de otra fuente con el toolchain del
proyecto no está soportado.

### Reto 3: `alloca` fuera del entry block

**Problema**: LLVM exige (no por sintaxis, sino por convención de
optimización y corrección) que los `alloca` de una función vivan en su entry
block; emitirlos dentro de basic blocks de bucles/condicionales impide la
promoción a SSA (`mem2reg`) y puede causar crecimiento de pila no acotado.

**Solución**: `IRBuilder` secundario fijo en el entry block, usado
exclusivamente para `alloca`s, independiente del builder que avanza por el
control de flujo (Fase L4).

### Reto 4: Hoisting total sin scoping por bloque

**Problema**: `GeneradorC` hoy declara TODAS las variables locales de una
función al inicio (sin scoping por bloque, similar a `var` en JS). Sería
tentador "arreglarlo" al reescribir el backend.

**Solución**: replicar el comportamiento actual sin cambios — es una decisión
de *lenguaje*, no de *backend*; cambiarla es un plan de lenguaje
independiente y no debe mezclarse con esta migración.

### Reto 5: Variádica sin varargs nativos de LLVM

**Problema**: podría parecer que `...` debe mapear a varargs nativos de LLVM
(`va_arg`, intrínsecos `llvm.va_start`/`llvm.va_end`), que son notoriamente
frágiles de generar correctamente a mano.

**Solución**: no hace falta — Latino ya modela "el resto" como una lista
construida por el *llamador* antes de invocar (`GeneradorC::genLlamada`
existente). `GeneradorLLVM` replica exactamente ese empaquetado (Fase L6),
evitando varargs nativos por completo.

### Reto 6: Colisión de nombres de símbolos POO en el módulo LLVM

**Problema**: un módulo LLVM exige nombres de función globalmente únicos,
igual que un archivo objeto C. El esquema `lat_fn_<Clase>_<metodo>` ya lo
asume hoy en el backend C.

**Solución**: verificar en la Fase L8 si el analizador semántico ya impide que
una clase y una estructura (u otras combinaciones) compartan nombre; si no,
añadir esa verificación — es un gap potencial que existe igual en el backend
C actual, esta migración solo lo hace más visible.

### Reto 7: `paquete` (carga dinámica) y el ABI derivado

**Problema**: `runtime/libs/paquete.c` carga módulos `.dll`/`.so` externos en
runtime (`LoadLibrary`/`dlopen`) con la convención
`LatValor fn(int nargs, LatValor* args)`. Si el ABI que asume el código
emitido por LLVM difiriera del que usa un módulo externo compilado por
separado, se rompería la interoperabilidad de forma silenciosa.

**Solución**: no requiere tratamiento especial de codegen — es responsabilidad
exclusiva de `paquete.c`, agnóstico al backend que generó el `.obj`
invocador. El mecanismo de ABI de la Decisión 2 (misma fuente de verdad,
Clang, para todo el proyecto) es la mitigación real; añadir el test explícito
de la Fase L7 para confirmarlo en la práctica.

### Reto 8: `elegir` como comparaciones secuenciales, no `switch` nativo

Ver Fase L5 — usar `SwitchInst` de LLVM rompería la semántica dinámica de
`elegir` (valores de caso evaluados en runtime, no constantes de compilación).

### Reto 9: Verificación de módulo antes de emitir

`llvm::verifyModule()` debe correr siempre antes de
`addPassesToEmitFile`/antes de entregar el módulo al `LLJIT`. Sin esto, bugs
del generador (bloques sin terminador, tipos no coincidentes en una llamada)
producen errores opacos del backend de LLVM o binarios corruptos.

### Reto 10: Resolución de símbolos del runtime en el proceso JIT

**Problema** (nuevo por el modo JIT): si el runtime se enlaza estáticamente en
`latino.exe` pero también existe como `.dll`/`.so` separado para el modo AOT,
hay dos copias del estado global del runtime (si alguna vez lo tuviera) y dos
superficies de símbolos distintas a mantener sincronizadas.

**Solución**: un único target CMake (`latino_runtime_estatico`) que se enlaza
dentro de `latino.exe` para el modo JIT, y sigue reutilizando los mismos
`.c` fuente para el modo AOT (compilados por separado por
`invocador_c.cpp`/`invocador_llvm.cpp` como siempre) — no se comparte binario
entre JIT y AOT, se comparte únicamente el código fuente C, que es la unidad
que ya se considera la fuente de verdad del runtime.

## Archivos críticos para la implementación

- [runtime/latino.h](../runtime/latino.h) — fuente de verdad del ABI de
  `LatValor` y de las firmas del runtime; base del mecanismo de sondeo de
  Clang.
- [src/compiler.cpp](../src/compiler.cpp) / [include/compiler.h](../include/compiler.h)
  — referencia 1:1 de todo el mapeo AST→código a replicar en
  `GeneradorLLVM` (tabla `fnBinaria`, `recolectarVariables`, `genLlamada`,
  `genClase`/`genMetodo`/`NuevoExpr`).
- [src/invocador_c.cpp](../src/invocador_c.cpp) / [include/invocador_c.h](../include/invocador_c.h)
  — infraestructura de enlace (VsDevCmd/cl.exe, cc/gcc/clang, recolección de
  `runtime/libs/*.c`) a extender para aceptar objetos ya compilados por LLVM.
- [src/main.cpp](../src/main.cpp) — punto de despacho de `--backend`/`--jit`
  y orquestación general del pipeline.
- `CMakeLists.txt` / `src/CMakeLists.txt` / `tests/CMakeLists.txt` —
  integración de `find_package(LLVM CONFIG)`, opción `LATINO_LLVM_BACKEND`,
  registro condicional de los tests LLVM.
- `include/compiler_llvm.h` / `src/compiler_llvm.cpp` — nuevo generador.
- `include/invocador_llvm.h` / `src/invocador_llvm.cpp` — nuevo driver
  AOT+JIT.
- `tools/abi_probe.c` — sondeo de ABI vía Clang.
- `tests/test_codegen_llvm.cpp` / `tests/test_e2e_llvm.cpp` — nuevas suites.

## Orden de implementación recomendado

El orden garantiza que cada paso compila y no rompe la suite existente del
backend C:

1. **L0–L1** — LLVM disponible en el build, smoke test de plumbing
   compilación+enlace, sin tocar el AST real.
2. **L2** — mecanismo de ABI verificado (`verifyModule` + prueba de
   `sizeof`), antes de cualquier codegen real.
3. **L3–L4** — expresiones y variables locales (con la disciplina de
   `alloca` en entry block).
4. **L5** — control de flujo.
5. **L6** — funciones de usuario y variádica.
6. **L7** — FFI a runtime/librerías.
7. **L8** — POO.
8. **L9** — driver AOT completo, con los 22 ejemplos corriendo con
   `--backend=llvm`.
9. **L10** — modo JIT (reutiliza el módulo ya validado en L3–L8).
10. **L11** — tests ampliados y comparativos.
11. **L12** — evaluación de paridad y decisión de default.
12. **L13** — *(no en este plan)* retiro eventual de `GeneradorC`.

## Garantía de retrocompatibilidad

- `--backend=c` (default durante toda la migración) no cambia: mismo
  `GeneradorC`, mismo runtime, misma salida en los 22 ejemplos y toda la
  suite de CTest actual.
- `tests/test_codegen.cpp` no se modifica.
- Ningún archivo de `runtime/` cambia de comportamiento observable — solo se
  agrega `lat_abi_verificar()` (Fase L2), que no altera ninguna función
  existente.
- Las máquinas sin LLVM instalado siguen pudiendo construir y probar el
  100% del backend C (`LATINO_LLVM_BACKEND=OFF` automático si
  `find_package(LLVM)` falla).

## Verificación

1. **Build**: `cmake -B build -DLATINO_LLVM_BACKEND=ON` (requiere LLVM 18.x
   instalado según la Decisión 1) y luego `cmake --build build --config
   Release`.
2. **Por fase (L1–L10)**: cada fase indica su propio criterio de aceptación
   arriba (compila, `verifyModule` pasa, ejemplo puntual produce la salida
   esperada).
3. **Suite completa**: `ctest --test-dir build -C Release --output-on-failure`
   — con `LATINO_LLVM_BACKEND=ON`, deben pasar tanto las suites existentes
   del backend C como las nuevas suites LLVM (`test_codegen_llvm`,
   `test_e2e_llvm`, variantes `--backend=llvm` de `test_lib_*`/
   `test_poo_e2e`/etc.).
4. **Paridad (gate de la Fase L12)**: script/target que compila y ejecuta
   los 22 `ejemplos/*.lat` con ambos backends y falla si alguna salida
   difiere.
5. **JIT**: `latino --jit ejemplos/hola.lat` (y progresivamente el resto de
   ejemplos a medida que L3–L8 avanzan) sin generar artefactos en disco.
