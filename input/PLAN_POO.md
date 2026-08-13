# Plan de trabajo — Programación Orientada a Objetos en Latino (al estilo C#)

## Contexto

Latino es un lenguaje de **tipado dinámico** que transpila a C. Actualmente soporta
tipos primitivos (`numero`, `cadena`, `logico`, `nulo`), colecciones (`lista`, `dic`),
funciones y un sistema de tipado gradual opcional (Fase 27). Este plan agrega soporte
de **clases**, **estructuras** e **interfaces** al estilo de C#, manteniendo
retrocompatibilidad total con el código existente.

El modelo de ejecución sigue siendo **transpilación a C**: cada instancia de clase se
representa como un `LatValor` de tipo `LAT_OBJETO`, que internamente es un diccionario
con metadatos de tipo y una tabla de métodos (vtable). La herencia se resuelve por
copia de vtable al crear la instancia. Las interfaces se verifican en el analizador
semántico (duck typing estructural con validación de firmas).

### Principios de diseño

1. **Español nativo**: palabras reservadas en español (`clase`, `estructura`, `interfaz`,
   `nuevo`, `este`, `publico`, `privado`, `protegido`, `implementa`, `extiende`,
   `abstracto`, `estatico`, `sobreescribir`).
2. **Retrocompatibilidad total**: el código existente sin POO sigue compilando y
   ejecutándose exactamente igual.
3. **Transpilación a C**: los objetos se representan como structs de C con tablas de
   funciones (vtable) y diccionarios de campos, reutilizando `LatValor` y el conteo
   de referencias existente (Fase 19).
4. **Consistencia con C#**: herencia simple de clases, implementación múltiple de
   interfaces, constructores, modificadores de acceso, miembros estáticos, clases
   abstractas y métodos abstractos/virtuales.

## Sintaxis propuesta

### Clases

```latino
clase Animal
    # Campos (propiedades)
    publico nombre: cadena
    privado _edad: numero
    protegido _especie: cadena

    # Constructor
    funcion Animal(nombre: cadena, edad: numero)
        este.nombre = nombre
        este._edad = edad
        este._especie = "desconocida"
    fin

    # Método público
    publico funcion hablar(): cadena
        retornar este.nombre .. " hace un sonido"
    fin

    # Método protegido
    protegido funcion obtenerEdad(): numero
        retornar este._edad
    fin

    # Método estático
    estatico funcion crear(nombre: cadena): Animal
        retornar nuevo Animal(nombre, 0)
    fin
fin
```

### Herencia

```latino
clase Perro extiende Animal
    privado _raza: cadena

    funcion Perro(nombre: cadena, edad: numero, raza: cadena)
        base(nombre, edad)          # llamada al constructor padre
        este._raza = raza
        este._especie = "canino"
    fin

    # Sobreescritura de método
    publico funcion hablar(): cadena sobreescribir
        retornar este.nombre .. " dice: ¡Guau!"
    fin

    publico funcion obtenerRaza(): cadena
        retornar este._raza
    fin
fin
```

### Interfaces

```latino
interfaz IComparable
    funcion compararCon(otro: IComparable): numero
fin

interfaz IImprimible
    funcion aCadena(): cadena
fin

clase Producto implementa IComparable, IImprimible
    publico precio: numero
    publico nombre: cadena

    funcion Producto(nombre: cadena, precio: numero)
        este.nombre = nombre
        este.precio = precio
    fin

    publico funcion compararCon(otro: IComparable): numero
        retornar este.precio - otro.precio
    fin

    publico funcion aCadena(): cadena
        retornar este.nombre .. ": $" .. este.precio
    fin
fin
```

### Estructuras (tipos valor)

```latino
estructura Punto
    x: numero
    y: numero

    funcion Punto(x: numero, y: numero)
        este.x = x
        este.y = y
    fin

    funcion distancia(otro: Punto): numero
        retornar mate.raiz((este.x - otro.x)^2 + (este.y - otro.y)^2)
    fin
fin
```

### Clases abstractas

```latino
abstracto clase Figura
    abstracto funcion area(): numero
    abstracto funcion perimetro(): numero

    publico funcion describir(): cadena
        retornar "Área: " .. este.area() .. ", Perímetro: " .. este.perimetro()
    fin
fin

clase Circulo extiende Figura
    privado _radio: numero

    funcion Circulo(radio: numero)
        este._radio = radio
    fin

    publico funcion area(): numero sobreescribir
        retornar 3.14159 * este._radio ^ 2
    fin

    publico funcion perimetro(): numero sobreescribir
        retornar 2 * 3.14159 * este._radio
    fin
fin
```

### Instanciación y uso

```latino
# Crear instancia
perro = nuevo Perro("Rex", 5, "Pastor Alemán")
escribir(perro.hablar())             # Rex dice: ¡Guau!
escribir(perro.obtenerRaza())        # Pastor Alemán

# Método estático
cachorro = Animal.crear("Luna")
escribir(cachorro.hablar())          # Luna hace un sonido

# Estructura
p1 = nuevo Punto(3, 4)
p2 = nuevo Punto(0, 0)
escribir(p1.distancia(p2))          # 5

# Polimorfismo
animales: lista = [
    nuevo Perro("Rex", 5, "Pastor"),
    nuevo Animal("Gato", 3)
]
desde(i = 0; i < lista.longitud(animales); i++)
    escribir(animales[i].hablar())
fin
```

### Operador `es` (instanceof)

```latino
si perro es Animal
    escribir("Es un animal")
fin

si perro es Perro
    escribir("Es un perro")
fin
```

### Tabla de palabras reservadas nuevas

| Palabra | Uso | Equivalente C# |
|---------|-----|-----------------|
| `clase` | Declaración de clase | `class` |
| `estructura` | Declaración de struct | `struct` |
| `interfaz` | Declaración de interfaz | `interface` |
| `nuevo` | Instanciación | `new` |
| `este` | Referencia a instancia actual | `this` |
| `base` | Referencia al constructor padre | `base` |
| `extiende` | Herencia de clase | `:` (herencia) |
| `implementa` | Implementación de interfaces | `:` (implementación) |
| `publico` | Modificador de acceso | `public` |
| `privado` | Modificador de acceso | `private` |
| `protegido` | Modificador de acceso | `protected` |
| `abstracto` | Clase/método abstracto | `abstract` |
| `estatico` | Miembro estático | `static` |
| `sobreescribir` | Override de método | `override` |
| `es` | Comprobación de tipo | `is` |

### Tabla de tipos ampliada

| Anotación | `LatTipo` en runtime |
|-----------|----------------------|
| `numero` | `LAT_NUMERO` |
| `cadena` | `LAT_CADENA` |
| `logico` | `LAT_LOGICO` |
| `lista` | `LAT_LISTA` |
| `dic` | `LAT_DICCIONARIO` |
| `nulo` | `LAT_NULO` |
| `NombreClase` | `LAT_OBJETO` (nuevo) |

## Código generado de ejemplo

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
fin

p = nuevo Persona("Juan", 30)
escribir(p.saludar())
```

Genera en C:

```c
/* --- Metadatos de clase: Persona --- */
static const char* _lat_clase_Persona_nombre = "Persona";
static const char* _lat_clase_Persona_padre = NULL;

/* Tabla de descriptores de campo (nombre, offset, acceso, tipo_anotado) */
typedef struct {
    const char* nombre;
    int acceso;  /* 0=publico, 1=privado, 2=protegido */
} LatCampoDesc;

static LatCampoDesc _lat_campos_Persona[] = {
    {"nombre", 0},
    {"_edad", 1},
};
static const int _lat_ncampos_Persona = 2;

/* Métodos de instancia */
static LatValor lat_met_Persona_saludar(LatValor v_este) {
    return lat_concatenar(
        lat_concatenar(lat_cadena("Hola, soy "),
                       lat_obj_get(v_este, "nombre")),
        lat_cadena(""));
}

/* Constructor */
static LatValor lat_ctor_Persona(LatValor v_nombre, LatValor v__edad) {
    lat_verificar_tipo(v_nombre, LAT_CADENA, "nombre", 5);
    lat_verificar_tipo(v__edad, LAT_NUMERO, "_edad", 5);
    LatValor v_este = lat_obj_nuevo("Persona");
    lat_obj_set(v_este, "nombre", v_nombre);
    lat_obj_set(v_este, "_edad", v__edad);
    /* Registrar métodos en la vtable del objeto */
    lat_obj_set_metodo(v_este, "saludar", lat_met_Persona_saludar);
    return v_este;
}

/* --- main --- */
int main(int argc, char *argv[]) {
    lat_set_args(argc, argv);
    LatValor v_p = lat_nulo();
    v_p = lat_ctor_Persona(lat_cadena("Juan"), lat_numero(30));
    lat_escribir(lat_obj_llamar_metodo(v_p, "saludar", 0));
    return 0;
}
```

### Herencia — código generado

```latino
clase Empleado extiende Persona
    publico puesto: cadena

    funcion Empleado(nombre: cadena, edad: numero, puesto: cadena)
        base(nombre, edad)
        este.puesto = puesto
    fin

    publico funcion saludar(): cadena sobreescribir
        retornar "Hola, soy " .. este.nombre .. ", " .. este.puesto
    fin
fin
```

Genera en C:

```c
/* Constructor de Empleado */
static LatValor lat_ctor_Empleado(LatValor v_nombre, LatValor v__edad, LatValor v_puesto) {
    /* Llamar al constructor padre */
    LatValor v_este = lat_ctor_Persona(v_nombre, v__edad);
    /* Reclasificar el objeto */
    lat_obj_set_clase(v_este, "Empleado", "Persona");
    /* Campos propios */
    lat_obj_set(v_este, "puesto", v_puesto);
    /* Sobreescribir método */
    lat_obj_set_metodo(v_este, "saludar", lat_met_Empleado_saludar);
    return v_este;
}
```

### Operador `es` — código generado

```latino
si p es Persona
    escribir("sí")
fin
```

Genera en C:

```c
if (lat_es_verdadero(lat_obj_es_instancia(v_p, "Persona"))) {
    lat_escribir(lat_cadena("sí"));
}
```

## Mensajes de error

### Semánticos (compilación)
```
Error semántico en línea 5: la clase 'Perro' extiende 'Animal' que no está definida
Error semántico en línea 8: campo privado '_edad' no accesible fuera de 'Animal'
Error semántico en línea 12: la clase 'Producto' no implementa el método 'compararCon' requerido por la interfaz 'IComparable'
Error semántico en línea 3: no se puede instanciar la clase abstracta 'Figura'
Error semántico en línea 7: el método 'area' está marcado como sobreescribir pero no existe en la clase padre
Error semántico en línea 15: no se puede heredar de la estructura 'Punto' (las estructuras no soportan herencia)
```

### Runtime
```
Error de tipo en línea 10: se esperaba un objeto de tipo 'Persona' pero se recibió 'numero'
Error en línea 5: campo privado '_edad' no accesible desde fuera de la clase 'Animal'
```

## Archivos modificados

| Archivo | Cambio |
|---------|--------|
| `include/ast.h` | Nodos `ClaseDef`, `EstructuraDef`, `InterfazDef`, `NuevoExpr`, `EsExpr`, `AccesoEste`, `LlamadaBase`; enums `ModificadorAcceso`; extensiones a `TipoAnotado` |
| `include/lexer.h` | (sin cambios de interfaz) |
| `src/lexer.cpp` | Registrar 15 nuevas palabras reservadas |
| `include/parser.h` | Declarar `parseClase()`, `parseEstructura()`, `parseInterfaz()`, `parseNuevo()` |
| `src/parser.cpp` | Implementar parseo de clases, estructuras, interfaces, `nuevo`, `este`, `base`, `es` |
| `include/analizador_semantico.h` | Tablas de clases/interfaces/structs, chequeo de acceso, verificación de interfaces |
| `src/analizador_semantico.cpp` | Visitantes para los nuevos nodos, verificación de herencia e interfaces |
| `include/compiler.h` | Métodos para generar clases/structs |
| `src/compiler.cpp` | Generación de vtables, constructores, métodos, `nuevo`, `es` |
| `runtime/latino.h` | `LAT_OBJETO`, `LatObjeto`, funciones `lat_obj_*` |
| `runtime/latino.c` | Implementación de `lat_obj_nuevo`, `lat_obj_get`, `lat_obj_set`, `lat_obj_set_metodo`, `lat_obj_llamar_metodo`, `lat_obj_es_instancia`, `lat_obj_set_clase` |
| `src/ast_impresor.cpp` | Volcado de los nuevos nodos en `--ast` |
| `tests/test_poo.cpp` (nuevo) | Suite completa: parser + semántico + codegen + E2E |
| `tests/CMakeLists.txt` | Target `test_poo` |

---

## Estado

### Fase 1 — Nuevas palabras reservadas en el Lexer (`src/lexer.cpp`)

Registrar las 15 nuevas palabras reservadas en la función `esPalabraReservada()`:

```cpp
// Agregar al conjunto de palabras reservadas:
"clase", "estructura", "interfaz",
"nuevo", "este", "base",
"extiende", "implementa",
"publico", "privado", "protegido",
"abstracto", "estatico", "sobreescribir",
"es"
```

**Impacto**: El lexer devolverá `TokenType::PalabraReservada` para estas palabras.
El código existente no usa ninguna de estas como identificador de variable (no hay
colisión), así que la retrocompatibilidad se mantiene. Si un programa existente usa
alguna como variable, se romperá — esto es aceptable porque son palabras con semántica
de POO que no deberían usarse como variables.

**Decisión de diseño**: Los nombres de tipos de clase (`Persona`, `Animal`, etc.) NO
son palabras reservadas. Son identificadores normales que el parser reconoce por contexto.

---

### Fase 2 — Nuevos nodos del AST (`include/ast.h`)

#### 2.1 Enums nuevos

```cpp
// Modificadores de acceso para campos y métodos
enum class ModificadorAcceso {
    Publico,     // accesible desde cualquier lugar
    Privado,     // solo dentro de la clase
    Protegido    // dentro de la clase y subclases
};
```

#### 2.2 Estructuras auxiliares

```cpp
// Declaración de un campo (propiedad) dentro de una clase/estructura
struct CampoDef {
    std::string nombre;
    TipoAnotado tipoAnotado = TipoAnotado::Ninguno;
    std::string tipoClase;        // nombre de clase si tipoAnotado == Ninguno pero hay tipo personalizado
    ModificadorAcceso acceso = ModificadorAcceso::Publico;
    bool esEstatico = false;
    ExprPtr valorDefecto;         // valor por defecto (opcional)
};

// Declaración de un método dentro de una clase/estructura/interfaz
struct MetodoDef {
    std::string nombre;
    std::vector<ParamFuncion> parametros;
    TipoAnotado tipoRetorno = TipoAnotado::Ninguno;
    std::string tipoRetornoClase;  // nombre de clase como tipo retorno
    ModificadorAcceso acceso = ModificadorAcceso::Publico;
    bool esEstatico = false;
    bool esAbstracto = false;
    bool esSobreescritura = false; // marcado con `sobreescribir`
    bool esConstructor = false;    // nombre == nombre de la clase
    ListaSent cuerpo;              // vacío si es abstracto o de interfaz
    int linea = 0;
};
```

#### 2.3 Nodos de sentencia

```cpp
// Declaración adelantada
struct ClaseDef;
struct EstructuraDef;
struct InterfazDef;

// clase NombreClase [extiende Padre] [implementa I1, I2, ...]
//     campos...
//     metodos...
// fin
struct ClaseDef : Sentencia {
    std::string nombre;
    std::string padre;                           // "" si no hereda
    std::vector<std::string> interfaces;         // interfaces implementadas
    bool esAbstracta = false;
    std::vector<CampoDef> campos;
    std::vector<MetodoDef> metodos;
    LATINO_ACEPTAR
};

// estructura NombreEstructura
//     campos...
//     metodos...
// fin
struct EstructuraDef : Sentencia {
    std::string nombre;
    std::vector<CampoDef> campos;
    std::vector<MetodoDef> metodos;
    LATINO_ACEPTAR
};

// interfaz NombreInterfaz
//     firmas de metodos...
// fin
struct InterfazDef : Sentencia {
    std::string nombre;
    std::vector<MetodoDef> metodos;  // todos sin cuerpo
    LATINO_ACEPTAR
};
```

#### 2.4 Nodos de expresión

```cpp
// nuevo NombreClase(arg1, arg2, ...)
struct NuevoExpr : Expresion {
    std::string clase;
    std::vector<ExprPtr> argumentos;
    LATINO_ACEPTAR
};

// expr es NombreClase
struct EsExpr : Expresion {
    ExprPtr objeto;
    std::string clase;
    LATINO_ACEPTAR
};

// este  (referencia a la instancia actual dentro de un método)
struct AccesoEste : Expresion {
    LATINO_ACEPTAR
};

// base(args...)  (llamada al constructor padre)
struct LlamadaBase : Sentencia {
    std::vector<ExprPtr> argumentos;
    LATINO_ACEPTAR
};
```

#### 2.5 Ampliar TipoAnotado

```cpp
enum class TipoAnotado {
    Ninguno,
    Numero,
    Cadena,
    Logico,
    Lista,
    Dic,
    Nulo,
    Objeto    // NUEVO — tipo personalizado (clase, estructura o interfaz)
};
```

Cuando `TipoAnotado == Objeto`, el nombre de la clase se almacena en un campo `std::string`
separado (en `ParamFuncion`, `CampoDef`, etc.).

#### 2.6 Ampliar la interfaz `Visitante`

```cpp
struct Visitante {
    // ... existentes ...

    // POO
    virtual void visitar(ClaseDef&) = 0;
    virtual void visitar(EstructuraDef&) = 0;
    virtual void visitar(InterfazDef&) = 0;
    virtual void visitar(NuevoExpr&) = 0;
    virtual void visitar(EsExpr&) = 0;
    virtual void visitar(AccesoEste&) = 0;
    virtual void visitar(LlamadaBase&) = 0;
};
```

**Nota**: Agregar métodos puros a `Visitante` requiere implementarlos en TODOS los
visitantes existentes (`ImpresorAST`, `AnalizadorSemantico`, `GeneradorC`). Si se
quiere evitar romper todo de golpe, usar la estrategia de métodos virtuales con default
(como ya se hizo con `visitar(Incluir&)`):

```cpp
virtual void visitar(ClaseDef&) {}
virtual void visitar(EstructuraDef&) {}
// ...
```

Y luego implementar cada visitante progresivamente. Sin embargo, dado que todos los
visitantes DEBEN manejar los nuevos nodos, es más seguro hacerlos puros para que el
compilador fuerze la implementación.

---

### Fase 3 — Modificaciones al Parser (`src/parser.cpp`, `include/parser.h`)

#### 3.1 Nuevos métodos de parseo

```cpp
// En parser.h — métodos privados:
SentPtr parseClase();
SentPtr parseEstructura();
SentPtr parseInterfaz();
ExprPtr parseNuevo();
SentPtr parseLlamadaBase();

// Auxiliares:
ModificadorAcceso parseModificadorAcceso();
MetodoDef parseMetodoDef(const std::string& nombreClase);
CampoDef parseCampoDef();
```

#### 3.2 Flujo de parseo de `parseSentencia()`

Ampliar el bloque de despacho de palabras reservadas:

```cpp
SentPtr Parser::parseSentencia() {
    if (actual.type == TokenType::PalabraReservada) {
        const std::string& p = actual.lexeme;
        // ... existentes (si, elegir, desde, etc.) ...
        if (p == "clase")       return parseClase();
        if (p == "estructura")  return parseEstructura();
        if (p == "interfaz")    return parseInterfaz();
        if (p == "abstracto") {
            // Lookahead: "abstracto clase NombreClase ..."
            avanzar();
            if (!esReservada("clase"))
                error("se esperaba 'clase' después de 'abstracto'");
            return parseClase(/*esAbstracta=*/true);
        }
        // ...
    }
    return parseSentenciaSimple();
}
```

#### 3.3 `parseClase()` — análisis detallado

```
parseClase(bool esAbstracta = false):
    l = línea actual
    avanzar()  // consume "clase"
    esperar Identificador → nombre

    // Herencia opcional
    padre = ""
    si esReservada("extiende"):
        avanzar()
        esperar Identificador → padre

    // Interfaces opcionales
    interfaces = []
    si esReservada("implementa"):
        avanzar()
        repetir:
            esperar Identificador → nombre_interfaz
            interfaces.agregar(nombre_interfaz)
            si esDelimitador(","):
                avanzar()
            sino:
                salir del bucle

    // Cuerpo de la clase
    saltarNuevasLineas()
    campos = []
    metodos = []

    mientras !esReservada("fin"):
        // Detectar modificadores
        acceso = ModificadorAcceso::Publico  (default)
        esEstatico = falso
        esAbstractoMiembro = falso

        si esReservada("publico"):
            acceso = Publico; avanzar()
        osi esReservada("privado"):
            acceso = Privado; avanzar()
        osi esReservada("protegido"):
            acceso = Protegido; avanzar()

        si esReservada("estatico"):
            esEstatico = cierto; avanzar()

        si esReservada("abstracto"):
            esAbstractoMiembro = cierto; avanzar()

        si esReservada("funcion") || esReservada("fun"):
            // Es un método
            metodo = parseMetodoDef(nombre)
            metodo.acceso = acceso
            metodo.esEstatico = esEstatico
            metodo.esAbstracto = esAbstractoMiembro
            metodos.agregar(metodo)
        sino:
            // Es un campo: "nombre : tipo [= valor]"
            si actual.type == Identificador:
                campo = parseCampoDef()
                campo.acceso = acceso
                campo.esEstatico = esEstatico
                campos.agregar(campo)
            sino:
                error("se esperaba un campo o método dentro de la clase")

        saltarNuevasLineas()

    esperarReservada("fin")
    retornar ClaseDef{nombre, padre, interfaces, esAbstracta, campos, metodos}
```

#### 3.4 `parseMetodoDef()` — dentro de una clase

```
parseMetodoDef(nombreClase):
    l = línea actual
    avanzar()  // consume "funcion" o "fun"
    esperar Identificador → nombreMetodo

    metodo.esConstructor = (nombreMetodo == nombreClase)

    esperarDelimitador("(")
    // Parsear parámetros (igual que parseFuncion existente)
    // Soporta anotaciones de tipo con ":"
    esperarDelimitador(")")

    // Tipo de retorno opcional
    si esOperador(":"):
        avanzar()
        leer tipo de retorno

    // Marcador "sobreescribir" opcional
    si esReservada("sobreescribir"):
        metodo.esSobreescritura = cierto
        avanzar()

    // Cuerpo (vacío si abstracto)
    si metodo.esAbstracto:
        consumirFinDeSentencia()
        metodo.cuerpo = []  // sin cuerpo
    sino:
        metodo.cuerpo = parseBloque({"fin"})
        esperarReservada("fin")

    retornar metodo
```

#### 3.5 `parseCampoDef()` — campo dentro de clase/estructura

```
parseCampoDef():
    esperar Identificador → nombre
    avanzar()
    esperarOperador(":")
    leer tipo (mapearNombreTipo o nombre de clase)
    avanzar()
    valorDefecto = nulo
    si esOperador("="):
        avanzar()
        valorDefecto = parseExpresion()
    consumirFinDeSentencia()
    retornar CampoDef{nombre, tipo, valorDefecto}
```

#### 3.6 `parseEstructura()` — similar a clase pero sin herencia

```
parseEstructura():
    l = línea actual
    avanzar()  // consume "estructura"
    esperar Identificador → nombre
    // No acepta "extiende" ni "implementa"
    saltarNuevasLineas()
    campos = []
    metodos = []
    // Mismo parseo de campos y métodos que en clase
    // (pero sin "abstracto", sin "protegido" — solo publico/privado)
    esperarReservada("fin")
    retornar EstructuraDef{nombre, campos, metodos}
```

#### 3.7 `parseInterfaz()` — solo firmas de métodos

```
parseInterfaz():
    l = línea actual
    avanzar()  // consume "interfaz"
    esperar Identificador → nombre
    saltarNuevasLineas()
    metodos = []
    mientras !esReservada("fin"):
        si esReservada("funcion") || esReservada("fun"):
            metodo = parseMetodoDef(nombre)
            metodo.esAbstracto = cierto  // forzar sin cuerpo
            metodo.acceso = Publico      // forzar público
            metodos.agregar(metodo)
        sino:
            error("las interfaces solo pueden contener firmas de métodos")
        saltarNuevasLineas()
    esperarReservada("fin")
    retornar InterfazDef{nombre, metodos}
```

#### 3.8 Parseo de `nuevo` en expresiones

En `parsePrimario()`:

```cpp
if (esReservada("nuevo")) {
    return parseNuevo();
}
```

```
parseNuevo():
    avanzar()  // consume "nuevo"
    esperar Identificador → nombreClase
    avanzar()
    esperarDelimitador("(")
    argumentos = []
    si !esDelimitador(")"):
        // parsear lista de argumentos (mismo patrón que parseLlamada)
    esperarDelimitador(")")
    retornar NuevoExpr{nombreClase, argumentos}
```

#### 3.9 Parseo de `este` en expresiones

En `parsePrimario()`:

```cpp
if (esReservada("este")) {
    avanzar();
    auto n = std::make_unique<AccesoEste>();
    n->linea = tk.line;
    return n;
}
```

`este` se comporta como un primario; después puede seguir `.campo` o `.metodo()`
gracias al parseo de postfijos existente (`parsePostfijo` maneja `.` y `[]`).

#### 3.10 Parseo de `base(args...)` como sentencia

En `parseSentencia()`, cuando se encuentre `base`:

```cpp
if (p == "base") {
    return parseLlamadaBase();
}
```

```
parseLlamadaBase():
    l = línea actual
    avanzar()  // consume "base"
    esperarDelimitador("(")
    argumentos = parsear argumentos
    esperarDelimitador(")")
    consumirFinDeSentencia()
    retornar LlamadaBase{argumentos}
```

#### 3.11 Parseo del operador `es`

El operador `es` tiene precedencia similar a la de los operadores relacionales.
Se parsea en `parseRelacional()`:

```cpp
ExprPtr Parser::parseRelacional() {
    ExprPtr e = parseConcatenacion();
    while (...) {
        // ... operadores existentes (<, >, <=, >=) ...
    }
    // Operador "es": expr es NombreClase
    if (esReservada("es")) {
        avanzar();
        if (actual.type != TokenType::Identificador)
            error("se esperaba un nombre de clase después de 'es'");
        std::string clase = actual.lexeme;
        avanzar();
        auto n = std::make_unique<EsExpr>();
        n->objeto = std::move(e);
        n->clase = clase;
        e = std::move(n);
    }
    return e;
}
```

---

### Fase 4 — Modificaciones al Analizador Semántico

#### 4.1 Tablas de tipos definidos por el usuario

```cpp
// En analizador_semantico.h, agregar:
struct InfoClase {
    std::string nombre;
    std::string padre;                    // "" si no hereda
    std::vector<std::string> interfaces;
    bool esAbstracta;
    std::vector<CampoDef> campos;         // incluye heredados
    std::vector<MetodoDef> metodos;       // incluye heredados
    int linea;
};

struct InfoInterfaz {
    std::string nombre;
    std::vector<MetodoDef> metodos;       // firmas requeridas
    int linea;
};

struct InfoEstructura {
    std::string nombre;
    std::vector<CampoDef> campos;
    std::vector<MetodoDef> metodos;
    int linea;
};

// Campos privados nuevos:
std::unordered_map<std::string, InfoClase> clases;
std::unordered_map<std::string, InfoInterfaz> interfacesDefinidas;
std::unordered_map<std::string, InfoEstructura> estructuras;
std::string claseActual;        // nombre de la clase que se está analizando ("" fuera)
bool enMetodo = false;          // true dentro de un método
bool enConstructor = false;     // true dentro de un constructor
bool enMetodoEstatico = false;  // true dentro de un método estático
```

#### 4.2 Primera pasada — recolectar tipos

Antes de analizar las sentencias, hacer una pasada para recolectar todas las
definiciones de tipo (similar a `recolectarFunciones`):

```cpp
void AnalizadorSemantico::recolectarTipos(Programa& programa) {
    for (auto& s : programa.sentencias) {
        if (auto* c = dynamic_cast<ClaseDef*>(s.get())) {
            if (clases.count(c->nombre) || estructuras.count(c->nombre))
                agregarError(c->linea, "tipo '" + c->nombre + "' ya definido");
            else
                clases[c->nombre] = InfoClase{...};
        }
        if (auto* e = dynamic_cast<EstructuraDef*>(s.get())) {
            // similar
        }
        if (auto* i = dynamic_cast<InterfazDef*>(s.get())) {
            // similar
        }
    }
}
```

#### 4.3 Verificaciones semánticas en `visitar(ClaseDef&)`

1. **Herencia válida**: Si `padre != ""`, verificar que `padre` esté definido como
   clase (no interfaz, no estructura). Verificar que no haya ciclos.
2. **Interfaces implementadas**: Verificar que cada nombre en `interfaces` corresponda
   a una `InterfazDef` registrada.
3. **Completitud de interfaces**: Para cada método de cada interfaz implementada,
   verificar que la clase tenga un método con la misma firma (nombre + número de
   parámetros). Reportar error si falta alguno.
4. **Clases abstractas**: Si la clase NO es abstracta, verificar que no tenga métodos
   abstractos sin implementar (heredados o propios).
5. **Constructor**: Si existe un constructor, verificar que su nombre coincida con el
   de la clase.
6. **`base()` solo en constructor**: Verificar que `LlamadaBase` solo aparezca como
   primera sentencia del constructor, y solo si la clase hereda.
7. **Duplicados**: No se pueden tener dos campos o dos métodos con el mismo nombre.
8. **`sobreescribir`**: Si un método tiene la marca, verificar que exista en la clase
   padre con la misma firma.
9. **Modificadores de acceso**: Validar que los campos/métodos privados de la clase
   padre no se acceden desde la subclase. Protegidos sí.

#### 4.4 Verificaciones en `visitar(EstructuraDef&)`

1. Las estructuras NO soportan herencia (ni `extiende` ni `implementa`).
2. Los campos solo pueden ser `publico` o `privado` (sin `protegido`).
3. No admite `abstracto`.

#### 4.5 Verificaciones en `visitar(InterfazDef&)`

1. Todos los métodos deben ser públicos y sin cuerpo.
2. No puede haber campos.
3. No puede haber constructor.

#### 4.6 Verificaciones en `visitar(NuevoExpr&)`

1. Verificar que el nombre de clase referenciado exista (en `clases` o `estructuras`).
2. No se puede instanciar una clase abstracta.
3. Verificar el número de argumentos contra el constructor.
4. No se puede instanciar una interfaz.

#### 4.7 Verificaciones en `visitar(AccesoEste&)`

1. Solo válido dentro de un método de instancia (no estático).
2. Fuera de un método: error "uso de 'este' fuera de un método".

#### 4.8 Verificaciones en `visitar(LlamadaBase&)`

1. Solo válido dentro de un constructor.
2. Solo si la clase actual tiene padre (`extiende`).
3. Verificar número de argumentos contra el constructor del padre.

#### 4.9 Verificaciones en `visitar(EsExpr&)`

1. Verificar que el nombre de clase referenciado exista.

#### 4.10 Control de acceso en `visitar(AccesoMiembro&)` (extender)

Cuando se accede a `objeto.campo`, el analizador debe verificar:
- Si `campo` es privado en la clase de `objeto`, solo se permite dentro de la misma clase.
- Si `campo` es protegido, solo dentro de la clase o subclases.
- Público: siempre permitido.

La verificación de acceso es mejor esfuerzo en compilación (requiere inferencia de tipo)
y se complementa con verificaciones en runtime.

---

### Fase 5 — Modificaciones al Runtime (`runtime/latino.h`, `runtime/latino.c`)

#### 5.1 Nuevo tipo `LAT_OBJETO`

```c
typedef enum {
    LAT_NULO,
    LAT_LOGICO,
    LAT_NUMERO,
    LAT_CADENA,
    LAT_LISTA,
    LAT_DICCIONARIO,
    LAT_MODULO,
    LAT_OBJETO          /* NUEVO */
} LatTipo;
```

#### 5.2 Estructura `LatObjeto`

```c
/* Puntero a método de instancia: recibe "este" + argumentos empaquetados */
typedef LatValor (*LatMetodoFn)(LatValor este, int nargs, LatValor* args);

/* Entrada en la vtable */
typedef struct {
    const char* nombre;
    LatMetodoFn fn;
} LatMetodoEntry;

/* Representación de un objeto en runtime */
struct LatObjeto {
    size_t refs;                /* conteo de referencias (Fase 19) */
    const char* clase;          /* nombre de la clase */
    const char* padre;          /* nombre de la clase padre (NULL si no hereda) */
    LatDic* campos;             /* diccionario de campos (nombre → LatValor) */
    LatMetodoEntry* vtable;     /* tabla de métodos */
    size_t num_metodos;
    size_t cap_metodos;
};
```

#### 5.3 Ampliar la unión de `LatValor`

```c
typedef struct {
    LatTipo tipo;
    union {
        int logico;
        double numero;
        char* cadena;
        LatLista* lista;
        LatDic* dic;
        LatModulo* modulo;
        LatObjeto* objeto;    /* NUEVO */
    } como;
} LatValor;
```

#### 5.4 API de objetos (`runtime/latino.h`)

```c
/* --- Objetos (POO) --- */

/* Crea un nuevo objeto vacío de la clase dada */
LatValor lat_obj_nuevo(const char* clase);

/* Lee un campo del objeto */
LatValor lat_obj_get(LatValor obj, const char* campo);

/* Escribe un campo del objeto */
void lat_obj_set(LatValor obj, const char* campo, LatValor valor);

/* Registra un método en la vtable del objeto */
void lat_obj_set_metodo(LatValor obj, const char* nombre, LatMetodoFn fn);

/* Llama un método del objeto por nombre */
LatValor lat_obj_llamar_metodo(LatValor obj, const char* nombre,
                                int nargs, ...);

/* Reclasifica un objeto (para herencia: cambia clase, preserva padre) */
void lat_obj_set_clase(LatValor obj, const char* nueva_clase,
                        const char* padre);

/* Verifica si un objeto es instancia de una clase (incluyendo ancestros) */
LatValor lat_obj_es_instancia(LatValor obj, const char* clase);

/* Verifica acceso a un campo (runtime) */
LatValor lat_obj_get_seguro(LatValor obj, const char* campo,
                             int acceso_requerido,
                             const char* clase_accedente, int linea);
```

#### 5.5 Implementación (`runtime/latino.c`)

**`lat_obj_nuevo`**: Alloca `LatObjeto` con `malloc`, inicializa el conteo de
referencias a 1, crea un `LatDic` vacío para campos, y una vtable vacía.

**`lat_obj_get` / `lat_obj_set`**: Delegan al diccionario de campos interno
usando `lat_obtener_indice` / `lat_asignar_indice` con clave de cadena.

**`lat_obj_set_metodo`**: Busca en la vtable si ya existe un método con ese nombre
(para sobreescritura); si existe, reemplaza el puntero de función. Si no, agrega
una nueva entrada (con crecimiento dinámico tipo `vector`).

**`lat_obj_llamar_metodo`**: Busca el método por nombre en la vtable, empaqueta los
argumentos variádicos en un array de `LatValor`, y lo invoca pasando `este` como
primer argumento.

**`lat_obj_set_clase`**: Cambia los campos `clase` y `padre` del objeto (usado por
constructores de subclases después de llamar al constructor padre).

**`lat_obj_es_instancia`**: Recorre la cadena de herencia (`clase` → `padre` → ...)
comparando con el nombre de clase dado. Retorna `lat_logico(1)` si coincide, `lat_logico(0)`
si no.

**Conteo de referencias**: Ampliar `lat_valor_retener` y `lat_valor_liberar` para
`LAT_OBJETO`: retener/liberar el diccionario de campos internamente, y liberar la
vtable cuando las refs llegan a 0.

#### 5.6 Ampliar funciones existentes

- **`lat_a_cadena`**: Para `LAT_OBJETO`, retornar `"<Objeto:NombreClase>"`.
- **`lat_igual`**: Para dos `LAT_OBJETO`, comparar por referencia (igualdad de puntero).
- **`lat_es_verdadero`**: Un `LAT_OBJETO` siempre es verdadero (como en C#, un objeto
  no nulo es truthy).
- **`_nombre_latipo`**: Agregar `case LAT_OBJETO: return "objeto";`.

---

### Fase 6 — Modificaciones al Generador de Código (`src/compiler.cpp`)

#### 6.1 Recolección de tipos

Antes de generar código, el generador necesita conocer todas las clases/structs/interfaces:

```cpp
struct InfoTipoPOO {
    std::string nombre;
    std::string padre;
    std::vector<std::string> interfaces;
    bool esAbstracta;
    std::vector<CampoDef> campos;
    std::vector<MetodoDef> metodos;
};

std::unordered_map<std::string, InfoTipoPOO> tiposPOO;

void GeneradorC::recolectarTipos(Programa& programa) {
    for (auto& s : programa.sentencias) {
        if (auto* c = dynamic_cast<ClaseDef*>(s.get()))
            tiposPOO[c->nombre] = {...};
        if (auto* e = dynamic_cast<EstructuraDef*>(s.get()))
            tiposPOO[e->nombre] = {...};
        // Las interfaces no generan código directo
    }
}
```

#### 6.2 Generación de una clase

Para cada `ClaseDef` o `EstructuraDef`, el generador produce:

1. **Prototipos de métodos** (forward declarations `static LatValor`)
2. **Funciones de métodos**: Cada método se genera como una función C estática con
   firma `static LatValor lat_met_<Clase>_<metodo>(LatValor v_este, ...)`.
   Dentro del método, `este` se traduce a `v_este`. Los accesos `este.campo` se
   traducen a `lat_obj_get(v_este, "campo")`. Las asignaciones `este.campo = valor`
   se traducen a `lat_obj_set(v_este, "campo", valor)`.
3. **Constructor**: Una función `lat_ctor_<Clase>` que:
   - Si hereda: llama a `lat_ctor_<Padre>(args...)` para obtener `v_este`.
   - Si no hereda: llama a `lat_obj_nuevo("<Clase>")`.
   - Reclasifica con `lat_obj_set_clase(v_este, "<Clase>", "<Padre>")` si hereda.
   - Inicializa los campos propios con `lat_obj_set(v_este, ...)`.
   - Registra los métodos de la clase en la vtable con `lat_obj_set_metodo(...)`.
   - Ejecuta el cuerpo del constructor.
   - Retorna `v_este`.

#### 6.3 Generación de `nuevo`

`nuevo Persona("Juan", 30)` → `lat_ctor_Persona(lat_cadena("Juan"), lat_numero(30))`

#### 6.4 Generación de `este`

`este` → `v_este` (variable implícita, primer parámetro del método).

#### 6.5 Generación de `este.campo` y `este.campo = valor`

Lectura: `este.campo` → `lat_obj_get(v_este, "campo")`
Escritura: `este.campo = valor` → `lat_obj_set(v_este, "campo", valor);`

#### 6.6 Generación de llamada a método

`p.saludar()` → `lat_obj_llamar_metodo(v_p, "saludar", 0)`
`p.calcular(1, 2)` → `lat_obj_llamar_metodo(v_p, "calcular", 2, lat_numero(1), lat_numero(2))`

#### 6.7 Generación de `base(args...)`

`base(nombre, edad)` dentro del constructor de `Empleado` →
ya se maneja por el esquema del constructor que llama a `lat_ctor_<Padre>`.

#### 6.8 Generación de `es`

`p es Persona` → `lat_obj_es_instancia(v_p, "Persona")`

#### 6.9 Generación de métodos estáticos

Un método estático no recibe `v_este`. Se genera como una función C normal:
`lat_met_estatico_<Clase>_<metodo>(...params...)`

La llamada `Animal.crear("Luna")` se traduce a:
`lat_met_estatico_Animal_crear(lat_cadena("Luna"))`

El acceso `Clase.metodo()` (AccesoMiembro sobre un Identificador que es nombre de clase)
se detecta en `genLlamada` verificando si el objeto es un nombre de clase registrado.

---

### Fase 7 — Actualizar el Impresor de AST (`src/ast_impresor.cpp`)

Implementar los visitantes para los nuevos nodos:

- `visitar(ClaseDef&)`: Imprimir nombre, padre, interfaces, campos y métodos.
- `visitar(EstructuraDef&)`: Similar sin herencia.
- `visitar(InterfazDef&)`: Solo firmas de métodos.
- `visitar(NuevoExpr&)`: Imprimir `nuevo <Clase>(args...)`.
- `visitar(EsExpr&)`: Imprimir `<expr> es <Clase>`.
- `visitar(AccesoEste&)`: Imprimir `este`.
- `visitar(LlamadaBase&)`: Imprimir `base(args...)`.

---

### Fase 8 — Tests (`tests/test_poo.cpp`, `tests/CMakeLists.txt`)

#### 8.1 Tests de parser

```
"clase Animal\n fin"
    → AST contiene ClaseDef{nombre: "Animal", padre: "", campos: [], metodos: []}

"clase Perro extiende Animal\n fin"
    → ClaseDef{padre: "Animal"}

"clase P implementa I1, I2\n fin"
    → ClaseDef{interfaces: ["I1", "I2"]}

"interfaz IComparable\n funcion comparar(otro: numero): numero\n fin"
    → InterfazDef con 1 método

"estructura Punto\n x: numero\n y: numero\n fin"
    → EstructuraDef con 2 campos

"abstracto clase Figura\n abstracto funcion area(): numero\n fin"
    → ClaseDef{esAbstracta: true} con método abstracto
```

#### 8.2 Tests semánticos

```
# Errores detectados:
"clase Perro extiende NoExiste\n fin"
    → error "extiende 'NoExiste' que no está definida"

"clase P implementa NoExiste\n fin"
    → error "interfaz 'NoExiste' no está definida"

"interfaz I\n funcion f(): numero\n fin\n clase C implementa I\n fin"
    → error "no implementa el método 'f' requerido por 'I'"

"abstracto clase A\n abstracto funcion f(): numero\n fin\n x = nuevo A()"
    → error "no se puede instanciar la clase abstracta 'A'"

"clase A\n privado x: numero\n fin\n a = nuevo A()\n escribir(a.x)"
    → error "campo privado 'x' no accesible fuera de 'A'"

# Sin error:
"clase A\n publico x: numero\n funcion A()\n este.x = 42\n fin\n fin"
    → OK

"este.x = 1"  (fuera de un método)
    → error "uso de 'este' fuera de un método"
```

#### 8.3 Tests de generación de código

```
"clase P\n publico n: cadena\n funcion P(n: cadena)\n este.n = n\n fin\n fin\n p = nuevo P(\"Juan\")"
    → C contiene "lat_ctor_P" y "lat_obj_nuevo" y "lat_obj_set"

"p es Persona"
    → C contiene "lat_obj_es_instancia(v_p, \"Persona\")"
```

#### 8.4 Tests E2E

```
# Clase simple con constructor y método
clase Saludo
    publico nombre: cadena
    funcion Saludo(nombre: cadena)
        este.nombre = nombre
    fin
    publico funcion decir(): cadena
        retornar "Hola, " .. este.nombre
    fin
fin
s = nuevo Saludo("Mundo")
escribir(s.decir())
# salida: Hola, Mundo

# Herencia y polimorfismo
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
    funcion Perro(nombre: cadena)
        base(nombre)
    fin
    publico funcion hablar(): cadena sobreescribir
        retornar este.nombre .. " dice: ¡Guau!"
    fin
fin
p = nuevo Perro("Rex")
escribir(p.hablar())
# salida: Rex dice: ¡Guau!
si p es Animal
    escribir("Es un animal")
fin
# salida: Es un animal
```

#### 8.5 Registro en `tests/CMakeLists.txt`

```cmake
# Pruebas de POO
add_executable(test_poo
  test_poo.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
  ${CMAKE_SOURCE_DIR}/src/analizador_semantico.cpp
  ${CMAKE_SOURCE_DIR}/src/compiler.cpp
)
target_include_directories(test_poo PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_poo COMMAND test_poo)
add_suite20(test_poo_e2e)
```

---

## Retos técnicos detallados

### Reto 1: Representación de objetos en C

**Problema**: C no tiene objetos. Cada instancia de clase necesita almacenar campos
dinámicos, una vtable de métodos, y metadatos de tipo para herencia y `es`.

**Solución**: `LatObjeto` usa un `LatDic` interno para campos (reutilizando la
infraestructura existente de diccionarios) y un array dinámico de `LatMetodoEntry`
para la vtable. Esto permite despacho dinámico de métodos sin conocer la clase en
tiempo de compilación.

**Trade-off**: El despacho por nombre (búsqueda lineal en la vtable) es O(n) donde n
es el número de métodos. Para clases con muchos métodos, se puede optimizar a hash table
en el futuro. Para la primera versión, la búsqueda lineal es suficiente.

### Reto 2: Herencia y constructores

**Problema**: Al crear un `Perro`, se debe ejecutar primero el constructor de `Animal`,
inicializar los campos del padre, y luego los del hijo. La vtable debe contener los
métodos del padre con los sobreescritos por el hijo.

**Solución**: El constructor del hijo llama al del padre, obtiene el objeto base
`v_este`, reclasifica con `lat_obj_set_clase`, y sobreescribe los métodos en la vtable.
Esto funciona porque `lat_obj_set_metodo` reemplaza entradas existentes por nombre.

### Reto 3: Despacho dinámico de métodos (polimorfismo)

**Problema**: `animal.hablar()` debe llamar al método correcto según la clase real del
objeto, no la clase declarada.

**Solución**: El despacho siempre usa la vtable del objeto, que ya contiene los métodos
sobreescritos. `lat_obj_llamar_metodo` busca por nombre en la vtable del objeto, que
ya refleja la clase real (por la cadena de constructores que sobreescribieron métodos).

### Reto 4: `este` como variable implícita

**Problema**: Dentro de un método, `este` debe referirse a la instancia actual. No es
una variable declarada por el usuario.

**Solución**: El generador agrega `LatValor v_este` como primer parámetro de cada método.
El parser reconoce `este` como `AccesoEste`, y el generador lo traduce a `v_este`. El
acceso `este.campo` se traduce a `lat_obj_get(v_este, "campo")`.

### Reto 5: Verificación de interfaces

**Problema**: Si `Producto implementa IComparable`, se debe verificar que `Producto`
tenga todos los métodos de `IComparable`.

**Solución**: El analizador semántico compara las firmas de métodos de la interfaz con
los métodos de la clase. La comparación es por nombre y número de parámetros (duck typing
estructural). Los tipos de parámetros se verifican cuando hay anotaciones.

### Reto 6: Control de acceso entre clases

**Problema**: Los campos privados no deben ser accesibles fuera de la clase. Los protegidos
solo dentro de la clase y subclases.

**Solución en compilación**: El analizador semántico rastrea la `claseActual` y puede
verificar accesos a miembros cuando el tipo del objeto es conocido (literal o declaración
con tipo).

**Solución en runtime**: `lat_obj_get_seguro` y `lat_obj_set_seguro` reciben el nombre
de la clase accedente y el nivel de acceso requerido, y lanzan error si no se permite.
La implementación inicial puede omitir la verificación en runtime y hacerla solo en
compilación (mejor esfuerzo).

### Reto 7: Métodos estáticos

**Problema**: `Animal.crear("Luna")` es una llamada a un método estático. El parser
debe distinguir entre `variable.metodo()` y `Clase.metodoEstatico()`.

**Solución**: En `genLlamada`, cuando el destino es `AccesoMiembro` y el objeto es un
`Identificador` cuyo nombre está en `tiposPOO`, se genera una llamada directa a
`lat_met_estatico_<Clase>_<metodo>(args...)` sin pasar `v_este`.

### Reto 8: Conteo de referencias para objetos

**Problema**: Los objetos contienen un `LatDic` interno para campos, que a su vez
contiene `LatValor`. Al liberar un objeto, se deben liberar recursivamente todos los
campos.

**Solución**: `lat_valor_liberar` para `LAT_OBJETO` libera el diccionario de campos
(que a su vez libera sus valores), libera la vtable, y libera el `LatObjeto`. El conteo
de referencias del objeto sigue el mismo patrón que listas y diccionarios.

---

## Orden de implementación recomendado

El orden garantiza que cada paso compila y los tests existentes siguen pasando:

### Paso 1 — Lexer (sin impacto en código existente)
- Agregar las 15 palabras reservadas.
- Compilar y verificar que los tests existentes pasan (ningún ejemplo usa estas palabras
  como variables).

### Paso 2 — AST (nuevos nodos con visitantes por defecto)
- Agregar `TipoAnotado::Objeto`.
- Agregar enums, structs auxiliares, y nodos.
- Agregar visitantes en `Visitante` con cuerpo vacío por defecto `{}`.
- Compilar y verificar que todo sigue compilando.

### Paso 3 — Runtime (API de objetos)
- Agregar `LAT_OBJETO` al enum `LatTipo`.
- Implementar `LatObjeto` y todas las funciones `lat_obj_*`.
- Ampliar `lat_valor_retener`, `lat_valor_liberar`, `lat_a_cadena`, `lat_es_verdadero`.
- Compilar el runtime y verificar.

### Paso 4 — Parser (parseo de nuevas construcciones)
- Implementar `parseClase()`, `parseEstructura()`, `parseInterfaz()`.
- Implementar `parseNuevo()`, `este`, `base()`, `es`.
- Agregar despacho en `parseSentencia()` y `parsePrimario()`.
- Compilar y verificar que el código sin POO sigue parseando correctamente.

### Paso 5 — Impresor de AST
- Implementar los visitantes de los nuevos nodos.
- Verificar con `--ast`.

### Paso 6 — Analizador semántico
- Implementar `recolectarTipos()`.
- Implementar todos los visitantes de POO con verificaciones.
- Compilar y verificar que los tests semánticos existentes pasan.

### Paso 7 — Generador de código
- Implementar `recolectarTipos()` en el generador.
- Implementar generación de clases, constructores, métodos.
- Implementar generación de `nuevo`, `este`, `base`, `es`.
- Compilar y verificar tests de codegen existentes.

### Paso 8 — Tests
- Crear `tests/test_poo.cpp` con todas las secciones.
- Registrar en `tests/CMakeLists.txt`.
- Ejecutar todos los tests.

### Paso 9 — Ejemplos
- Crear `ejemplos/clases.lat`, `ejemplos/herencia.lat`, `ejemplos/interfaces.lat`,
  `ejemplos/estructuras.lat` con programas demostrativos.

---

## Garantía de retrocompatibilidad

- Todos los nodos existentes del AST no cambian.
- Los nuevos nodos solo se crean cuando el parser encuentra las nuevas palabras reservadas.
- `LatValor` agrega un nuevo miembro a la unión (`objeto`), pero la unión sigue siendo
  una unión — no afecta el tamaño ni la alineación de los demás miembros.
- El nuevo `LAT_OBJETO` en el enum `LatTipo` no colisiona con los valores existentes.
- Todos los switches sobre `LatTipo` que no manejen `LAT_OBJETO` llegarán al `default`,
  que ya existe en todos los casos.
- Las únicas palabras que se "rompen" son las 15 nuevas reservadas. Si algún programa
  existente usa `clase`, `nuevo`, `este`, etc. como nombre de variable, dejará de compilar.
  Esto es aceptable porque son palabras con semántica clara de POO.

---

## Archivos críticos para la implementación

- `include/ast.h` — nuevos nodos y enums
- `src/lexer.cpp` — palabras reservadas
- `src/parser.cpp` — parseo de clases, estructuras, interfaces
- `src/analizador_semantico.cpp` — verificaciones de tipos definidos
- `src/compiler.cpp` — generación de código para POO
- `runtime/latino.h` — `LAT_OBJETO`, `LatObjeto`, API `lat_obj_*`
- `runtime/latino.c` — implementación del runtime de objetos
- `src/ast_impresor.cpp` — volcado de nuevos nodos
