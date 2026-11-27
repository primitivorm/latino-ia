/* hilos.h — librería de hilos para programas Latino transpilados.
 *
 * Uso en Latino: hilos.crear(f), hilos.esperar(t), hilos.dormir(0.5),
 * hilos.cerrojo(), etc. El compilador mapea  hilos.foo(args) →
 * lat_hilos_foo(args) en C. Además de esta API explícita, el lenguaje ofrece
 * las sentencias `hilo`, `esperar` y `bloquear` (ver PLAN_HILOS.md).
 */

#ifndef LATINO_HILOS_H
#define LATINO_HILOS_H

#include "latino.h"

LatValor lat_hilos_crear(LatValor fn, LatValor args);
LatValor lat_hilos_esperar(LatValor hilo);
LatValor lat_hilos_dormir(LatValor segundos);
LatValor lat_hilos_rendir(void);
LatValor lat_hilos_cerrojo(void);
LatValor lat_hilos_entrar(LatValor cerrojo, LatValor fn, LatValor args);
LatValor lat_hilos_salir(LatValor cerrojo);
LatValor lat_hilos_cerrojo_liberar(LatValor cerrojo);

#endif /* LATINO_HILOS_H */
