/* latino.h
 *
 * Biblioteca de soporte (runtime) para los programas Latino transpilados a C.
 * Define el tipo dinámico LatValor y las operaciones que el código generado por
 * el compilador (Fase 5) utiliza.
 *
 * Fase 19: El runtime usa conteo de referencias para cadenas, listas y
 * diccionarios.  Llama a lat_valor_retener/lat_valor_liberar al asignar o
 * descartar valores de heap para controlar el ciclo de vida de la memoria.
 */

#ifndef LATINO_H
#define LATINO_H

#include <stddef.h>

typedef enum {
    LAT_NULO,
    LAT_LOGICO,
    LAT_NUMERO,
    LAT_CADENA,
    LAT_LISTA,
    LAT_DICCIONARIO,
    LAT_OBJETO,
    LAT_FUNCION,
    LAT_MODULO
} LatTipo;

typedef struct LatValor LatValor;
typedef struct LatLista LatLista;
typedef struct LatDic LatDic;
typedef struct LatObjeto LatObjeto;
typedef struct LatModulo LatModulo;

typedef LatValor (*LatFnModulo)(int nargs, LatValor* args);

struct LatValor {
    LatTipo tipo;
    union {
        int logico;
        double numero;
        char* cadena;
        LatLista* lista;
        LatDic* dic;
        LatObjeto* objeto;
        LatFnModulo funcion;
        LatModulo* modulo;
    } como;
};

/* Tipo función exportada por módulos dinámicos:
 *   LatValor mi_funcion(int nargs, LatValor* args)
 */

struct LatModulo {
    void* handle;  /* LoadLibrary / dlopen handle */
};

struct LatLista {
    size_t refs;      /* Fase 19: contador de referencias */
    LatValor* datos;
    size_t longitud;
    size_t capacidad;
};

struct LatDic {
    size_t refs;      /* Fase 19: contador de referencias */
    char** claves;
    LatValor* valores;
    size_t longitud;
    size_t capacidad;
};

/* --- Constructores --- */
LatValor lat_nulo(void);
LatValor lat_logico(int b);
LatValor lat_numero(double n);
LatValor lat_cadena(const char* s);
LatValor lat_lista_de(size_t n, ...);            /* n elementos LatValor */
LatValor lat_dic_de(size_t n, ...);              /* n pares: clave, valor (ambos LatValor) */

/* --- Objetos (POO) --- */
/* Nota: API mínima para soporte POO en el runtime. El compilador usará estas
 * funciones para crear instancias, leer/escribir campos y registrar métodos.
 */
LatValor lat_obj_nuevo(const char* clase);
LatValor lat_obj_get(LatValor objeto, const char* nombre);
LatValor lat_obj_get_seguro(LatValor objeto, const char* nombre);
void     lat_obj_set(LatValor objeto, const char* nombre, LatValor valor);
void     lat_obj_set_metodo(LatValor objeto, const char* nombre, LatValor fn);
LatValor lat_obj_llamar_metodo(LatValor objeto, const char* nombre, int nargs, ...);
LatValor lat_funcion_nueva(LatFnModulo fn);
void     lat_obj_set_clase(LatValor objeto, const char* clase);
int      lat_obj_es_instancia(LatValor objeto, const char* clase);

/* --- Aritmética --- */
LatValor lat_sumar(LatValor a, LatValor b);
LatValor lat_restar(LatValor a, LatValor b);
LatValor lat_multiplicar(LatValor a, LatValor b);
LatValor lat_dividir(LatValor a, LatValor b);
LatValor lat_modulo(LatValor a, LatValor b);
LatValor lat_potencia(LatValor a, LatValor b);
LatValor lat_negar(LatValor a);

/* --- Concatenación --- */
LatValor lat_concatenar(LatValor a, LatValor b);

/* --- Relacionales y lógicos (devuelven un LatValor lógico) --- */
LatValor lat_igual(LatValor a, LatValor b);
LatValor lat_distinto(LatValor a, LatValor b);
LatValor lat_menor(LatValor a, LatValor b);
LatValor lat_mayor(LatValor a, LatValor b);
LatValor lat_menor_igual(LatValor a, LatValor b);
LatValor lat_mayor_igual(LatValor a, LatValor b);
LatValor lat_y(LatValor a, LatValor b);
LatValor lat_o(LatValor a, LatValor b);
LatValor lat_coincide(LatValor a, LatValor b);   /* ~= : por ahora, igualdad de cadenas */

int lat_es_verdadero(LatValor v);

/* --- Indexación / acceso --- */
LatValor lat_obtener_indice(LatValor cont, LatValor indice);
void lat_asignar_indice(LatValor cont, LatValor indice, LatValor valor);

/* --- Conversión a cadena (devuelve memoria en el heap) --- */
char* lat_a_cadena(LatValor v);

/* --- Funciones incorporadas --- */
LatValor lat_escribir(LatValor v);   /* imprime y añade salto de línea */
LatValor lat_imprimir(LatValor v);   /* imprime y añade salto de línea */
LatValor lat_acadena(LatValor v);
LatValor lat_alogico(LatValor v);
LatValor lat_anumero(LatValor v);
LatValor lat_leer(void);
LatValor lat_tipo(LatValor v);
void lat_imprimirf(size_t n, ...);
LatValor lat_limpiar(void);
LatValor lat_error(LatValor v);
LatValor lat_incluir(LatValor v);

/* --- Gestión de memoria por conteo de referencias (Fase 19) --- */
/* Incrementa el contador del valor heap; devuelve el mismo valor. */
LatValor lat_valor_retener(LatValor v);
/* Decrementa el contador; libera la memoria cuando llega a cero. */
void     lat_valor_liberar(LatValor v);

/* --- Fase 26: argumentos de línea de comandos --- */
extern int   lat_argc;
extern char **lat_argv;
void lat_set_args(int argc, char **argv);

/* --- Tipado gradual: verificación de tipo en asignación/parámetro ---
 * Verifica que v.tipo == tipo_esperado (valor LatTipo).
 * Si no coincide, imprime un mensaje de error a stderr y termina con exit(1).
 * Devuelve v sin modificarlo si la verificación pasa.
 * nombre_var y linea se usan solo para el mensaje de error.
 */
LatValor lat_verificar_tipo(LatValor v, int tipo_esperado,
                             const char *nombre_var, int linea);

/* --- Backend LLVM: verificación de ABI (Fase L2 de input/PLAN_LLVM.md) ---
 * GeneradorLLVM deriva el tamaño, la alineación y el offset del campo
 * 'tipo' de LatValor a partir de generated/runtime_abi.ll (generado por
 * Clang) y los incrusta como constantes en el 'main' de cada programa
 * compilado con --backend=llvm. lat_abi_verificar() comprueba en runtime
 * que ese ABI derivado por Clang coincide con el que usó el compilador de C
 * que construyó ESTE runtime.c (pueden ser toolchains distintos). Si no
 * coinciden, termina con un mensaje claro en vez de corromper memoria en
 * silencio.
 */
void lat_abi_verificar(size_t tam_esperado, size_t alineacion_esperada,
                        size_t offset_tipo_esperado);

#endif /* LATINO_H */
