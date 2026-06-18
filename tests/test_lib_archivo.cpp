// test_lib_archivo.cpp — Suite de prueba para la librería archivo (Fase 20).
// Cubre las 9 funciones de Manual-Latino/docs/librerias/archivo.rst
// Los archivos temporales usan prefijo "_archt_" para no colisionar con otros tests.

#include "test_harness.h"

#define INC "incluir \"archivo\"\n" "incluir \"lista\"\n"

static const harness::CasoTest CASOS[] = {

    // archivo.escribir + archivo.leer (round-trip básico)
    { "arch_escribir_leer",
      INC "archivo.escribir(\"_archt_rw.txt\", \"hola\")\n"
          "escribir(archivo.leer(\"_archt_rw.txt\"))\n"
          "archivo.borrar(\"_archt_rw.txt\")",
      "hola" },

    // archivo.leer: contenido exacto con espacios
    { "arch_leer_espacios",
      INC "archivo.escribir(\"_archt_sp.txt\", \"Hola Latino\")\n"
          "escribir(archivo.leer(\"_archt_sp.txt\"))\n"
          "archivo.borrar(\"_archt_sp.txt\")",
      "Hola Latino" },

    // archivo.lineas: cuenta las líneas
    { "arch_lineas_una",
      INC "archivo.escribir(\"_archt_ln1.txt\", \"linea uno\")\n"
          "escribir(lista.longitud(archivo.lineas(\"_archt_ln1.txt\")))\n"
          "archivo.borrar(\"_archt_ln1.txt\")",
      "1" },

    { "arch_lineas_tres",
      INC "archivo.escribir(\"_archt_ln3.txt\", \"a\\nb\\nc\")\n"
          "escribir(lista.longitud(archivo.lineas(\"_archt_ln3.txt\")))\n"
          "archivo.borrar(\"_archt_ln3.txt\")",
      "3" },

    // archivo.anexar: agrega contenido al final
    { "arch_anexar",
      INC "archivo.escribir(\"_archt_ap.txt\", \"hola\")\n"
          "archivo.anexar(\"_archt_ap.txt\", \" mundo\")\n"
          "escribir(archivo.leer(\"_archt_ap.txt\"))\n"
          "archivo.borrar(\"_archt_ap.txt\")",
      "hola mundo" },

    // archivo.renombrar
    { "arch_renombrar",
      INC "archivo.escribir(\"_archt_old.txt\", \"contenido\")\n"
          "archivo.renombrar(\"_archt_old.txt\", \"_archt_new.txt\")\n"
          "escribir(archivo.leer(\"_archt_new.txt\"))\n"
          "archivo.borrar(\"_archt_new.txt\")",
      "contenido" },

    // archivo.duplicar
    { "arch_duplicar",
      INC "archivo.escribir(\"_archt_src.txt\", \"original\")\n"
          "archivo.duplicar(\"_archt_src.txt\", \"_archt_dst.txt\")\n"
          "escribir(archivo.leer(\"_archt_dst.txt\"))\n"
          "archivo.borrar(\"_archt_src.txt\")\n"
          "archivo.borrar(\"_archt_dst.txt\")",
      "original" },

    // archivo.borrar (verificar indirectamente que no hay error)
    { "arch_borrar",
      INC "archivo.escribir(\"_archt_del.txt\", \"borrar\")\n"
          "archivo.borrar(\"_archt_del.txt\")\n"
          "escribir(\"ok\")",
      "ok" },

    // archivo.crear: crear archivo vacío y luego volcar contenido
    { "arch_crear",
      INC "archivo.crear(\"_archt_cre.txt\")\n"
          "archivo.anexar(\"_archt_cre.txt\", \"creado\")\n"
          "escribir(archivo.leer(\"_archt_cre.txt\"))\n"
          "archivo.borrar(\"_archt_cre.txt\")",
      "creado" },
};

#undef INC

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
