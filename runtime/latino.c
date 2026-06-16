/* latino.c — implementación del runtime de Latino. Ver latino.h. */

#include "latino.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Utilidades internas                                                */
/* ------------------------------------------------------------------ */
static char* dup_cadena(const char* s) {
    size_t n = strlen(s);
    char* p = (char*)malloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static void abortar(const char* mensaje) {
    fprintf(stderr, "Error de ejecución: %s\n", mensaje);
    exit(1);
}

/* ------------------------------------------------------------------ */
/* Constructores                                                      */
/* ------------------------------------------------------------------ */
LatValor lat_nulo(void) {
    LatValor v;
    v.tipo = LAT_NULO;
    v.como.numero = 0;
    return v;
}

LatValor lat_logico(int b) {
    LatValor v;
    v.tipo = LAT_LOGICO;
    v.como.logico = b ? 1 : 0;
    return v;
}

LatValor lat_numero(double n) {
    LatValor v;
    v.tipo = LAT_NUMERO;
    v.como.numero = n;
    return v;
}

LatValor lat_cadena(const char* s) {
    LatValor v;
    v.tipo = LAT_CADENA;
    v.como.cadena = dup_cadena(s);
    return v;
}

static LatLista* lista_nueva(size_t capacidad) {
    LatLista* l = (LatLista*)malloc(sizeof(LatLista));
    l->longitud = 0;
    l->capacidad = capacidad ? capacidad : 4;
    l->datos = (LatValor*)malloc(sizeof(LatValor) * l->capacidad);
    return l;
}

static void lista_agregar(LatLista* l, LatValor v) {
    if (l->longitud == l->capacidad) {
        l->capacidad *= 2;
        l->datos = (LatValor*)realloc(l->datos, sizeof(LatValor) * l->capacidad);
    }
    l->datos[l->longitud++] = v;
}

LatValor lat_lista_de(size_t n, ...) {
    LatValor v;
    va_list ap;
    size_t i;
    LatLista* l = lista_nueva(n);
    va_start(ap, n);
    for (i = 0; i < n; i++)
        lista_agregar(l, va_arg(ap, LatValor));
    va_end(ap);
    v.tipo = LAT_LISTA;
    v.como.lista = l;
    return v;
}

static LatDic* dic_nuevo(size_t capacidad) {
    LatDic* d = (LatDic*)malloc(sizeof(LatDic));
    d->longitud = 0;
    d->capacidad = capacidad ? capacidad : 4;
    d->claves = (char**)malloc(sizeof(char*) * d->capacidad);
    d->valores = (LatValor*)malloc(sizeof(LatValor) * d->capacidad);
    return d;
}

static void dic_poner(LatDic* d, const char* clave, LatValor valor) {
    size_t i;
    for (i = 0; i < d->longitud; i++) {
        if (strcmp(d->claves[i], clave) == 0) {
            d->valores[i] = valor;
            return;
        }
    }
    if (d->longitud == d->capacidad) {
        d->capacidad *= 2;
        d->claves = (char**)realloc(d->claves, sizeof(char*) * d->capacidad);
        d->valores = (LatValor*)realloc(d->valores, sizeof(LatValor) * d->capacidad);
    }
    d->claves[d->longitud] = dup_cadena(clave);
    d->valores[d->longitud] = valor;
    d->longitud++;
}

LatValor lat_dic_de(size_t n, ...) {
    LatValor v;
    va_list ap;
    size_t i;
    LatDic* d = dic_nuevo(n);
    va_start(ap, n);
    for (i = 0; i < n; i++) {
        LatValor clave = va_arg(ap, LatValor);
        LatValor valor = va_arg(ap, LatValor);
        char* k = lat_a_cadena(clave);
        dic_poner(d, k, valor);
        free(k);
    }
    va_end(ap);
    v.tipo = LAT_DICCIONARIO;
    v.como.dic = d;
    return v;
}

/* ------------------------------------------------------------------ */
/* Aritmética                                                         */
/* ------------------------------------------------------------------ */
static double num(LatValor v) {
    if (v.tipo == LAT_NUMERO) return v.como.numero;
    if (v.tipo == LAT_LOGICO) return v.como.logico;
    return 0;
}

LatValor lat_sumar(LatValor a, LatValor b)       { return lat_numero(num(a) + num(b)); }
LatValor lat_restar(LatValor a, LatValor b)      { return lat_numero(num(a) - num(b)); }
LatValor lat_multiplicar(LatValor a, LatValor b) { return lat_numero(num(a) * num(b)); }

LatValor lat_dividir(LatValor a, LatValor b) {
    double d = num(b);
    if (d == 0) abortar("división entre cero");
    return lat_numero(num(a) / d);
}

LatValor lat_modulo(LatValor a, LatValor b) {
    double d = num(b);
    if (d == 0) abortar("módulo entre cero");
    return lat_numero(fmod(num(a), d));
}

LatValor lat_potencia(LatValor a, LatValor b) { return lat_numero(pow(num(a), num(b))); }
LatValor lat_negar(LatValor a)                { return lat_numero(-num(a)); }

/* ------------------------------------------------------------------ */
/* Concatenación                                                      */
/* ------------------------------------------------------------------ */
LatValor lat_concatenar(LatValor a, LatValor b) {
    char* sa = lat_a_cadena(a);
    char* sb = lat_a_cadena(b);
    size_t na = strlen(sa), nb = strlen(sb);
    char* r = (char*)malloc(na + nb + 1);
    memcpy(r, sa, na);
    memcpy(r + na, sb, nb + 1);
    free(sa);
    free(sb);
    {
        LatValor v;
        v.tipo = LAT_CADENA;
        v.como.cadena = r;
        return v;
    }
}

/* ------------------------------------------------------------------ */
/* Relacionales y lógicos                                             */
/* ------------------------------------------------------------------ */
/* Compara: -1, 0, 1. Sólo números entre sí y cadenas entre sí. */
static int comparar(LatValor a, LatValor b) {
    if (a.tipo == LAT_CADENA && b.tipo == LAT_CADENA)
        return strcmp(a.como.cadena, b.como.cadena);
    {
        double x = num(a), y = num(b);
        if (x < y) return -1;
        if (x > y) return 1;
        return 0;
    }
}

static int son_iguales(LatValor a, LatValor b) {
    if (a.tipo == LAT_NULO || b.tipo == LAT_NULO)
        return a.tipo == b.tipo;
    if (a.tipo == LAT_CADENA && b.tipo == LAT_CADENA)
        return strcmp(a.como.cadena, b.como.cadena) == 0;
    if ((a.tipo == LAT_NUMERO || a.tipo == LAT_LOGICO) &&
        (b.tipo == LAT_NUMERO || b.tipo == LAT_LOGICO))
        return num(a) == num(b);
    return 0;
}

LatValor lat_igual(LatValor a, LatValor b)        { return lat_logico(son_iguales(a, b)); }
LatValor lat_distinto(LatValor a, LatValor b)     { return lat_logico(!son_iguales(a, b)); }
LatValor lat_menor(LatValor a, LatValor b)        { return lat_logico(comparar(a, b) < 0); }
LatValor lat_mayor(LatValor a, LatValor b)        { return lat_logico(comparar(a, b) > 0); }
LatValor lat_menor_igual(LatValor a, LatValor b)  { return lat_logico(comparar(a, b) <= 0); }
LatValor lat_mayor_igual(LatValor a, LatValor b)  { return lat_logico(comparar(a, b) >= 0); }

int lat_es_verdadero(LatValor v) {
    switch (v.tipo) {
        case LAT_NULO:        return 0;
        case LAT_LOGICO:      return v.como.logico;
        case LAT_NUMERO:      return v.como.numero != 0;
        case LAT_CADENA:      return v.como.cadena[0] != '\0';
        case LAT_LISTA:       return v.como.lista->longitud > 0;
        case LAT_DICCIONARIO: return v.como.dic->longitud > 0;
    }
    return 0;
}

LatValor lat_y(LatValor a, LatValor b) {
    return lat_logico(lat_es_verdadero(a) && lat_es_verdadero(b));
}
LatValor lat_o(LatValor a, LatValor b) {
    return lat_logico(lat_es_verdadero(a) || lat_es_verdadero(b));
}
LatValor lat_coincide(LatValor a, LatValor b) {
    return lat_logico(son_iguales(a, b));
}

/* ------------------------------------------------------------------ */
/* Indexación / acceso                                                */
/* ------------------------------------------------------------------ */
static size_t indice_lista(LatLista* l, LatValor indice) {
    long i;
    if (indice.tipo != LAT_NUMERO) abortar("índice de lista no numérico");
    i = (long)indice.como.numero;
    if (i < 0) i += (long)l->longitud;  /* índice negativo desde el final */
    if (i < 0 || (size_t)i >= l->longitud) abortar("índice de lista fuera de rango");
    return (size_t)i;
}

LatValor lat_obtener_indice(LatValor cont, LatValor indice) {
    if (cont.tipo == LAT_LISTA) {
        return cont.como.lista->datos[indice_lista(cont.como.lista, indice)];
    }
    if (cont.tipo == LAT_DICCIONARIO) {
        char* k = lat_a_cadena(indice);
        size_t i;
        LatDic* d = cont.como.dic;
        for (i = 0; i < d->longitud; i++) {
            if (strcmp(d->claves[i], k) == 0) {
                free(k);
                return d->valores[i];
            }
        }
        free(k);
        return lat_nulo();
    }
    abortar("el valor no admite indexación");
    return lat_nulo();
}

void lat_asignar_indice(LatValor cont, LatValor indice, LatValor valor) {
    if (cont.tipo == LAT_LISTA) {
        LatLista* l = cont.como.lista;
        long i;
        if (indice.tipo != LAT_NUMERO) abortar("índice de lista no numérico");
        i = (long)indice.como.numero;
        if (i < 0) i += (long)l->longitud;
        if (i == (long)l->longitud) { lista_agregar(l, valor); return; }
        if (i < 0 || (size_t)i >= l->longitud) abortar("índice de lista fuera de rango");
        l->datos[i] = valor;
        return;
    }
    if (cont.tipo == LAT_DICCIONARIO) {
        char* k = lat_a_cadena(indice);
        dic_poner(cont.como.dic, k, valor);
        free(k);
        return;
    }
    abortar("el valor no admite asignación por índice");
}

/* ------------------------------------------------------------------ */
/* Conversión a cadena                                                */
/* ------------------------------------------------------------------ */
char* lat_a_cadena(LatValor v) {
    switch (v.tipo) {
        case LAT_NULO:
            return dup_cadena("nulo");
        case LAT_LOGICO:
            return dup_cadena(v.como.logico ? "cierto" : "falso");
        case LAT_NUMERO: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", v.como.numero);
            return dup_cadena(buf);
        }
        case LAT_CADENA:
            return dup_cadena(v.como.cadena);
        case LAT_LISTA: {
            LatLista* l = v.como.lista;
            size_t i;
            /* "[a, b, c]" */
            size_t cap = 2, len = 0;
            char* r = (char*)malloc(cap);
            r[0] = '\0';
            #define APPEND(str)                                              \
                do {                                                         \
                    const char* _s = (str);                                 \
                    size_t _n = strlen(_s);                                  \
                    while (len + _n + 1 > cap) { cap *= 2; r = (char*)realloc(r, cap); } \
                    memcpy(r + len, _s, _n + 1);                            \
                    len += _n;                                               \
                } while (0)
            APPEND("[");
            for (i = 0; i < l->longitud; i++) {
                char* e = lat_a_cadena(l->datos[i]);
                if (i) APPEND(", ");
                APPEND(e);
                free(e);
            }
            APPEND("]");
            return r;
        }
        case LAT_DICCIONARIO: {
            LatDic* d = v.como.dic;
            size_t i;
            size_t cap = 2, len = 0;
            char* r = (char*)malloc(cap);
            r[0] = '\0';
            APPEND("{");
            for (i = 0; i < d->longitud; i++) {
                char* val = lat_a_cadena(d->valores[i]);
                if (i) APPEND(", ");
                APPEND(d->claves[i]);
                APPEND(": ");
                APPEND(val);
                free(val);
            }
            APPEND("}");
            return r;
            #undef APPEND
        }
    }
    return dup_cadena("");
}

/* ------------------------------------------------------------------ */
/* Funciones incorporadas                                             */
/* ------------------------------------------------------------------ */
LatValor lat_escribir(LatValor v) {
    char* s = lat_a_cadena(v);
    fputs(s, stdout);
    fputc('\n', stdout);
    free(s);
    return lat_nulo();
}

LatValor lat_imprimir(LatValor v) {
    return lat_escribir(v);
}
