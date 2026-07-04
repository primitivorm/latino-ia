# Plan de trabajo — Tipado gradual opcional en Latino (Fase 27)

## Contexto

Latino es un lenguaje de **tipado dinámico**: todas las variables se compilan a
`LatValor` (unión tagueada en C). Esta fase agrega **anotaciones de tipo opcionales**
con verificación en dos capas:

1. **Compilación**: el analizador semántico detecta errores evidentes cuando el valor
   es un literal (`n: numero = "hola"` → error estático).
2. **Runtime**: el generador emite llamadas a `lat_verificar_tipo()` para verificar
   tipos en casos dinámicos (ej. resultado de `leer()`).

El código sin anotaciones sigue funcionando exactamente igual — retrocompatibilidad total.

## Sintaxis

```latino
# Variable con tipo anotado
n: numero = 42
s: cadena = "hola"
b: logico = cierto
lst: lista = [1, 2, 3]
d: dic = {"clave": "valor"}

# Función con tipos en parámetros y retorno
funcion suma(a: numero, b: numero): numero
    retornar a + b
fin

# Sin anotación — sigue funcionando
x = 42
```

### Tipos soportados

| Anotación | `LatTipo` en runtime |
|-----------|----------------------|
| `numero`  | `LAT_NUMERO`         |
| `cadena`  | `LAT_CADENA`         |
| `logico`  | `LAT_LOGICO`         |
| `lista`   | `LAT_LISTA`          |
| `dic`     | `LAT_DICCIONARIO`    |
| `nulo`    | `LAT_NULO`           |

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `include/ast.h` | `TipoAnotado` enum, `ParamFuncion` struct, `tiposDestino` en `Asignacion`, `parametros` y `tipoRetorno` en `FuncionDef` |
| `src/parser.cpp` / `include/parser.h` | Buffer de token, `mapearNombreTipo()`, parseo de `: tipo` en asignaciones y funciones |
| `src/analizador_semantico.cpp` / `.h` | `ambitos` con tipo, verificación estática de literales |
| `runtime/latino.c` / `runtime/latino.h` | `lat_verificar_tipo()` |
| `src/compiler.cpp` | `tipoALatTipo()`, `lat_verificar_tipo()` en asignaciones y parámetros anotados |
| `src/ast_impresor.cpp` | Muestra tipos en el volcado `--ast` |
| `tests/test_tipado.cpp` (nuevo) | Suite completa: parser + semántico + codegen |
| `tests/CMakeLists.txt` | Target `test_tipado` |

## Código generado de ejemplo

```latino
n: numero = leer()
```

Genera en C:

```c
v_n = lat_verificar_tipo(lat_leer(), LAT_NUMERO, "n", 1);
```

```latino
funcion suma(a: numero, b: numero): numero
    retornar a + b
fin
```

Genera en C:

```c
static LatValor lat_fn_suma(LatValor v_a, LatValor v_b) {
    lat_verificar_tipo(v_a, LAT_NUMERO, "a", 1);
    lat_verificar_tipo(v_b, LAT_NUMERO, "b", 1);
    return lat_sumar(v_a, v_b);
    return lat_nulo();
}
```

## Mensaje de error en runtime

```
Error de tipo en línea 3: la variable 'n' se declaró como 'numero' pero recibió un valor de tipo 'cadena'
```

## Estado

Fase 1 — Modificaciones al AST (include/ast.h)
1.1 Agregar un enum de tipos anotados
Añadir, justo antes de las declaraciones adelantadas de nodos, el siguiente enum:


enum class TipoAnotado {
    Ninguno,        // sin anotación
    Numero,
    Cadena,
    Logico,
    Lista,
    Dic,
    Nulo
};
Este enum vive en el AST (no en el runtime) porque es información de compilación. La conversión a LatTipo se hace en el generador.

1.2 Ampliar Asignacion
El nodo Asignacion almacena las anotaciones de tipo para cada destino (paralelo a destinos):


struct Asignacion : Sentencia {
    std::vector<ExprPtr> destinos;
    std::vector<ExprPtr> valores;
    std::vector<TipoAnotado> tiposDestino;  // NUEVO — mismo tamaño que destinos
    LATINO_ACEPTAR
};
tiposDestino[i] corresponde a destinos[i]. El valor TipoAnotado::Ninguno significa "sin anotación" y preserva retrocompatibilidad.

1.3 Ampliar FuncionDef
Los parámetros deben conservar tanto nombre como tipo opcional:


struct ParamFuncion {
    std::string nombre;
    TipoAnotado tipo = TipoAnotado::Ninguno;
};

struct FuncionDef : Sentencia {
    std::string nombre;
    std::vector<ParamFuncion> parametros;   // CAMBIO: de vector<string> a vector<ParamFuncion>
    TipoAnotado tipoRetorno = TipoAnotado::Ninguno;  // NUEVO
    bool variadico = false;
    ListaSent cuerpo;
    LATINO_ACEPTAR
};
Este es el cambio más impactante: todos los visitantes que acceden a f->parametros[i] (cadena pura) necesitan actualizarse a f->parametros[i].nombre.

Fase 2 — Modificaciones al Lexer (src/lexer.cpp)
No se modifica lexer.h ni se agrega ningún token nuevo. El token : ya existe como TokenType::Operador con lexema ":". El lexer es correcto tal como está.

El único cambio en el lexer es registrar los nombres de tipo como palabras reservadas o dejarlos como identificadores — se recomienda dejarlos como identificadores para no romper programas que usen numero, cadena, etc. como nombres de variable (ya que actualmente no son palabras reservadas). El parser los reconoce por lexema en contexto, no por tipo de token.

Fase 3 — Modificaciones al Parser (src/parser.cpp y include/parser.h)
Este es el cambio más complejo. El reto central es la ambigüedad del :.

3.1 El problema del : — análisis completo
El token : aparece en tres contextos actuales:

Ternario: condicion ? siCierto : siFalso — dentro de parseTernario()
caso valor: en parseElegir() — llamado con esperarOperador(":")
Clave de diccionario: {"a": 1} — llamado con esperarOperador(":")
El nuevo contexto de anotación de tipo es a nivel de sentencia, antes del = de asignación. La forma de distinguirlos es contextual:

Anotación de variable: identificador : tipo_nombre = valor — ocurre solo cuando el parser está en parseAsignacionOExpr() y acaba de leer un único identificador seguido de : seguido de un identificador de tipo válido seguido de =.
Ternario: expr ? expr : expr — solo aparece después de ?, así que el : siempre tiene ? antes. No hay ambigüedad aquí.
Anotación de parámetro: param : tipo_nombre — solo dentro de parseFuncion().
La clave es que la anotación de tipo solo puede aparecer en posiciones donde el = de asignación sigue al bloque identificador : tipo. En ningún otro contexto aparece la secuencia Identificador Operador(":")  Identificador Operador("=") a nivel de sentencia.

3.2 Función auxiliar privada parseTipoAnotado()
Agregar al parser una función que intenta leer una anotación de tipo. Se llama solo cuando el parser ya consumió un identificador y ve : como siguiente token. La función hace lookahead de un token:


parseTipoAnotado():
    si actual == Operador(":") Y peekChar_siguiente_es_tipo_valido():
        avanzar()  // consume el ":"
        leer nombre_tipo (un Identificador)
        avanzar()
        retornar mapearTipo(nombre_tipo)
    sino:
        retornar TipoAnotado::Ninguno
La verificación peekChar_siguiente_es_tipo_valido() no es necesaria porque el parser tiene lookahead de un token. El enfoque correcto: el : en el contexto de asignación solo se intenta consumir como anotación cuando se está en parseAsignacionOExpr() y se tiene exactamente un destino simple (un Identificador), seguido de :, seguido de un identificador de nombre de tipo conocido.

Mapeo de nombres de tipo en el parser:


static TipoAnotado mapearNombreTipo(const std::string& nombre) {
    if (nombre == "numero")  return TipoAnotado::Numero;
    if (nombre == "cadena")  return TipoAnotado::Cadena;
    if (nombre == "logico")  return TipoAnotado::Logico;
    if (nombre == "lista")   return TipoAnotado::Lista;
    if (nombre == "dic")     return TipoAnotado::Dic;
    if (nombre == "nulo")    return TipoAnotado::Nulo;
    return TipoAnotado::Ninguno;
}
Si el nombre no es un tipo válido, TipoAnotado::Ninguno y el : no se consume — el parser lo devuelve al flujo para que el ternario u otro contexto lo use.

3.3 Reescribir parseAsignacionOExpr()
El flujo actual parsea una lista de expresiones y luego chequea =. El nuevo flujo:


parseAsignacionOExpr():
    l = línea actual
    
    // Intento de anotación de tipo en variable simple
    si actual es Identificador:
        guardar nombre = actual.lexeme, linea
        avanzar()
        si actual == Operador(":"):
            tipo = intentarLeerTipoAnotado()
            // tipo != Ninguno solo si lo que sigue es un nombre de tipo válido
            si tipo != Ninguno:
                esperarOperador("=")
                valores = parseListaExpresiones()
                crear Asignacion con [Identificador(nombre)], valores, [tipo]
                retornar
            sino:
                // No era anotación. Retroceder: crear Identificador y continuar normalmente
                // El ":" sigue en la corriente — puede ser ternario
                izqs = [Identificador(nombre)] luego continuar con parseTernario parcial
        
        // Sin ":", parsear el resto de la expresión que comienza con el identificador
    
    // Ruta normal
    izqs = parseListaExpresiones()
    si esOperador("="):
        avanzar()
        ders = parseListaExpresiones()
        crear Asignacion con tiposDestino = [Ninguno] * n
        retornar
    retornar ExprSentencia
El reto del retroceso cuando : no va seguido de un tipo válido se resuelve con un diseño más simple: mirar hacia delante dos tokens. El parser ya mantiene actual como un token de lookahead. Para el caso identificador : ?, cuando se ve :, se llama a lexer.getNextToken() para previsualizar el token siguiente sin consumirlo. Si ese siguiente token es un Identificador con nombre de tipo válido, se consume el : y el nombre de tipo como anotación. Si no, se deja el : sin consumir (se añade un buffer de un token de "devuelto" al lexer, o alternativamente se reorganiza el parser para que en ese punto el : solo pueda ser parte de un ternario, lo cual es siempre cierto porque el ternario requiere ? antes).

Solución más limpia y sin complicar el lexer: El : en el ternario solo aparece cuando antes hubo un ?. Por tanto, en parseAsignacionOExpr(), cuando se detecta la secuencia Identificador ":" a nivel de sentencia (no dentro de una subexpresión), siempre se puede intentar leer el tipo. Si el token después del : es uno de los seis identificadores de tipo válidos, es una anotación. Si es cualquier otra cosa, es un error de sintaxis que el parser puede reportar claramente ("se esperaba un tipo válido después de ':'"). Esto es válido porque un : a nivel de sentencia fuera del ternario (que requiere ?) y fuera de elegir/caso (que tiene su propio contexto) no tiene otro significado.

La implementación concreta en parseAsignacionOExpr():


// Nuevo bloque antes de parseListaExpresiones():
if (actual.type == TokenType::Identificador) {
    Token tk = actual;
    avanzar();
    if (esOperador(":")) {
        // Lookahead: ¿es una anotación de tipo?
        avanzar(); // consume ":"
        TipoAnotado tipo = TipoAnotado::Ninguno;
        if (actual.type == TokenType::Identificador) {
            tipo = mapearNombreTipo(actual.lexeme);
        }
        if (tipo != TipoAnotado::Ninguno) {
            avanzar(); // consume el nombre del tipo
            esperarOperador("=");
            auto valores = parseListaExpresiones();
            auto id = make_unique<Identificador>();
            id->nombre = tk.lexeme; id->linea = tk.line;
            auto a = make_unique<Asignacion>();
            a->linea = l;
            a->destinos.push_back(move(id));
            a->valores = move(valores);
            a->tiposDestino.push_back(tipo);
            return a;
        }
        // No es tipo válido: error de sintaxis descriptivo
        error("se esperaba un tipo válido después de ':' (numero, cadena, logico, lista, dic, nulo)");
    }
    // Sin ":" — el identificador ya fue consumido, continuar parseando
    // como si el identificador fuera el inicio de una expresión
    // [reconstruir la expresión usando el token ya consumido]
    ...
}
El "reconstruir la expresión" después de consumir el identificador se maneja creando el Identificador AST y pasándolo como inicio de parseTernario (actualmente el parser no soporta una "expresión prefijada"). La solución más limpia es agregar una función parseTernarioDesde(ExprPtr base) que reanuda el parseo desde una expresión ya construida, o alternativamente — y más simple — usar un campo tokenDevuelto en el parser (un buffer de un token):


// En parser.h, agregar campo privado:
bool tieneTokenDevuelto = false;
Token tokenDevuelto;

// devolver un token al stream:
void devolverToken(Token t) { tokenDevuelto = t; tieneTokenDevuelto = true; }

// modificar avanzar() para consumir tokenDevuelto primero:
void Parser::avanzar() {
    if (tieneTokenDevuelto) {
        actual = tokenDevuelto;
        tieneTokenDevuelto = false;
    } else {
        actual = lexer.getNextToken();
    }
}
Con esto, si : no va seguido de un tipo válido, se devuelve el : al stream y se deja que el flujo normal lo maneje como parte de una expresión.

Sin embargo, dado que en la práctica un : a nivel de sentencia fuera de elegir/ternario es un error de todas formas en el lenguaje actual, se puede optar por el enfoque más directo: si hay : después de un identificador a nivel de sentencia y lo que sigue no es un tipo válido, reportar error descriptivo. Esto es correcto y más simple.

3.4 Reescribir parseFuncion()
En el bloque de parámetros, después de leer el nombre del parámetro, intentar : + tipo:


// Dentro del bucle de parámetros:
if (actual.type != TokenType::Identificador)
    error("se esperaba el nombre de un parámetro");
ParamFuncion param;
param.nombre = actual.lexeme;
avanzar();
if (esOperador(":")) {
    avanzar(); // consume ":"
    if (actual.type != TokenType::Identificador)
        error("se esperaba un tipo válido después de ':'");
    param.tipo = mapearNombreTipo(actual.lexeme);
    if (param.tipo == TipoAnotado::Ninguno)
        error("tipo no reconocido '" + actual.lexeme + "'");
    avanzar();
}
nodo->parametros.push_back(param);
Para el tipo de retorno de la función, después del ) de parámetros:


esperarDelimitador(")");
if (esOperador(":")) {
    avanzar(); // consume ":"
    if (actual.type != TokenType::Identificador)
        error("se esperaba un tipo de retorno válido");
    nodo->tipoRetorno = mapearNombreTipo(actual.lexeme);
    if (nodo->tipoRetorno == TipoAnotado::Ninguno)
        error("tipo de retorno no reconocido '" + actual.lexeme + "'");
    avanzar();
}
nodo->cuerpo = parseBloque({"fin"});
3.5 Actualizar parser.h
Agregar las declaraciones privadas:

static TipoAnotado mapearNombreTipo(const std::string&);
bool tieneTokenDevuelto = false; Token tokenDevuelto; (si se usa buffer de devolución)
Fase 4 — Modificaciones al Analizador Semántico (include/analizador_semantico.h, src/analizador_semantico.cpp)
4.1 Tabla de tipos de variables
El analizador actualmente almacena en ambitos solo unordered_set<string>. Necesita almacenar también el tipo anotado para hacer verificación estática:


// En analizador_semantico.h, reemplazar ambitos por:
std::vector<std::unordered_map<std::string, TipoAnotado>> ambitos;
Cambiar declararVariable() para aceptar el tipo:


void declararVariable(const std::string& nombre, TipoAnotado tipo, int linea);
Cambiar estaDeclarada() para devolver también el tipo:


TipoAnotado tipoDe(const std::string& nombre) const; // Ninguno si no declarada o sin anotación
4.2 Verificación estática en visitar(Asignacion&)
Agregar verificación de tipo cuando el valor es un literal conocido:


void AnalizadorSemantico::visitar(Asignacion& n) {
    for (auto& v : n.valores)
        if (v) v->aceptar(*this);

    for (size_t i = 0; i < n.destinos.size(); i++) {
        TipoAnotado tipoAnotado = (i < n.tiposDestino.size()) ? n.tiposDestino[i] : TipoAnotado::Ninguno;
        
        if (auto* id = dynamic_cast<Identificador*>(n.destinos[i].get())) {
            // Verificación estática: literal vs tipo anotado
            if (tipoAnotado != TipoAnotado::Ninguno && i < n.valores.size()) {
                verificarCompatibilidadLiteral(n.valores[i].get(), tipoAnotado, id->linea);
            }
            declararVariable(id->nombre, tipoAnotado, id->linea);
        } else if (n.destinos[i]) {
            n.destinos[i]->aceptar(*this);
        }
    }
}
La función verificarCompatibilidadLiteral() determina el tipo de un literal y compara con la anotación:


void AnalizadorSemantico::verificarCompatibilidadLiteral(
        Expresion* expr, TipoAnotado esperado, int linea) {
    TipoAnotado real = TipoAnotado::Ninguno;
    if (dynamic_cast<LitNumero*>(expr))        real = TipoAnotado::Numero;
    else if (dynamic_cast<LitCadena*>(expr))   real = TipoAnotado::Cadena;
    else if (dynamic_cast<LitLogico*>(expr))   real = TipoAnotado::Logico;
    else if (dynamic_cast<LitNulo*>(expr))     real = TipoAnotado::Nulo;
    else if (dynamic_cast<ListaLiteral*>(expr)) real = TipoAnotado::Lista;
    else if (dynamic_cast<DiccionarioLiteral*>(expr)) real = TipoAnotado::Dic;
    else return; // expresión dinámica: no verificar estáticamente

    if (real != esperado) {
        agregarError(linea, "tipo incompatible: se declaró '" +
                     nombreTipo(esperado) + "' pero el valor es '" +
                     nombreTipo(real) + "'");
    }
}
Esta función retorna sin error para cualquier expresión no-literal (llamadas a función, operaciones binarias, identificadores, etc.) — esos casos se delegan al chequeo en runtime.

4.3 Verificación en visitar(FuncionDef&)
Actualizar para usar ParamFuncion en lugar de string:


for (const ParamFuncion& p : n.parametros) {
    if (!vistos.insert(p.nombre).second)
        agregarError(n.linea, "parámetro duplicado '" + p.nombre + "'");
    declararVariable(p.nombre, p.tipo, n.linea);
}
4.4 Actualizar recolectarFunciones() e InfoFuncion
InfoFuncion almacena el número de parámetros. Actualizar para acceder a .nombre:


funciones[f->nombre] = InfoFuncion{f->parametros.size(), f->variadico, f->linea};
Esto ya funciona porque vector<ParamFuncion>.size() sigue dando el conteo correcto.

4.5 Función auxiliar nombreTipo()

static std::string nombreTipo(TipoAnotado t) {
    switch (t) {
        case TipoAnotado::Numero: return "numero";
        case TipoAnotado::Cadena: return "cadena";
        case TipoAnotado::Logico: return "logico";
        case TipoAnotado::Lista:  return "lista";
        case TipoAnotado::Dic:    return "dic";
        case TipoAnotado::Nulo:   return "nulo";
        default: return "desconocido";
    }
}
Fase 5 — Modificaciones al Runtime (runtime/latino.h, runtime/latino.c)
5.1 Agregar lat_verificar_tipo() en latino.h

/* --- Tipado gradual (Fase 30) --- */
/* Verifica que v.tipo == tipo_esperado. Si no, imprime mensaje de error y
 * termina el proceso con exit(1).
 * tipo_esperado es un valor LatTipo (LAT_NUMERO, LAT_CADENA, etc.).
 * nombre_var es el nombre de la variable (para el mensaje de error).
 * linea es el número de línea en el fuente .lat (para el mensaje de error).
 * Devuelve v sin modificarlo si la verificación pasa.
 */
LatValor lat_verificar_tipo(LatValor v, int tipo_esperado,
                             const char* nombre_var, int linea);
5.2 Implementar lat_verificar_tipo() en latino.c

static const char* nombre_tipo_c(LatTipo t) {
    switch (t) {
        case LAT_NULO:        return "nulo";
        case LAT_LOGICO:      return "logico";
        case LAT_NUMERO:      return "numero";
        case LAT_CADENA:      return "cadena";
        case LAT_LISTA:       return "lista";
        case LAT_DICCIONARIO: return "dic";
        case LAT_MODULO:      return "modulo";
        default:              return "desconocido";
    }
}

LatValor lat_verificar_tipo(LatValor v, int tipo_esperado,
                             const char* nombre_var, int linea) {
    if (v.tipo != (LatTipo)tipo_esperado) {
        fprintf(stderr,
                "Error de tipo en línea %d: la variable '%s' se declaró como '%s' "
                "pero recibió un valor de tipo '%s'\n",
                linea, nombre_var,
                nombre_tipo_c((LatTipo)tipo_esperado),
                nombre_tipo_c(v.tipo));
        exit(1);
    }
    return v;
}
El uso de int en lugar de LatTipo para tipo_esperado en la firma de C evita incluir el enum en contextos que solo necesitan la constante entera; en la implementación se castea de vuelta a LatTipo.

Fase 6 — Modificaciones al Generador de Código (src/compiler.cpp, include/compiler.h)
6.1 Función de conversión de tipo AST a constante C
En el generador, agregar una función privada estática:


static std::string tipoAnotadoALatTipo(TipoAnotado t) {
    switch (t) {
        case TipoAnotado::Numero: return "LAT_NUMERO";
        case TipoAnotado::Cadena: return "LAT_CADENA";
        case TipoAnotado::Logico: return "LAT_LOGICO";
        case TipoAnotado::Lista:  return "LAT_LISTA";
        case TipoAnotado::Dic:    return "LAT_DICCIONARIO";
        case TipoAnotado::Nulo:   return "LAT_NULO";
        default: return "";
    }
}
6.2 Modificar la generación de asignaciones en genSentencia()
En el bloque if (auto* a = dynamic_cast<Asignacion*>(s)), cuando se genera una asignación simple (1 destino, 1 valor) y el tipo anotado no es Ninguno:


if (a->destinos.size() == 1 && a->valores.size() == 1) {
    TipoAnotado tipo = a->tiposDestino.empty() ? TipoAnotado::Ninguno : a->tiposDestino[0];
    std::string valorExpr = genExpr(a->valores[0].get());
    
    if (tipo != TipoAnotado::Ninguno) {
        // Envolver con lat_verificar_tipo()
        std::string constTipo = tipoAnotadoALatTipo(tipo);
        std::string nombreVar = "";
        if (auto* id = dynamic_cast<Identificador*>(a->destinos[0].get()))
            nombreVar = id->nombre;
        valorExpr = "lat_verificar_tipo(" + valorExpr + ", " + constTipo +
                    ", \"" + nombreVar + "\", " + std::to_string(a->linea) + ")";
    }
    emitir(genAsignacionDestino(a->destinos[0].get(), valorExpr));
    return;
}
Optimización: Si el valor es un literal puro (ya validado estáticamente), se puede omitir el lat_verificar_tipo en tiempo de compilación (el valor literal nunca va a fallar en runtime). Se recomienda emitir el chequeo siempre en esta fase (es el analizador semántico quien evitará el error temprano en literales), y dejarlo como optimización futura.

6.3 Actualizar genFuncion() para leer ParamFuncion
El bucle que genera la firma de función C usa f->parametros[i] (string). Actualizarlo a f->parametros[i].nombre. El tipo de parámetro no genera chequeo en la firma (los parámetros son siempre LatValor); los chequeos de tipo de parámetros se generan al inicio del cuerpo de la función:


void GeneradorC::genFuncion(FuncionDef* f) {
    // ... firma igual pero usando .nombre ...
    
    // Al inicio del cuerpo, generar verificaciones de parámetros anotados:
    for (const auto& param : f->parametros) {
        if (param.tipo != TipoAnotado::Ninguno) {
            std::string constTipo = tipoAnotadoALatTipo(param.tipo);
            emitir("lat_verificar_tipo(" + varC(param.nombre) + ", " + constTipo +
                   ", \"" + param.nombre + "\", " + std::to_string(f->linea) + ");");
        }
    }
    
    // ... variables locales, genBloque, return lat_nulo() ...
}
6.4 Actualizar generarCuerpo() — prototipos de funciones
El bucle de prototipos también usa f->parametros.size(). Solo cambia el acceso interno si itera sobre elementos; el size() sigue siendo correcto.

6.5 Actualizar recolectarFunciones() en el generador

funciones[f->nombre] = InfoFuncion{f->parametros.size(), f->variadico};
Funciona sin cambio porque .size() es igual.

6.6 Actualizar la variable excluir en genFuncion()

std::set<std::string> excluir;
for (const auto& p : f->parametros) excluir.insert(p.nombre);
6.7 Actualizar colectar() (función anónima en compiler.cpp)
La función colectar en el namespace anónimo del generador no toca parámetros, solo destinos de asignación. No necesita cambios.

Fase 7 — Actualizar el Impresor de AST (src/ast_impresor.cpp)
En visitar(FuncionDef& n), la firma que imprime necesita mostrar los tipos:


for (size_t i = 0; i < n.parametros.size(); ++i) {
    if (i) firma += ", ";
    firma += n.parametros[i].nombre;
    if (n.parametros[i].tipo != TipoAnotado::Ninguno)
        firma += ":" + nombreTipoAst(n.parametros[i].tipo);
}
En visitar(Asignacion& n), mostrar los tipos si presentes:


for (size_t i = 0; i < n.destinos.size(); i++) {
    if (n.destinos[i]) hijo(*n.destinos[i]);
    TipoAnotado t = (i < n.tiposDestino.size()) ? n.tiposDestino[i] : TipoAnotado::Ninguno;
    if (t != TipoAnotado::Ninguno)
        linea("  tipo: " + nombreTipoAst(t));
}
Fase 8 — Tests (tests/test_tipado.cpp, tests/CMakeLists.txt)
Crear el archivo de pruebas combinando los tres tipos de test que ya existen en el proyecto:

8.1 Tests de parser (test_tipado.cpp, sección parser)
Usando el patrón de test_parser.cpp (parseo + volcado AST):


"n: numero = 42"        → AST contiene Asignacion con tipo "numero"
"s: cadena = \"hola\""  → AST contiene tipo "cadena"
"b: logico = cierto"    → AST contiene tipo "logico"
"n = 42"                → AST sin anotación de tipo (retrocompat)
"funcion f(a: numero, b: cadena): logico" → AST con tipos en parámetros y retorno
8.2 Tests semánticos (sección analizador)
Usando el patrón de test_semantico.cpp:


// Errores estáticos detectados:
"n: numero = \"hola\""    → error "tipo incompatible: se declaró 'numero' pero el valor es 'cadena'"
"b: logico = 42"          → error "tipo incompatible"
"lst: lista = {\"a\":1}"  → error "tipo incompatible"

// Sin error estático (valor dinámico):
"n: numero = leer()"      → OK (se verifica en runtime)

// Retrocompatibilidad:
"n = 42\n escribir(n)"    → OK sin anotación
8.3 Tests de generación de código (sección codegen)
Usando el patrón de test_codegen.cpp:


"n: numero = 42"  → el C contiene "lat_verificar_tipo(lat_numero(42), LAT_NUMERO, \"n\","
"s: cadena = leer()"  → el C contiene "lat_verificar_tipo(lat_leer(), LAT_CADENA, \"s\","
8.4 Tests E2E (sección harness)
Usando el patrón de test_harness.h con casos que se compilan y ejecutan:


// Tipos correctos: sin error
"n: numero = 42\n escribir(n)"           → "42"
"s: cadena = \"hola\"\n escribir(s)"     → "hola"

// Error de tipo en runtime: el proceso termina con exit(1)
// (estos se verifican con rc_exec != 0)
"n: numero = leer()"  con input que devuelve cadena → falla
8.5 Registro en tests/CMakeLists.txt

# Pruebas de tipado gradual (Fase 30)
add_executable(test_tipado
  test_tipado.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
  ${CMAKE_SOURCE_DIR}/src/analizador_semantico.cpp
  ${CMAKE_SOURCE_DIR}/src/compiler.cpp
)
target_include_directories(test_tipado PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_tipado COMMAND test_tipado)

# Para tests E2E de tipado, usar add_suite20:
add_suite20(test_tipado_e2e)
Retos Técnicos Detallados
Reto 1: Ambigüedad : en el parser
Descrito exhaustivamente en la Fase 3. La conclusión es:

El : del ternario solo ocurre dentro de parseTernario(), que se llama desde dentro de parseExpresion(), nunca como primer token a nivel de sentencia después de un identificador puro.
El : de caso: y defecto: solo ocurre dentro de parseElegir().
El : de diccionario solo ocurre dentro de parseDiccionario().
El : de anotación de tipo solo se intenta en parseAsignacionOExpr() cuando hay exactamente un identificador seguido de :.
No hay ambigüedad real. La solución del buffer de un token (tokenDevuelto) en el parser cubre el único caso borde: si : aparece después de un identificador pero lo siguiente no es un tipo válido, se devuelve el : al stream y se reporta un error descriptivo (o se puede dejar pasar para que el ternario lo maneje si alguna vez se usa id : expr_ternaria sin ?, lo que sería un error de sintaxis igualmente).

Reto 2: Cambio de vector<string> a vector<ParamFuncion> en FuncionDef
Este cambio rompe el acceso a f->parametros[i] en cinco lugares:

parser.cpp — parseFuncion(): ya se migra en la Fase 3.
analizador_semantico.cpp — visitar(FuncionDef&): accede a p (string) en el bucle.
compiler.cpp — genFuncion(): usa f->parametros[i] para la firma.
compiler.cpp — generarCuerpo(): bucle de prototipos, acceso a f->parametros.size().
ast_impresor.cpp — visitar(FuncionDef&): usa n.parametros[i] para imprimir.
Todos se actualizan a .nombre. La búsqueda sistemática con grep evita omisiones: grep -n "->parametros\[" src/*.cpp.

Reto 3: Retrocompatibilidad de tiposDestino
El campo tiposDestino en Asignacion tiene TipoAnotado::Ninguno en todos los índices cuando no hay anotación. La comprobación if (tiposDestino.empty() || tiposDestino[i] == TipoAnotado::Ninguno) evita emitir lat_verificar_tipo para código sin anotaciones — cero overhead en código existente.

Reto 4: El generador no conoce si el valor es literal
El analizador semántico ya hizo el chequeo estático antes de que el generador actúe. El generador puede emitir siempre lat_verificar_tipo, incluso para literales (que nunca fallarán en runtime porque el semántico ya lo verificó). Esto es correcto y seguro. Si en el futuro se quiere eliminar el overhead en literales, se puede agregar un flag al nodo Asignacion (bool valorEsLiteralVerificado) que el semántico activa cuando ya verificó estáticamente, y el generador omite lat_verificar_tipo cuando ese flag está activo.

Orden de Implementación Recomendado
El orden garantiza que cada paso compila y los tests pasan antes de continuar:

Paso 1 — Fundamentos del AST (no hay compilación rota porque los campos nuevos son opcionales/tienen defaults)

Agregar TipoAnotado enum y ParamFuncion struct a include/ast.h
Agregar tiposDestino a Asignacion y cambiar parametros en FuncionDef
Compilar el proyecto — los visitantes existentes deben seguir compilando; el único error de compilación será en los accesos f->parametros[i] que esperan string y ahora reciben ParamFuncion
Paso 2 — Actualizar todos los consumidores de parametros

src/ast_impresor.cpp — visitar(FuncionDef&): cambiar acceso a .nombre
src/analizador_semantico.cpp — visitar(FuncionDef&): cambiar acceso a .nombre y actualizar declararVariable para aceptar TipoAnotado
src/compiler.cpp — genFuncion() y generarCuerpo(): cambiar acceso a .nombre
Compilar y verificar que todos los tests existentes siguen pasando
Paso 3 — Runtime

Agregar lat_verificar_tipo() a runtime/latino.h
Implementar lat_verificar_tipo() en runtime/latino.c
Compilar el runtime solo (add_library(latino_runtime ...)) y verificar
Paso 4 — Parser

Agregar mapearNombreTipo() y el buffer tokenDevuelto en parser.h/parser.cpp
Modificar parseAsignacionOExpr() para detectar anotaciones en variables
Modificar parseFuncion() para detectar anotaciones en parámetros y retorno
Actualizar Parser::avanzar() si se usa buffer de devolución
Compilar y verificar que el código existente sin anotaciones parsea igual (tests test_parser)
Paso 5 — Analizador Semántico

Cambiar ambitos de vector<unordered_set<string>> a vector<unordered_map<string, TipoAnotado>>
Actualizar declararVariable() y estaDeclarada()
Agregar verificarCompatibilidadLiteral() y nombreTipo()
Actualizar visitar(Asignacion&) para llamar a la verificación
Compilar y verificar que los tests semánticos existentes siguen pasando
Paso 6 — Generador de Código

Agregar tipoAnotadoALatTipo() en compiler.cpp
Modificar genSentencia() para el caso Asignacion con tipo anotado
Modificar genFuncion() para emitir verificaciones de parámetros anotados
Compilar y verificar que el test test_codegen sigue pasando
Paso 7 — Tests de la nueva funcionalidad

Crear tests/test_tipado.cpp con secciones parser/semántico/codegen/e2e
Agregar el target a tests/CMakeLists.txt
Ejecutar todos los tests
Garantía de Retrocompatibilidad
Todos los nodos Asignacion creados sin anotación tienen tiposDestino vacío o lleno de TipoAnotado::Ninguno. Todos los nodos FuncionDef sin anotación tienen ParamFuncion con tipo = TipoAnotado::Ninguno y tipoRetorno = TipoAnotado::Ninguno. En todos los puntos donde se chequea el tipo, la condición tipo != TipoAnotado::Ninguno actúa como guard — si no hay anotación, el código sigue exactamente el mismo camino que antes. El test de regresión más importante es ejecutar toda la suite tests/ existente después de cada paso sin modificar ningún archivo .lat de ejemplos.

Critical Files for Implementation
c:/Github/latino-ia/include/ast.h
c:/Github/latino-ia/src/parser.cpp
c:/Github/latino-ia/src/analizador_semantico.cpp
c:/Github/latino-ia/runtime/latino.c
c:/Github/latino-ia/src/compiler.cpp