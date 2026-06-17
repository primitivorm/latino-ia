# Plan de Implementación 100% del Lenguaje Latino

Basado en la documentación oficial del [Manual-Latino](https://github.com/lenguaje-latino/Manual-Latino).

---

## Estado Actual

El compilador `latino-ia` tiene el **núcleo del lenguaje completamente funcional** (Fases 0–8):
transpila código Latino → C → ejecutable nativo vía Lexer, Parser, AST, Análisis Semántico y
Generador de Código.

**Lo que YA funciona:**
- Variables, constantes, tipos de datos (número, cadena, lógico, nulo, lista, diccionario)
- Todos los operadores (aritméticos, relacionales, lógicos, concatenación, ternario)
- Estructuras de control: `si/osi/sino`, `elegir/caso`, `desde`, `mientras`, `repetir/hasta`, `romper`
- Funciones con parámetros variádicos
- Asignación múltiple
- `escribir()` / `imprimir()` / `poner()`
- 11 ejemplos E2E compilando y ejecutando correctamente

**Lo que FALTA:**
- 9 funciones base adicionales
- 7 librerías estándar (87 funciones en total)
- Soporte de módulos con `incluir()`

---

## Resumen de Brechas

| Categoría              | Total | Implementadas | Faltantes |
|------------------------|-------|---------------|-----------|
| Funciones base         | 12    | 3             | 9         |
| Librería `cadena`      | 22    | 0             | 22        |
| Librería `lista`       | 11    | 0             | 11        |
| Librería `dic`         | 5     | 0             | 5         |
| Librería `mate`        | 31    | 0             | 31        |
| Librería `sis`         | 8     | 0             | 8         |
| Librería `archivo`     | 9     | 0             | 9         |
| Librería `paquete`     | 1     | 0             | 1         |
| **TOTAL**              | **99**| **3**         | **96**    |

---

## Arquitectura de las Librerías

Cada librería se implementa en dos capas:

```
archivo.lat
    ↓ parser detecta llamada  cadena.longitud("hola")
    ↓ compiler.cpp  →  genera:  lat_cadena_longitud(...)
    ↓ runtime/libs/cadena.h + cadena.c  →  implementación en C
```

**Archivos nuevos a crear:**
```
runtime/
  libs/
    cadena.h / cadena.c
    lista.h  / lista.c
    dic.h    / dic.c
    mate.h   / mate.c
    sis.h    / sis.c
    archivo.h/ archivo.c
    paquete.h/ paquete.c
  latino.h   (actualizar con nuevas funciones base)
  latino.c   (actualizar con nuevas funciones base)
```

**Cambios en el compilador:**
- `src/compiler.cpp` — mapear llamadas `cadena.xxx()` → `lat_cadena_xxx()`
- `src/lexer.cpp` — asegurar que `cadena`, `lista`, etc. se tokenicen como identificadores (ya funciona)
- `src/parser.cpp` — soportar acceso por punto `cadena.longitud(s)` como llamada de librería
- `src/analizador_semantico.cpp` — registrar funciones de librería como símbolos conocidos

---

## Fase 9 — Funciones Base Faltantes

**Archivos:** `runtime/latino.h`, `runtime/latino.c`, `src/compiler.cpp`

### 9.1 Conversión de tipos

| Función      | Descripción                               | Implementación C              |
|--------------|-------------------------------------------|-------------------------------|
| `acadena(v)` | Convierte cualquier valor a cadena        | `lat_acadena(LatValor v)`     |
| `alogico(v)` | Convierte a lógico (0/"" → falso, resto → cierto) | `lat_alogico(LatValor v)` |
| `anumero(v)` | Convierte cadena o lógico a número        | `lat_anumero(LatValor v)`     |

### 9.2 Entrada estándar

| Función   | Descripción                               | Implementación C          |
|-----------|-------------------------------------------|---------------------------|
| `leer()`  | Lee una línea de stdin y la devuelve como cadena | `lat_leer()`        |

### 9.3 Introspección

| Función   | Descripción                                   | Implementación C          |
|-----------|-----------------------------------------------|---------------------------|
| `tipo(v)` | Devuelve cadena con el tipo: "numero", "cadena", "logico", "lista", "dic", "nulo" | `lat_tipo(LatValor v)` |

### 9.4 Salida con formato

| Función        | Descripción                                    | Implementación C              |
|----------------|------------------------------------------------|-------------------------------|
| `imprimirf(fmt, ...)` | Imprime con formato estilo printf       | `lat_imprimirf(...)`          |

### 9.5 Pantalla

| Función      | Descripción             | Implementación C        |
|--------------|-------------------------|-------------------------|
| `limpiar()`  | Limpia la terminal      | `lat_limpiar()`         |

### 9.6 Flujo y errores

| Función       | Descripción                                 | Implementación C         |
|---------------|---------------------------------------------|--------------------------|
| `error(msg)`  | Imprime mensaje de error a stderr y termina | `lat_error(LatValor msg)`|
| `incluir(archivo)` | Carga y ejecuta otro archivo `.lat`    | Requiere cambios en driver/parser |

**Criterio de aceptación:** Los 9 ejemplos siguientes compilan y ejecutan:
```latino
# tipo()
escribe tipo(42)        # "numero"
escribe tipo("hola")    # "cadena"
escribe tipo(cierto)    # "logico"

# conversiones
n = anumero("3.14")
s = acadena(100)
b = alogico(0)

# leer()
nombre = leer()
escribe "Hola " .. nombre

# imprimirf()
imprimirf("Pi = %.2f\n", 3.14159)
```

---

## Fase 10 — Librería `cadena`

**Archivos:** `runtime/libs/cadena.h`, `runtime/libs/cadena.c`, actualizar `compiler.cpp`

Referencia: `Manual-Latino/docs/librerias/cadena.rst`

| # | Función                              | Descripción                                   |
|---|--------------------------------------|-----------------------------------------------|
| 1 | `cadena.bytes(s)`                    | Lista de bytes UTF-8 de la cadena             |
| 2 | `cadena.char(n)`                     | Carácter ASCII del número dado                |
| 3 | `cadena.comparar(s1, s2)`            | Compara lexicográficamente (−1, 0, 1)         |
| 4 | `cadena.concatenar(s1, s2)`          | Une dos cadenas (equivalente a `..`)          |
| 5 | `cadena.contiene(s, sub)`            | `cierto` si `sub` está en `s`                 |
| 6 | `cadena.encontrar(s, sub)` / `indice`| Posición de primera ocurrencia (−1 si no)     |
| 7 | `cadena.es_alfa(s)`                  | `cierto` si todos los caracteres son letras   |
| 8 | `cadena.es_igual(s1, s2)`            | Comparación exacta sin distinguir mayúsculas  |
| 9 | `cadena.es_numerico(s)`              | `cierto` si representa un número              |
|10 | `cadena.esta_vacia(s)`               | `cierto` si longitud es 0                     |
|11 | `cadena.formato(fmt, ...)`           | Equivalente a sprintf                         |
|12 | `cadena.inicia_con(s, prefijo)`      | `cierto` si `s` empieza con `prefijo`         |
|13 | `cadena.insertar(s, pos, sub)`       | Inserta `sub` en posición `pos`               |
|14 | `cadena.invertir(s)`                 | Invierte la cadena                            |
|15 | `cadena.longitud(s)`                 | Número de caracteres                          |
|16 | `cadena.mayusculas(s)`               | Convierte a mayúsculas                        |
|17 | `cadena.minusculas(s)`               | Convierte a minúsculas                        |
|18 | `cadena.recortar(s)`                 | Elimina espacios al inicio y fin              |
|19 | `cadena.reemplazar(s, viejo, nuevo)` | Reemplaza todas las ocurrencias               |
|20 | `cadena.regex(s, patron)`            | Retorna primera coincidencia de regex         |
|21 | `cadena.regexl(s, patron)`           | Lista de todas las coincidencias              |
|22 | `cadena.rellenar_derecha(s, n, c)`   | Rellena por la derecha hasta longitud `n`     |
|23 | `cadena.rellenar_izquierda(s, n, c)` | Rellena por la izquierda hasta longitud `n`   |
|24 | `cadena.separar(s, delim)`           | Divide en lista usando `delim`                |
|25 | `cadena.subcadena(s, inicio, fin)`   | Extrae subcadena                              |
|26 | `cadena.termina_con(s, sufijo)`      | `cierto` si `s` termina con `sufijo`          |
|27 | `cadena.ultimo_indice(s, sub)`       | Posición de última ocurrencia                 |

**Criterio de aceptación:**
```latino
incluir "cadena"
s = "Hola Mundo"
escribe cadena.longitud(s)           # 10
escribe cadena.mayusculas(s)         # HOLA MUNDO
escribe cadena.separar(s, " ")       # ["Hola", "Mundo"]
escribe cadena.contiene(s, "Mundo")  # cierto
```

---

## Fase 11 — Librería `lista`

**Archivos:** `runtime/libs/lista.h`, `runtime/libs/lista.c`, actualizar `compiler.cpp`

Referencia: `Manual-Latino/docs/librerias/lista.rst`

| # | Función                           | Descripción                                   |
|---|-----------------------------------|-----------------------------------------------|
| 1 | `lista.agregar(l, v)`             | Agrega `v` al final                           |
| 2 | `lista.comparar(l1, l2)`          | Compara dos listas elemento a elemento        |
| 3 | `lista.concatenar(l1, l2)`        | Une dos listas en una nueva                   |
| 4 | `lista.contiene(l, v)`            | `cierto` si `v` está en la lista              |
| 5 | `lista.crear(n, v)`               | Crea lista de `n` elementos con valor `v`     |
| 6 | `lista.eliminar(l, v)`            | Elimina primera ocurrencia de `v`             |
| 7 | `lista.eliminar_indice(l, i)`     | Elimina el elemento en índice `i`             |
| 8 | `lista.encontrar(l, v)` / `indice`| Índice de primera ocurrencia (−1 si no)       |
| 9 | `lista.extender(l1, l2)`          | Agrega todos los elementos de `l2` a `l1`     |
|10 | `lista.insertar(l, i, v)`         | Inserta `v` en posición `i`                   |
|11 | `lista.invertir(l)`               | Invierte el orden de los elementos            |
|12 | `lista.longitud(l)`               | Número de elementos                           |
|13 | `lista.separador(l, sep)`         | Une elementos como cadena con separador       |

**Criterio de aceptación:**
```latino
incluir "lista"
nums = [3, 1, 4, 1, 5]
lista.agregar(nums, 9)
escribe lista.longitud(nums)    # 6
escribe lista.contiene(nums, 4) # cierto
lista.invertir(nums)
escribe nums                    # [9, 5, 1, 4, 1, 3]
```

---

## Fase 12 — Librería `dic`

**Archivos:** `runtime/libs/dic.h`, `runtime/libs/dic.c`, actualizar `compiler.cpp`

Referencia: `Manual-Latino/docs/librerias/dic.rst`

| # | Función                    | Descripción                                         |
|---|----------------------------|-----------------------------------------------------|
| 1 | `dic.contiene(d, clave)`   | `cierto` si `clave` existe en el diccionario        |
| 2 | `dic.eliminar(d, clave)`   | Elimina la entrada con esa `clave`                  |
| 3 | `dic.llaves(d)`            | Lista con todas las claves                          |
| 4 | `dic.longitud(d)`          | Número de entradas                                  |
| 5 | `dic.valores(d)` / `vals`  | Lista con todos los valores                         |

**Criterio de aceptación:**
```latino
incluir "dic"
persona = {"nombre": "Ana", "edad": 30}
escribe dic.llaves(persona)           # ["nombre", "edad"]
escribe dic.contiene(persona, "edad") # cierto
dic.eliminar(persona, "edad")
escribe dic.longitud(persona)         # 1
```

---

## Fase 13 — Librería `mate`

**Archivos:** `runtime/libs/mate.h`, `runtime/libs/mate.c`, actualizar `compiler.cpp`

Referencia: `Manual-Latino/docs/librerias/mate.rst`

La mayoría mapean directamente a `<math.h>` de C.

### Constantes

| # | Función      | Valor                   | C              |
|---|--------------|-------------------------|----------------|
| 1 | `mate.pi()`  | 3.14159265358979...     | `M_PI`         |
| 2 | `mate.tau()` | 2π = 6.28318...         | `2 * M_PI`     |
| 3 | `mate.e()`   | 2.71828...              | `M_E`          |

### Trigonometría

| # | Función           | C            |
|---|-------------------|--------------|
| 4 | `mate.sen(x)`     | `sin(x)`     |
| 5 | `mate.cos(x)`     | `cos(x)`     |
| 6 | `mate.tan(x)`     | `tan(x)`     |
| 7 | `mate.asen(x)`    | `asin(x)`    |
| 8 | `mate.acos(x)`    | `acos(x)`    |
| 9 | `mate.atan(x)`    | `atan(x)`    |
|10 | `mate.atan2(y,x)` | `atan2(y,x)` |

### Hiperbólicas

| # | Función          | C         |
|---|------------------|-----------|
|11 | `mate.senh(x)`   | `sinh(x)` |
|12 | `mate.cosh(x)`   | `cosh(x)` |
|13 | `mate.tanh(x)`   | `tanh(x)` |
|14 | `mate.asenh(x)`  | `asinh(x)`|
|15 | `mate.acosh(x)`  | `acosh(x)`|
|16 | `mate.atanh(x)`  | `atanh(x)`|

### Exponencial y logaritmo

| # | Función          | C           |
|---|------------------|-------------|
|17 | `mate.exp(x)`    | `exp(x)`    |
|18 | `mate.log(x)`    | `log(x)`    |
|19 | `mate.log10(x)`  | `log10(x)`  |

### Potencia y raíz

| # | Función             | C             |
|---|---------------------|---------------|
|20 | `mate.pot(b,e)`     | `pow(b,e)`    |
|21 | `mate.raiz(x)`      | `sqrt(x)`     |
|22 | `mate.raizc(x)`     | `cbrt(x)`     |

### Redondeo

| # | Función              | C           |
|---|----------------------|-------------|
|23 | `mate.piso(x)`       | `floor(x)`  |
|24 | `mate.techo(x)`      | `ceil(x)`   |
|25 | `mate.redondear(x)`  | `round(x)`  |
|26 | `mate.truncar(x)`    | `trunc(x)`  |

### Utilidades

| # | Función                       | Descripción                          |
|---|-------------------------------|--------------------------------------|
|27 | `mate.abs(x)`                 | Valor absoluto                       |
|28 | `mate.max(a,b)`               | Máximo de dos valores                |
|29 | `mate.min(a,b)`               | Mínimo de dos valores                |
|30 | `mate.aleatorio()` / `alt()`  | Número aleatorio en [0.0, 1.0)       |
|31 | `mate.frexp(x)`               | Mantisa y exponente (lista de 2)     |
|32 | `mate.ldexp(m,e)`             | Reconstruye número desde mantisa/exp |
|33 | `mate.base(x,b)`              | Convierte número a cadena en base `b`|
|34 | `mate.parte(x)`               | Parte entera y fraccionaria (lista)  |
|35 | `mate.porc(v,total)`          | Porcentaje de `v` sobre `total`      |

**Criterio de aceptación:**
```latino
incluir "mate"
escribe mate.pi()           # 3.14159265358979
escribe mate.raiz(16)       # 4
escribe mate.piso(3.7)      # 3
escribe mate.abs(-5)        # 5
escribe mate.aleatorio()    # [0.0, 1.0)
```

---

## Fase 14 — Librería `sis`

**Archivos:** `runtime/libs/sis.h`, `runtime/libs/sis.c`, actualizar `compiler.cpp`

Referencia: `Manual-Latino/docs/librerias/sis.rst`

| # | Función                  | Descripción                                       | C / SO                        |
|---|--------------------------|---------------------------------------------------|-------------------------------|
| 1 | `sis.dormir(ms)`         | Pausa ejecución `ms` milisegundos                 | `Sleep()` / `usleep()`        |
| 2 | `sis.ejecutar(cmd)`      | Ejecuta comando del shell y devuelve salida        | `popen()` + lectura           |
| 3 | `sis.fecha()`            | Fecha actual como cadena `"DD/MM/AAAA"`           | `<time.h>`                    |
| 4 | `sis.salir(codigo)`      | Termina el programa con código de salida          | `exit()`                      |
| 5 | `sis.cwd()`              | Directorio de trabajo actual                      | `getcwd()`                    |
| 6 | `sis.iraxy(x, y)`        | Mueve el cursor de la terminal a columna x, fila y| Secuencias ANSI / `gotoxy()`  |
| 7 | `sis.tiempo()`           | Timestamp Unix en segundos (float)                | `time()` / `clock_gettime()`  |
| 8 | `sis.usuario()`          | Nombre del usuario del sistema                    | `getenv("USER")` / `LOGNAME`  |
| 9 | `sis.operativo()` / `op()`| Cadena del SO: "windows", "linux", "macos"       | Macros de preprocesador       |

**Criterio de aceptación:**
```latino
incluir "sis"
escribe sis.operativo()    # "windows" | "linux" | "macos"
escribe sis.fecha()        # "17/06/2026"
escribe sis.cwd()          # ruta actual
sis.dormir(500)
escribe sis.usuario()
```

---

## Fase 15 — Librería `archivo`

**Archivos:** `runtime/libs/archivo.h`, `runtime/libs/archivo.c`, actualizar `compiler.cpp`

Referencia: `Manual-Latino/docs/librerias/archivo.rst`

| # | Función                           | Descripción                                    | C              |
|---|-----------------------------------|------------------------------------------------|----------------|
| 1 | `archivo.anexar(ruta, texto)`     | Agrega texto al final de un archivo            | `fopen("a")`   |
| 2 | `archivo.borrar(ruta)` / `eliminar`| Elimina el archivo                            | `remove()`     |
| 3 | `archivo.crear(ruta)`             | Crea archivo vacío (error si ya existe)        | `fopen("wx")`  |
| 4 | `archivo.duplicar(origen, dest)`  | Copia un archivo                               | `fread`/`fwrite`|
| 5 | `archivo.ejecutar(ruta)`          | Ejecuta un archivo como script Latino          | Driver interno |
| 6 | `archivo.escribir(ruta, texto)`   | Sobreescribe el archivo con el texto dado      | `fopen("w")`   |
| 7 | `archivo.leer(ruta)`              | Devuelve contenido como cadena                 | `fread()`      |
| 8 | `archivo.lineas(ruta)`            | Devuelve lista de líneas                       | `fgets()`      |
| 9 | `archivo.renombrar(viejo, nuevo)` | Renombra / mueve un archivo                    | `rename()`     |

**Criterio de aceptación:**
```latino
incluir "archivo"
archivo.escribir("test.txt", "Hola Latino\n")
contenido = archivo.leer("test.txt")
escribe contenido               # "Hola Latino\n"
lineas = archivo.lineas("test.txt")
escribe lista.longitud(lineas)  # 1
archivo.borrar("test.txt")
```

---

## Fase 16 — Librería `paquete`

**Archivos:** `runtime/libs/paquete.h`, `runtime/libs/paquete.c`, actualizar `compiler.cpp`

Referencia: `Manual-Latino/docs/librerias/paquete.rst`

| # | Función              | Descripción                                                              |
|---|----------------------|--------------------------------------------------------------------------|
| 1 | `paquete.cargar(ruta)` | Carga una librería dinámica (`.dll`/`.so`) compilada en C y expone sus funciones al runtime |

**Notas de implementación:**
- Windows: `LoadLibrary()` + `GetProcAddress()`
- Linux/macOS: `dlopen()` + `dlsym()`
- La librería C debe exportar una función `lat_registrar(LatVM *vm)` que registra sus funciones

**Criterio de aceptación:**
```latino
incluir "paquete"
milib = paquete.cargar("miextension.dll")
resultado = milib.mi_funcion(42)
escribe resultado
```

---

## Fase 17 — `incluir()` y Sistema de Módulos

**Archivos:** `src/compiler.cpp`, `src/main.cpp`, `src/parser.cpp`

Para que las librerías funcionen con `incluir "cadena"`:

### 17.1 Resolución de módulos

1. El lexer/parser reconoce `incluir "nombre"` como sentencia de importación
2. El compilador marca las librerías requeridas
3. El generador C emite `#include "libs/nombre.h"` en el archivo C generado
4. El driver pasa la ruta de `runtime/libs/` al compilador C

### 17.2 Resolución de llamadas de librería

Actualmente `cadena.longitud(s)` se parsea como `acceso_miembro(cadena, longitud)(s)`.  
Se debe agregar resolución en el análisis semántico y generador de código:

```
cadena.longitud(s)  →  lat_cadena_longitud(s)
lista.agregar(l, v) →  lat_lista_agregar(l, v)
mate.raiz(x)        →  lat_mate_raiz(x)
```

### 17.3 Inclusión de archivos `.lat`

```latino
incluir "utilidades.lat"
```

El driver debe:
1. Detectar si la extensión es `.lat` (archivo) o sin extensión (librería estándar)
2. Para `.lat`: leer, compilar y enlazar el archivo junto al principal
3. Para nombre sin extensión: agregar el header de la librería estándar correspondiente

---

## Fase 18 — Operador RegEx (`~=`)

**Archivos:** `runtime/latino.c`, `src/compiler.cpp`

El operador `~=` ya se tokeniza. Falta la implementación en el runtime.

```latino
s = "hola mundo 123"
escribe s ~= "[0-9]+"   # cierto
```

**Implementación:**
- Windows: usar `<regex.h>` de MSVC o miniimplementación POSIX
- Linux/macOS: `<regex.h>` POSIX
- Función: `lat_regex_match(LatValor cadena, LatValor patron)` → `LatValor` lógico

---

## Fase 19 — Manejo de Memoria ✅

**Archivos:** `runtime/latino.h`, `runtime/latino.c`, `runtime/libs/cadena.c`, `runtime/libs/lista.c`

Conteo de referencias implementado:

- `LatCadenaBloque` (interno a `latino.c`): encabezado `size_t refs` + datos de cadena
  contigüos en memoria; `v.como.cadena` apunta a `datos[0]`, justo después del encabezado.
- `LatLista.refs` y `LatDic.refs`: campo `size_t` añadido como primer miembro.
- `lat_valor_retener(LatValor v)` — incrementa el contador; devuelve el mismo valor.
- `lat_valor_liberar(LatValor v)` — decrementa; libera en cascada cuando llega a 0.
- Constructores `lat_cadena`, `lat_concatenar`, `lista_nueva`, `dic_nuevo` inicializan `refs = 1`.
- Las tres asignaciones directas de `LatLista` en `cadena.c` y la de `lista.c` también
  inicializan `refs = 1`.
- Ejemplo: `ejemplos/memoria_ejemplo.lat`.

---

## Fase 20 — Pruebas de Cobertura Completa

**Archivos:** `tests/`

Para cada librería agregar:

| Suite de prueba              | Archivo               |
|------------------------------|-----------------------|
| Funciones base faltantes     | `test_funciones_base.cpp` |
| Librería cadena              | `test_lib_cadena.cpp`     |
| Librería lista               | `test_lib_lista.cpp`      |
| Librería dic                 | `test_lib_dic.cpp`        |
| Librería mate                | `test_lib_mate.cpp`       |
| Librería sis                 | `test_lib_sis.cpp`        |
| Librería archivo             | `test_lib_archivo.cpp`    |
| Sistema de módulos           | `test_incluir.cpp`        |

Cada suite debe:
1. Compilar un programa `.lat` que use la librería
2. Ejecutar el binario resultante
3. Comparar la salida con el valor esperado

---

## Orden de Implementación Recomendado

```
Fase 9  → Funciones base (desbloquea conversión de tipos y entrada)
Fase 10 → Librería cadena  (la más usada en ejemplos reales)
Fase 17 → Sistema incluir  (necesario para que las librerías sean accesibles)
Fase 11 → Librería lista
Fase 12 → Librería dic
Fase 13 → Librería mate
Fase 14 → Librería sis
Fase 15 → Librería archivo
Fase 16 → Librería paquete
Fase 18 → RegEx
Fase 19 → Manejo de memoria
Fase 20 → Pruebas completas
```

---

## Criterio de "100% Implementado"

El lenguaje se considera **100% implementado** cuando:

- [ ] Los 12 programas de ejemplo de `Manual-Latino/docs/ejemplos/` compilan y ejecutan correctamente
- [ ] Cada función documentada en `Manual-Latino/docs/librerias/` tiene al menos un test que pasa
- [ ] Cada función documentada en `Manual-Latino/docs/funciones/` tiene al menos un test que pasa
- [ ] Cada estructura de control de `Manual-Latino/docs/sintaxis/` compila y ejecuta correctamente
- [ ] `incluir "cadena"`, `incluir "lista"`, `incluir "mate"`, `incluir "sis"`, `incluir "archivo"`, `incluir "dic"`, `incluir "paquete"` resuelven correctamente
- [ ] El operador `~=` (regex) funciona sobre cadenas
- [ ] `leer()` funciona en modo interactivo y en pipe

---

## Referencias

- Documentación oficial: `C:\Github\Manual-Latino\docs\`
- Especificación del lenguaje: `C:\Github\latino-ia\WORK.md`
- Runtime actual: `C:\Github\latino-ia\runtime\latino.c`
- Generador de código: `C:\Github\latino-ia\src\compiler.cpp`
