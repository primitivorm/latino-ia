/* hilos.c — implementación de runtime/libs/hilos.h. */

#include "hilos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

/* Trampolín que adapta la convención empaquetada de un callable de Latino
 * (LatFnModulo) a la firma nativa de un hilo. Los argumentos se copian antes
 * de arrancar el hilo (ver lat_hilo_crear), así que esta copia es privada. */
static void* lat_hilos_trampolin(void* ptr) {
    LatHiloArgs* a = (LatHiloArgs*)ptr;
    LatValor* args = a->args;
    LatValor resultado = lat_nulo();
    /* Los argumentos capturados (incluida la función en args[0]) viven en la
     * copia privada de LatHiloArgs: publicarlos como raíces evita que el
     * barrido del hilo creador los libere antes de que este hilo los use. */
    LatValor** raices = NULL;
    if (a->nargs > 0 && args) {
        raices = (LatValor**)malloc(a->nargs * sizeof(LatValor*));
        if (raices) for (size_t i = 0; i < a->nargs; i++) raices[i] = &args[i];
    }
    lat_raices_empujar(raices, raices ? a->nargs : 0);
    if (args && args[0].tipo == LAT_FUNCION && args[0].como.funcion) {
        resultado = args[0].como.funcion(a->nargs - 1, args + 1);
    }
    (void)resultado;
    lat_recoger(lat_nulo());
    lat_raices_sacar();
    free(raices);
    lat_hilo_liberar_recursos();
    return NULL;
}

LatValor lat_hilos_crear(LatValor fn, LatValor args) {
    if (fn.tipo != LAT_FUNCION || !fn.como.funcion) {
        fprintf(stderr, "Error: hilos.crear espera una funcion\n");
        exit(1);
    }
    size_t n = 0;
    if (args.tipo == LAT_LISTA && args.como.lista) n = args.como.lista->longitud;
    LatHiloArgs a;
    a.fn = lat_hilos_trampolin;
    a.nargs = n + 1;
    a.args = (LatValor*)malloc(a.nargs * sizeof(LatValor));
    if (!a.args) {
        fprintf(stderr, "Error: sin memoria para los argumentos del hilo\n");
        exit(1);
    }
    a.args[0] = fn;
    for (size_t i = 0; i < n; i++) a.args[i + 1] = args.como.lista->datos[i];
    LatValor hilo = lat_hilo_crear(lat_hilos_trampolin, a);
    free(a.args);
    lat_hilo_iniciar(hilo);
    return hilo;
}

LatValor lat_hilos_esperar(LatValor hilo) {
    lat_hilo_esperar(hilo);
    return lat_nulo();
}

LatValor lat_hilos_dormir(LatValor segundos) {
    lat_hilo_dormir(segundos.tipo == LAT_NUMERO ? segundos.como.numero : 0.0);
    return lat_nulo();
}

LatValor lat_hilos_rendir(void) {
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
    return lat_nulo();
}

LatValor lat_hilos_cerrojo(void) {
    return lat_hilo_cerrojo();
}

/* hilos.entrar(cerrojo, funcion, args): azúcar para `bloquear(cerrojo)`
 * alrededor de una llamada con argumentos empaquetados en una lista. */
LatValor lat_hilos_entrar(LatValor cerrojo, LatValor fn, LatValor args) {
    if (fn.tipo != LAT_FUNCION || !fn.como.funcion) {
        fprintf(stderr, "Error: hilos.entrar espera una funcion\n");
        exit(1);
    }
    size_t n = (args.tipo == LAT_LISTA && args.como.lista) ? args.como.lista->longitud : 0;
    LatValor realArgs[64];
    if (n >= 64) {
        fprintf(stderr, "Error: demasiados argumentos en hilos.entrar\n");
        exit(1);
    }
    for (size_t i = 0; i < n; i++) realArgs[i] = args.como.lista->datos[i];
    LatValor* raices[64];
    for (size_t i = 0; i < n; i++) raices[i] = &realArgs[i];
    lat_raices_empujar(raices, n);
    lat_hilo_entrar(cerrojo);
    LatValor resultado = fn.como.funcion((int)n, realArgs);
    lat_hilo_salir(cerrojo);
    lat_raices_sacar();
    return resultado;
}

LatValor lat_hilos_salir(LatValor cerrojo) {
    lat_hilo_salir(cerrojo);
    return lat_nulo();
}

LatValor lat_hilos_cerrojo_liberar(LatValor cerrojo) {
    lat_hilo_cerrojo_liberar(cerrojo);
    return lat_nulo();
}
