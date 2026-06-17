/* sis.c — implementación de la librería de sistema de Latino. */

#define _CRT_SECURE_NO_WARNINGS

#include "sis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#   include <windows.h>
#   include <direct.h>   /* _getcwd */
    /* popen/pclose son _popen/_pclose en MSVC */
#   ifndef popen
#       define popen  _popen
#       define pclose _pclose
#   endif
#else
#   include <unistd.h>
#endif

/* -------------------------------------------------------------------------
 * 1. sis.dormir(ms) — pausa ms milisegundos
 * ---------------------------------------------------------------------- */
LatValor lat_sis_dormir(LatValor ms_v) {
    long ms = (ms_v.tipo == LAT_NUMERO) ? (long)ms_v.como.numero : 0;
    if (ms < 0) ms = 0;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 2. sis.ejecutar(cmd) — ejecuta comando shell y devuelve su stdout
 * ---------------------------------------------------------------------- */
LatValor lat_sis_ejecutar(LatValor cmd_v) {
    if (cmd_v.tipo != LAT_CADENA || !cmd_v.como.cadena) return lat_cadena("");
    FILE *fp = popen(cmd_v.como.cadena, "r");
    if (!fp) return lat_cadena("");

    char buf[256];
    size_t usado = 0, cap = 512;
    char *out = (char *)malloc(cap);
    if (!out) { pclose(fp); return lat_cadena(""); }
    out[0] = '\0';

    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (usado + len + 1 > cap) {
            cap = cap * 2 + len;
            char *tmp = (char *)realloc(out, cap);
            if (!tmp) break;
            out = tmp;
        }
        memcpy(out + usado, buf, len);
        usado += len;
        out[usado] = '\0';
    }
    pclose(fp);

    /* Elimina \r\n o \n final */
    if (usado > 0 && out[usado - 1] == '\n') { out[--usado] = '\0'; }
    if (usado > 0 && out[usado - 1] == '\r') { out[--usado] = '\0'; }

    LatValor v = lat_cadena(out);
    free(out);
    return v;
}

/* -------------------------------------------------------------------------
 * 3. sis.fecha() — fecha actual "DD/MM/AAAA"
 * ---------------------------------------------------------------------- */
LatValor lat_sis_fecha(void) {
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    char buf[11];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
             t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    return lat_cadena(buf);
}

/* -------------------------------------------------------------------------
 * 4. sis.salir(codigo) — termina el proceso
 * ---------------------------------------------------------------------- */
LatValor lat_sis_salir(LatValor codigo_v) {
    int code = (codigo_v.tipo == LAT_NUMERO) ? (int)codigo_v.como.numero : 0;
    exit(code);
    return lat_nulo(); /* inalcanzable, silencia warnings */
}

/* -------------------------------------------------------------------------
 * 5. sis.cwd() — directorio de trabajo actual
 * ---------------------------------------------------------------------- */
LatValor lat_sis_cwd(void) {
    char buf[4096];
#ifdef _WIN32
    char *p = _getcwd(buf, sizeof(buf));
#else
    char *p = getcwd(buf, sizeof(buf));
#endif
    return lat_cadena(p ? p : "");
}

/* -------------------------------------------------------------------------
 * 6. sis.iraxy(x, y) — mueve el cursor ANSI a columna x, fila y
 * ---------------------------------------------------------------------- */
LatValor lat_sis_iraxy(LatValor x_v, LatValor y_v) {
    int x = (x_v.tipo == LAT_NUMERO) ? (int)x_v.como.numero : 1;
    int y = (y_v.tipo == LAT_NUMERO) ? (int)y_v.como.numero : 1;
    /* ESC[y;xH — 1-indexed */
    printf("\033[%d;%dH", y, x);
    fflush(stdout);
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 7. sis.tiempo() — timestamp Unix en segundos (double)
 * ---------------------------------------------------------------------- */
LatValor lat_sis_tiempo(void) {
    return lat_numero((double)time(NULL));
}

/* -------------------------------------------------------------------------
 * 8. sis.usuario() — nombre del usuario del sistema
 * ---------------------------------------------------------------------- */
LatValor lat_sis_usuario(void) {
    const char *u = NULL;
#ifdef _WIN32
    u = getenv("USERNAME");
#else
    u = getenv("USER");
    if (!u) u = getenv("LOGNAME");
#endif
    return lat_cadena(u ? u : "");
}

/* -------------------------------------------------------------------------
 * 9. sis.operativo() / sis.op() — "windows" | "linux" | "macos"
 * ---------------------------------------------------------------------- */
LatValor lat_sis_operativo(void) {
#if defined(_WIN32) || defined(_WIN64)
    return lat_cadena("windows");
#elif defined(__APPLE__) || defined(__MACH__)
    return lat_cadena("macos");
#else
    return lat_cadena("linux");
#endif
}

LatValor lat_sis_op(void) { return lat_sis_operativo(); }
