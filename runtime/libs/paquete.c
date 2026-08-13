/* paquete.c — implementación de la librería de módulos dinámicos de Latino. */

#define _CRT_SECURE_NO_WARNINGS

#include "paquete.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Carga de librerías dinámicas: plataforma específica. */
#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
    typedef HMODULE HandleLib;
    static HandleLib abrir_lib(const char* ruta)       { return LoadLibraryA(ruta); }
    static void*     buscar_sym(HandleLib h, const char* s) { return (void*)GetProcAddress(h, s); }
    static void      cerrar_lib(HandleLib h)           { FreeLibrary(h); }
#else
#   include <dlfcn.h>
    typedef void* HandleLib;
    static HandleLib abrir_lib(const char* ruta)       { return dlopen(ruta, RTLD_LAZY); }
    static void*     buscar_sym(HandleLib h, const char* s) { return dlsym(h, s); }
    static void      cerrar_lib(HandleLib h)           { dlclose(h); }
#endif

/* -------------------------------------------------------------------------
 * lat_paquete_cargar(ruta) — carga una librería dinámica y devuelve un módulo
 * ---------------------------------------------------------------------- */
LatValor lat_paquete_cargar(LatValor ruta_v) {
    if (ruta_v.tipo != LAT_CADENA || !ruta_v.como.cadena) {
        fprintf(stderr, "paquete.cargar: se esperaba una ruta de cadena\n");
        return lat_nulo();
    }

    HandleLib h = abrir_lib(ruta_v.como.cadena);
    if (!h) {
        fprintf(stderr, "paquete.cargar: no se pudo cargar '%s'\n",
                ruta_v.como.cadena);
        return lat_nulo();
    }

    LatModulo* mod = (LatModulo*)malloc(sizeof(LatModulo));
    if (!mod) { cerrar_lib(h); return lat_nulo(); }
    mod->handle = (void*)h;

    LatValor v;
    v.tipo = LAT_MODULO;
    v.como.modulo = mod;
    return v;
}

/* -------------------------------------------------------------------------
 * lat_paquete_llamar(modulo, nombre, nargs, arg0, ...) — despacho dinámico
 *
 * Busca el símbolo 'nombre' en el módulo y lo llama con la firma:
 *   LatValor fn(int nargs, LatValor* args)
 * ---------------------------------------------------------------------- */
LatValor lat_paquete_llamar(LatValor modulo_v, LatValor nombre_v,
                             int nargs, ...) {
    if (modulo_v.tipo != LAT_MODULO || !modulo_v.como.modulo) {
        fprintf(stderr, "paquete: el objeto no es un módulo cargado\n");
        return lat_nulo();
    }
    if (nombre_v.tipo != LAT_CADENA || !nombre_v.como.cadena) {
        fprintf(stderr, "paquete: nombre de función inválido\n");
        return lat_nulo();
    }

    HandleLib h = (HandleLib)modulo_v.como.modulo->handle;
    const char* nombre = nombre_v.como.cadena;

    void* sym = buscar_sym(h, nombre);
    if (!sym) {
        fprintf(stderr, "paquete: función '%s' no encontrada en el módulo\n",
                nombre);
        return lat_nulo();
    }

    /* Recopilar argumentos variádicos en un arreglo. */
    LatValor* args = NULL;
    if (nargs > 0) {
        args = (LatValor*)malloc(sizeof(LatValor) * (size_t)nargs);
        if (!args) return lat_nulo();
        va_list ap;
        va_start(ap, nargs);
        for (int i = 0; i < nargs; i++)
            args[i] = va_arg(ap, LatValor);
        va_end(ap);
    }

    LatValor resultado = lat_paquete_llamar_args(modulo_v, nombre_v, nargs, args);
    free(args);
    return resultado;
}

LatValor lat_paquete_llamar_args(LatValor modulo_v, LatValor nombre_v,
                                 int nargs, LatValor* args) {
    if (modulo_v.tipo != LAT_MODULO || !modulo_v.como.modulo) {
        fprintf(stderr, "paquete: el objeto no es un módulo cargado\n");
        return lat_nulo();
    }
    if (nombre_v.tipo != LAT_CADENA || !nombre_v.como.cadena) {
        fprintf(stderr, "paquete: nombre de función inválido\n");
        return lat_nulo();
    }

    HandleLib h = (HandleLib)modulo_v.como.modulo->handle;
    const char* nombre = nombre_v.como.cadena;

    void* sym = buscar_sym(h, nombre);
    if (!sym) {
        fprintf(stderr, "paquete: función '%s' no encontrada en el módulo\n",
                nombre);
        return lat_nulo();
    }

    LatFnModulo fn = (LatFnModulo)(size_t)sym;
    LatValor resultado = fn(nargs, args);
    return resultado;
}
