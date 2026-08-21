/* abi_probe.c — sondeo de ABI para el backend LLVM (Fase L2 de input/PLAN_LLVM.md).
 *
 * Este archivo no se enlaza ni se ejecuta nunca. Su único propósito es
 * dejar que Clang (el mismo compilador que se usa para construir el backend
 * LLVM) materialice el layout real de `LatValor` y las firmas de las
 * funciones del runtime, para que `GeneradorLLVM` las importe desde el IR
 * resultante (generated/runtime_abi.ll) en vez de reconstruirlas a mano.
 * Reconstruirlas a mano es exactamente el riesgo que este mecanismo evita:
 * la clasificación ABI de `LatValor` (un struct de 16 bytes pasado/devuelto
 * por valor) difiere entre Windows x64 y SysV x86-64, y un desajuste entre
 * lo que asume el generador y lo que espera el `.c` real compilado corrompe
 * memoria en silencio.
 *
 * Build: ver el `add_custom_command` en CMakeLists.txt que invoca a Clang
 * con `-S -emit-llvm -target <triple-real>` sobre este archivo.
 */

#include "latino.h"
#include "libs/archivo.h"
#include "libs/cadena.h"
#include "libs/dic.h"
#include "libs/lista.h"
#include "libs/mate.h"
#include "libs/paquete.h"
#include "libs/sis.h"

/* Función identidad: fuerza a Clang a materializar el layout completo de
 * LatValor (parámetro y valor de retorno por valor), no solo declararlo. */
LatValor lat_valor_layout_probe(LatValor v) {
    return v;
}

/* Referencia (sin llamar) todas las funciones del runtime para forzar a
 * Clang a emitir un 'declare' de cada una en el IR resultante — un simple
 * #include no genera 'declare' para símbolos que nunca se referencian. */
void lat_abi_referenciar_todo(void) {
    (void)lat_a_cadena;
    (void)lat_acadena;
    (void)lat_alogico;
    (void)lat_anumero;
    (void)lat_archivo_anexar;
    (void)lat_archivo_borrar;
    (void)lat_archivo_crear;
    (void)lat_archivo_duplicar;
    (void)lat_archivo_ejecutar;
    (void)lat_archivo_eliminar;
    (void)lat_archivo_escribir;
    (void)lat_archivo_existe;
    (void)lat_archivo_leer;
    (void)lat_archivo_lineas;
    (void)lat_archivo_listar;
    (void)lat_archivo_renombrar;
    (void)lat_archivo_tamanio;
    (void)lat_asignar_indice;
    (void)lat_cadena;
    (void)lat_cadena_bytes;
    (void)lat_cadena_capitalizar;
    (void)lat_cadena_char;
    (void)lat_cadena_comparar;
    (void)lat_cadena_concatenar;
    (void)lat_cadena_contar;
    (void)lat_cadena_contiene;
    (void)lat_cadena_encontrar;
    (void)lat_cadena_es_alfa;
    (void)lat_cadena_es_espacio;
    (void)lat_cadena_es_igual;
    (void)lat_cadena_es_numerico;
    (void)lat_cadena_esta_vacia;
    (void)lat_cadena_formato;
    (void)lat_cadena_indice;
    (void)lat_cadena_inicia_con;
    (void)lat_cadena_insertar;
    (void)lat_cadena_invertir;
    (void)lat_cadena_longitud;
    (void)lat_cadena_mayusculas;
    (void)lat_cadena_minusculas;
    (void)lat_cadena_recortar;
    (void)lat_cadena_recortar_der;
    (void)lat_cadena_recortar_izq;
    (void)lat_cadena_reemplazar;
    (void)lat_cadena_regex;
    (void)lat_cadena_regexl;
    (void)lat_cadena_rellenar_derecha;
    (void)lat_cadena_rellenar_izquierda;
    (void)lat_cadena_separar;
    (void)lat_cadena_subcadena;
    (void)lat_cadena_termina_con;
    (void)lat_cadena_titulo;
    (void)lat_cadena_ultimo_indice;
    (void)lat_coincide;
    (void)lat_concatenar;
    (void)lat_dic_actualizar;
    (void)lat_dic_combinar;
    (void)lat_dic_contiene;
    (void)lat_dic_copiar;
    (void)lat_dic_de;
    (void)lat_dic_elementos;
    (void)lat_dic_eliminar;
    (void)lat_dic_llaves;
    (void)lat_dic_longitud;
    (void)lat_dic_valores;
    (void)lat_dic_vals;
    (void)lat_distinto;
    (void)lat_dividir;
    (void)lat_error;
    (void)lat_es_verdadero;
    (void)lat_escribir;
    (void)lat_funcion_nueva;
    (void)lat_igual;
    (void)lat_imprimir;
    (void)lat_imprimirf;
    (void)lat_incluir;
    (void)lat_leer;
    (void)lat_limpiar;
    (void)lat_lista_agregar;
    (void)lat_lista_comparar;
    (void)lat_lista_concatenar;
    (void)lat_lista_contar;
    (void)lat_lista_contiene;
    (void)lat_lista_crear;
    (void)lat_lista_de;
    (void)lat_lista_eliminar;
    (void)lat_lista_eliminar_indice;
    (void)lat_lista_encontrar;
    (void)lat_lista_extender;
    (void)lat_lista_indice;
    (void)lat_lista_insertar;
    (void)lat_lista_invertir;
    (void)lat_lista_longitud;
    (void)lat_lista_ordenar;
    (void)lat_lista_primero;
    (void)lat_lista_rebanada;
    (void)lat_lista_separador;
    (void)lat_lista_ultimo;
    (void)lat_lista_unico;
    (void)lat_logico;
    (void)lat_mate_abs;
    (void)lat_mate_acos;
    (void)lat_mate_acosh;
    (void)lat_mate_aleatorio;
    (void)lat_mate_alt;
    (void)lat_mate_asen;
    (void)lat_mate_asenh;
    (void)lat_mate_atan;
    (void)lat_mate_atan2;
    (void)lat_mate_atanh;
    (void)lat_mate_base;
    (void)lat_mate_cos;
    (void)lat_mate_cosh;
    (void)lat_mate_e;
    (void)lat_mate_es_primo;
    (void)lat_mate_exp;
    (void)lat_mate_factorial;
    (void)lat_mate_fibonacci;
    (void)lat_mate_frexp;
    (void)lat_mate_ldexp;
    (void)lat_mate_log;
    (void)lat_mate_log10;
    (void)lat_mate_max;
    (void)lat_mate_mcd;
    (void)lat_mate_mcm;
    (void)lat_mate_min;
    (void)lat_mate_parte;
    (void)lat_mate_pi;
    (void)lat_mate_piso;
    (void)lat_mate_porc;
    (void)lat_mate_pot;
    (void)lat_mate_raiz;
    (void)lat_mate_raizc;
    (void)lat_mate_redondear;
    (void)lat_mate_sen;
    (void)lat_mate_senh;
    (void)lat_mate_tan;
    (void)lat_mate_tanh;
    (void)lat_mate_tau;
    (void)lat_mate_techo;
    (void)lat_mate_truncar;
    (void)lat_mayor;
    (void)lat_mayor_igual;
    (void)lat_menor;
    (void)lat_menor_igual;
    (void)lat_modulo;
    (void)lat_multiplicar;
    (void)lat_negar;
    (void)lat_nulo;
    (void)lat_numero;
    (void)lat_o;
    (void)lat_obj_es_instancia;
    (void)lat_obj_get;
    (void)lat_obj_get_seguro;
    (void)lat_obj_llamar_metodo;
    (void)lat_obj_nuevo;
    (void)lat_obj_set;
    (void)lat_obj_set_clase;
    (void)lat_obj_set_metodo;
    (void)lat_obtener_indice;
    (void)lat_paquete_cargar;
    (void)lat_paquete_llamar;
    (void)lat_paquete_llamar_args;
    (void)lat_potencia;
    (void)lat_restar;
    (void)lat_set_args;
    (void)lat_sis_args;
    (void)lat_sis_cwd;
    (void)lat_sis_dormir;
    (void)lat_sis_ejecutar;
    (void)lat_sis_env;
    (void)lat_sis_fecha;
    (void)lat_sis_iraxy;
    (void)lat_sis_op;
    (void)lat_sis_operativo;
    (void)lat_sis_pid;
    (void)lat_sis_salir;
    (void)lat_sis_tiempo;
    (void)lat_sis_usuario;
    (void)lat_sumar;
    (void)lat_tipo;
    (void)lat_valor_liberar;
    (void)lat_valor_retener;
    (void)lat_verificar_tipo;
    (void)lat_y;
}
