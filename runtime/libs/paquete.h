/* paquete.h — librería de carga dinámica para programas Latino transpilados.
 *
 * Uso en Latino: paquete.cargar(ruta)  devuelve un módulo dinámico.
 * Llamadas sobre el módulo (milib.fn(args)) el compilador las emite como:
 *   lat_paquete_llamar(milib, "fn", nargs, arg0, arg1, ...)
 *
 * Convención de la librería externa: cada función exportada debe tener la firma
 *   LatValor nombre_funcion(int nargs, LatValor* args)
 */

#ifndef LATINO_PAQUETE_H
#define LATINO_PAQUETE_H

#include "latino.h"

/* Carga una librería dinámica (.dll / .so) y devuelve un LatValor LAT_MODULO. */
LatValor lat_paquete_cargar(LatValor ruta);

/* Llama a la función 'nombre' dentro del módulo con nargs argumentos variádicos. */
LatValor lat_paquete_llamar(LatValor modulo, LatValor nombre, int nargs, ...);

#endif /* LATINO_PAQUETE_H */
