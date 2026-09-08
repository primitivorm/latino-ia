// resolutor_modulos.h
//
// PLAN_MODULOS.md: resuelve "exportar"/"importar" antes del análisis
// semántico, reescribiendo el AST con nombres únicos (mangling) en vez de
// requerir que AnalizadorSemantico/GeneradorC/GeneradorLLVM sepan que
// existen módulos.
//
// M3 (esta fase): resolución de un solo módulo, SIN "importar" todavía
// (llega en M4). Dado el Programa del archivo de entrada, si contiene al
// menos una declaración de nivel superior marcada con "exportar" (y ninguna
// sentencia "importar"/"exportar ... desde", ver participaDeModulos),
// renombra toda declaración de nivel superior (función, clase, estructura,
// interfaz, destino de var/const/asignación simple) a
// "__mod_<slug>__<nombre_original>" y reescribe toda referencia interna
// (llamadas recursivas, `nuevo`, `es`, `base`, tipos por nombre de clase),
// respetando el sombreado de parámetros/variables locales. Ver "Riesgos
// técnicos" del plan.
//
// Un archivo alcanzado por `incluir "x.lat"` (en vez de compilado
// directamente o alcanzado por `importar`) nunca pasa por este resolutor:
// "exportar" es puramente documental bajo `incluir` (Decisión de diseño 6).

#ifndef RESOLUTOR_MODULOS_H
#define RESOLUTOR_MODULOS_H

#include <string>
#include <unordered_map>

#include "ast.h"

class ResolutorModulos {
public:
    // nombre_exportado -> nombre_interno ya renombrado; "__defecto__" es la
    // clave especial de "exportar por defecto" (ver PLAN_MODULOS.md).
    using TablaExportacion = std::unordered_map<std::string, std::string>;

    // Deriva un slug válido como fragmento de identificador C a partir de una
    // ruta de archivo (todo carácter que no sea alfanumérico se reemplaza por
    // '_'). No resuelve colisiones entre rutas distintas con el mismo slug
    // todavía: eso requiere un registro de módulos ya cargados, que llega en
    // M4 junto con la resolución multi-archivo.
    static std::string slugDesdeRuta(const std::string& ruta);

    // true si `programa` contiene, entre sus sentencias de nivel superior, al
    // menos una declaración "exportada" o alguna sentencia ImportarDecl/
    // ExportarDesde (ver "Cuándo un archivo entra al sistema de módulos").
    static bool participaDeModulos(const Programa& programa);

    // Mangla los nombres de nivel superior de `programa` (funciones, clases,
    // estructuras, interfaces, destinos de asignaciones de nivel superior) y
    // reescribe toda referencia interna a esos nombres en el resto del
    // árbol. Devuelve la tabla de exportación resultante.
    //
    // Precondición: `programa` no contiene ImportarDecl ni ExportarDesde
    // (M3 no resuelve imports; el llamador debe verificarlo con
    // participaDeModulos + su propio chequeo, y no invocar esta función si
    // hay imports, dejando el archivo con el comportamiento previo a M3).
    static TablaExportacion resolverModuloUnico(Programa& programa,
                                                 const std::string& rutaCanonica);
};

#endif  // RESOLUTOR_MODULOS_H
