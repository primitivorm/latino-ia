# SINTAXIS DE LATINO
A continuación se explica de manera general la definición del lenguaje de programación *Latino*
<a name="comt"></a>
##  I. Comentarios:
Los comentarios de una linea empezaran por el signo de gato `#` (como en python) o `//` (como en C).
```js
#Este es un comentario estilo python
//Este es un comentario estilo C
```
Los comentarios multilínea son al estilo C. inician con `/*` y terminan con `*/`.
```c
/*
Este es un
comentario
multilínea
*/
```
<a name="var"></a>
## II. Variables:
Las variables son identificadores asociados a valores.
```python
nombre = "Juan Perez"
calificacion = 10
numeros = [1, 2, 3, 4, 5]  //esto es una lista ó arreglo.
sueldos = { "Jesus" : 10000, "Maria" : 20000, "Jose" : 30000 } //esto es un diccionario
```

También se puede declarar una variable con anotación de tipo estática, aunque
Latino mantiene tipado gradual y el valor real sigue siendo dinámico en runtime:
```python
nombre: cadena = "Juan Perez"
edad: numero = 30
activo: logico = cierto
p: Pila<numero> = nuevo Pila<numero>()
```

Es posible asignar más de una variable en una sola línea de código
```python
a, b, c = 1, 2, 3   #a = 1  b = 2  c = 3
a, b, c = 1, 2      #a = 1  b = 2  c = nulo
a, b    = 1, 2, 3   #a = 1  b = 2  se descarta el valor 3
```

### Asignación de tipos y constantes
```python
PI = 3.14159        # constante por convención, pero no es una palabra reservada
G = 9.8

const LOGIN = "admin"      # ojo: 'const' es una forma válida de declarar constantes
var contador = 0            # forma equivalente a una variable normal
```

La sintaxis actual de Latino también soporta declarar constantes en mayúsculas
y `global` a nivel superior del archivo para compartir estado entre funciones:
```python
global _VERSION_ = "1.0"
global contador = 0
```
<a name="puede"></a>
### __Un identificador puede:__
1. Empezar por guión bajo `_` o letras `a-z ó A-Z`. No son validas las letras acentuadas ni la `ñ` como letras en los identificadores.
2. Contener caracteres en mayúsculas y minúsculas.
Latino  es sensible a mayúsculas y minúsculas. Por lo que los siguientes identificadores no son los mismos.
```python
mensaje = "Hola mundo"
Mensaje = "¡Hasta la vista baby!"
```
<a name="NOpuede"></a>
### __Un identificador NO puede:__
1. Empezar por un número.
2. Empezar por un símbolo, ni tampoco que sea una palabra reservada.

<a name="global"></a>
### Variables globales: `global`

Una variable declarada con `global` a nivel superior del archivo es visible
desde cualquier función o método, tanto para leerla como para modificarla
-- una asignación a ese nombre dentro de una función muta la variable real,
en vez de crear una copia local como ocurriría con una variable de nivel
superior sin `global` (o con `var`/`const`).

```python
global _VERSION_ = "1.0"
poner(_VERSION_)   # 1.0

global contador = 0

funcion incrementar()
    contador = contador + 1
fin

incrementar()
incrementar()
poner(contador)    # 2
```

`global` solo es válida a nivel superior del módulo: usarla dentro de una
función o método es un error de compilación.

<a name="const"></a>
## III. Constantes:
Cuando se declara una constante, también se debe asignar el valor forzosamente.

Las constantes se deben declarar en **mayúsculas**.
```python
PI = 3.14159
G = 9.8
```

<a name="datos"></a>
## IV. Tipos de datos
Latino tiene tipos dinámicos en runtime, pero acepta anotaciones de tipo
estáticas y nombres de tipos de clase/estructura/interfaz en varias
posiciones. Los tipos básicos y los usados por FFI/genéricos se manejan así:

| latino | significado |
| --- | --- |
| `logico` | valor booleano (`cierto`/`falso`) |
| `numero` | numérico de punto flotante (`double`) |
| `cadena` | texto |
| `lista` | colección ordenada |
| `dic` / `diccionario` | colección asociativa |
| `nulo` | valor nulo |
| `entero8`…`entero64` | enteros con signo en FFI |
| `natural8`…`natural64` | enteros sin signo en FFI |
| `puntero` | puntero opaco en FFI |
| `NombreClase` / `ns.NombreClase` | tipo de objeto definido por el usuario |
| `lista<T>` / `dic<T>` | formularios genéricos documentados/estáticos |

En C, el runtime usa `LatValor` con tipo dinámico; los tipos de usuario y los
genéricos se compilan a ese mismo sistema, sin un tipo de runtime separado.

<a name="opds"></a>
## V. Operadores:
A continuación se listan los operadores:
| Categoría              | Operadores                             |
| ---------------------- | -------------------------------------- |
| Aritmético             | `+, -, *, /, % (modulo), ^ (potencia)` |
| Lógico                 | `&&` (y), `||` (o)                     |
| Concatenación          | `..`                                   |
| Incremento, decremento | `++, --` (válido solo en post)        |
| Relacional             | `==, !=, <, >, <=, >=, ~= (regex)`     |
| Asignación             | `=`                                    |
| Acceso a miembro       | `.`                                    |
| Acceso por índice      | `[]`                                   |
| Comparación de tipo    | `es`                                   |
| Operador ternario      | `(condicion) ? opcion1 : opcion2`      |

La precedencia de operadores es la misma que otros lenguajes de programación como C / Python / Lua.

El operador ternario se puede usar para asignar valores basado en una condición, los paréntesis son requeridos para obtener el resultado esperado.
```python
numero = -5
mensaje = (numero < 0) ? " es negativo" : " es positivo"
escribir("El número " .. numero .. mensaje)
#salida: El número -5 es negativo
```

La comparación `es` se usa para comprobar si un objeto es instancia de una
clase o interfaz, con soporte a nombres calificados por namespace (`geo.Circulo`):
```python
si perro es Animal
    escribir("es un animal")
fin
```
<a name="estrCtrl"></a>
## VI. Estructuras de control:
<a name="si"></a>
### si (if)
Evalua una condición y si el resultado es `verdadero` ejecuta
las instrucciones a continuación. Los paréntesis son opcionales en la condición.
```python
si (condicion)
   #codigo
fin

edad = 18
si edad >= 18
    escribir("Eres mayor de edad, ya puedes votar.")
fin
#salida: Eres mayor de edad, ya puedes votar.
```

En caso de que el resultado de la condición sea `falso` se deberá de ejecutar el código después de la palabra reservada `sino`.
```python
si condicion
   #codigo
sino
   #codigo
fin

edad = 5
si edad >= 18
    escribir("Eres mayor de edad, ya puedes votar.")
sino
    escribir("Eres un niño, tienes que ir a la escuela.")
fin
#salida: Eres un niño, tienes que ir a la escuela.
```

Es posible el anidamiento de las sentencias `si-sino` de la siguiente manera:
```python
si condicion
   #codigo
sino
    si condicion
        #codigo
    sino
        #codigo
    fin
fin
```
Existe una forma más elegante de escribir el código anterior con la palabra reservada `osi`. En python es `elif expresion:`.
```python
si condicion1
    #codigo
osi condicion2
    #codigo
osi condicion3
    #codigo
sino
    #codigo
fin
```

---

<a name="elegir"></a>
### elegir (switch case)
Evalúa una opción de múltiples posibles casos.
La opción puede ser una expresión, en caso de que ninguna opción se cumpla en algún caso se ejecuta el código después de la palabra reservada `otro` o `defecto`.
Los valores de los casos solo pueden ser numéricos o cadenas.
```python
elegir (opcion)
    caso valor1:
        #codigo
    caso valor2:
        #codigo
    caso valor3:
        #codigo
    defecto:
        #codigo
fin

calificacion = 'B'
elegir(calificacion)
    caso 'A':
        escribir("Excelente!.")
    caso 'B':
        escribir("Bien echo!.")
    caso 'C':
        escribir("Bien echo!.")
    caso 'D':
        escribir("Todavia pasas.")
    caso 'F':
        escribir("Estas reprobado.")
    otro:
        escribir("Calificación invalida.")
fin
#salida: Bien echo!
```
Notese que se tiene que repetir el caso para `B` y `C` por el momento no se permiten casos múltiples como una sola opción.

La forma `elegir` puede usarse con cualquier expresión y el `defecto`/`otro`
actúa como caso por omisión; no hay `break` explícito porque cada caso finaliza
el bloque de elección.

---

<a name="desde"></a>
### desde (for loop)
Hace que una instrucción o bloque de instrucciones se repitan
desde una condición inicial y mientras la condición se cumpla (es decir sea `verdadero`), en esta sentencia los paréntesis si son requeridos.
```python
desde (condicion_inicial; condicion; incremento)
     #codigo
fin

desde (i=0; i <= 10; i++)
     escribir(i)
fin
#salida: 0 1 2 3 4 5 6 7 8 9 10

desde (i=10; i >= 0; i--)
     escribir(i)
fin
#salida: 10 9 8 7 6 5 4 3 2 1 0
```

Se puede especificar un salto diferente cambiando la expresión de incremento.
```python
desde(i=0; i < 50; i=i+10)
    imprimir(i)
fin
#salida: 0 10 20 30 40
```

### para ... en ... (recorrido de rangos)

`para` recorre únicamente valores `rango`; no recorre listas, cadenas ni
diccionarios. `rango(fin)` comienza en 0, `rango(inicio, fin)` usa paso 1 y
`rango(inicio, fin, paso)` permite indicar un paso entero. El límite final
siempre es exclusivo y un paso negativo recorre hacia atrás. El valor
`rango` es lazy; para crear una lista explícita se usa
`lista.a_lista(rango(...))`. El paso no puede ser cero y los argumentos deben
ser números enteros finitos.

```latino
para i en rango(5)
    escribir(i)
fin
#salida: 0 1 2 3 4

para i en rango(2, 7)
    escribir(i)
fin
#salida: 2 3 4 5 6

para i en rango(1, 10, 2)
    escribir(i)
fin
#salida: 1 3 5 7 9

para i en rango(5, 0, -1)
    escribir(i)
fin
#salida: 5 4 3 2 1

escribir(lista.a_lista(rango(0, 10, 2)))
#salida: [0, 2, 4, 6, 8]
escribir(tipo(rango(3)))
#salida: rango
```

La cabecera no lleva dos puntos al final. `romper` sale del bucle `para`
igual que de los demás bucles.

---

<a name="mientras"></a>
### mientras (do while)
Hace que una instrucción o bloque de instrucciones se repitan
mientras la condición sea `verdadero`, los paréntesis son opcionales en la condición.
```python
mientras (condicion)
    #codigo
fin

i=0
mientras i < 10
    escribir(i)
    i++ #incremento
fin
#salida: 0 1 2 3 4 5 6 7 8 9
```

---

<a name="rept"></a>
### repetir-hasta (parecido al `repeat` de Lua)
Hace que una instrucción o bloque de instrucciones
se repitan mientra la condición sea `falso`,
las instrucciones de código se ejecutan al menos una vez.
Los paréntesis son opcionales en la condición.
```python
repetir
    #codigo
hasta condicion

i=0
repetir
    escribir(i)
    i++ #incremento
hasta i == 10
#salida: 0 1 2 3 4 5 6 7 8 9
```

Se puede salir de los bucles (`desde`, `mientras` y `repetir`) con la palabra reservada `romper`.
```python
desde(i=0; i < 10; i++)
    si (i == 5)
        romper    #se sale del ciclo desde
    fin
    imprimir(i)
fin
#salida: 0 1 2 3 4   #se imprime solo hasta el 4
```
<a name="fun"></a>
## VII. Funciones
Una función consiste generalmente de una lista de sentencias a ejecutar, una lista de parámetros de entrada que regularán dicha acción.

Las funciones se definen con la palabra reservada `funcion` o la forma corta `fun`.

Se puede regresar el valor con la palabra clave `regresar`, `retornar`, `retorno` o la forma corta `ret`.
La estructura de una función es la siguiente:
```python
funcion nombreFuncion (argumento1, argumento2)
    #codigo
    retornar resultado
fin

#La siguiente función suma dos números
fun sumar(a, b)
  ret a + b
fin
```

Para invocar (ejecutar) una función se hace con el nombre de la función y la lista de parámetros entre paréntesis.
```python
r = sumar(2, 3)  #se almacena el resultado en la variable r
escribir(r)
#salida: 5
```

Se pueden crear funciones con un número variable de argumentos con `...` en el ultimo parámetro de la definición.
```python
funcion varArgs(arg1, arg2, ...)
  va = [...] #se obtienen los parametros 3 en adelante como una lista y se asignan a la variable va
  escribir("parametro 1: " .. arg1) #imprime el parametro 1
  escribir("parametro 2: " .. arg2) #imprime el parametro 2
  escribir("parametro 3: " .. va[0]) #imprime el parametro 3
  retornar arg1 + arg2
fin

#llamada a función varArgs con 2 elementos
r = varArgs(1, 2)
escribir(r)
#salida:
parametro 1: 1
parametro 2: 2
parametro 3: nulo
3

#llamada a funcion varArgs con 3 elementos
r = varArgs(1, 2, 3)
escribir(r)
#salida:
parametro 1: 1
parametro 2: 2
parametro 3: 3
3
```

### VII.A. Funciones anónimas
También se puede crear una función sin nombre (función anónima) con `fun` (o `funcion`) seguida de sus parámetros y cuerpo, y usarla como un valor cualquiera: asignarla a una variable, guardarla en una lista o un diccionario, o llamarla directamente.
```python
#asignamos una funcion anonima a una variable
x = fun (a,b)
    si b == 0
        b=1
    fin
    retorno a / b
fin

#igual las podemos agregar a una lista
hola = [ fun (a, b) ret a+b fin, fun (a, b) ret a*b fin ]

#ejecutamos cada funcion
poner(hola[0](1,2))  #suma
poner(hola[1](1,2))  #multiplica
poner(x(1,2))        #divide

#salida:
# 3
# 2
# 0.5
```
Una función anónima acepta los mismos elementos que una función nombrada: parámetros (opcionalmente con anotación de tipo), el parámetro variádico `...`, sentencias de control de flujo y `retornar`. Al no tener nombre, su valor es un `funcion` que se invoca con `(argumentos)` sobre la variable o expresión que la contenga.

### VII.B. Punto de entrada del programa

Una función de nivel superior llamada `principal` se ejecuta después de las
sentencias de nivel superior al compilar y ejecutar el programa. `inicio` es
un alias. Puede declararse sin parámetros o con `(argc, argv)`; en este
último caso, `argc` es el número de argumentos del usuario y `argv` es una
lista de cadenas. El nombre del ejecutable o archivo no se incluye en ninguno
de los dos.

Latino también soporta funciones anidadas dentro de otra función: se declaran
al mismo nivel que el resto del cuerpo y solo son visibles dentro de la
función que las contiene.

```latino
funcion principal(argc, argv)
    poner("recibiendo " .. argc .. " parametros:")
    poner(argv)
fin
#salida:
#recibiendo 2 parametros:
#["parametros", "aqui"]
```

La función `principal` tiene prioridad si se declaran ambos nombres. Al
pasar argumentos al CLI, por ejemplo
`latino principal.lat "parametros" "aqui"`, Latino compila y ejecuta el
programa; `--` permite pasar valores que comienzan con `-`. Sin argumentos
adicionales, el CLI mantiene el comportamiento de solo compilar cuando el
programa no define `principal` ni `inicio`.

<a name="lista"></a>
## VIII. Listas
Una lista es un tipo de colección. Es equivalente a lo que en otros lenguajes se conoce por arreglos o vectores.

Las listas pueden contener cualquier tipo de dato: lógico, númerico, literal, cadena, listas, diccionarios.

Crear una lista es tan sencillo como indicar entre corchetes y separados por comas los valores que queremos incluir en la lista.
```python
#declaracion de listas
numeros = [1, 2, 3, 4, 5]
vacia = []
```

Podemos acceder a cada uno de los elementos de la lista escribiendo el nombre de la lista e indicando el índice del elemento entre corchetes `[]`.
Ten en cuenta que el índice del primer elemento de la lista es 0:
```python
escribir(numeros[1])
#salida: 2
```

También podemos utilizar este operador para modificar un elemento de la lista si lo
colocamos en la parte izquierda de una asignación:
```python
numeros[0] = 99
escribir(numeros)
#salida: [99, 2, 3, 4, 5]
```

Podemos utilizar también números negativos. Si se utiliza un número negativo como índice, esto se traduce en que el índice empieza a contar desde el final, hacia la izquierda; es decir, con [-1] accederíamos al último elemento de la lista, con [-2] al penúltimo, con [-3], al antepenúltimo, y así sucesivamente.
```python
escribir(numeros[-1])
#salida: 5
```

La librería `lista` expone una API útil para manipular colecciones completas:
`lista.agregar`, `lista.longitud`, `lista.contiene`, `lista.invertir`,
`lista.concatenar`, `lista.encontrar`, `lista.insertar`, `lista.eliminar`,
`lista.eliminar_indice`, `lista.extender`, `lista.separador` y `lista.crear`.

### Indexación de cadenas
Una cadena es, internamente, un arreglo de caracteres, así que admite el mismo operador `[]` que las listas, tanto para leer como para modificar un carácter en una posición dada. El índice también puede ser negativo, con el mismo significado que en las listas.
```python
str = "hola mundo"
escribir(str[0])
#salida: h

str[4] = "-"
escribir(str)
#salida: hola-mundo
```
El valor asignado debe ser una cadena de un solo carácter. Como las cadenas se comparten por referencia igual que las listas, modificar una cadena por índice también afecta a cualquier otra variable que apunte al mismo valor.
<a name="dic"></a>
## IX. Diccionarios
Los diccionarios, también llamados matrices asociativas, deben su nombre a que son colecciones que relacionan una clave y un valor, entre llaves y separado el valor de la clave con dos puntos.
```python
#Diccionario de personajes y actores:
actores = { "Chilindrina": "Maria Antonieta de las Nieves",
     "El chavo del 8": "Roberto Gomez Bolaños",
     "Tres patines": "Leopoldo Fernandez" }
```

El primer valor se trata de la clave y el segundo del valor asociado a la clave.

Como clave usamos cadenas, esto es así porque los diccionarios se implementan como tablas hash, y a la hora de introducir un nuevo par clave-valor en el diccionario se calcula el hash de la clave para después poder encontrar la entrada correspondiente rápidamente.

Si se modificara el objeto clave después de haber sido introducido en el diccionario, evidentemente,
su hash también cambiaría y no podría ser encontrado.

La diferencia principal entre los diccionarios y las listas es que a los valores almacenados en un diccionario se les accede no por su índice, porque de hecho no tienen orden, sino por su clave, utilizando corchetes `[]`.
```python
escribir(actores["Chilindrina"])
#salida: Maria Antonieta de las Nieves
```

Al igual que en listas también se puede utilizar corchetes para reasignar valores.
```python
actores["Chilindrina"] = "Maria Antonieta"
escribir(actores["Chilindrina"])
#salida: Maria Antonieta
```

La librería `dic` expone los accesos típicos: `dic.llaves`, `dic.valores`,
`dic.contiene`, `dic.eliminar`, `dic.longitud` y `dic.elementos`.

### Librerías estándar e `incluir`
Además del código en un mismo archivo, Latino puede cargar librerías estándar
con `incluir "nombre"`, donde `nombre` suele ser un módulo del runtime:

```latino
incluir "cadena"
incluir "lista"
incluir "dic"
incluir "mate"
incluir "sis"
incluir "archivo"
incluir "paquete"
```

Esto hace visibles las funciones del módulo en el ámbito actual, sin aislar
el archivo en un módulo propio. Es el mecanismo recomendado para librerías y
scripts de utilidad, mientras que `exportar`/`importar` definen un módulo con
ámbito propio.

### POO: clases, estructuras, interfaces y herencia
Latino soporta programación orientada a objetos con clases y estructuras,
modificadores de acceso, herencia, interfaces y `nuevo`.

```latino
clase Persona
    publico nombre: cadena
    privado _edad: numero

    funcion Persona(nombre: cadena, edad: numero)
        este.nombre = nombre
        este._edad = edad
    fin

    publico funcion saludar(): cadena
        retornar "Hola, soy " .. este.nombre
    fin

    estatico funcion crear(nombre: cadena): Persona
        retornar nuevo Persona(nombre, 0)
    fin
fin

p = nuevo Persona("Ana", 30)
escribir(p.saludar())
```

Las estructuras se usan como tipos con campos y métodos, pero sin herencia:

```latino
estructura Punto
    x: numero
    y: numero

    funcion Punto(x: numero, y: numero)
        este.x = x
        este.y = y
    fin

    funcion suma(): numero
        retornar este.x + este.y
    fin
fin
```

Las interfaces describen un contrato de métodos que una clase debe implementar:

```latino
interfaz IImprimible
    funcion aCadena(): cadena
fin

clase Producto implementa IImprimible
    publico nombre: cadena

    publico funcion aCadena(): cadena
        retornar este.nombre
    fin
fin
```

La herencia usa `extiende` y el constructor de la clase base se llama con
`base(...)`:

```latino
clase Animal
    publico nombre: cadena

    funcion Animal(nombre: cadena)
        este.nombre = nombre
    fin
fin

clase Perro extiende Animal
    funcion Perro(nombre: cadena)
        base(nombre)
    fin

    publico funcion hablar(): cadena sobreescribir
        retornar este.nombre .. " dice: Guau"
    fin
fin
```

Los modificadores de acceso son `publico`, `privado` y `protegido`; la
sobrescritura usa `sobreescribir` y el operador `es` permite comprobar
instancias.

## X. Módulos: `exportar` / `importar`

Cada archivo `.lat` puede comportarse como un módulo con ámbito propio, al
estilo de ES Modules/TypeScript. Si un archivo no usa ni `exportar` ni
`importar`, sigue siendo un script plano de siempre, compatible con
`incluir "archivo.lat"` — ese mecanismo no cambia y sigue siendo el
indicado para librerías estándar (`incluir "cadena"`, etc.) y para scripts
que no necesitan aislamiento de nombres.

### Exportar

Anteponer `exportar` a una declaración de nivel superior (función, clase,
estructura, interfaz, `var`/`const` o asignación simple) la hace parte de
la API pública del archivo. Toda declaración de nivel superior que **no**
lleve `exportar` es privada a ese módulo y no puede importarse desde otro
archivo.

```python
exportar const PI = 3.14159

exportar funcion area_circulo(r)
    retornar PI * r * r
fin

funcion normalizar_radio(r)     # privada: no visible desde otro archivo
    retornar r < 0 ? 0 : r
fin
```

También existe `exportar por defecto`, del cual solo puede haber uno por
archivo, y que puede envolver una expresión, una función o una clase:

```python
exportar por defecto funcion saludar(nombre)
    retornar "Hola, " .. nombre
fin
```

### Importar

```python
# Nombrado, con alias opcional
importar { area_circulo, Circulo } desde "geometria.lat"
importar { area_circulo como area } desde "geometria.lat"

# Espacio de nombres completo
importar * como geo desde "geometria.lat"

# Por defecto
importar Config desde "config.lat"

escribir(area_circulo(2))
escribir(geo.area_circulo(2))
c = nuevo geo.Circulo(3)
```

Un nombre calificado por namespace (`geo.X`) también puede usarse como tipo:
`nuevo geo.Circulo(...)`, `expr es geo.Circulo`, `extiende geo.Circulo`,
`implementa geo.Figura`, o como anotación de tipo de un campo/parámetro.

Importar de un archivo que no exportó ese nombre es un error de
compilación (`el módulo 'X.lat' no exporta 'nombre'`); importar de un
archivo que nunca usó `exportar` también lo es, y sugiere `incluir` en su
lugar. Los imports circulares también son un error de compilación, con la
cadena de archivos involucrados.

`exportar { X } desde "otro.lat"` (re-export/"barril", para reexportar
piezas de otro módulo sin darles un nombre local en el archivo actual) se
acepta en el parser, pero su resolución todavía no está implementada.

### Palabras reservadas nuevas

| Palabra | Uso |
|---|---|
| `exportar` | marca una declaración como parte de la API pública del módulo |
| `importar` | trae nombres exportados de otro módulo al ámbito actual |
| `como` | alias en `importar`/`exportar` (`X como Y`, `importar * como ns`) |

(`desde` y `defecto` ya eran palabras reservadas antes de este mecanismo.)

## XI. Genéricos

Funciones, clases, estructuras e interfaces pueden declarar parámetros de
tipo entre `<>`, al estilo de Rust. Un parámetro genérico se comporta como
"cualquier tipo, pero siempre el mismo dentro de ese uso" — a diferencia de
Rust, el runtime de Latino ya es dinámicamente tipado (`LatValor`), así que
no hay especialización de código por tipo concreto: `identidad(5)` e
`identidad("hola")` comparten exactamente el mismo código compilado.

```latino
funcion identidad<T>(x: T): T
    retornar x
fin

escribir(identidad(5))                # 5 — T inferido de "5"
escribir(identidad::<cadena>("hola"))  # hola — turbofish explícito
```

### Restricciones ("bounds")

Un bound exige que el tipo concreto implemente una interfaz. Se combinan
con `+`, y también pueden escribirse en una cláusula `donde` al final de
la firma:

```latino
interfaz Comparable
    funcion compararCon(otroValor: Comparable): numero
fin

funcion maximo<T: Comparable>(a: T, b: T): T
    retornar a.compararCon(b) >= 0 ? a : b
fin

# Equivalente con "donde":
funcion maximo2<T>(a: T, b: T): T donde T: Comparable
    retornar a.compararCon(b) >= 0 ? a : b
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

    publico funcion cima(): T
        retornar lista.ultimo(este.items)
    fin
fin

p: Pila<numero> = nuevo Pila<numero>()
p.apilar(1)
p.apilar(2)
escribir(p.cima())   # 2
```

`lista`/`dic` aceptan la misma sintaxis como azúcar documental/estática:
`nums: lista<numero> = [1, 2, 3]`.

### Turbofish (`::<...>`)

`<...>` solo es válido en posición de tipo (declaración, anotación,
`nuevo Clase<...>()`) — nunca en posición de expresión, porque
`identidad<numero>(5)` sería ambiguo con una comparación encadenada. Para
dar un argumento de tipo explícito en una llamada se usa `::<...>`, igual
que en Rust:

```latino
funcion vacio<T>(): T
    retornar nulo
fin

x = vacio::<numero>()   # T solo aparece en el retorno: turbofish obligatorio
```

Los bounds se verifican en tiempo de compilación cuando el tipo concreto
se conoce (literal, `nuevo Clase(...)`, variable anotada, o turbofish); si
el valor es dinámico, se degrada igual que el resto del tipado gradual
(sin chequeo). Plan completo, decisiones de diseño y alcance en
[input/PLAN_GENERICOS.md](input/PLAN_GENERICOS.md).

## XII. FFI con C: `externo` / `inseguro`

Un bloque `externo` declara la firma **real** de una función C (tipos con
ancho fijo, punteros opacos) para llamarla directamente, sin escribir
ningún código C intermedio — el mismo modelo mental que `extern "C"` de
Rust. Se resuelve en tiempo de **compilación** (el linker del sistema
busca el símbolo al enlazar el ejecutable), a diferencia de
`paquete.cargar`/`paquete.llamar` (carga dinámica en runtime, con una
firma ya empaquetada a Latino) — ambos mecanismos conviven, cada uno
resuelve un problema distinto.

```latino
# Sin "enlazar": el símbolo ya está en el runtime C que todo ejecutable
# Latino enlaza siempre (msvcrt/ucrt en Windows, libc en Linux/macOS).
externo
    funcion abs(n: entero32): entero32
    funcion strlen(s: cadena): entero64
fin

# Con "enlazar": biblioteca de importación adicional para el enlace.
externo enlazar "user32"
    funcion MessageBoxA(hwnd: puntero, texto: cadena, titulo: cadena,
                         tipo: entero32): entero32
fin
```

Toda llamada a una función `externo` debe ocurrir dentro de un bloque
`inseguro ... fin`, o dentro de una función marcada `funcion inseguro`
— análogo a `unsafe` en Rust: un chequeo puramente estático (para que el
uso de FFI sea visible en una revisión de código), sin ningún efecto en
tiempo de ejecución.

```latino
inseguro
    escribir(abs(-5))            # 5
    escribir(strlen("hola"))     # 4
fin

funcion inseguro saludar_nativo()
    MessageBoxA(nulo, "Hola desde Latino", "FFI", 0)
fin
```

### Tipos FFI

`numero`/`cadena`/`logico`/`nulo` se reutilizan tal cual en una firma
`externo` (mapeo exacto y sin pérdida a `double`/`const char*`/`int`/
`void`). Los anchos fijos y el puntero opaco son exclusivos de FFI —
nunca se exponen los tipos C ambiguos por plataforma (`int`, `long`,
`size_t`):

| Tipo | Tipo C | Nota |
|---|---|---|
| `entero8` / `16` / `32` / `64` | `int8_t` … `int64_t` | con signo |
| `natural8` / `16` / `32` / `64` | `uint8_t` … `uint64_t` | sin signo |
| `puntero` | `void*` | opaco: no se indexa ni se lee desde Latino |

Un `entero64`/`natural64` que vuelve a Latino con un valor mayor a 2^53
pierde precisión (`LatValor` numérico es un `double`), igual que los
`Number` de JavaScript.

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
    sino
        free(p)
    fin
fin
```

`p == nulo` / `p != nulo` funcionan con el operador de igualdad normal;
`p es nulo` **no** — `es` solo acepta un nombre de clase a su derecha. El
ciclo de vida de un `puntero` es responsabilidad del programador: Latino
no tiene un destructor determinístico para él (misma responsabilidad que
deja `unsafe` en Rust).

### Palabras reservadas nuevas

| Palabra | Uso |
|---|---|
| `externo` | abre un bloque de declaraciones de funciones nativas |
| `enlazar` | (dentro de `externo`) biblioteca de importación adicional a enlazar |
| `inseguro` | habilita, en un bloque o una función completa, llamar funciones `externo` |

Plan completo, decisiones de diseño y alcance en
[input/PLAN_FFI.md](input/PLAN_FFI.md).

<a name="plbrsRvds"></a>
## XIII. Palabras reservadas hasta el momento
```
caso
cierto  | verdadero
como
defecto | otro
desde
donde
elegir
enlazar
exportar
externo
falso
fin
funcion | fun
global
hasta
importar
inseguro
mientras
nulo
para
repetir
regresar | retornar | retorno | ret
romper
si
sino
osi
```