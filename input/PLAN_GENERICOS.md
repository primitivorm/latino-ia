# Plan de trabajo — Genéricos al estilo de Rust en Latino

## Contexto

Latino tiene hoy dos capas de tipado que conviven (ver
[CLAUDE.md](../CLAUDE.md)):

1. **Tipado gradual opcional** (Fase 27, [PLAN_TIPADO.md](PLAN_TIPADO.md)):
   anotaciones `: tipo` en variables, parámetros y retornos, verificadas
   estáticamente cuando el valor es un literal y dinámicamente (una sola
   llamada a `lat_verificar_tipo()`) en el resto de los casos. Sin anotación,
   todo sigue siendo `LatValor` sin chequeo.
2. **POO al estilo C#** (Fase 28, [PLAN_POO.md](PLAN_POO.md)): `clase`,
   `estructura`, `interfaz`, herencia (`extiende`), implementación
   (`implementa`), y un registro estático de tipos en el analizador
   semántico (`AnalizadorSemantico::tipos`, `InfoTipo`) que sabe qué clase
   hereda de qué padre y qué interfaces implementa.

Ninguna de las dos capas permite hoy escribir una función o una clase que
opere sobre "un tipo cualquiera, pero siempre el mismo dentro de esa
llamada/instancia", ni exigir que ese tipo cumpla una capacidad mínima
(una interfaz). Hoy la única forma de escribir, por ejemplo, una pila
reutilizable para cualquier tipo de dato es no anotar el tipo en absoluto
(`items: lista`, sin garantías) o duplicar la clase por cada tipo
concreto (`PilaDeNumeros`, `PilaDeCadenas`, ...).

Este plan agrega **genéricos** (`clase Pila<T>`, `funcion maximo<T:
Comparable>(a: T, b: T): T`) inspirados explícitamente en el modelo de
Rust: parámetros de tipo entre `<>`, restricciones ("trait bounds") con
`:` y `+`, una cláusula `donde` para legibilidad, y el operador turbofish
`::<...>` para instanciar genéricos explícitamente en posición de
expresión. Rust monomorphiza (genera una copia especializada del código
por cada instanciación concreta) porque compila a máquina con tipos de
tamaño fijo; Latino no lo necesita ni debe imitarlo — ver la decisión de
diseño 1.

## Motivación

- **Reutilización de estructuras de datos.** `Pila`, `Cola`, `Arbol`,
  `Resultado` (éxito/error) son el ejemplo canónico de POO genérica; hoy
  cada una debe escribirse sin garantías de tipo o duplicarse por tipo
  concreto.
- **Funciones utilitarias sin perder el chequeo de tipos.** `maximo(a,
  b)`, `intercambiar(a, b)`, `primero(lista)` son correctas para
  cualquier tipo, pero hoy anotarlas obliga a elegir un tipo concreto o a
  no anotarlas (perdiendo toda verificación).
- **Encaja con el tipado gradual ya elegido.** Fase 27 ya estableció la
  filosofía "sin anotación no hay chequeo, con anotación se chequea lo que
  se pueda estáticamente y se degrada a runtime el resto" — los genéricos
  extienden esa misma filosofía a "un tipo anotado con una variable, en
  vez de un nombre fijo", no una capa de verificación nueva y distinta.
- **Rust es el modelo mental más transferible para bounds.** La sintaxis
  `T: Comparable + Imprimible` de Rust es más compacta y más popular hoy
  que las alternativas (Java `<T extends Comparable>`, C#
  `where T : IComparable`), y Latino ya tomó prestado un modelo conocido
  para módulos (TS/ESM, [PLAN_MODULOS.md](PLAN_MODULOS.md)) y para POO
  (C#, [PLAN_POO.md](PLAN_POO.md)) en vez de inventar uno propio — mismo
  criterio aquí.

### Decisiones de diseño

1. **Erasure en tiempo de compilación, no monomorphización.** El tipo
   central del runtime, `LatValor`, ya es una unión tagueada dinámica
   (`runtime/latino.c`/`.h`) — todo valor Latino, genérico o no, se pasa
   exactamente igual en C/LLVM (por puntero a `LatValor`, ver hallazgo de
   ABI de `PLAN_LLVM.md` L2). Un parámetro de tipo `T` **no necesita**
   una representación en memoria distinta por cada tipo concreto: se
   compila exactamente como si fuera `TipoAnotado::Ninguno` (sin
   anotación) — mismo código C/LLVM generado para `identidad<numero>(5)`
   y para `identidad<cadena>("x")`, una sola vez. Esto es lo opuesto a
   Rust real (que monomorphiza porque compila a tipos de tamaño fijo en
   máquina) pero es la elección correcta para Latino: **cero cambios en
   `GeneradorC`/`GeneradorLLVM`** más allá de lo que ya hacen hoy para
   parámetros sin anotar o anotados como `Objeto`. "Al estilo de Rust" en
   este plan se refiere a la **sintaxis** (`<T>`, bounds con `:`/`+`,
   `donde`, turbofish) y a la **ergonomía de chequeo estático**, no a la
   estrategia de compilación.
2. **Los bounds son solo estáticos en v1.** Un bound `T: Interfaz`
   restringe qué tipo concreto puede sustituir a `T`, y eso se verifica
   en el analizador semántico cuando el tipo concreto es conocido en
   tiempo de compilación (literal `nuevo X(...)`, variable con anotación
   de tipo, argumento de tipo explícito vía turbofish). Hoy **no existe**
   en el runtime un verificador de "este objeto implementa la interfaz
   X" — `lat_verificar_tipo()` solo compara la etiqueta `LAT_OBJETO`
   contra el valor esperado, nunca qué interfaz implementa el objeto (ver
   `src/compiler.cpp`, `tipoALatTipo`/`validarTipoObjeto`). Igual que
   Fase 27 hace con los literales, cuando el tipo concreto **no** se
   puede determinar estáticamente (p. ej. viene de `leer()` sin anotar),
   el bound simplemente no se verifica — no se agrega ninguna llamada de
   runtime nueva en v1 (ver "Fuera de alcance").
3. **Los parámetros de tipo son solo un nombre, no un `LatTipo` nuevo.**
   No se agrega ningún valor a `enum class TipoAnotado`
   (`include/ast.h`). Un parámetro genérico se representa igual que un
   tipo de clase hoy: `TipoAnotado::Objeto` con `tipoClase` puesto al
   nombre del parámetro (`"T"`). La diferencia la hace el analizador
   semántico al resolver el nombre: antes de buscarlo en el registro de
   clases (`tipos`), revisa si es un parámetro genérico activo en el
   ámbito de la declaración actual (función/método/clase que se está
   analizando) — ver Fase G5.
4. **`<...>` solo es válido en posición de tipo, nunca en posición de
   expresión.** Esto evita por completo la ambigüedad clásica de
   `identificador < expr > expr` (¿comparación encadenada o genérico?)
   sin necesitar backtracking ni lookahead arbitrario: el parser solo
   intenta leer una lista de parámetros/argumentos de tipo entre `<>`
   en cinco puntos gramaticales fijos, todos ya distinguibles por
   contexto (ver "Resolución de la ambigüedad `<`/`>`" y decisión 5). En
   posición de expresión (llamar a una función genérica) la única forma
   de dar un argumento de tipo explícito es el turbofish `::<...>`,
   igual que en Rust y por la misma razón: `identidad<numero>(5)` sería
   ambiguo con `(identidad < numero) > (5)`, `identidad::<numero>(5)` no
   lo es porque `::` no es un operador de expresión válido en Latino.
5. **Reutiliza tokens existentes.** `<`, `>`, `:`, `,`, `+` ya son
   operadores/delimitadores lexados (`include/lexer.h`). Solo se agregan:
   la palabra reservada `donde` y el operador de dos caracteres `::`
   (turbofish) — ningún cambio a `enum class TokenType`.
6. **Sin genéricos por defecto (`T = Tipo`), sin generics const, sin
   variance.** Ver "Fuera de alcance".
7. **Compatibilidad total con módulos.** `ResolutorModulos`
   (`src/resolutor_modulos.cpp`, [PLAN_MODULOS.md](PLAN_MODULOS.md)) manda
   por nombre de declaración de nivel superior, sin mirar si esa
   declaración es genérica — una `clase Pila<T>` exportada se mangla
   igual que cualquier otra clase (`__mod_<slug>__Pila`), y `T` sigue
   siendo un nombre local a esa declaración, no algo que el resolutor
   necesite tocar. **No se esperan cambios en `resolutor_modulos.cpp`.**

## Sintaxis propuesta

### Funciones genéricas

```latino
funcion identidad<T>(x: T): T
    retornar x
fin

escribir(identidad(5))          # T inferido = numero
escribir(identidad("hola"))     # T inferido = cadena
escribir(identidad::<numero>(5))   # turbofish explícito (ver más abajo)
```

### Restricciones de tipo ("trait bounds")

Un bound restringe qué tipos concretos puede tomar un parámetro genérico,
exigiendo que implementen una o más interfaces. Se combinan con `+`,
igual que Rust:

```latino
interfaz Comparable
    funcion compararCon(otro: Comparable): numero
fin

funcion maximo<T: Comparable>(a: T, b: T): T
    retornar a.compararCon(b) >= 0 ? a : b
fin

interfaz Imprimible
    funcion aCadena(): cadena
fin

funcion describirMayor<T: Comparable + Imprimible>(a: T, b: T): cadena
    retornar maximo(a, b).aCadena()
fin
```

### Cláusula `donde`

Alternativa más legible cuando hay varios parámetros y/o bounds largos —
azúcar sintáctica pura, equivalente a poner el bound inline entre `<>`:

```latino
funcion combinar<T, U>(a: T, b: U): cadena donde T: Imprimible, U: Imprimible
    retornar a.aCadena() .. b.aCadena()
fin
```

### Clases y estructuras genéricas

```latino
clase Pila<T>
    privado items: lista

    funcion Pila()
        este.items = []
    fin

    publico funcion apilar(valor: T)
        lista.agregar(este.items, valor)
    fin

    publico funcion desapilar(): T
        retornar lista.quitarUltimo(este.items)
    fin

    publico funcion vacia(): logico
        retornar lista.longitud(este.items) == 0
    fin
fin

p: Pila<numero> = nuevo Pila<numero>()
p.apilar(1)
p.apilar(2)
escribir(p.desapilar())      # 2
```

```latino
estructura Par<A, B>
    primero: A
    segundo: B

    funcion Par(primero: A, segundo: B)
        este.primero = primero
        este.segundo = segundo
    fin
fin

par = nuevo Par<cadena, numero>("edad", 30)
```

Igual que en Rust, un tipo genérico puede tener sus propios bounds:

```latino
clase ConjuntoOrdenado<T: Comparable>
    privado items: lista
    # ...
fin
```

### Interfaces genéricas

```latino
interfaz Contenedor<T>
    funcion obtener(indice: numero): T
    funcion agregar(valor: T)
fin

clase ListaFija<T> implementa Contenedor<T>
    privado items: lista
    # ...
fin
```

### Instanciación explícita — turbofish (`::<...>`)

Cuando el compilador no puede inferir el/los parámetro(s) de tipo a
partir de los argumentos (por ejemplo, porque `T` solo aparece en el
tipo de retorno), se exige un turbofish; también puede usarse siempre
que se prefiera ser explícito:

```latino
funcion vacio<T>(): lista
    retornar []
fin

x = vacio::<numero>()     # T no aparece en ningún argumento: turbofish obligatorio
y = identidad::<cadena>("hola")   # opcional aquí; identidad("hola") ya lo infiere
```

`nuevo` usa `<...>` directamente (posición de tipo, no de expresión, ver
decisión 4), nunca turbofish:

```latino
p = nuevo Pila<numero>()
```

### Anotaciones de variable/campo/parámetro con tipo genérico concreto

```latino
p: Pila<numero> = nuevo Pila<numero>()
par: Par<cadena, numero> = nuevo Par<cadena, numero>("x", 1)

funcion cima(p: Pila<numero>): numero
    retornar p.desapilar()
fin
```

`lista`/`dic` (ya genéricos de facto en el runtime) aceptan la misma
sintaxis como azúcar puramente documental/estática — ver "Genéricos
integrados: `lista<T>` / `dic<K, V>`" más abajo.

### Genéricos integrados: `lista<T>` / `dic<K, V>`

`lista` y `dic` ya son contenedores homogéneos-por-convención sin
anotación de elemento. Se extiende su anotación existente
(`TipoAnotado::Lista`/`Dic`, Fase 27) para aceptar parámetros de tipo
opcionales, verificados **solo estáticamente sobre literales** (mismo
alcance que hoy tiene Fase 27 para tipos simples — no se agrega
verificación de runtime elemento por elemento, ver "Fuera de alcance"):

```latino
nums: lista<numero> = [1, 2, 3]        # ok
malos: lista<numero> = [1, "x"]        # error de compilación (literal no homogéneo)
edades: dic<cadena, numero> = {"ana": 30}
```

### Tabla de palabras reservadas nuevas

| Palabra | Uso |
|---|---|
| `donde` | cláusula de bounds alternativa/legible tras la firma de una función genérica |

(`::` es un operador de dos caracteres nuevo, no una palabra reservada;
`<`, `>`, `:`, `,`, `+` ya existen.)

### Gramática de posiciones donde `<...>` es válido

`<...>` **solo** se intenta parsear como lista de parámetros/argumentos
de tipo en estos cinco puntos — en cualquier otro lugar, `<` es siempre
el operador de comparación:

| Posición | Ejemplo | Contenido de `<...>` |
|---|---|---|
| Tras el nombre en `clase`/`estructura`/`interfaz` | `clase Pila<T>` | Declaración: nombres + bounds opcionales |
| Tras el nombre en `funcion` (de nivel superior o método) | `funcion maximo<T: Comparable>(...)` | Declaración: nombres + bounds opcionales |
| Tras un nombre de tipo en posición de anotación (`: Tipo`, retorno, campo) | `p: Pila<numero>` | Argumentos: nombres de tipo concretos |
| Tras `nuevo NombreClase` | `nuevo Pila<numero>()` | Argumentos: nombres de tipo concretos |
| Tras `::` en posición de expresión (turbofish) | `identidad::<numero>(5)` | Argumentos: nombres de tipo concretos |

## Resolución de la ambigüedad `<` / `>`

El parser de Latino es descendente recursivo con un token de lookahead
(mismo enfoque que ya resolvió la ambigüedad del `:` en Fase 27, ver
`input/PLAN_TIPADO.md` sección 3.1). Los cinco puntos de la tabla
anterior son *todos* posiciones donde, hoy, un identificador **no puede**
ir seguido de una expresión de comparación válida según la gramática
actual:

- Tras `clase Nombre`, `estructura Nombre`, `interfaz Nombre` y tras
  `funcion nombre`: la gramática actual solo espera `extiende`,
  `implementa`, `(` o `fin`/salto de línea — nunca un operador binario.
  Ver un `<` ahí ya es inequívoco.
- En posición de anotación de tipo (tras `:` en variable/parámetro/campo,
  tras `):` en retorno): la gramática actual solo espera un identificador
  de tipo seguido de `=`, `,`, `)`, salto de línea, o `donde` — nunca una
  expresión.
- Tras `nuevo NombreClase`: la gramática actual solo espera `(` — nunca
  un operador binario.
- El turbofish exige el prefijo literal `::`, que hoy no es un token
  válido en ninguna posición de expresión — su sola presencia ya
  desambigua sin lookahead adicional.

Por eso **no se necesita** ningún mecanismo de backtracking, conteo de
`<`/`>` balanceados especulativo, ni el truco de "reinterpretar `>>`
como dos `>`" que sí necesitan C++/Java: el punto gramatical ya decide.
La única regla operativa para el parser: *"si acabo de terminar de leer
un nombre de clase/función en una de las cinco posiciones de la tabla y
el siguiente token es `<`, es una lista de tipos; en cualquier otro
lugar, `<` es el operador de comparación"*.

## Verificación de tipos

### Registro de parámetros genéricos activos

`AnalizadorSemantico` (`include/analizador_semantico.h`) gana una pila de
conjuntos de nombres de parámetros genéricos activos, empujada al entrar
a analizar una declaración genérica (clase o función/método) y desapilada
al salir — igual patrón que `ambitos` para variables:

```cpp
struct InfoParametroGenerico {
    std::string nombre;
    std::vector<std::string> bounds;   // nombres de interfaces
    int linea = 0;
};

std::vector<std::unordered_map<std::string, InfoParametroGenerico>> genericosActivos;
```

Al resolver cualquier nombre de tipo (`validarTipoObjeto`, ya existente),
el orden de búsqueda pasa a ser:

1. ¿Es un parámetro genérico activo en el ámbito actual (tope de
   `genericosActivos`, y si estamos dentro de un método, también el de la
   clase envolvente)? → válido sin más, se recuerda su(s) bound(s) para
   chequeo posterior en el sitio de uso concreto.
2. ¿Es una clase/estructura/interfaz real (`tipos`, como hoy)? → validado
   como hoy.
3. Ninguna de las dos → error de compilación, igual que hoy
   (`"tipo desconocido 'X'"`).

Esto es un cambio puramente aditivo a `validarTipoObjeto`: el código no
genérico (99% de la base actual) nunca tiene ningún parámetro genérico
activo, así que el paso 1 siempre falla rápido y el comportamiento es
idéntico al de hoy.

### Inferencia en sitios de llamada

Para una llamada a una función/método genérico sin turbofish, se
construye un mapa de sustitución `nombreParametro -> tipoConcreto`
unificando, en orden, el tipo declarado de cada parámetro contra el tipo
estático del argumento correspondiente en el sitio de llamada:

- Tipo estático de un argumento = tipo del literal (`42` → `numero`,
  `"x"` → `cadena`, `nuevo Clase(...)` → `Clase`, ...) o la anotación de
  la variable si el argumento es un identificador ya declarado con tipo
  (`ambitos`); si no hay ninguna de las dos, el argumento no aporta
  información de inferencia (mismo caso que hoy con valores no anotados).
- Si `T` ya tiene una entrada en el mapa y el nuevo argumento unifica con
  un tipo concreto distinto → error: `"el tipo genérico 'T' se infirió
  como 'numero' pero este argumento es de tipo 'cadena'"`.
- Si al terminar de procesar los argumentos algún parámetro genérico de
  la declaración no obtuvo entrada en el mapa (solo aparece en el tipo de
  retorno, por ejemplo) → error pidiendo turbofish:
  `"no se puede inferir el tipo genérico 'T'; usá 'nombre::<Tipo>(...)'"`.
- Con turbofish, el mapa se llena directamente desde los argumentos de
  tipo explícitos, sin unificación — y aun así se valida cada argumento
  normal contra el tipo ya sustituido (mismo chequeo estático de Fase 27
  que ya existe para parámetros con tipo fijo).

No hay unificación estructural recursiva (al estilo Hindley-Milner) ni
inferencia bidireccional entre parámetros — es intencionalmente el
subconjunto más simple que cubre los casos reales de la tabla de
sintaxis. Ver "Fuera de alcance".

### Chequeo de bounds

Una vez resuelto el mapa de sustitución para un sitio de uso concreto
(llamada de función, instanciación con `nuevo Clase<Tipo>(...)`,
anotación de variable `x: Pila<numero>`), por cada parámetro genérico con
bound(s):

- Si el tipo concreto sustituido es una clase/estructura conocida →
  verificar que `tipos[claseConcreta].interfaces` (transitivamente, vía
  `padre`) contenga cada interfaz del bound; si no, error:
  `"'ClaseConcreta' no implementa 'Comparable', requerido por el
  parámetro genérico 'T'"`.
- Si el tipo concreto sustituido es un tipo primitivo (`numero`, `cadena`,
  `logico`, `lista`, `dic`) → error: `"los tipos primitivos no pueden
  satisfacer la restricción 'Comparable'"` (los primitivos no implementan
  interfaces en el modelo POO actual; ver Fase 28).
- Si el tipo concreto no se pudo determinar estáticamente (argumento sin
  anotación, valor dinámico) → **no se verifica nada** (mismo criterio
  de degradación de Fase 27); el único chequeo que sobrevive en runtime
  es el `lat_verificar_tipo(..., LAT_OBJETO, ...)` que ya se emite hoy
  para cualquier parámetro anotado como tipo de objeto, genérico o no.

## Código generado de ejemplo

Por la decisión de diseño 1 (erasure), el código C/LLVM generado para una
función o clase genérica es **idéntico** al que ya se genera hoy para la
misma declaración con sus parámetros de tipo anotados como
`TipoAnotado::Objeto`/`Ninguno` sin bounds (es decir: el analizador
semántico consume la información genérica para verificar, pero nunca la
propaga a `compiler.cpp`/`GeneradorC`/`GeneradorLLVM` — la lista de
parámetros genéricos y los bounds simplemente se descartan después del
análisis semántico, igual que hoy se descartan las anotaciones de tipo
una vez verificadas).

```latino
funcion identidad<T>(x: T): T
    retornar x
fin

escribir(identidad(5))
escribir(identidad("hola"))
```

Genera exactamente:

```c
static LatValor lat_fn_identidad(LatValor v_x) {
    return v_x;
}
...
lat_escribir(lat_fn_identidad(lat_numero(5)));
lat_escribir(lat_fn_identidad(lat_cadena("hola")));
```

Una sola función C, reutilizada para ambas instanciaciones — no hay
`lat_fn_identidad__numero` ni `lat_fn_identidad__cadena`. `nuevo
Pila<numero>()` genera la misma llamada al constructor que generaría hoy
`nuevo Pila()` sin argumento de tipo; el `<numero>` se descarta tras la
verificación estática de que `apilar`/`desapilar` se usan consistentemente
como `numero` en ese sitio de uso.

## Mensajes de error

### Semánticos (compilación)

```
Error en línea 12: no se puede inferir el tipo genérico 'T' de 'identidad'; usá 'identidad::<Tipo>(...)'
Error en línea 8: el tipo genérico 'T' se infirió como 'numero' pero este argumento es de tipo 'cadena'
Error en línea 20: 'Circulo' no implementa 'Comparable', requerido por el parámetro genérico 'T' de 'maximo'
Error en línea 5: los tipos primitivos no pueden satisfacer la restricción genérica 'Comparable'
Error en línea 3: 'Pila' espera 1 argumento de tipo (<T>), se dieron 2
Error en línea 1: tipo desconocido 'T' (¿olvidaste declarar '<T>' en la firma?)
```

### Runtime

Ninguno nuevo — sigue siendo exactamente el mensaje ya existente de Fase
27 para valores de tipo objeto (`lat_verificar_tipo`), sin mención de
genéricos ni de qué interfaz se esperaba (ver decisión 2).

## Interacción con otros sistemas

- **Tipado gradual (Fase 27).** Los genéricos son una extensión directa
  de `TipoAnotado`/`validarTipoObjeto`, no un sistema paralelo — el
  código existente que usa `: numero`, `: cadena`, etc. no cambia en
  absoluto.
- **POO (Fase 28).** Bounds se verifican contra el mismo registro
  `AnalizadorSemantico::tipos`/`InfoTipo` que ya usa `implementa`; una
  clase genérica es, para el registro de tipos, una clase más (con un
  campo adicional `parametrosGenericos`) — herencia (`extiende`) e
  implementación (`implementa`) de una clase/interfaz genérica funcionan
  sin cambios adicionales al motor de herencia existente.
- **Módulos (Fase 30).** Sin cambios esperados en `ResolutorModulos` —
  ver decisión 7. Los tests de integración deben cubrir explícitamente
  `exportar clase Pila<T>` + `importar { Pila } desde "pila.lat"` para
  confirmarlo empíricamente, no solo por análisis.
- **Backend LLVM.** Por la decisión de erasure, `GeneradorLLVM` no
  necesita ningún cambio: ve exactamente el mismo `Programa` con
  parámetros ya verificados y sin metadata genérica adjunta (la
  información genérica vive solo en los nodos AST de declaración —
  `ClaseDef`/`EstructuraDef`/`InterfazDef`/`FuncionDef`/`MetodoDef` — y el
  analizador semántico es su único consumidor). Debe correrse la misma
  suite E2E en ambos backends (`c`/`llvm`) para confirmar paridad de
  salida, igual que exige `PLAN_LLVM.md` L12 para toda construcción
  nueva del lenguaje.
- **`incluir`.** Sin cambios: un archivo con `clase Pila<T>` incluido
  textualmente con `incluir "pila.lat"` se comporta igual que hoy con
  cualquier otra clase.

## Fuera de alcance (v1)

- **Monomorphización real / especialización de código por tipo
  concreto.** Ver decisión 1 — no aporta nada en un runtime ya
  dinámicamente tipado, y agregaría una complejidad de codegen
  significativa sin beneficio medible.
- **Verificación de bounds en runtime para valores no anotados.**
  Requeriría un nuevo primitivo de runtime (`lat_obj_implementa(v,
  "NombreInterfaz")`) que hoy no existe; queda como extensión futura
  natural si se necesita cerrar el hueco de la degradación gradual.
- **Verificación profunda elemento-por-elemento de `lista<T>`/`dic<K,
  V>` en runtime** (p. ej. al hacer `lista.agregar` en runtime con un
  valor de tipo incorrecto) — v1 solo chequea literales estáticamente,
  igual que Fase 27 hace con tipos simples.
- **Parámetros de tipo con valor por defecto** (`clase Pila<T =
  numero>`, al estilo de las plantillas de C++/genéricos de TypeScript).
- **Tipos de argumento genérico anidados** (`nuevo Pila<Pila<numero>>()`,
  `Dic<cadena, lista<numero>>`) — v1 solo acepta una lista plana de
  nombres de tipo simples/de clase entre `<>`; extender la gramática a
  ser recursiva es straightforward pero se deja para una fase futura
  para mantener acotado el primer corte.
- **Const generics** (parámetros de tipo que son valores, al estilo
  `[T; N]` de Rust) — no tiene un caso de uso claro en Latino hoy.
- **Varianza explícita** (`out`/`in`, covarianza/contravarianza) — el
  modelo de tipos de Latino (gradual, sin genéricos de colección
  estrictos hoy) no la necesita todavía.
- **Bounds sobre operadores** (exigir que `T` soporte `+`/`-`/comparación
  directamente, al estilo `T: std::ops::Add` de Rust) — v1 solo soporta
  bounds de "implementa esta interfaz"; operadores sobrecargables por
  interfaz quedan fuera hasta que exista sobrecarga de operadores en
  Latino (no existe hoy, fuera del alcance de este plan).
- **`exportar { X } desde "otro.lat"` con genéricos** — hereda la misma
  limitación que ya tiene M7 de `PLAN_MODULOS.md` (re-export sin
  resolver), no es específica de genéricos.

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `include/lexer.h` / `src/lexer.cpp` | Nueva palabra reservada `donde`; nuevo operador de dos caracteres `::` |
| `include/ast.h` | `struct ParametroGenerico { nombre; bounds; linea; }`; campo `std::vector<ParametroGenerico> genericos` en `ClaseDef`, `EstructuraDef`, `InterfazDef`, `FuncionDef`, `MetodoDef`; campo `std::vector<std::string> tipoArgs` junto a cada `tipoClase` existente (`ParamFuncion`, `CampoDef`, `MetodoDef`, `FuncionDef`); campo `std::vector<std::string> tipoArgs` en `NuevoExpr`; campo `std::vector<std::string> tipoArgsExplicitos` en `Llamada` (turbofish) |
| `include/parser.h` / `src/parser.cpp` | `parseParametrosGenericos()`, `parseBound()`, `parseClausulaDonde()`; extensión de `parseClase`/`parseEstructura`/`parseInterfaz`/`parseFuncion`/`parseMetodoDef` para leer `<...>` tras el nombre; extensión de `parseNombreTipoCalificado()` y `parseNuevo()` para leer `<...>` de argumentos de tipo; reconocimiento de `::<...>` tras un identificador en posición de llamada |
| `include/analizador_semantico.h` / `src/analizador_semantico.cpp` | `InfoParametroGenerico`, pila `genericosActivos`; extensión de `validarTipoObjeto` (orden de resolución); `InfoTipo`/`InfoMetodo` ganan `parametrosGenericos`; nueva lógica de inferencia/unificación y chequeo de bounds en sitios de llamada/instanciación/anotación |
| `src/ast_impresor.cpp` | Mostrar `<T: Bound>` en el volcado `--ast` para declaraciones y usos genéricos |
| `tests/test_genericos.cpp` (nuevo) | Suite: parser (declaración/uso/turbofish/`donde`), semántico (inferencia, bounds, errores), codegen (erasure — mismo C/LLVM que la versión no genérica equivalente) |
| `tests/test_genericos_e2e.cpp` (nuevo) | Programas completos: `Pila<T>`, `maximo<T: Comparable>`, interacción con módulos |
| `tests/CMakeLists.txt` | Targets `test_genericos`, `test_genericos_e2e` (+ variante `_llvm` del E2E, mismo patrón que Fase L11) |
| `ejemplos/` | 2-3 archivos `.lat` nuevos con anotación `#salida:` (pila genérica, función con bound, `donde`) |
| `SINTAXIS.md` | Nueva sección XII "Genéricos", palabras reservadas actualizadas |
| `CLAUDE.md` | Entrada de estado una vez implementado, igual que Fases 27/28/30 |

`runtime/latino.c`/`.h`, `runtime/libs/*`, `src/compiler.cpp`,
`src/resolutor_modulos.cpp`, `src/generador_llvm.cpp` (o equivalente):
**sin cambios** — consecuencia directa de la decisión de erasure (1) y de
que el mangling de módulos ya es agnóstico a si una declaración es
genérica (7).

## Estado

No iniciado. Fases propuestas, en orden de dependencia:

### Fase G1 — Lexer

Agregar la palabra reservada `donde` a la tabla de palabras reservadas
(`esPalabraReservada` en `src/lexer.cpp`, mismo mecanismo que `exportar`/
`importar`/`como` en Fase 30). Agregar el operador de dos caracteres
`::` junto a los demás operadores multi-carácter ya lexados (`..`, `~=`,
`==`, `!=`, `<=`, `>=`, `++`, `--`) — se lexa como un único token
`TokenType::Operador` con lexema `"::"`. No se agrega ningún valor nuevo
a `TokenType`.

### Fase G2 — AST

Agregar `ParametroGenerico` y los campos `genericos`/`tipoArgs` descritos
en "Archivos modificados". Ampliar `Visitante` **no** es necesario: los
genéricos no son nodos nuevos, son campos adicionales en nodos ya
existentes (`ClaseDef`, `FuncionDef`, etc.), así que ningún visitante
(`GeneradorC`, `GeneradorLLVM`, `AstImpresor`, `AnalizadorSemantico`)
necesita un método `visitar()` nuevo.

### Fase G3 — Parser: posiciones de declaración

`parseParametrosGenericos()`: tras leer el nombre de una
clase/estructura/interfaz/función/método, si el siguiente token es `<`,
parsear una lista separada por comas de `nombre [: Bound ['+' Bound]*]`
hasta `>`. `parseClausulaDonde()`: tras el tipo de retorno (o tras `)` si
no hay retorno) de una función/método, si el siguiente token es
`donde`, parsear una lista separada por comas de `nombre: Bound ['+'
Bound]*` y fusionar esos bounds con los ya declarados inline (error si el
mismo nombre no fue declarado en `<...>`). Integrar en
`parseClase`/`parseEstructura`/`parseInterfaz`/`parseFuncion`/
`parseMetodoDef` (todas ya reciben el nombre antes de decidir el resto de
la gramática, según Fase 28 — el punto de inserción es justo después de
consumir el nombre).

### Fase G4 — Parser: posiciones de uso

Extender `parseNombreTipoCalificado()` (ya existe desde Fase 30 para
`ns.Clase`) para, tras leer el nombre de tipo (calificado o no), mirar si
sigue `<`; si es así, parsear una lista separada por comas de nombres de
tipo (recursión sobre el mismo `parseNombreTipoCalificado()` para
permitir `ns.Contenedor<...>`, aunque v1 no soporte anidamiento profundo
de argumentos — ver "Fuera de alcance") hasta `>`, y guardarla en
`tipoArgs`. Extender `parseNuevo()` de forma idéntica entre el nombre de
clase y el `(`. Agregar reconocimiento de turbofish: en el punto donde el
parser de expresiones ya decide que un `Identificador` es el inicio de
una `Llamada` (mira si sigue `(`), agregar la alternativa "si sigue `::`,
consumir `::`, luego `<...>` como lista de tipos, y **entonces** exigir
`(`".

### Fase G5 — Análisis semántico

Implementar `genericosActivos` y el nuevo orden de resolución de
`validarTipoObjeto` (ver "Registro de parámetros genéricos activos").
Implementar la construcción del mapa de sustitución en sitios de llamada
(`visitar(Llamada&)`) y de instanciación (`visitar(NuevoExpr&)` — ya
existe para clases no genéricas, se extiende), y el chequeo de bounds
(ver "Chequeo de bounds"). Este es el grueso del trabajo, análogo en
tamaño a la Fase 3 de `PLAN_TIPADO.md` (fue "el cambio más complejo" de
esa fase por la ambigüedad del `:`; aquí el volumen equivalente está en
la inferencia y el chequeo de bounds, no en el parsing).

### Fase G6 — Codegen (confirmación de no-op)

No se espera ningún cambio productivo en `compiler.cpp` ni en el backend
LLVM (ver decisión 1 y "Código generado de ejemplo"). Esta fase es
explícitamente de **verificación**: compilar los ejemplos genéricos con
`--solo-c`/`--solo-ir` y confirmar bytes idénticos contra la versión
manualmente "desgenerificada" equivalente (mismo código con los `<T>`
quitados y los parámetros sin anotar). Si aparece alguna diferencia,
significa que algún campo nuevo de AST (`genericos`, `tipoArgs`) se está
filtrando sin querer a un generador que no debería mirarlo.

### Fase G7 — Interacción con módulos (verificación empírica)

Confirmar con un test E2E real (`test_genericos_e2e.cpp` +
`test_modulos_multi.cpp` o un nuevo caso en él) que `exportar clase
Pila<T>` + `importar { Pila } desde "pila.lat"` + `nuevo Pila<numero>()`
en el archivo importador compila y ejecuta correctamente sin cambios en
`resolutor_modulos.cpp` (ver decisión 7). Si el mangling rompe algo
(por ejemplo, si `tipoArgs` almacenara nombres de tipo que también deban
manglearse cuando refieren a una clase importada — `Pila<OtraClaseDelModulo>`),
corregir en esta fase, no en G5.

### Fase G8 — Pruebas y documentación

`tests/test_genericos.cpp` (unitario: parser + semántico + codegen-no-op),
`tests/test_genericos_e2e.cpp` (programas completos, backends `c` y
`llvm`), 2-3 ejemplos nuevos en `ejemplos/` con `#salida:`. Actualizar
`SINTAXIS.md` (sección XII) y `CLAUDE.md` (tabla de suites de prueba +
estado de fase) una vez todas las fases anteriores estén verdes.

## Pruebas

| Suite | Qué cubre |
|---|---|
| `test_genericos` | Parser: `<T>`, bounds inline, `+`, `donde`, turbofish, tipos de argumento en anotaciones/`nuevo`. Semántico: inferencia exitosa, error de inferencia ambigua, error de inferencia imposible (pide turbofish), bound satisfecho, bound no satisfecho, bound sobre primitivo (error), resolución de `T` como no-error dentro de su propio ámbito, `T` fuera de su ámbito sigue siendo "tipo desconocido". Codegen: C generado para una función/clase genérica es idéntico byte a byte al de su equivalente no genérica |
| `test_genericos_e2e` | `Pila<T>` completa (apilar/desapilar/vacía), `maximo<T: Comparable>` con una clase de usuario, `Par<A, B>`, interacción con `exportar`/`importar` (Fase 30), paridad `c`/`llvm` |
