# Plan de trabajo — Foreign Function Interface (FFI) con C al estilo de Rust

## Contexto

Latino ya tiene un mecanismo para consumir código nativo en runtime:
`incluir "paquete"` (Fase 16 de [PLAN_LIBS.md](PLAN_LIBS.md),
`runtime/libs/paquete.c`/`.h`). Funciona así:

```latino
incluir "paquete"
milib = paquete.cargar("mi_extension.dll")
r = paquete.llamar(milib, "mi_funcion", 2, a, b)
```

`paquete.cargar` hace `LoadLibrary`/`dlopen` **en tiempo de ejecución** y
`paquete.llamar` busca el símbolo por nombre y lo invoca — pero exige que la
función nativa tenga, textualmente, la firma empaquetada que define
`LatFnModulo` (`runtime/latino.h`):

```c
typedef LatValor (*LatFnModulo)(int nargs, LatValor* args);
```

Es decir: **no se puede llamar a una función C arbitraria** (`abs`,
`strlen`, una función de `user32.dll`, una biblioteca científica en C/C++
con ABI-C) — primero hay que escribir, a mano, un archivo `.c` que envuelva
cada función real detrás de esa firma fija y la compile como biblioteca
dinámica. Es el mismo costo que tendría exponer una API a Python sin usar
`ctypes`/`cffi`: funciona, pero exige una capa de C intermedia por cada
función que se quiera exponer.

Este plan agrega un segundo mecanismo, **`externo`**, que permite declarar
la firma **real** de una función C —tipos con ancho fijo, punteros
opacos— directamente en un programa Latino y llamarla sin escribir ningún
código C intermedio. Es el mismo modelo mental que
[`extern "C"` de Rust](https://doc.rust-lang.org/nomicon/ffi.html):

```rust
#[link(name = "user32")]
extern "C" {
    fn MessageBoxA(hwnd: *mut c_void, text: *const c_char,
                    caption: *const c_char, boxtype: u32) -> i32;
}

unsafe {
    MessageBoxA(std::ptr::null_mut(), c"Hola".as_ptr(), c"Latino".as_ptr(), 0);
}
```

`incluir "paquete"` y `externo` no se reemplazan entre sí: resuelven
problemas distintos y conviven (ver Decisión de diseño 8) — el mismo
principio de convivencia que ya usó `PLAN_MODULOS.md` entre `incluir` y
`exportar`/`importar`.

## Motivación

- Permite usar bibliotecas C/C++ (ABI-C) ya existentes — `libc`/`msvcrt`,
  el API de Win32, SQLite, zlib, etc. — sin escribir una sola línea de C de
  por medio, algo que hoy es literalmente imposible con `paquete` salvo
  reescribiendo la biblioteca.
- Es la vía natural para que la propia librería estándar de Latino
  (`runtime/libs/*`) deje de depender de que **todo** viva reescrito en C
  dentro del repo: un futuro `mate.raiz_rapida` podría enlazar contra una
  biblioteca matemática real en vez de reimplementarla.
- Buen valor educativo/demostrativo para el lenguaje: "FFI al estilo de
  Rust" es un modelo mental conocido y ya se usó como referencia de diseño
  en otras partes del proyecto (POO "al estilo C#", módulos "al estilo
  TypeScript/ES Modules", genéricos "al estilo de Rust" —
  [PLAN_GENERICOS.md](PLAN_GENERICOS.md)).
- El compilador ya resuelve, para ambos backends, el problema más difícil
  de interoperar con C real: el ABI exacto de un valor agregado (`LatValor`
  pasado por `sret`/puntero en MSVC x64, ver Fase L2 de
  [PLAN_LLVM.md](PLAN_LLVM.md)). Ese conocimiento se reutiliza aquí para
  *desempaquetar* un `LatValor` a un tipo C primitivo, no para inventar un
  mecanismo nuevo desde cero.

### Decisiones de diseño

1. **Enlace en tiempo de compilación, no carga dinámica en runtime.** Un
   bloque `externo` declara una firma que el backend elegido (C o LLVM)
   resuelve como cualquier símbolo externo de C: el linker del sistema
   (`cl.exe`/`link.exe`, o `ld`/`lld` vía Clang) lo busca al enlazar el
   ejecutable final. Esto es lo opuesto a `paquete.cargar`, que resuelve el
   símbolo en runtime con `GetProcAddress`/`dlsym` — la diferencia central
   entre ambos mecanismos (ver Contexto).
2. **Vocabulario de tipos FFI propio, no `TipoAnotado`.** Los tipos que ya
   usa el tipado gradual (`numero`, `cadena`, `logico`, `nulo` — Fase 27,
   [PLAN_TIPADO.md](PLAN_TIPADO.md)) siguen significando "un `LatValor`
   dinámico verificado en ese punto" y **se reutilizan tal cual** en una
   firma `externo` cuando el mapeo a C es exacto y sin pérdida:
   - `numero` ↔ `double` (el propio storage de `LAT_NUMERO` ya es
     `double`, cero conversión).
   - `logico` ↔ `int` de 0/1 (mismo storage que `LAT_LOGICO`).
   - `cadena` ↔ `const char*` (el storage de `LAT_CADENA` ya es un buffer
     `char*` terminado en nulo — ver Riesgos técnicos).
   - `nulo` ↔ `void`, solo válido como tipo de retorno.

   Se agregan tipos **nuevos**, exclusivos de `externo`, solo donde no hay
   equivalente sin pérdida: anchos enteros fijos (`entero8/16/32/64`,
   `natural8/16/32/64` sin signo) y un puntero opaco (`puntero`). Nunca se
   exponen los tipos C ambiguos por plataforma (`int`, `long`, `size_t`) —
   ver Riesgos técnicos.
3. **Marshalling automático en el borde, no manual.** El compilador genera,
   en cada llamada a una función `externo`, la conversión `LatValor →
   tipo C` de cada argumento y `tipo C → LatValor` del retorno, según la
   firma declarada. El programador nunca escribe la conversión a mano (a
   diferencia de `paquete.llamar`, donde hoy el usuario arma el arreglo de
   `LatValor` y la función C envoltorio decide cómo interpretarlo).
4. **Puntero opaco nuevo en el runtime.** `puntero` se representa con un
   `LatTipo` nuevo, `LAT_PUNTERO` (`void*` crudo en la unión de
   `LatValor`), análogo a `*mut c_void`/`*const c_void` en Rust: no se
   puede indexar ni leer desde Latino, solo pasar de una llamada `externo`
   a otra o comparar contra `nulo`. Sin esto, una función C que devuelve un
   handle opaco (un `FILE*`, un HWND) no tendría dónde vivir en el sistema
   de valores de Latino.
5. **`inseguro` marca el borde, igual que `unsafe` en Rust.** Toda llamada
   a una función `externo` debe ocurrir dentro de un bloque `inseguro ...
   fin`, o dentro de una función declarada `funcion inseguro nombre(...)`.
   Es un chequeo puramente estático (análisis semántico) sin efecto en
   runtime — el objetivo es el mismo que en Rust: que el uso de FFI sea
   *visualmente* imposible de pasar por alto en una revisión de código,
   nunca "seguridad" real (Latino no puede verificar en compilación que un
   puntero nativo siga siendo válido).
6. **`enlazar "nombre"` es opcional.** Sin `enlazar`, se asume que el
   símbolo ya es resoluble contra el runtime C que todo ejecutable Latino
   enlaza siempre (`msvcrt`/`ucrt` en Windows, `libc` en Linux/macOS) — el
   caso común de llamar `abs`, `strlen`, `toupper`, etc. sin configuración
   adicional. `externo enlazar "user32"` asocia el bloque con una
   biblioteca de importación adicional, que cada backend agrega a su línea
   de compilación/enlace (ver "Codegen").
7. **Convención de llamada: la nativa del ABI de destino, sin variantes
   explícitas en v1.** Alcanza para x64 Windows/SysV (única plataforma real
   hoy — ver Riesgos técnicos de `PLAN_LLVM.md`); no se expone
   `stdcall`/`__cdecl` como en Rust (`extern "system"` vs `extern "C"`).
8. **Sin relación con `incluir "paquete"`.** Mecanismos complementarios que
   conviven indefinidamente, igual que `incluir` y `exportar`/`importar`
   conviven tras `PLAN_MODULOS.md`: `paquete` sigue siendo la vía para
   cargar un plugin en runtime con una firma ya adaptada a Latino;
   `externo` es la vía para consumir una firma C real, resuelta en
   compilación.

## Sintaxis propuesta

### Declaración de funciones externas

```latino
# Sin "enlazar": el símbolo ya está en el runtime C que todo ejecutable
# Latino enlaza siempre (msvcrt/ucrt en Windows, libc en Linux/macOS).
externo
    funcion abs(n: entero32): entero32
    funcion toupper(c: entero32): entero32
    funcion strlen(s: cadena): entero64
fin

# Con "enlazar": biblioteca adicional que el backend debe agregar a su
# línea de enlace.
externo enlazar "user32"
    funcion MessageBoxA(hwnd: puntero, texto: cadena, titulo: cadena,
                         tipo: entero32): entero32
fin
```

### Llamar una función externa — siempre dentro de `inseguro`

```latino
inseguro
    r = abs(-5)
    escribir(r)                                   # 5
    escribir(strlen("hola"))                       # 4
    MessageBoxA(nulo, "Hola desde Latino", "FFI", 0)
fin
```

Una función completa puede marcarse `inseguro` en vez de envolver cada
llamada en un bloque:

```latino
funcion inseguro saludar_nativo()
    MessageBoxA(nulo, "Hola desde Latino", "FFI", 0)
fin
```

Llamar una función `externo` fuera de un bloque/función `inseguro` es un
error de compilación (ver "Mensajes de error").

### Punteros opacos

```latino
externo
    funcion malloc(tam: entero64): puntero
    funcion free(p: puntero)
fin

inseguro
    p = malloc(16)
    si p == nulo
        escribir("sin memoria")
    fin
    free(p)
fin
```

(`p == nulo` usa el operador de igualdad normal, no `es`: `es` en Latino
solo acepta un nombre de clase a la derecha —`Parser::parseNombreTipoCalificado()`
exige un `TokenType::Identificador`, y `nulo` es palabra reservada, no
identificador— así que `p es nulo` sería un error de sintaxis. Ver Decisión
de diseño 4 y F3 en "Fases de implementación".)

### Tabla de tipos FFI

| Tipo en `externo` | Tipo C | Storage de `LatValor` | Nota |
|---|---|---|---|
| `numero` | `double` | `LAT_NUMERO` (ya es `double`) | reutilizado, sin tipo nuevo |
| `logico` | `int` (0/1) | `LAT_LOGICO` (ya es `int`) | reutilizado, sin tipo nuevo |
| `cadena` | `const char*` | `LAT_CADENA` (ya es `char*` con `\0`) | reutilizado, sin tipo nuevo |
| `nulo` | `void` | — | solo como tipo de retorno |
| `entero8`/`16`/`32`/`64` | `int8_t`/`int16_t`/`int32_t`/`int64_t` | nuevo, empaqueta/desempaqueta en `LAT_NUMERO` | ver pérdida de precisión en Riesgos técnicos para 64 bits |
| `natural8`/`16`/`32`/`64` | `uint8_t`/`uint16_t`/`uint32_t`/`uint64_t` | ídem, sin signo | ídem |
| `puntero` | `void*` | nuevo `LAT_PUNTERO` | opaco, solo `nulo`-comprobable |

### Tabla de palabras reservadas nuevas

| Palabra | Uso |
|---|---|
| `externo` | abre un bloque de declaraciones de funciones nativas |
| `enlazar` | (dentro de `externo`) nombra la biblioteca de importación a enlazar |
| `inseguro` | marca un bloque de sentencias, o una función completa, habilitado para llamar funciones `externo` |

(`entero8`/`16`/`32`/`64`, `natural8`/`16`/`32`/`64` y `puntero` se resuelven
por lexema en contexto de tipo, igual que `numero`/`cadena`/`logico` hoy —
no son palabras reservadas nuevas, ver nota de `PLAN_TIPADO.md` sobre no
romper variables que ya se llamen así.)

## Semántica

### Parser

`externo [enlazar "lib"] ( funcion nombre(param: TipoFFI, ...) [: TipoFFI] )+ fin`
— cada `funcion` dentro del bloque es solo una firma (sin cuerpo, sin
`retornar`). `inseguro ... fin` se parsea como un bloque de sentencias más
(mismo shape que el cuerpo de `si`/`mientras`); `funcion inseguro
nombre(...)` agrega el modificador `inseguro` a la declaración de función
ya existente.

### Análisis semántico

- Tabla nueva `funcionesExternas` (nombre → firma: parámetros tipados +
  retorno), separada de la tabla de funciones normales de
  `AnalizadorSemantico` — un nombre no puede estar en ambas tablas a la vez
  (error si colisiona con una función Latino normal).
- Pila `inseguroActivo` (booleano anidado, mismo patrón que
  `genericosActivos` de `PLAN_GENERICOS.md`): se empuja al entrar a un
  bloque `inseguro` o a una función marcada `inseguro`; toda `Llamada` cuyo
  destino resuelva a `funcionesExternas` se valida contra el tope de esta
  pila.
- Aridad: número de argumentos debe coincidir exactamente con la firma
  (no hay variádicas en `externo`, a diferencia de las funciones Latino
  normales).
- Chequeo de tipo: cuando el argumento en el sitio de llamada es un
  literal o una variable con anotación de tipo ya conocida (misma
  infraestructura que `PLAN_TIPADO.md`), se verifica en compilación contra
  el `TipoFFI` esperado; si no se puede determinar estáticamente, la
  verificación queda diferida a runtime (ver Codegen).

### Codegen — backend C (`GeneradorC`, `src/compiler.cpp`)

1. Por cada `ExternoBloque`, al principio del `.c` generado:
   - `#pragma comment(lib, "<lib>.lib")` bajo `#ifdef _MSC_VER` (no-op en
     otros compiladores — ver punto 3).
   - `extern <tipo_C_retorno> <simbolo>(<tipos_C_parametros>);` — una
     declaración real, no una firma `LatValor`.
2. Cada `Llamada` a una función externa se traduce a: extraer el campo
   `.como.X` de cada `LatValor` argumento (con un chequeo dinámico de tipo
   si no se resolvió en compilación — nueva función auxiliar de runtime,
   ver más abajo), invocar el símbolo real, y envolver el resultado con el
   constructor de `LatValor` correspondiente (`lat_numero`, `lat_logico`,
   `lat_cadena`, o el nuevo `lat_puntero`).
3. `invocador_c.cpp` necesita poder agregar `-l<lib>` a la línea de
   `gcc`/`clang` en plataformas no-MSVC (el `#pragma comment(lib,...)` del
   punto 1 solo lo entiende MSVC) — requiere que `GeneradorC` exponga la
   lista de bibliotecas declaradas con `enlazar` para que `main.cpp` se la
   pase a `invocador_c.cpp`.

### Codegen — backend LLVM (`GeneradorLLVM`, `src/compiler_llvm.cpp`)

1. Por cada `ExternoFn`, `M.getOrInsertFunction(simbolo, tipo_LLVM_real)`
   con la firma nativa (`i8`/`i16`/`i32`/`i64`/`double`/`ptr`) — a
   diferencia de las funciones de usuario (L6) y los builtins (L7), **no**
   se usa la firma empaquetada `(sret, nargs, args)`, ni hace falta pasar
   por `RuntimeAbiLLVM`: son tipos LLVM primitivos que `IRBuilder` maneja
   de forma nativa.
2. Marshalling: `extractvalue`/GEP + `load` sobre la `%struct.LatValor` ya
   conocida (mismo layout que expone `RuntimeAbiLLVM` desde L2 de
   `PLAN_LLVM.md`), conversión con `trunc`/`fptrunc`/`bitcast` según el
   `TipoFFI`, `call` directo al símbolo externo, y empaquetado del
   resultado con las funciones constructoras del runtime ya declaradas.
3. `invocador_llvm.cpp` necesita agregar la biblioteca de `enlazar` a la
   invocación del linker (AOT) — mismo requisito que el punto 3 del backend
   C, forma de código distinta.

### Runtime (`runtime/latino.h`/`.c`)

- `LAT_PUNTERO` nuevo en `LatTipo`; campo `void* puntero;` nuevo en la
  unión de `LatValor`.
- `LatValor lat_puntero(void* p);` — constructor.
- `son_iguales` (usada por `lat_igual`/`lat_distinto`, es decir por `==`/
  `!=`) trata un `LAT_PUNTERO` con `puntero == NULL` como equivalente a
  `LAT_NULO`: `p == nulo` da `cierto` cuando el puntero nativo es `NULL`,
  en cualquier orden de los operandos. **No** se usa el operador `es` para
  esto — `es` en Latino solo acepta un nombre de clase a su derecha
  (`Parser::parseNombreTipoCalificado()` exige un `Identificador`, y
  `nulo` es palabra reservada, no identificador), así que `p es nulo`
  sería un error de sintaxis (decidido en F3, ver "Fases de
  implementación").
- `LatValor lat_ffi_verificar_tipo(LatValor v, int tipo_esperado, const char* nombre_fn, int indice_arg);`
  — chequeo dinámico usado cuando el compilador no pudo verificar el tipo
  del argumento en compilación; `tipo_esperado` es un `LatTipo` (no un
  `TipoFFI`: los distintos anchos de entero/natural de una firma FFI
  comparten el mismo `LAT_NUMERO` en runtime, el ancho exacto se aplica al
  convertir a C, no aquí); aborta con mensaje claro si no coincide (mismo
  espíritu que `lat_verificar_tipo`, reutilizado por POO/genéricos).

## Mensajes de error

Compilación (`stderr`, mismo estilo que `AnalizadorSemantico`):

- `llamada a función externa 'X' fuera de un bloque 'inseguro'`
- `tipo FFI desconocido: 'Y' en la firma de 'X'`
- `número de argumentos incorrecto para la función externa 'X': se esperaban N, se recibieron M`
- `'X' ya está declarada como función externa` (colisión entre dos bloques `externo`)
- `'X' ya está declarada como función Latino` (colisión `externo` vs. función normal)

Runtime (vía `lat_ffi_verificar_tipo`):

- `ffi: se esperaba 'cadena' para el argumento 2 de 'MessageBoxA', se recibió lista`

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `src/lexer.cpp` | 3 palabras reservadas nuevas: `externo`, `enlazar`, `inseguro` |
| `include/ast.h` | `ExternoBloque`/`ExternoFn`/`InseguroBloque`; enum `TipoFFI`; campo `inseguro` en `FuncionDef` |
| `include/parser.h`, `src/parser.cpp` | `parseExterno()`, `parseInseguro()`, modificador `inseguro` en `parseFuncion()` |
| `include/ast_impresor.h`, `src/ast_impresor.cpp` | `visitar(ExternoBloque&)`/`visitar(InseguroBloque&)` (debug) |
| `include/analizador_semantico.h`, `src/analizador_semantico.cpp` | tabla `funcionesExternas`, pila `inseguroActivo`, chequeos de aridad/tipo |
| `src/compiler.cpp` | declaraciones `extern`/`#pragma comment(lib,...)`, marshalling de llamadas, lista de bibliotecas a enlazar |
| `src/compiler_llvm.cpp` | `declare` con firma nativa, marshalling con IR, lista de bibliotecas a enlazar |
| `src/invocador_c.cpp` | agregar `-l<lib>` en plataformas no-MSVC |
| `src/invocador_llvm.cpp` | agregar biblioteca de enlace al paso de linking AOT |
| `runtime/latino.h`, `runtime/latino.c` | `LAT_PUNTERO`, `lat_puntero`, `lat_ffi_verificar_tipo` |
| `tests/` | `test_ffi` (parser/semántico/codegen, backends `c` y `llvm`) y `test_ffi_e2e` (programas reales llamando símbolos de `libc`/`msvcrt` ya enlazados, sin depender de una `.dll` externa al repo) |
| `SINTAXIS.md` | sección nueva documentando `externo`/`enlazar`/`inseguro` |
| `CLAUDE.md`, `README.md` | entrada de estado al completar el plan, igual que se hizo con `PLAN_LLVM.md`/`PLAN_MODULOS.md`/`PLAN_GENERICOS.md` |

## Fases de implementación

- **F1 — Lexer y AST.** Palabras reservadas `externo`/`enlazar`/`inseguro`;
  nodos `ExternoBloque`/`ExternoFn`/`InseguroBloque`; enum `TipoFFI`; campo
  `inseguro` en `FuncionDef`. Sin lógica de resolución. Pruebas:
  `test_lexer`, `test_ast`.

- **F2 — Parser.** `parseExterno()` (con `enlazar` opcional, N firmas de
  función sin cuerpo), `parseInseguro()`, modificador `inseguro` en
  `parseFuncion()`. Pruebas: `test_parser` con las variantes de "Sintaxis
  propuesta".

- **F3 — Runtime: puntero opaco.** `LAT_PUNTERO` en `LatTipo`/`LatValor`,
  `lat_puntero`, decisión sobre `es nulo` para punteros, `lat_ffi_verificar_tipo`.
  Sin integración con el compilador todavía.

- **F4 — Análisis semántico.** Tabla `funcionesExternas`, pila
  `inseguroActivo`, chequeo de aridad y de colisión de nombres, chequeo
  estático de tipo cuando es posible (reusa infraestructura de
  `PLAN_TIPADO.md`). Pruebas: `test_semantico` (llamada fuera de
  `inseguro` → error, aridad incorrecta → error, tipo estático incorrecto
  → error).

- **F5 — Codegen backend C.** Declaraciones `extern`/`#pragma comment`,
  marshalling de argumentos/retorno, cambios en `invocador_c.cpp` para
  `-l<lib>` en no-MSVC. Verificar con un caso real (`abs`/`strlen` de
  `msvcrt`, sin `enlazar`).

- **F6 — Codegen backend LLVM.** `declare` con firma nativa, marshalling
  con IR (`extractvalue`/`trunc`/`call`), cambios en `invocador_llvm.cpp`
  para agregar bibliotecas de enlace. Verificar paridad de salida con el
  backend C sobre los mismos casos de F5 (mismo criterio de
  `PLAN_LLVM.md`/L12).

- **F7 — Pruebas de cobertura.** `tests/test_ffi.cpp` (parser/semántico/
  codegen, con subcadena de IR + `verifyModule` para LLVM, mismo patrón de
  `test_codegen_llvm.cpp`) y `tests/test_ffi_e2e.cpp` (programas `.lat`
  reales en ambos backends, sin depender de ninguna `.dll`/`.so` externa al
  repo — solo símbolos ya enlazados por defecto, ver Decisión de diseño 6).

- **F8 — Documentación y cierre.** Sección nueva en `SINTAXIS.md`, entrada
  de estado en `CLAUDE.md`/`README.md`, pase completo de `ctest` en serie.

## Riesgos técnicos

- **Pérdida de precisión en enteros de 64 bits.** `LAT_NUMERO` es un
  `double` (53 bits de mantisa); un `entero64`/`natural64` devuelto por una
  función C con un valor mayor a 2^53 pierde precisión al volver a Latino —
  limitación estructural del valor dinámico de Latino, igual que los
  `Number` de JavaScript. v1 documenta la limitación en `SINTAXIS.md` y no
  la resuelve (un `LAT_ENTERO64` dedicado, sin pérdida, queda para un plan
  futuro si aparece un caso de uso real).
- **Anchos de tipo C ambiguos por plataforma.** `int`/`long`/`size_t` no
  tienen el mismo ancho en todas las plataformas (p. ej. `long` es 32 bits
  en Windows x64 pero 64 bits en Linux x64). Mitigación ya incorporada en
  la Decisión de diseño 2: `externo` solo expone anchos fijos explícitos
  (`entero32`, `entero64`, etc.), nunca los nombres C ambiguos — el usuario
  debe saber el ancho real de la función que está declarando (igual que en
  Rust con `std::os::raw::c_long` vs. `i64` explícito).
- **Invariante de `cadena` sin bytes nulos embebidos.** El marshalling
  `cadena → const char*` asume terminador nulo simple (confirmado: el
  storage de `LAT_CADENA` en `runtime/latino.c` es un buffer
  `malloc`-eado de `len + 1` bytes, null-terminated). Si una cadena Latino
  contuviera un byte nulo interno, se truncaría silenciosamente al cruzar
  el borde FFI — riesgo ya inherente al valor `cadena` de Latino, no nuevo
  de este plan, pero más visible aquí porque C la trata como C-string.
- **Ciclo de vida de `puntero` es responsabilidad del programador.**
  `LAT_PUNTERO` es completamente opaco: Latino no tiene un destructor
  determinístico como el `Drop` de Rust, así que un `malloc` vía FFI que
  nunca recibe su `free` correspondiente simplemente fuga memoria — la
  misma responsabilidad que el `unsafe` de Rust deja en manos del
  programador, documentada como tal y no resuelta por el compilador.
- **Enlace multiplataforma con dos caminos de código.** `#pragma
  comment(lib,...)` solo lo entiende MSVC; GCC/Clang necesitan `-l<lib>`
  pasado por línea de comandos — mismo patrón de bifurcación por
  plataforma que ya usa `runtime/libs/paquete.c` (`LoadLibrary` vs.
  `dlopen`), pero ahora también en `invocador_c.cpp`/`invocador_llvm.cpp`,
  no solo en el runtime.
- **Convención de llamada asumida, no verificada.** v1 asume siempre la
  convención nativa del ABI de destino (correcta para x64 Windows/Linux,
  la única plataforma real hoy — ver `PLAN_LLVM.md`); declarar una función
  Win32 clásica de 32 bits con `stdcall` explícito quedaría mal enlazada
  sin ningún error de compilación, solo un crash o corrupción de pila en
  runtime. Documentado como asunción explícita, no un caso a detectar.

## Fuera de alcance

- **Bindgen automático** (generar firmas `externo` a partir de un header
  `.h` real, como el `bindgen` de Rust). v1 requiere escribir la firma a
  mano.
- **Structs/uniones C pasadas por valor.** v1 solo cubre tipos primitivos y
  punteros opacos; pasar o retornar un struct C arbitrario por valor
  requiere conocer su layout y reglas de ABI de agregados — el mismo
  problema que ya costó resolver para `LatValor` en la Fase L2 de
  `PLAN_LLVM.md`, pero generalizado a cualquier struct definido por el
  usuario. Se revisa en un plan futuro si aparece un caso de uso real.
- **Callbacks nativos** (pasar una función Latino como puntero a función C,
  p. ej. para `qsort`). Requiere generar una trampolina nativa con la
  firma C correcta por cada callback — fuera de v1.
- **Convenciones de llamada no nativas** (`stdcall` explícito en x86 de 32
  bits). v1 apunta solo a x64 (ver Decisión de diseño 7).
- **Unificar `externo` y `paquete` en una sola API.** Quedan como dos
  mecanismos independientes y complementarios (ver Decisión de diseño 8).
- **Un `LAT_ENTERO64` dedicado sin pérdida de precisión.** Ver Riesgos
  técnicos.
- **`link_name`/alias de símbolo** (declarar un nombre Latino distinto del
  símbolo C real, como `#[link_name = "..."]` en Rust) — se puede agregar
  sin romper compatibilidad en una fase posterior si hace falta (p. ej.
  para exponer un símbolo cuyo nombre colisione con una palabra reservada
  de Latino), pero v1 exige que el nombre de la función Latino coincida
  exactamente con el símbolo C.

## Estado

F1 completa (lexer y AST): tres palabras reservadas nuevas (`externo`,
`enlazar`, `inseguro`) en `src/lexer.cpp`; `enum class TipoFFI` y
`struct ParamFFI`/`FuncionExterna` en `include/ast.h` (vocabulario de tipos
FFI de la tabla de "Sintaxis propuesta"); nodos `ExternoBloque`/
`InseguroBloque` con `visitar()` no-op por defecto en `Visitante` (mismo
patrón que `ClaseDef`/`EstructuraDef`/`InterfazDef` en su fase inicial —
ningún visitante existente se rompe hasta que F4/F5/F6 les den lógica real);
campo `bool inseguro = false;` en `FuncionDef`. Sin lógica de parser ni de
resolución todavía. Pruebas: `test_lexer` (tokeniza las 3 palabras nuevas),
`test_ast` (construcción directa de `ExternoBloque` con/sin `enlazar`,
`FuncionExterna`/`ParamFFI` con los distintos `TipoFFI`, `InseguroBloque`, y
el modificador `FuncionDef::inseguro`).

F2 completa (parser): `Parser::parseExterno()`/`parseFuncionExterna()`/
`parseInseguro()` en `src/parser.cpp`, con despacho desde `parseSentencia()`
para las palabras reservadas `externo`/`inseguro`. `externo [enlazar
"lib"]` seguido de N firmas `funcion nombre(param: TipoFFI, ...): TipoFFI`
sin cuerpo, terminado en `fin`; el tipo de retorno es opcional y por
defecto es `TipoFFI::Nulo` (equivalente a omitir `: void` en C), pero el
tipo de cada parámetro es obligatorio (`esperarOperador(":")`, a diferencia
de los parámetros sin anotar de una función Latino normal) — no tiene
sentido un parámetro FFI sin tipo C explícito. `Parser::mapearNombreTipoFFI`
(nuevo, junto a `mapearNombreTipo`) traduce el lexema al enum `TipoFFI` y
devuelve `false` si no lo reconoce (a diferencia de `mapearNombreTipo`, que
nunca falla porque cualquier lexema no reconocido se interpreta como nombre
de clase de usuario — en FFI no existe ese fallback, así que un tipo
desconocido es directamente `tipo FFI desconocido: 'Y'`, error de
compilación). El modificador `inseguro` de `funcion inseguro nombre(...)`
se parsea dentro de `parseFuncion()`, entre `funcion`/`fun` y el nombre —
sin ambigüedad con el `inseguro` que abre un bloque, porque son posiciones
gramaticales distintas (`parseSentencia()` solo llega a la segunda forma
cuando `inseguro` es la primera palabra de la sentencia). `ImpresorAST`
gana `visitar(ExternoBloque&)`/`visitar(InseguroBloque&)` (volcado de una
línea por firma / del cuerpo del bloque, respectivamente, ya no no-op) y
marca `[inseguro]` en la firma de una `FuncionDef` que lleva el modificador
— mismo criterio que M2 de `PLAN_MODULOS.md` (el impresor gana soporte real
recién cuando el parser puede producir el nodo).

Pruebas: `tests/test_parser.cpp` (bloque `externo` con y sin `enlazar`,
tipo de retorno por defecto `nulo` cuando se omite `:`, bloque `inseguro`,
`funcion inseguro nombre(...)`, y dos casos negativos que confirman que el
parser devuelve `nullptr` en un tipo FFI desconocido y en un parámetro
`externo` sin `:` de tipo). Suite completa de CTest verificada en verde
tras el cambio (ver nota de `ctest -j` en `CLAUDE.md`).

Alcance de F1/F2, tal como lo define este plan: solo sintaxis y estructura
del AST. Una llamada a una función `externo` fuera de un bloque `inseguro`
**no** se rechaza todavía (llega en F4); una llamada a una función
`externo` dentro de `inseguro` tampoco genera código real todavía (F5/F6)
— hoy el compilador la trataría como una llamada a una función Latino
normal no declarada, y fallaría más adelante en el análisis semántico con
un mensaje genérico ("función no declarada"), no con los mensajes
específicos de FFI de la sección "Mensajes de error" — comportamiento
esperado en esta fase, no un bug.

F3 completa (runtime: puntero opaco): `LAT_PUNTERO` nuevo al final de
`LatTipo` y campo `void* puntero;` nuevo en la unión de `LatValor`
(`runtime/latino.h`) — no cambia `sizeof(LatValor)` ni su alineación en
x64 (un `void*` mide lo mismo que los demás punteros/el `double` ya
presentes en la unión), así que la verificación de ABI de `lat_abi_verificar`
(Fase L2 de `PLAN_LLVM.md`) no se ve afectada y `generated/runtime_abi.ll`
no necesita regenerarse. `lat_puntero(void* p)` nuevo (`runtime/latino.c`).
Hallazgo real de esta fase, no previsto en el diseño original: el operador
`es` (`n es NombreClase`) **no** sirve para comprobar nulidad de un
puntero — `Parser` exige un `Identificador` después de `es`, y `nulo` es
palabra reservada, no identificador, así que `p es nulo` es un error de
sintaxis. Se corrigió el ejemplo de "Sintaxis propuesta" (usaba `si p es
nulo`) a `si p == nulo`, y se implementó la comparación real en
`son_iguales` (`runtime/latino.c`): un `LAT_PUNTERO` con `puntero == NULL`
se trata como equivalente a `LAT_NULO` en cualquier orden de los
operandos (`p == nulo`, `nulo == p`); dos `LAT_PUNTERO` no nulos se
comparan por identidad del puntero nativo, igual que ya hacía `LAT_OBJETO`.
Se agregó también el caso `LAT_PUNTERO` a los demás `switch (v.tipo)`
exhaustivos de `runtime/latino.c` que ya enumeraban todos los `LatTipo`
existentes (`lat_es_verdadero` → un puntero no-NULL es verdadero;
`lat_a_cadena` → `"<puntero>"`; `lat_tipo` → `"puntero"`; `_nombre_latipo`
→ `"puntero"`, usado por los mensajes de error de `lat_verificar_tipo` y
del nuevo `lat_ffi_verificar_tipo`) — `lat_valor_retener`/
`lat_valor_liberar` no necesitaron cambios porque ya tienen `default:
break;` y un puntero FFI nunca participa del conteo de referencias
(Decisión de diseño 4: el ciclo de vida es responsabilidad del
programador, igual que `unsafe` en Rust).

`lat_ffi_verificar_tipo(LatValor v, int tipo_esperado, const char*
nombre_fn, int indice_arg)` nuevo, mismo patrón que `lat_verificar_tipo`
(imprime a `stderr` y termina con `exit(1)` si `v.tipo` no coincide con el
`LatTipo` esperado, devuelve `v` sin modificar si coincide) pero con el
mensaje de error de "Mensajes de error" del plan (`ffi: se esperaba 'X'
para el argumento N de 'fn', se recibió 'Y'`) en vez del de tipado
gradual. Recibe un `LatTipo`, no un `TipoFFI` del AST — los distintos
anchos de entero/natural de una firma FFI comparten el mismo `LAT_NUMERO`
en runtime; F5/F6 son quienes truncan/convierten al ancho C real después
de esta verificación, no esta función.

Sin integración con el compilador todavía (F4/F5/F6 la agregan). Pruebas:
`tests/test_runtime_ffi.cpp`, suite nueva registrada directamente en
`tests/CMakeLists.txt` (no vía `add_suite20`, reservado a E2E que invocan
el binario `latino`) — compila `runtime/latino.c` +
`runtime/libs/paquete.c` (el segundo hace falta porque
`lat_obj_llamar_metodo`, despacho dinámico de POO, ya requería
`lat_paquete_llamar_args` antes de este plan) y enlaza contra ellos
directamente, sin pasar por el target `latino_runtime` del CMakeLists.txt
raíz (ese target solo compila `runtime/latino.c` "para verificar que
sigue siendo válido", según su propio comentario, y no incluye
`runtime/libs/*` — enlazarlo solo habría producido un símbolo sin
resolver). 19 comprobaciones: construcción de `lat_puntero` (con puntero
real y con `NULL`), igualdad `puntero == nulo`/`!= nulo` en ambos
órdenes, igualdad por identidad entre dos `LAT_PUNTERO`, `lat_es_verdadero`,
`lat_tipo`/`lat_a_cadena` de un puntero, y la ruta exitosa de
`lat_ffi_verificar_tipo` (la ruta de error termina el proceso con
`exit(1)`, igual que `lat_verificar_tipo`/`lat_dividir`, y ningún test del
proyecto ejercita esas rutas en el mismo proceso — se deja para
verificación E2E vía subproceso en F7, cuando ya haya código Latino real
que la dispare). Suite completa de CTest verificada en verde tras el
cambio.

F4 completa (análisis semántico): tabla nueva `funcionesExternas`
(`nombre -> InfoFuncionExterna`: `parametrosTipo` como `TipoFFI`,
`tipoRetorno`, `linea`), separada de `funciones`, poblada en
`recolectarFunciones` (`src/analizador_semantico.cpp`) — se extendió el
mismo bucle existente sobre `programa.sentencias` (en vez de agregar un
método nuevo) para poder detectar colisión de nombres entre `FuncionDef` y
`ExternoBloque` **en cualquier orden de declaración** dentro del archivo:
si el `FuncionDef` aparece primero, `ExternoBloque` lo detecta consultando
`funciones` (ya poblada en esa misma pasada); si `ExternoBloque` aparece
primero, el `FuncionDef` posterior lo detecta consultando
`funcionesExternas` (ya poblada en la misma pasada, porque ambos casos se
resuelven en un único recorrido lineal de las sentencias de nivel
superior, sin dos pasadas separadas). Contador `profundidadInseguro`
(mismo patrón que `profundidadBucle`/`profundidadFuncion`/
`profundidadVariadica`, no la pila de mapas de `genericosActivos` que
sugería el borrador original de este plan — un simple contador anidado
alcanza porque solo hace falta un booleano, no datos por nivel),
incrementado en `visitar(InseguroBloque&)` (nuevo) y en `visitar(FuncionDef&)`
cuando `n.inseguro` es cierto. `visitar(Llamada&)` gana una rama nueva
(antes de `esIncorporada`/`estaDeclarada`) para un nombre resuelto contra
`funcionesExternas`: error si `profundidadInseguro == 0`, chequeo de
aridad exacta (sin variádicas), y chequeo estático de tipo por argumento
cuando es un literal o una variable ya anotada (reusa `tipoDelLiteral`/
`tipoDeVariable`, misma infraestructura de tipado gradual que
`PLAN_TIPADO.md`) — degradado a runtime (`lat_ffi_verificar_tipo`, sin
error de compilación) cuando el argumento es dinámico, igual filosofía que
el resto del tipado gradual y de la inferencia genérica de
`PLAN_GENERICOS.md`.

Para el chequeo estático de tipo, cada `TipoFFI` se reduce a una
`CategoriaFFI` amplia (`Numero` agrupa `numero` + los ocho anchos de
entero/natural, más `Logico`/`Cadena`/`Puntero`/`Nulo`) porque un literal
como `5` es válido para cualquier ancho de entero/natural — el ancho
exacto solo importa al convertir al tipo C real en el marshalling de
F5/F6, no en este chequeo (mismo motivo por el que `lat_ffi_verificar_tipo`,
F3, recibe un `LatTipo` y no un `TipoFFI`).

Hallazgo real de esta fase, que obligó a corregir el propio ejemplo de
"Sintaxis propuesta" (`MessageBoxA(nulo, "Hola desde Latino", "FFI", 0)`,
ahí desde F1): sin una regla especial, pasar el literal `nulo`
(`TipoAnotado::Nulo`) a un parámetro `puntero` habría sido rechazado por
el chequeo estático (`Nulo` no es la categoría `Puntero`) — pero
`nulo` **debe** poder representar un puntero nulo (el `NULL`/`nullptr` de
C), que es exactamente el caso de uso de `hwnd: puntero` en ese ejemplo.
Se agregó una excepción explícita: un argumento literal `nulo` es
compatible con cualquier parámetro `puntero`, sin error estático. Queda
pendiente para F5/F6 la otra mitad de este hallazgo (documentada, no
resuelta en F4 porque es codegen): un `LatValor` con `tipo == LAT_NULO`
literal no satisface hoy `lat_ffi_verificar_tipo(v, LAT_PUNTERO, ...)` en
runtime (`LAT_NULO != LAT_PUNTERO`), así que el marshalling de un
argumento `nulo` hacia un parámetro `puntero` deberá generar un `NULL`
nativo directamente (o extender el chequeo dinámico para aceptar
`LAT_NULO` como equivalente a un puntero nulo), nunca despachar por
`lat_ffi_verificar_tipo` sin más para ese caso puntual.

Mensaje de error nuevo, no listado originalmente en "Mensajes de error"
(se agrega ahí en esta misma revisión): `tipo incompatible: el argumento N
de la función externa 'X' espera 'TIPO_ESPERADO' pero se pasó un valor de
tipo 'TIPO_REAL'` — mismo estilo que el ya existente para `Asignacion`
(`tipo incompatible: se declaró '...' pero el valor es '...'`).

Pruebas: `tests/test_semantico.cpp`, 11 casos nuevos (58 comprobaciones en
total en la suite) — llamada fuera de `inseguro` (error) y dentro de un
bloque `inseguro`/una `funcion inseguro` (ambos OK), aridad incorrecta,
tipo estático incompatible (`cadena` para `entero32`, `numero` para
`puntero`), `nulo` para `puntero` (OK, el caso del hallazgo de arriba), y
las tres variantes de colisión de nombres (función-luego-externo,
externo-luego-función, externo-luego-externo). Suite completa de CTest en
serie verificada en verde tras el cambio.

Sin cambios en `GeneradorC`/`GeneradorLLVM`/`runtime` en esta fase — F4 es
puramente análisis estático; una llamada a una función `externo` que pasa
F4 sin errores todavía no genera código FFI real (llega en F5/F6).
Verificado que **no** produce un error de compilación de C/LLVM como
suponía una revisión anterior de este párrafo: `GeneradorC::genLlamada`
(`src/compiler.cpp`) no conoce `funcionesExternas`, así que una llamada a
un nombre externo cae en su rama final genérica (ni builtin ni en
`funciones`) y emite literalmente `lat_nulo() /* llamada no soportada:
NOMBRE */` — C válido que compila sin error y devuelve `nulo` en
silencio, sin invocar el símbolo nativo. Es decir: hoy, pasar F4 sin
errores no implica que el programa haga lo correcto en ejecución, solo
que la sintaxis/aridad/tipos estáticos de la llamada son válidos —
comportamiento esperado en esta fase (el codegen real es responsabilidad
de F5/F6), pero vale la pena tenerlo presente para no confundir "compila"
con "ya funciona" al probar manualmente antes de F5.

F5 completa (codegen backend C): `GeneradorC` (`include/compiler.h`/
`src/compiler.cpp`) gana una tabla propia `funcionesExternas` (nombre →
`InfoFuncionExterna`: tipos de parámetros + retorno, sin nombres — igual
alcance que la de `AnalizadorSemantico` en F4, pero recolectada de forma
independiente por `recolectarExterno()`, ya que `compiler.cpp` no depende
de `analizador_semantico.h`) y un `std::set<std::string> bibliotecasEnlazar`
con los nombres de cada `externo enlazar "lib"`, ambos poblados recorriendo
`programa.sentencias` en busca de `ExternoBloque` — mismo patrón que
`recolectarFunciones`/`recolectarTipos` ya usaban para `FuncionDef`/
`ClaseDef`. `generar()` emite, en el preámbulo (Fase 2, junto a los
`#include`): `#include <stdint.h>` solo si hay al menos una función
externa (ancho fijo de entero/natural), un `#ifdef _MSC_VER` /
`#pragma comment(lib, "<lib>.lib")` / `#endif` por cada biblioteca de
`enlazar`, y un `extern <tipo_C_retorno> <simbolo>(<tipos_C_parametros>);`
por cada `FuncionExterna` con la firma C real (`tipoFFIaC`, nuevo, mapea
cada `TipoFFI` a su tipo C: `double`/`int`/`const char*`/`void`/
`int8_t`..`uint64_t`/`void*`) — nunca la firma empaquetada `LatValor
lat_fn_x(LatValor...)` de una función Latino normal.

`GeneradorC::genLlamada` gana una rama nueva justo antes del fallback
`lat_nulo() /* llamada no soportada */` (la rama en la que hoy caía toda
llamada a un nombre `externo`, según el hallazgo documentado al cierre de
F4): si el nombre resuelve contra `funcionesExternas`, despacha a
`genLlamadaExterna()`, que arma la llamada real al símbolo nativo
marshallando cada argumento con `genArgumentoFFI()` (extrae el valor C
primitivo de un `LatValor` con un chequeo dinámico
`lat_ffi_verificar_tipo(...)` — emitido siempre, sin importar si F4 ya lo
verificó estáticamente, mismo criterio de "defensa en profundidad" que ya
usa `lat_verificar_tipo` para un parámetro anotado de una función Latino
normal — seguido de `.como.<campo>` y, para los ocho anchos de entero/
natural, un cast C al tipo exacto vía `tipoFFIaC`) y empaquetando el
resultado según `TipoFFI::tipoRetorno` (`lat_numero`/`lat_logico`/
`lat_cadena`/`lat_puntero`, o `(<llamada>, lat_nulo())` para retorno `nulo`
— la coma permite descartar el resultado de una llamada C que devuelve
`void` y devolver igual un `LatValor` en la misma expresión).

Caso especial ya anticipado como pendiente en el cierre de F4 (`nulo` como
puntero nulo hacia un parámetro `puntero`): `genArgumentoFFI` no llama a
`lat_ffi_verificar_tipo` directamente para un parámetro `puntero`, porque
`LAT_NULO != LAT_PUNTERO` haría fallar en runtime el caso legítimo
`MessageBoxA(nulo, ...)`. En vez de eso, evalúa el argumento una sola vez
en un temporal (`nuevoTemp()` + `emitir(...)`, mismo patrón ya usado por
`genLlamada` para el arreglo de argumentos de un método estático) y
genera `(<t>.tipo == LAT_NULO ? NULL : lat_ffi_verificar_tipo(<t>,
LAT_PUNTERO, ...).como.puntero)` — el temporal evita evaluar dos veces una
expresión con posibles efectos de lado (p. ej. otra llamada anidada).

Hallazgo real de esta fase, no anticipado en el diseño original: ni
`GeneradorC::genSentencia` ni `recolectarVariables`
(`include/recolector_variables.h`/`src/recolector_variables.cpp`, código
compartido con `GeneradorLLVM` según su propio comentario de cabecera)
tenían ningún caso para `InseguroBloque` — antes de este cambio, el cuerpo
completo de un bloque `inseguro ... fin` se descartaba en silencio al
generar C (caía en el comentario final "FuncionDef u otros: no se emiten
dentro de un bloque"), y ninguna variable asignada dentro de él se
hoisteaba. Se agregó un caso explícito en ambos: `genSentencia` genera el
cuerpo tal cual (`genBloque(ib->cuerpo)`, sin envoltorio — `inseguro` es
puramente un marcador estático ya consumido por F4, sin efecto en
runtime) y `recolectarVariables` desciende en él igual que en
`si`/`mientras`/`repetir`. `ExternoBloque` sigue sin necesitar un caso en
`genSentencia`: ya se recolectó en `recolectarExterno`/el preámbulo, y si
apareciera anidado (la gramática lo permite igual que a un `FuncionDef`,
ver `parseSentencia`) cae en el mismo fallback silencioso que un
`FuncionDef` anidado, comportamiento ya existente y no nuevo de este
plan.

Cambios de enlazado: `OpcionesC` (`include/invocador_c.h`) gana
`std::vector<std::string> bibliotecasEnlazar`; `main.cpp` la llena con
`GeneradorC::bibliotecasEnlazadas()` (getter nuevo, público); `ejecutarGnu`
(`src/invocador_c.cpp`) agrega `-l<lib>` por cada una a la línea de
`gcc`/`clang` (GNU/Clang no entiende `#pragma comment`). `ejecutarMsvc` no
cambió: el `#ifdef _MSC_VER` / `#pragma comment(lib,...)` ya embebido en el
`.c` generado alcanza para MSVC sin tocar la línea de `cl.exe`.

Verificado manualmente en esta máquina de desarrollo (MSVC/Visual Studio
17 2022, sin LLVM instalado — mismo entorno que fases previas): (1) el caso
recomendado por el propio plan, sin `enlazar` — `externo funcion
abs(n: entero32): entero32` / `funcion strlen(s: cadena): entero64` dentro
de un bloque `inseguro`, compilado y ejecutado de punta a punta, imprime
`5` y `4`; (2) punteros opacos — `malloc`/`free` con `si p == nulo`,
compilado y ejecutado, imprime `con memoria` y no crashea al liberar; (3)
`externo enlazar "user32"` con `MessageBoxA` y `funcion inseguro
saludar_nativo()` (la otra forma de habilitar `inseguro`, sin bloque
envolvente) — verificado solo el C generado (`--solo-c`: `#pragma
comment(lib, "user32.lib")` bajo `#ifdef _MSC_VER`, `extern int32_t
MessageBoxA(void*, const char*, const char*, int32_t);`, marshalling
correcto de `nulo` a `NULL`), sin compilar a ejecutable porque `MessageBoxA`
abre un diálogo real que bloquearía la terminal. Los tres casos se
descartaron después de la verificación (no quedan como archivos
permanentes de este plan — `tests/test_ffi_e2e.cpp` los formaliza en F7).
Suite completa de CTest (50 pruebas — build sin backend LLVM en esta
máquina, ver nota de G7/L11 en `CLAUDE.md`) verificada en verde en serie
tras el cambio (1226 s reales).

Sin cambios en `GeneradorLLVM`/`runtime` en esta fase. F6 (backend LLVM)
sigue pendiente.

F6 completa (codegen backend LLVM): `GeneradorLLVM` (`include/compiler_llvm.h`/
`src/compiler_llvm.cpp`) gana la misma pareja tabla+set que F5 agregó a
`GeneradorC` -- `funcionesExternas_` (nombre → `InfoFuncionExterna`: tipos de
parámetros + retorno, poblada por `recolectarExterno()`, llamada junto a
`recolectarTipos()` al principio de `generar()`) y `bibliotecasEnlazar_`
(nombres de cada `externo enlazar "lib"`), expuesto este último vía el
getter público `bibliotecasEnlazadas()` -- y una función libre nueva,
`tipoLLVMdeFFI(TipoFFI, LLVMContext&)`, que mapea cada `TipoFFI` a su
`llvm::Type*` primitivo real (`i8`/`i16`/`i32`/`i64`/`double`/`ptr`/`void`) --
equivalente a `tipoFFIaC` de F5, pero devolviendo un tipo LLVM en vez de un
nombre C. `declararExterno()` declara (o recupera) el símbolo nativo en el
módulo destino con esa firma primitiva real vía
`llvm::Function::Create(..., ExternalLinkage, ...)` -- nunca la firma
empaquetada `(sret, ptr...)` de una función de usuario/runtime (Fase L6/L2):
a diferencia de toda otra llamada de este generador, una llamada FFI no pasa
por `RuntimeAbiLLVM`, porque el símbolo no es parte del runtime de Latino.

`genExpr(Llamada)` gana la misma tercera rama que F5 agregó a
`GeneradorC::genLlamada`: si el nombre no resuelve contra ningún builtin ni
`funciones_` (usuario), se prueba contra `funcionesExternas_` y, si
coincide, despacha a `genLlamadaExterna()`. Marshalling de cada argumento vía
`genArgumentoFFI()`: a diferencia del backend C (que necesita un temporal
`_t<N>` explícito para no evaluar dos veces una expresión con efectos de
lado, ver F5), acá `genExpr()` del argumento se llama una única vez y su
resultado (`celdaArg`, un puntero a la celda `%LatValor` ya evaluada) se
reutiliza en todas las bifurcaciones subsiguientes -- no hace falta ningún
temporal adicional. Con `celdaArg` en mano: `lat_ffi_verificar_tipo` se
invoca siempre (defensa en profundidad, igual que F5), y el valor primitivo
se extrae con `CreateStructGEP(tipoLatValor, verificado, 1)` + `CreateLoad`
sobre el campo `como` (el índice 0 es `tipo`, confirmado por el propio
`generar()` preexistente, que ya usa `layout->getElementOffset(0)` para
`lat_abi_verificar`) -- la traducción literal de "extractvalue/GEP + load"
que anticipaba la sección "Codegen" del plan, en vez del `.como.X` que emite
C. Con punteros opaco (LLVM 18), ese GEP no necesita ningún `bitcast`
posterior: el resultado ya es un `ptr` sin tipo apuntado fijo, así que
"reinterpretar" el campo como `double`/`i32`/`ptr` es simplemente elegir el
tipo del `CreateLoad`. Los ocho anchos de entero/natural se leen primero
como `double` (mismo storage que `numero`) y se convierten al ancho C real
con `CreateFPToSI`/`CreateFPToUI` según signo -- equivalente al cast C
`(int32_t)v.como.numero` de F5.

Mismo hallazgo de F5 sobre el puntero nulo, resuelto con una técnica
distinta por ser LLVM IR real: el literal `nulo` hacia un parámetro
`puntero` no puede pasar por `lat_ffi_verificar_tipo` sin más (`LAT_NULO !=
LAT_PUNTERO` aborta el proceso), pero acá no hay una expresión-ternaria de C
que compile a un `select` -- hace falta una bifurcación real (dos
`BasicBlock` + `PHINode`), porque el lado "verificar" NO debe ejecutarse
cuando el valor ya es `LAT_NULO` (un `select`/`CreateSelect` evaluaría
ambos lados incondicionalmente, y el lado "verificar" terminaría el proceso
para ese caso legítimo). `genArgumentoFFI` lee el campo `tipo` (índice 0) de
la celda ya evaluada, compara contra `LAT_NULO`, y arma
`ffi_ptr_nulo`/`ffi_ptr_verificar`/`ffi_ptr_fin` con un `PHINode` de tipo
`ptr` que fusiona `ConstantPointerNull` (rama nula) con el puntero
verificado (rama no nula) -- ningún temporal extra hace falta porque
`celdaArg` ya es, por construcción de este generador (a diferencia de C), el
único punto de evaluación de la expresión del argumento.

Empaquetado del resultado (`genLlamadaExterna`): la llamada nativa real
(`CreateCall` sobre el `llvm::Function*` de `declararExterno`) se envuelve
con el constructor del runtime correspondiente al `TipoFFI` de retorno
(`lat_logico`/`lat_cadena`/`lat_puntero`/`lat_nulo`, vía `RuntimeAbiLLVM` --
estos sí son funciones del runtime, a diferencia del propio símbolo FFI) o,
para `numero`+entero/natural, `lat_numero` tras una conversión
`CreateSIToFP`/`CreateUIToFP` si el ancho real no era ya `double` --
paridad con el `switch` de `GeneradorC::genLlamadaExterna`. Un retorno
`nulo` (función C `void`) no produce ningún valor de la llamada nativa que
envolver; simplemente se llama y se empaqueta un `lat_nulo()` en la celda de
retorno, igual que la coma en el C generado por F5.

Hallazgo real de esta fase, no anticipado en el diseño original: F3 dejó
`lat_puntero`/`lat_ffi_verificar_tipo` fuera de `tools/abi_probe.c`
(`lat_abi_referenciar_todo`), porque en F3 "sin integración con el
compilador todavía" era literal -- pero `RuntimeAbiLLVM::declarar` sólo
puede resolver un nombre si Clang emitió su `declare` en
`generated/runtime_abi.ll`, y eso solo ocurre para símbolos referenciados
dentro de `abi_probe.c`. Sin este cambio, `abi_->declarar(modulo,
"lat_ffi_verificar_tipo")`/`"lat_puntero"` habrían devuelto `nullptr` en
cualquier programa que use `externo`, rompiendo con una violación de acceso
en vez de un error claro. Se agregaron ambos a la lista alfabética de
referencias en `abi_probe.c`; `generated/runtime_abi.ll` se regenera solo
(la regla de CMake ya declara `tools/abi_probe.c` como `DEPENDS`), sin
cambios adicionales de build.

Segundo hallazgo, en el paso de enlace AOT: a diferencia del backend C
(cuyo `.c` generado embebe `#ifdef _MSC_VER` / `#pragma comment(lib,
"<lib>.lib")` -- ver F5 --, mecanismo puramente textual de código C), un
objeto `.obj` emitido por el backend LLVM no tiene ninguna forma equivalente
de "pedirle" a `cl.exe`/`link.exe` que enlace una biblioteca adicional. Se
extendió `OpcionesLLVM` (`include/invocador_llvm.h`) con
`bibliotecasEnlazar` (llenado en `main.cpp` desde
`generadorLlvm.bibliotecasEnlazadas()`, análogo a como ya se llena
`OpcionesC::bibliotecasEnlazar` para el backend C), reenviado por
`compilarLLVMAEjecutable` (`src/invocador_llvm.cpp`) a
`OpcionesC::bibliotecasEnlazar` -- el mismo campo que el paso de enlace ya
comparte con el backend C, porque `compilarAEjecutable` no distingue de
dónde vino el `.c`/`.obj` que recibe. `ejecutarGnu` (`src/invocador_c.cpp`)
ya soportaba esto sin cambios (agrega `-l<lib>` sin importar si el archivo
de entrada es `.c` o `.obj`); `ejecutarMsvc` SÍ necesitó un parámetro nuevo
(`bibliotecasEnlazar`) para agregar `<lib>.lib` explícitamente a la línea de
`cl.exe` -- necesario siempre para el backend LLVM (única vía de enlazar en
MSVC) e inofensivo para el backend C (enlazar la misma import lib que el
`#pragma comment` ya embebe no produce error ni advertencia).

Verificado en esta máquina de desarrollo (MSVC/Visual Studio 17 2022, sin
LLVM 18.1 instalado -- mismo entorno que F1-F5, y el mismo motivo por el que
G7/L11 quedaron sin verificación con LLVM real en `CLAUDE.md`): el build del
backend C (`cmake --build build --config Release --target latino`) compila
sin errores tras los cambios de `invocador_c.cpp`/`invocador_llvm.h`/
`main.cpp` (código compartido entre ambos backends), y el caso de F5 sin
`enlazar` (`abs`/`strlen` dentro de `inseguro`) sigue compilando y
ejecutando correctamente con `--backend c` (imprime `5`/`4`), confirmando
que el nuevo parámetro de `ejecutarMsvc` no rompe la ruta del backend C. La
suite completa de CTest (50 pruebas, backend LLVM no registrado en este
build por la misma razón de siempre) se verificó en verde en serie tras el
cambio. El código nuevo de `compiler_llvm.h`/`compiler_llvm.cpp`
(`recolectarExterno`, `declararExterno`, `genArgumentoFFI`,
`genLlamadaExterna`, la rama nueva de `genExpr(Llamada)`, y el caso
`InseguroBloque` de `genSentencia`, que -- mismo hallazgo que F5 encontró en
`GeneradorC`/`recolectarVariables` -- tampoco existía todavía en
`GeneradorLLVM::genSentencia`) no se compiló ni se ejecutó con LLVM real en
esta máquina; queda pendiente de esa verificación (paridad de salida con el
backend C sobre los mismos casos de F5, criterio de L12) para cuando F7
agregue `test_codegen_llvm`/`test_ffi_e2e` con el backend LLVM habilitado, o
en cualquier máquina con LLVM 18.1.x instalado.

F7 completa (pruebas de cobertura): tres archivos nuevos en `tests/`, en vez
de un único `test_ffi.cpp` dual-backend como sugería la tabla "Archivos
modificados" original del plan -- desviación consistente con la que F2/F3/F4
ya habían tomado (el parser/runtime/semántico de FFI quedaron cubiertos
directamente en `test_parser.cpp`/`test_runtime_ffi.cpp`/`test_semantico.cpp`,
no en un archivo unificado), así que lo único sin cobertura al llegar a F7
era el codegen de ambos backends:

- `tests/test_ffi.cpp` (nuevo, siempre se compila): codegen del backend C
  (F5), mismo patrón que `test_codegen.cpp` (`generar(src)` parsea con
  Lexer/Parser y llama a `GeneradorC::generar`, se compara el C emitido por
  subcadena, sin compilar ni ejecutar). 17 comprobaciones: preámbulo (`extern
  <firma real>;`, `#include <stdint.h>`, `#ifdef _MSC_VER` / `#pragma
  comment(lib, "user32.lib")` con `enlazar`, ausencia de `#pragma comment`
  sin `enlazar`, tipo de retorno por defecto `void` al omitir `:`), y
  marshalling exacto (`(int32_t)lat_ffi_verificar_tipo(..., LAT_NUMERO,
  ...).como.numero` para `entero32`, `.como.cadena` para `cadena`, el
  temporal + ternario `_tN.tipo == LAT_NULO ? NULL : ...` para el literal
  `nulo` hacia un parámetro `puntero`, el mismo patrón para un puntero que
  *no* es el literal `nulo` -- encadenando `malloc`/`free` --, y
  `lat_logico((int)(...))` para un retorno `logico`). Este archivo formaliza
  también la verificación manual de F5 con `enlazar`/`MessageBoxA` (a
  propósito, no se ejecuta -- ver más abajo).

- `tests/test_codegen_llvm.cpp` (existente, sección nueva "PLAN_FFI.md F6"):
  codegen del backend LLVM (F6), mismo patrón que las Fases L2-L8 ya
  presentes en este archivo -- AST construido a mano (`externoBloque`/
  `funcionExterna`/`inseguroBloque`, helpers nuevos junto a los ya
  existentes `llamada`/`litNumero`/`litCadena`) y subcadena de IR +
  `verifyModule`, SIN pasar por `generar()` completo. Para esto,
  `GeneradorLLVM::recolectarExterno` (agregada en F6 como método privado)
  pasó a ser pública en `compiler_llvm.h` -- mismo motivo exacto por el que
  `recolectarTipos`/`declararFuncion`/`genFuncion`/`declararMetodo`/
  `genMetodo` ya eran públicos desde L6/L8: permitir que una prueba puebla
  el estado interno mínimo necesario (`funcionesExternas_`/
  `bibliotecasEnlazar_`) y después ejercite `genExpr(Llamada)`/
  `genSentencia(InseguroBloque)` de forma aislada, sin construir un
  `Programa` completo ni un `main`. `declararExterno`/`genArgumentoFFI`/
  `genLlamadaExterna` siguen privados: no hizo falta exponerlos porque
  `genExpr(Llamada)` ya los invoca internamente. 6 pruebas nuevas: llamada
  sin `enlazar` (`declare i32 @abs(i32)`, verificación dinámica, llamada
  real, empaquetado con `lat_numero`), `enlazar` registra la biblioteca en
  `bibliotecasEnlazadas()` (sin necesidad de generar ningún IR), el literal
  `nulo` hacia un parámetro `puntero` bifurca con basic blocks reales
  (`ffi_ptr_nulo:`/`ffi_ptr_verificar:`/`phi ptr`) en vez de un `select`, un
  puntero que no es `nulo` encadenado (`malloc`/`free`, con
  `declare ptr @malloc(i64)`/`declare void @free(ptr)`), retorno `cadena` +
  `entero64` (conversión `sitofp`/`uitofp` antes de `lat_numero`), e
  `InseguroBloque` traduciendo su cuerpo tal cual sin ningún basic block
  propio. **Sin verificar con LLVM real en esta máquina** (mismo motivo de
  siempre): compilar/ejecutar esta suite queda pendiente para cuando el
  backend LLVM esté habilitado en algún build.

- `tests/test_ffi_e2e.cpp` (nuevo, registrado con la macro `add_suite20` --
  variantes automáticas `test_ffi_e2e`/`test_ffi_e2e_llvm`, backend C
  verificado, LLVM no registrado en este build): 4 `CasoTest` reales,
  compilados y ejecutados de punta a punta contra el binario `latino`, todos
  sin `enlazar` (Decisión de diseño 6) -- `abs`/`strlen` (el caso recomendado
  del plan), `malloc`/`free` con `si p == nulo`/`sino`, `funcion inseguro
  calcular(n)` con un argumento SIN anotación estática (ejercita la ruta
  dinámica de F4, `lat_ffi_verificar_tipo` en runtime, no un chequeo en
  compilación), e `isalpha` con retorno `logico` (confirma que
  `lat_logico((int)(...))` normaliza cualquier entero de C distinto de cero
  a `cierto`). El caso con `enlazar`/`MessageBoxA` de F5 **no** se formaliza
  aquí a propósito -- abre un diálogo real que bloquearía la ejecución
  automática, exactamente la razón por la que F5 solo lo verificó con
  `--solo-c`; su cobertura vive en `test_ffi.cpp` (verificación del C
  generado, sin compilar a ejecutable, mismo criterio que F5). Las 4 pruebas
  pasan con el backend C (26 s reales).

Hallazgo real de esta fase, no un bug de F1-F6 sino una característica
preexistente y sistémica de la gramática de anotaciones de tipo (no
específica de FFI): un `: nulo` **explícito** como tipo de retorno de una
firma `externo` no se puede escribir -- `Parser::parseFuncionExterna` exige
`TokenType::Identificador` después de `:`, pero `nulo` lexa como
`TokenType::PalabraReservada` (es palabra reservada desde antes de este
plan), así que `funcion foo(p: puntero): nulo` falla con "se esperaba un
tipo de retorno FFI válido". La misma restricción ya existía, sin relación
con FFI, en la anotación de tipo de una variable normal
(`Parser::parseAsignacionOExpr`, tipado gradual de `PLAN_TIPADO.md`): ningún
`TipoAnotado`/`TipoFFI` cuyo nombre sea una palabra reservada es alcanzable
por anotación explícita, solo por default. No se trata como bug a corregir
en este plan -- la propia sección "Sintaxis propuesta" y el ejemplo de F2
siempre usan la forma de omisión (`funcion free(p: puntero)`, sin `:`) para
expresar un retorno `nulo`, nunca `: nulo` explícito, así que el
comportamiento real coincide con el uso documentado; se ajustó únicamente
el caso de prueba que asumía lo contrario.

Suite completa de CTest (52 pruebas -- las 50 de F6 más `test_ffi` y
`test_ffi_e2e`, nuevas de esta fase; backend LLVM sigue sin registrarse en
este build) verificada en verde en serie tras el cambio.
Con esto el plan v1 (fases F1-F7) queda completo en lo que a implementación
y pruebas respecta; F8 (documentación y cierre: sección en `SINTAXIS.md`,
entrada de estado en `CLAUDE.md`/`README.md`) sigue pendiente.
