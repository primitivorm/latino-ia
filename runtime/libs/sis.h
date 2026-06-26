/* sis.h — librería de sistema para programas Latino transpilados.
 *
 * Uso en Latino: sis.fecha(), sis.dormir(ms), etc.
 * El compilador mapea  sis.foo(args)  →  lat_sis_foo(args)  en C.
 */

#ifndef LATINO_SIS_H
#define LATINO_SIS_H

#include "latino.h"

LatValor lat_sis_dormir(LatValor ms);
LatValor lat_sis_ejecutar(LatValor cmd);
LatValor lat_sis_fecha(void);
LatValor lat_sis_salir(LatValor codigo);
LatValor lat_sis_cwd(void);
LatValor lat_sis_iraxy(LatValor x, LatValor y);
LatValor lat_sis_tiempo(void);
LatValor lat_sis_usuario(void);
LatValor lat_sis_operativo(void);
LatValor lat_sis_op(void);   /* alias de operativo */

/* Fase 26 */
LatValor lat_sis_args(void);
LatValor lat_sis_env(LatValor nombre);
LatValor lat_sis_pid(void);

#endif /* LATINO_SIS_H */
