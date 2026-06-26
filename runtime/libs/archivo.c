/* archivo.c — implementación de la librería de archivos de Latino. */

#define _CRT_SECURE_NO_WARNINGS

#include "archivo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#else
#  include <dirent.h>
#endif

/* -------------------------------------------------------------------------
 * Utilidades internas
 * ---------------------------------------------------------------------- */

static const char *ruta_de_valor(LatValor v) {
    return (v.tipo == LAT_CADENA && v.como.cadena) ? v.como.cadena : NULL;
}

static const char *texto_de_valor(LatValor v) {
    return (v.tipo == LAT_CADENA && v.como.cadena) ? v.como.cadena : "";
}

/* Crea una LatLista vacía con capacidad inicial. */
static LatLista *lista_nueva(size_t cap) {
    LatLista *l = (LatLista *)malloc(sizeof(LatLista));
    if (!l) return NULL;
    l->refs      = 1;
    l->capacidad = cap ? cap : 8;
    l->longitud  = 0;
    l->datos = (LatValor *)malloc(sizeof(LatValor) * l->capacidad);
    if (!l->datos) { free(l); return NULL; }
    return l;
}

static void lista_push(LatLista *l, LatValor v) {
    if (l->longitud == l->capacidad) {
        size_t nueva_cap = l->capacidad * 2;
        LatValor *tmp = (LatValor *)realloc(l->datos, sizeof(LatValor) * nueva_cap);
        if (!tmp) return;
        l->datos = tmp;
        l->capacidad = nueva_cap;
    }
    l->datos[l->longitud++] = v;
}

static LatValor hacer_lista(LatLista *l) {
    LatValor v;
    v.tipo = LAT_LISTA;
    v.como.lista = l;
    return v;
}

/* -------------------------------------------------------------------------
 * 1. archivo.anexar(ruta, texto) — agrega al final del archivo
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_anexar(LatValor ruta_v, LatValor texto_v) {
    const char *ruta = ruta_de_valor(ruta_v);
    if (!ruta) return lat_nulo();
    FILE *f = fopen(ruta, "a");
    if (!f) return lat_nulo();
    fputs(texto_de_valor(texto_v), f);
    fclose(f);
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 2. archivo.borrar(ruta) / archivo.eliminar(ruta) — elimina el archivo
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_borrar(LatValor ruta_v) {
    const char *ruta = ruta_de_valor(ruta_v);
    if (!ruta) return lat_nulo();
    remove(ruta);
    return lat_nulo();
}

LatValor lat_archivo_eliminar(LatValor ruta_v) {
    return lat_archivo_borrar(ruta_v);
}

/* -------------------------------------------------------------------------
 * 3. archivo.crear(ruta) — crea archivo vacío; error si ya existe
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_crear(LatValor ruta_v) {
    const char *ruta = ruta_de_valor(ruta_v);
    if (!ruta) return lat_nulo();
    /* Verificar que no exista antes de crear */
    FILE *prueba = fopen(ruta, "r");
    if (prueba) {
        fclose(prueba);
        return lat_nulo();   /* ya existe — sin efecto */
    }
    FILE *f = fopen(ruta, "w");
    if (f) fclose(f);
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 4. archivo.duplicar(origen, destino) — copia un archivo
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_duplicar(LatValor origen_v, LatValor destino_v) {
    const char *origen  = ruta_de_valor(origen_v);
    const char *destino = ruta_de_valor(destino_v);
    if (!origen || !destino) return lat_nulo();

    FILE *src = fopen(origen, "rb");
    if (!src) return lat_nulo();
    FILE *dst = fopen(destino, "wb");
    if (!dst) { fclose(src); return lat_nulo(); }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
        fwrite(buf, 1, n, dst);

    fclose(src);
    fclose(dst);
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 5. archivo.ejecutar(ruta) — ejecuta un .lat como script (stub)
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_ejecutar(LatValor ruta_v) {
    (void)ruta_v;
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 6. archivo.escribir(ruta, texto) — sobreescribe el archivo
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_escribir(LatValor ruta_v, LatValor texto_v) {
    const char *ruta = ruta_de_valor(ruta_v);
    if (!ruta) return lat_nulo();
    FILE *f = fopen(ruta, "w");
    if (!f) return lat_nulo();
    fputs(texto_de_valor(texto_v), f);
    fclose(f);
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 7. archivo.leer(ruta) — devuelve el contenido completo como cadena
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_leer(LatValor ruta_v) {
    const char *ruta = ruta_de_valor(ruta_v);
    if (!ruta) return lat_cadena("");

    FILE *f = fopen(ruta, "rb");
    if (!f) return lat_cadena("");

    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    rewind(f);

    if (tam <= 0) { fclose(f); return lat_cadena(""); }

    char *buf = (char *)malloc((size_t)tam + 1);
    if (!buf) { fclose(f); return lat_cadena(""); }

    size_t leido = fread(buf, 1, (size_t)tam, f);
    buf[leido] = '\0';
    fclose(f);

    LatValor v = lat_cadena(buf);
    free(buf);
    return v;
}

/* -------------------------------------------------------------------------
 * 8. archivo.lineas(ruta) — lista de líneas (sin el '\n' final)
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_lineas(LatValor ruta_v) {
    const char *ruta = ruta_de_valor(ruta_v);
    LatLista *l = lista_nueva(8);
    if (!l) return lat_nulo();

    if (!ruta) return hacer_lista(l);

    FILE *f = fopen(ruta, "r");
    if (!f) return hacer_lista(l);

    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        size_t len = strlen(buf);
        /* Quitar \r y \n del final */
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
        lista_push(l, lat_cadena(buf));
    }
    fclose(f);
    return hacer_lista(l);
}

/* -------------------------------------------------------------------------
 * 9. archivo.renombrar(viejo, nuevo) — renombra / mueve
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_renombrar(LatValor viejo_v, LatValor nuevo_v) {
    const char *viejo = ruta_de_valor(viejo_v);
    const char *nuevo = ruta_de_valor(nuevo_v);
    if (!viejo || !nuevo) return lat_nulo();
    rename(viejo, nuevo);
    return lat_nulo();
}

/* =========================================================================
 * Fase 24 — Consulta de sistema de archivos
 * ====================================================================== */

/* -------------------------------------------------------------------------
 * archivo.existe(ruta) — cierto si la ruta existe (archivo o directorio)
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_existe(LatValor ruta_v) {
    const char *ruta = ruta_de_valor(ruta_v);
    if (!ruta) return lat_logico(0);
#ifdef _WIN32
    return lat_logico(GetFileAttributesA(ruta) != INVALID_FILE_ATTRIBUTES ? 1 : 0);
#else
    struct stat st;
    return lat_logico(stat(ruta, &st) == 0 ? 1 : 0);
#endif
}

/* -------------------------------------------------------------------------
 * archivo.tamanio(ruta) — tamaño en bytes (-1 si error)
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_tamanio(LatValor ruta_v) {
    const char *ruta = ruta_de_valor(ruta_v);
    if (!ruta) return lat_numero(-1.0);
    struct stat st;
#ifdef _WIN32
    if (_stat(ruta, (struct _stat *)&st) != 0) return lat_numero(-1.0);
#else
    if (stat(ruta, &st) != 0) return lat_numero(-1.0);
#endif
    return lat_numero((double)st.st_size);
}

/* -------------------------------------------------------------------------
 * archivo.listar(ruta) — lista de nombres de entradas en el directorio
 * ---------------------------------------------------------------------- */
LatValor lat_archivo_listar(LatValor ruta_v) {
    const char *ruta = ruta_de_valor(ruta_v);

    /* Construir lista dinámica de cadenas */
    size_t cap = 16, count = 0;
    LatValor *arr = (LatValor *)malloc(cap * sizeof(LatValor));
    if (!arr) return lat_nulo();

#ifdef _WIN32
    char patron[4096];
    if (!ruta) { free(arr); return lat_nulo(); }
    snprintf(patron, sizeof(patron), "%s\\*", ruta);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(patron, &fd);
    if (h == INVALID_HANDLE_VALUE) { free(arr); return lat_nulo(); }
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (count >= cap) { cap *= 2; arr = (LatValor *)realloc(arr, cap * sizeof(LatValor)); }
        arr[count++] = lat_cadena(fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    if (!ruta) { free(arr); return lat_nulo(); }
    DIR *d = opendir(ruta);
    if (!d) { free(arr); return lat_nulo(); }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (count >= cap) { cap *= 2; arr = (LatValor *)realloc(arr, cap * sizeof(LatValor)); }
        arr[count++] = lat_cadena(entry->d_name);
    }
    closedir(d);
#endif

    LatLista *l = (LatLista *)malloc(sizeof(LatLista));
    if (!l) { free(arr); return lat_nulo(); }
    l->refs      = 1;
    l->datos     = arr;
    l->longitud  = count;
    l->capacidad = cap;
    LatValor v;
    v.tipo = LAT_LISTA;
    v.como.lista = l;
    return v;
}
