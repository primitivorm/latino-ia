// resolutor_modulos.h
//
// PLAN_MODULOS.md: resuelve "exportar"/"importar" antes del análisis
// semántico, reescribiendo el AST con nombres únicos (mangling) en vez de
// requerir que AnalizadorSemantico/GeneradorC/GeneradorLLVM sepan que
// existen módulos.
//
// M3: resolución de un solo módulo, SIN "importar" (resolverModuloUnico).
// Dado el Programa del archivo de entrada, si contiene al menos una
// declaración de nivel superior marcada con "exportar" (y ninguna sentencia
// "importar"/"exportar ... desde", ver participaDeModulos), renombra toda
// declaración de nivel superior (función, clase, estructura, interfaz,
// destino de var/const/asignación simple) a
// "__mod_<slug>__<nombre_original>" y reescribe toda referencia interna
// (llamadas recursivas, `nuevo`, `es`, `base`, tipos por nombre de clase),
// respetando el sombreado de parámetros/variables locales. Ver "Riesgos
// técnicos" del plan.
//
// M4 (esta fase): resolución multi-módulo con "importar { a, b como c }
// desde ..." nombrado (resolverProyecto). DFS con memoización por ruta
// canónica: cada módulo referenciado se parsea y se procesa una sola vez,
// en orden de dependencias (los importados antes que quien importa), y un
// import circular se reporta como error de compilación con la cadena de
// archivos involucrados. "importar * como ns"/"importar Nombre desde ..."
// (namespace/por defecto) y "exportar { ... } desde ..." (re-export) quedan
// para M5/M7 respectivamente: si se encuentran, resolverProyecto reporta un
// error explícito en vez de ignorarlos en silencio.
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

    // M4: resuelve el sistema de módulos completo a partir del archivo de
    // entrada ya parseado (`entrada`, ubicado en `rutaEntrada`).
    //
    // Si `entrada` no contiene ninguna sentencia ImportarDecl/ExportarDesde,
    // delega en resolverModuloUnico (comportamiento M3, sin cambios) y
    // devuelve `entrada` tal cual (mutado in-place).
    //
    // Si contiene "importar"/"exportar ... desde", resuelve transitivamente
    // el grafo de módulos referenciado: parsea cada módulo desde disco
    // (memoizado por ruta canónica, así que un módulo importado por varios
    // otros se procesa una sola vez), en orden de dependencias (DFS
    // post-order), y devuelve un único Programa nuevo con todas las
    // declaraciones de nivel superior de todos los módulos alcanzados ya
    // manglados/reescritas y las sentencias importar/exportar-desde
    // eliminadas -- indistinguible, para AnalizadorSemantico/GeneradorC/
    // GeneradorLLVM, de un programa escrito a mano sin módulos.
    //
    // Devuelve nullptr en error de compilación (ya reportado por stderr):
    // módulo no encontrado, error de sintaxis en un módulo importado,
    // nombre no exportado, import circular, módulo importado que nunca usa
    // "exportar", o formas de import/export todavía no implementadas
    // (namespace, por defecto, re-export -- ver M5/M7 en PLAN_MODULOS.md).
    static std::unique_ptr<Programa> resolverProyecto(std::unique_ptr<Programa> entrada,
                                                        const std::string& rutaEntrada);
};

#endif  // RESOLUTOR_MODULOS_H
