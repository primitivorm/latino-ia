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

Es posible asignar más de una variable en una sola línea de código
```python
a, b, c = 1, 2, 3   #a = 1  b = 2  c = 3
a, b, c = 1, 2      #a = 1  b = 2  c = nulo
a, b    = 1, 2, 3   #a = 1  b = 2  se descarta el valor 3
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
| latino           | tipo de dato en C |
| ---------------- | :---------------: |
| lógico           |     **bool**      |
| numérico         |    **double**     |
| cadena o literal |    **`char*`**    |
| lista (arreglo)  |    **struct**     |
| diccionario      |    **struct**     |

<a name="opds"></a>
## V. Operadores:
A continuación se listan los operadores:
| Categoría              | Operadores                             |
| ---------------------- | -------------------------------------- |
| Aritmético             | `+, -, *, /, % (modulo), ^ (potencia)` |
| Lógico                 | && (y), &#124;&#124; (o)               |
| Concatenación          | `..`                                   |
| Incremento, decremento | `++, --` (valido solo en post)         |
| Relacional             | `==, !=, <, >, <=, >=, ~= (regex)`     |
| Asignación             | `=`                                    |
| Acceso a miembro       | `.`                                    |
| Operador ternario      | `(condicion) ? opcion1 : opcion2`      |

La precedencia de operadores es la misma que otros lenguajes de programación como C / Python / Lua.

El operador ternario se puede usar para asignar valores basado en una condición, los paréntesis son requeridos para obtener el resultado esperado.
```python
numero = -5
mensaje = (numero < 0) ? " es negativo" : " es positivo"
escribir("El número " .. numero .. mensaje)
#salida: El número -5 es negativo
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

Se puede regresar el valor con la palabra clave `regresar`, `retornar` o la forma corta `ret`.
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
<a name="plbrsRvds"></a>
## X. Palabras reservadas hasta el momento
```
caso
cierto  | verdadero
defecto | otro
desde
elegir
falso
fin
funcion | fun
global
hasta
mientras
nulo
para
repetir
regresar | retornar | ret
romper
si
sino
osi
```