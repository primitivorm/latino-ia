/* paquete_fixture.c — biblioteca nativa mínima usada únicamente por
 * tests/test_lib_paquete.cpp para ejercitar "incluir \"paquete\"" de punta a
 * punta (PLAN_LIBS.md, Fase 16). No forma parte del runtime de Latino: es
 * exactamente el tipo de extensión nativa de terceros que un usuario final
 * escribiría para cargar con paquete.cargar/paquete.llamar -- cada función
 * exportada respeta la convención documentada en runtime/libs/paquete.h:
 *   LatValor nombre_funcion(int nargs, LatValor* args)
 */

#include "latino.h"

LatValor sumar_dos(int nargs, LatValor* args) {
    if (nargs < 2) return lat_nulo();
    return lat_numero(args[0].como.numero + args[1].como.numero);
}

LatValor saludo_fijo(int nargs, LatValor* args) {
    (void)nargs;
    (void)args;
    return lat_cadena("hola desde paquete_fixture");
}
