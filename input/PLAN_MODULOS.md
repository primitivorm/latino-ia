# Plan de trabajo — Módulos al estilo TypeScript en Latino

## Contexto

Latino ya tiene un mecanismo de inclusión: `incluir "nombre"` (ver
[CLAUDE.md](../CLAUDE.md), Fase 17 de [PLAN_LIBS.md](PLAN_LIBS.md)). Cumple
dos roles distintos con la misma palabra clave:

1. `incluir "cadena"` (sin `.lat`) — referencia a una librería estándar
   embebida en el runtime (`cadena`, `lista`, `dic`, `mate`, `sis`,
   `archivo`, `paquete`). El compilador reconoce el nombre y mapea
   `lib.fn(args)` → `lat_lib_fn(args)` (`src/compiler.cpp`); no hay parseo
   de un archivo real.
2. `incluir "archivo.lat"` — inclusión textual. `main.cpp`
   (`procesarInclusioneesLat`) parsea el archivo referenciado y **empalma
   sus sentencias tal cual** en el lugar del nodo `Incluir`, de forma
   recursiva, con un `std::set` de rutas visitadas solo para cortar ciclos
   (una inclusión repetida se ignora en silencio). El AST resultante — un
   único `Programa` plano — es el que ve el analizador semántico y ambos
   backends (`GeneradorC`, `GeneradorLLVM`).

Esto funciona como un `#include` de C: no hay ámbito de módulo, ni
exportación explícita, ni importación selectiva. Todo identificador de
nivel superior (función, clase, estructura, interfaz, variable global) de
cualquier archivo incluido cae en el mismo espacio de nombres global
(`AnalizadorSemantico::funciones`, el ámbito 0 de `ambitos`, y los
registros de tipos de POO son mapas planos por nombre, sin prefijo de
archivo). Dos archivos que declaren una función con el mismo nombre
colisionan sin aviso claro, y no existe forma de decir "esto es privado a
este archivo" ni de importar solo una parte de un módulo.

Este plan agrega un segundo mecanismo, **`exportar` / `importar`**, que le
da a Latino módulos con ámbito de archivo y una API explícita — el mismo
modelo mental que ES Modules / TypeScript (`export` / `import ... from
...`). `incluir` no se modifica ni se retira: sigue siendo el mecanismo
para librerías estándar y para scripts que no necesitan aislamiento (compat
total, ver "Convivencia con `incluir`").

## Motivación

- Los ejemplos y programas de usuario crecen (`ejemplos/`, futuros
  proyectos multi-archivo reales) y hoy no hay forma de evitar que un
  archivo "interno" contamine el espacio de nombres global de quien lo
  incluye.
- Sin un límite de módulo explícito, refactors simples (renombrar una
  función auxiliar en un archivo) pueden romper otro archivo que
  casualmente declaraba algo con el mismo nombre y dependía de que
  `incluir` lo pisara o conviviera.
- Un modelo de exportación explícita (`exportar`) documenta, en el propio
  código, cuál es la API pública de un archivo — hoy esa intención solo
  vive en la cabeza de quien lo escribió.
- TypeScript/ES Modules es el modelo mental más transferible para quien
  llega a Latino desde JS/TS (público declarado del lenguaje, ver
  [SINTAXIS.md](../SINTAXIS.md)), y encaja con el estilo ya elegido para
  POO ("al estilo C#", [PLAN_POO.md](PLAN_POO.md)): tomar prestado un
  modelo conocido en vez de inventar uno propio.

### Decisiones de diseño

1. **Un archivo `.lat` es un módulo.** Si un archivo no usa ni `exportar`
   ni `importar`, se comporta exactamente igual que hoy (script plano,
   compatible con `incluir "archivo.lat"`) — ver punto 6.
2. **Ámbito de módulo por defecto.** Dentro de un archivo que participa del
   nuevo sistema, toda declaración de nivel superior (función, clase,
   estructura, interfaz, `var`/`const` global) es **privada al módulo** a
   menos que esté precedida por `exportar`. Esto es lo opuesto al modelo
   de `incluir`, y es intencional (paridad con TS/ESM: nada es global salvo
   que se declare público).
3. **Resolución en tiempo de compilación, no en runtime.** No existe un
   objeto "módulo" en el runtime de Latino (`runtime/latino.c` no cambia).
   `importar`/`exportar` se resuelven **antes** del análisis semántico,
   reescribiendo el AST a un único `Programa` plano con nombres
   renombrados (mangling) para evitar colisiones — el mismo punto de
   inserción que hoy usa `procesarInclusioneesLat` en `main.cpp`. Esto
   significa que **ni el analizador semántico ni `GeneradorC` ni
   `GeneradorLLVM` necesitan enterarse de que existen módulos**: siguen
   viendo el mismo `Programa` plano de siempre, con nombres ya únicos. Es
   la misma estrategia que ya funcionó para mantener ambos backends en
   paridad durante `PLAN_LLVM.md` (transformar el AST una sola vez, antes
   de la bifurcación de backend).
4. **Import estilo TS**: nombrado con posible alias (`importar { a, b como
   c } desde "ruta"`), de espacio de nombres (`importar * como ns desde
   "ruta"`), y por defecto (`importar Nombre desde "ruta"` +
   `exportar por defecto ...`). Ver "Sintaxis propuesta".
5. **Reutiliza palabras reservadas existentes.** `desde` (ya reservada por
   `desde i = 1 hasta 5`) y `defecto` (ya reservada por
   `elegir/caso/defecto`) se reutilizan tal cual en la sintaxis de import;
   solo se agregan **tres** palabras reservadas nuevas: `exportar`,
   `importar`, `como`.
6. **Compatibilidad total con `incluir`.**
   - `incluir "nombre"` (librería estándar) no cambia en absoluto.
   - `incluir "archivo.lat"` no cambia en absoluto: sigue empalmando todo
     el contenido del archivo en el espacio de nombres global del que
     incluye, exactamente como hoy. Es el mecanismo correcto para scripts
     que no necesitan aislamiento y para no romper los 27 ejemplos
     existentes ni ningún código ya escrito.
   - Un archivo puede usar `exportar` y seguir siendo incluido con
     `incluir` en vez de `importar` — en ese caso `exportar` se comporta
     como una anotación sin efecto (documental) y el archivo se empalma
     completo y sin renombrar, igual que hoy. Solo `importar` activa el
     mangling/aislamiento; `incluir` nunca lo hace. Esto evita tener que
     migrar nada de golpe.
7. **Imports circulares: error de compilación en v1.** Ver "Fuera de
   alcance".
8. **Sin relación con `incluir "paquete"`** (carga dinámica de `.dll`/`.so`
   en runtime, `runtime/libs/paquete.c`) ni con el tipo `LAT_MODULO` que
   ya usa esa librería — son mecanismos distintos (FFI a bibliotecas
   nativas en runtime vs. organización de código fuente Latino en tiempo
   de compilación) y este plan no los toca.

## Sintaxis propuesta

### Exportar declaraciones

```latino
# archivo: geometria.lat

exportar const PI = 3.14159

exportar funcion area_circulo(r)
    retornar PI * r * r
fin

funcion normalizar_radio(r)     # NO exportada: privada a este módulo
    retornar r < 0 ? 0 : r
fin

exportar clase Circulo
    funcion nuevo(este, r)
        este.r = normalizar_radio(r)
    fin

    funcion area(este)
        retornar area_circulo(este.r)
    fin
fin

exportar estructura Punto
    x: numero
    y: numero
fin

exportar interfaz Figura
    funcion area(este)
fin
```

### Export por defecto

```latino
# archivo: config.lat
exportar por defecto {
    "version": "1.0",
    "modo_debug": falso
}
```

Solo puede haber **un** `exportar por defecto` por archivo (error semántico
si se repite). Puede envolver una expresión, una función o una clase:

```latino
exportar por defecto funcion saludar(nombre)
    retornar "Hola, " .. nombre
fin
```

### Importar

```latino
# archivo: main.lat

# Nombrado — igual que "import { a, b } from './ruta'" en TS
importar { area_circulo, Circulo } desde "geometria.lat"

# Nombrado con alias — igual que "import { a as c } from './ruta'"
importar { area_circulo como area } desde "geometria.lat"

# Espacio de nombres completo — igual que "import * as geo from './ruta'"
importar * como geo desde "geometria.lat"

# Por defecto — igual que "import Config from './ruta'"
importar Config desde "config.lat"

escribir(area_circulo(2))
escribir(geo.Circulo)          # acceso calificado: geo.<nombre_exportado>
c = nuevo geo.Circulo(3)
escribir(Config.version)
```

`geo.Circulo`, `geo.area_circulo`, etc. se resuelven **en tiempo de
compilación** (no hay un diccionario `geo` en runtime): el resolutor de
módulos reescribe cada `geo.X` por el nombre interno único de `X` en el
módulo `geometria.lat`. Acceder a un miembro que `geometria.lat` no
exportó (`geo.normalizar_radio`) es un error semántico de compilación, no
un `nulo` en runtime.

### Re-export (barril)

```latino
# archivo: figuras.lat — re-exporta piezas de otros módulos sin
# darles un nombre local utilizable dentro de este archivo.
exportar { Circulo, area_circulo } desde "geometria.lat"
exportar { Punto } desde "geometria.lat"
```

### Tabla de palabras reservadas nuevas

| Palabra | Uso |
|---|---|
| `exportar` | marca una declaración de nivel superior (o un `{ ... } desde ...`) como parte de la API pública del módulo |
| `importar` | trae nombres exportados de otro módulo al ámbito actual |
| `como` | alias en `importar`/`exportar` (`X como Y`) y en `importar * como ns` |

(`desde` y `defecto` ya existen — ver Decisión de diseño 5.)

## Semántica de resolución

### Cuándo un archivo "entra" al sistema de módulos

Un archivo pasa a resolverse como módulo con ámbito propio si contiene al
menos una sentencia `exportar` o `importar`. Un archivo sin ninguna de las
dos sigue siendo un script plano: si lo alcanza un `importar { x } desde
"script.lat"`, es un error semántico ("el módulo 'script.lat' no exporta
nada — ¿querías 'incluir'?"); si lo alcanza un `incluir "script.lat"`, se
comporta exactamente como hoy.

### Algoritmo (nuevo paso entre el parseo y el análisis semántico)

Reemplaza/extiende `procesarInclusioneesLat` en `src/main.cpp` con un
`ResolutorModulos` que corre **antes** de `AnalizadorSemantico::analizar`:

1. Recorrer el `Programa` de entrada; por cada archivo referenciado por
   `importar`/`exportar ... desde`, resolver la ruta (relativa al archivo
   que importa, igual que hoy `incluir "archivo.lat"`) a una ruta
   canónica absoluta.
2. **Memoización por ruta canónica**: cada módulo se parsea y se procesa
   **una sola vez**, sin importar cuántos otros módulos lo importen
   (a diferencia de `incluir`, que hoy no memoiza — solo corta ciclos).
   Esto es necesario porque el resultado del procesamiento (su tabla de
   exportaciones + sus nombres ya renombrados) se reutiliza en cada punto
   de importación.
3. **Orden de procesamiento por dependencias (DFS post-order)**: antes de
   reescribir los identificadores de un módulo `A` que importa de `B`, `B`
   debe estar completamente procesado (su tabla de exportaciones,
   `nombre_exportado → nombre_interno_único`, ya construida). Si durante el
   DFS se revisita un módulo que sigue "en proceso" en la pila (no
   terminado), es un **import circular → error de compilación**, reportado
   con la cadena de archivos involucrados.
4. **Mangling de nombres de nivel superior**: a cada módulo se le asigna un
   identificador único derivado de su ruta canónica (slug válido en C:
   caracteres no alfanuméricos → `_`, con un sufijo numérico si dos rutas
   distintas generan el mismo slug). Toda declaración de nivel superior del
   módulo — `FuncionDef`, `ClaseDef`, `EstructuraDef`, `InterfazDef`, y
   destinos de `Asignacion` de nivel superior (`var`/`const` o asignación
   simple) — se renombra a `__mod_<slug>__<nombre_original>`,
   independientemente de si está exportada o no (así los nombres privados
   nunca colisionan entre módulos aunque terminen en el mismo `Programa`
   final).
5. **Reescritura de referencias internas**: dentro del propio módulo, todo
   uso del nombre original (llamadas recursivas, `nuevo NombreClase(...)`,
   `es NombreClase`, `base(...)`, y las referencias de tipo por nombre de
   clase que hoy se guardan como `std::string` en el AST —
   `CampoDef::tipoClase`, `InfoMetodo`/parámetros vía `tipoRetornoClase`,
   `parametrosClase`, ver `include/ast.h`) se reescribe al nombre
   renombrado. Esta pasada es puramente léxica sobre el propio subárbol del
   módulo (mismo conjunto de nombres definidos ahí), no requiere resolver
   imports todavía.
6. **Tabla de exportación del módulo**: `nombre_exportado → nombre_interno
   renombrado` (más una entrada especial `"__defecto__"` si hay `exportar
   por defecto`). Un re-export (`exportar { X } desde "otro.lat"`) copia la
   entrada correspondiente de la tabla de exportación de `"otro.lat"` (ya
   resuelta, por el orden DFS del punto 3) a la tabla propia, sin crear un
   alias utilizable dentro del archivo actual.
7. **Reescritura de cada `importar`**: por cada nombre importado (con o sin
   alias), se busca en la tabla de exportación del módulo referenciado; si
   no existe, error semántico ("el módulo 'geometria.lat' no exporta
   'foo'"). El nombre local (el alias, o el nombre original si no hay
   alias) queda mapeado al nombre interno renombrado del módulo origen.
   Luego se recorre el AST del módulo importador reescribiendo cada
   ocurrencia de ese nombre local por el nombre interno renombrado —
   incluyendo el caso `importar * como ns`, donde `ns.X` (un
   `AccesoMiembro` cuyo objeto es el `Identificador ns`) se reescribe
   directamente al nombre interno de `X`, y usar `ns` sin calificar, o
   calificar un nombre que `ns` no exporta, es error semántico.
8. **Ensamblado final**: se concatenan, en orden de dependencia (los
   importados antes que quien importa — irrelevante en la práctica porque
   todos los nombres ya son únicos y `GeneradorC`/`GeneradorLLVM` no
   dependen de orden de declaración para funciones/clases de nivel
   superior, igual que hoy), todas las sentencias de nivel superior de
   todos los módulos alcanzados transitivamente, con las sentencias
   `importar`/`exportar ... desde` (re-export) eliminadas del árbol — el
   `exportar` que prefija una declaración normal se descarta dejando la
   declaración desnuda (ya renombrada). El `Programa` resultante es
   indistinguible, para el analizador semántico y ambos backends, de uno
   escrito a mano sin módulos.

### Nodos de AST nuevos (`include/ast.h`)

- `ImportarDecl : Sentencia` — ruta, tipo (Nombrado/Espacio/PorDefecto),
  lista de `(nombre_origen, alias_local)`, alias de espacio si aplica.
- `ExportarDesde : Sentencia` — re-export (`exportar { ... } desde
  "ruta"`); mismo shape que `ImportarDecl` pero no crea bindings locales.
- Un campo `bool exportado = false;` agregado a `FuncionDef`, `ClaseDef`,
  `EstructuraDef`, `InterfazDef`, y a `Asignacion` (para `exportar const
  X = ...` / `exportar var X = ...` / `exportar X = ...` de nivel
  superior).
- Un campo `bool esDefecto = false;` junto con `exportado` en los mismos
  nodos, más una variante para `exportar por defecto <expresion>` cuando
  no envuelve una declaración con nombre (podría modelarse como una
  `Asignacion` sintética a un identificador interno reservado, p. ej.
  `__defecto__`, reutilizando el mismo mecanismo de mangling).

`ImportarDecl` y `ExportarDesde` **no necesitan** `visitar()` en
`AnalizadorSemantico`, `GeneradorC` ni `GeneradorLLVM`: el `ResolutorModulos`
los consume y elimina del árbol antes de que esas etapas lo vean — igual
que hoy pasa con los nodos `Incluir` de archivos `.lat` (ver
`src/main.cpp:34-93`). Sí conviene un `visitar(ImportarDecl&)` en
`ImpresorAST` (no-op o de una línea) por si `--ast` alguna vez se invoca en
un punto anterior a la resolución al depurar el propio resolutor.

## Mensajes de error

Todos reportados por `stderr` con número de línea, mismo estilo que
`AnalizadorSemantico` y `procesarInclusioneesLat` hoy:

- `el módulo 'X.lat' no exporta 'nombre'`
- `import circular detectado: A.lat → B.lat → A.lat`
- `'exportar por defecto' repetido en el módulo 'X.lat'`
- `'ns' no es un import de espacio de nombres` (usar `ns.X` sin que `ns`
  venga de `importar * como ns`)
- `'geo.normalizar_radio' no está exportado por 'geometria.lat'`
- `no se pudo abrir el módulo importado: 'ruta'` (igual que hoy para
  `incluir "archivo.lat"`)
- `'X.lat' no usa 'exportar' — ¿querías 'incluir "X.lat"'?` (cuando
  `importar` apunta a un archivo que nunca usa `exportar`)

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `src/lexer.cpp` | 3 palabras reservadas nuevas: `exportar`, `importar`, `como` |
| `include/ast.h` | nodos `ImportarDecl`, `ExportarDesde`; campos `exportado`/`esDefecto` en `FuncionDef`/`ClaseDef`/`EstructuraDef`/`InterfazDef`/`Asignacion` |
| `include/parser.h`, `src/parser.cpp` | `parseExportar()`, `parseImportar()`; despacho en `parseSentencia()` |
| `include/ast_impresor.h`, `src/ast_impresor.cpp` | `visitar(ImportarDecl&)`/`visitar(ExportarDesde&)` (debug, no-op funcional) |
| `src/main.cpp` | nuevo `ResolutorModulos` (reemplaza/extiende `procesarInclusioneesLat`), corre antes de `AnalizadorSemantico::analizar` |
| `tests/` | `test_parser` (sintaxis nueva), `test_ast` (nodos nuevos), suite nueva `test_modulos` (resolución: export/import nombrado, alias, namespace, default, re-export, colisión de nombres entre módulos, import circular → error, `incluir` sin cambios) + casos E2E en `ejemplos/` con anotación `#salida:` usando 2-3 archivos `.lat` reales |
| `SINTAXIS.md` | sección nueva documentando `exportar`/`importar` |
| `CLAUDE.md` | entrada de estado al completar el plan, igual que se hizo con `PLAN_LLVM.md`/`PLAN_POO.md` |

`runtime/latino.c`, `runtime/libs/*`, `GeneradorC`, `GeneradorLLVM` y
`AnalizadorSemantico` **no cambian** (Decisión de diseño 3) salvo por el
hecho de que el `Programa` que reciben ya viene con nombres únicos — no
necesitan saber que existieron módulos.

## Fases de implementación

- **M1 — Lexer y AST.** Palabras reservadas nuevas; nodos `ImportarDecl`/
  `ExportarDesde`; campos `exportado`/`esDefecto`. Sin lógica de
  resolución todavía. Pruebas: `test_lexer` (tokeniza las 3 palabras
  nuevas), `test_ast` (construcción de los nodos nuevos).

- **M2 — Parser.** `parseExportar()` (prefijo de `funcion`/`clase`/
  `estructura`/`interfaz`/`var`/`const`/asignación simple, y `exportar por
  defecto ...`), `parseImportar()` (las 3 formas: nombrado con alias,
  namespace, default), `parseExportar` con `desde` (re-export). Pruebas:
  `test_parser` con las variantes de sintaxis de "Sintaxis propuesta".

- **M3 — Resolución de un solo módulo (sin imports).** `ResolutorModulos`
  con mangling de nombres de nivel superior y reescritura de referencias
  internas (recursión, `nuevo`, `es`, `base`, tipos por nombre de clase),
  para un módulo que solo usa `exportar` pero no `importar` nada. Verificar
  que el `Programa` resultante compila igual con `--backend c` y
  `--backend llvm`.

- **M4 — Import nombrado + alias, entre dos archivos.** DFS con
  memoización por ruta canónica, tabla de exportación, reescritura de
  `importar { a, b como c } desde "ruta"`. Detección de import circular
  (error, sin intentar resolverlo). Pruebas E2E con 2 archivos `.lat`.

- **M5 — Import de namespace y por defecto.** `importar * como ns` (con
  reescritura de `ns.X`) e `importar Nombre desde "ruta"` +
  `exportar por defecto`. Pruebas E2E correspondientes.

- **M6 — Interacción con POO.** Casos específicos: clase exportada con
  herencia (`extiende`) de una clase exportada de *otro* módulo importado;
  interfaz exportada implementada (`implementa`) desde otro módulo;
  parámetros/retorno con tipo anotado de una clase importada
  (`CampoDef::tipoClase`, `tipoRetornoClase`) — confirmar que el mangling
  también alcanza esas referencias por nombre de tipo. Ampliar
  `test_poo`/`test_poo_e2e` con variantes multi-módulo.

- **M7 — Re-export (barril)** *(opcional, puede diferirse sin bloquear
  M1-M6)*. `exportar { X } desde "otro.lat"` mezclando tablas de
  exportación sin crear bindings locales.

- **M8 — Documentación y cierre.** Sección nueva en `SINTAXIS.md`, entrada
  de estado en `CLAUDE.md`, pase completo de `ctest` en serie (ver nota de
  `ctest -j` en `CLAUDE.md`).

## Riesgos técnicos

- **Reescritura de identificadores como pasada léxica sobre el AST.** No
  existe hoy en la base de código una pasada genérica de "recorrer y
  reemplazar todos los `Identificador`/`AccesoMiembro` que coincidan con
  un nombre" — hay que escribirla desde cero, con cuidado de no tocar
  identificadores que son parámetros/variables locales que *casualmente*
  coinciden con un nombre de nivel superior renombrado (un parámetro
  `area_circulo` dentro de una función no debe reescribirse). Mitigación:
  la reescritura debe respetar sombreado de ámbito (no bajar a renombrar
  dentro de un `FuncionDef`/bloque si ese ámbito ya declaró localmente el
  mismo nombre como parámetro o `var` local) — reusar la lógica de pilas de
  ámbito que ya tiene `AnalizadorSemantico::ambitos`, o replicar una
  versión mínima solo para esta pasada.
- **Doble analizador de nombres de tipo por string.** Las referencias de
  tipo de usuario en el AST son `std::string` sueltos (`tipoClase`,
  `tipoRetornoClase`, `parametrosClase`), no nodos `Identificador` — hay que
  auditar `include/ast.h` completo para no dejar ninguna referencia de tipo
  por nombre sin reescribir (un caso omitido produciría un error confuso
  recién en `AnalizadorSemantico`, "tipo desconocido", con la línea del uso
  pero sin pista de que la causa fue un mangling incompleto).
- **`--ast` y `--solo-ir`/`--solo-c` deben ejecutarse después de la
  resolución de módulos**, igual que `--ast` ya corre después de
  `procesarInclusioneesLat` hoy — solo hay que insertar el nuevo paso en el
  mismo punto de `main.cpp`, no un riesgo nuevo pero sí un punto a no
  regresionar.
- **Mensajes de error con nombres ya renombrados.** Si `AnalizadorSemantico`
  reporta un error sobre `__mod_geometria_lat__area_circulo` en vez de
  `area_circulo`, la experiencia de depuración empeora respecto a hoy.
  Mitigación: mantener, en el `ResolutorModulos`, un mapa
  `nombre_renombrado → (nombre_original, archivo)` y hacer que
  `AnalizadorSemantico`/`GeneradorC`/`GeneradorLLVM` lo consulten al
  formatear mensajes de error (cambio pequeño y aislado, o — más simple —
  hacer que el propio `ResolutorModulos` intercepte y traduzca los mensajes
  de error semánticos antes de imprimirlos, sin tocar
  `AnalizadorSemantico`).

## Fuera de alcance

- **Imports circulares "vivos"** (como permite ES Modules con bindings
  diferidos). v1 los trata como error de compilación con la cadena de
  archivos involucrados; revisar en un plan futuro si aparece un caso de
  uso real que lo requiera.
- **Resolución de paquetes externos** (equivalente a `node_modules`/npm).
  `importar ... desde "ruta"` siempre resuelve a una ruta de archivo
  relativa, igual que `incluir "archivo.lat"` hoy — no hay concepto de
  paquete instalable ni de registro.
- **Exports en runtime / reflexión de módulos** (p. ej. iterar
  dinámicamente qué exporta un módulo cargado). Todo se borra en tiempo de
  compilación; no hay objeto módulo en `runtime/latino.c`.
- **Retiro o deprecación de `incluir`.** Ambos mecanismos conviven
  indefinidamente (Decisión de diseño 6), igual que `--backend=c` y
  `--backend=llvm` conviven tras `PLAN_LLVM.md`.
- **Cambios a `incluir "paquete"` / `LAT_MODULO`** (carga dinámica de
  `.dll`/`.so` en runtime) — dominio no relacionado, ver Decisión de
  diseño 8.

## Estado

Sin empezar. Este documento fija sintaxis y estrategia de implementación
(resolución en el AST antes del análisis semántico, sin tocar
`AnalizadorSemantico`/`GeneradorC`/`GeneradorLLVM`/runtime) para que M1
pueda arrancar directamente.
