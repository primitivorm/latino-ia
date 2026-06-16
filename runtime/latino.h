/* latino.h
 *
 * Biblioteca de soporte (runtime) para los programas Latino transpilados a C.
 * Define el tipo dinámico LatValor y las operaciones que el código generado por
 * el compilador (Fase 5) utiliza.
 *
 * Nota: por simplicidad, esta primera versión NO libera memoria (cadenas,
 * listas y diccionarios se asignan en el heap y se filtran). Es suficiente para
 * los programas cortos de ejemplo; una estrategia de liberación/recolección
 * llegará más adelante.
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
    LAT_DICCIONARIO
} LatTipo;

typedef struct LatLista LatLista;
typedef struct LatDic LatDic;

typedef struct {
    LatTipo tipo;
    union {
        int logico;
        double numero;
        char* cadena;
        LatLista* lista;
        LatDic* dic;
    } como;
} LatValor;

struct LatLista {
    LatValor* datos;
    size_t longitud;
    size_t capacidad;
};

struct LatDic {
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

#endif /* LATINO_H */
