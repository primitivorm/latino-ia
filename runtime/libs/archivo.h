/* archivo.h — librería de E/S de archivos para programas Latino transpilados.
 *
 * Uso en Latino: archivo.leer(ruta), archivo.escribir(ruta, texto), etc.
 * El compilador mapea  archivo.foo(args)  →  lat_archivo_foo(args)  en C.
 */

#ifndef LATINO_ARCHIVO_H
#define LATINO_ARCHIVO_H

#include "latino.h"

LatValor lat_archivo_anexar(LatValor ruta, LatValor texto);
LatValor lat_archivo_borrar(LatValor ruta);
LatValor lat_archivo_eliminar(LatValor ruta);   /* alias de borrar */
LatValor lat_archivo_crear(LatValor ruta);
LatValor lat_archivo_duplicar(LatValor origen, LatValor destino);
LatValor lat_archivo_ejecutar(LatValor ruta);
LatValor lat_archivo_escribir(LatValor ruta, LatValor texto);
LatValor lat_archivo_leer(LatValor ruta);
LatValor lat_archivo_lineas(LatValor ruta);
LatValor lat_archivo_renombrar(LatValor viejo, LatValor nuevo);

/* Fase 24 — Consulta de sistema de archivos */
LatValor lat_archivo_existe(LatValor ruta);
LatValor lat_archivo_tamanio(LatValor ruta);
LatValor lat_archivo_listar(LatValor ruta);

#endif /* LATINO_ARCHIVO_H */
