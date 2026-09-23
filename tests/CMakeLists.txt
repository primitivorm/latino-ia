# Pruebas unitarias.
#
# El lexer se compila junto con cada prueba (todavía no hay una biblioteca
# separada; eso llegará cuando el compilador crezca).

add_executable(test_lexer
  test_lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
)
target_include_directories(test_lexer PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_lexer COMMAND test_lexer)

# Pruebas del AST (Fase 2).
add_executable(test_ast
  test_ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
)
target_include_directories(test_ast PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_ast COMMAND test_ast)

# Pruebas del parser (Fase 3).
add_executable(test_parser
  test_parser.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
)
target_include_directories(test_parser PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_parser COMMAND test_parser)

# Pruebas del análisis semántico (Fase 4).
add_executable(test_semantico
  test_semantico.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
  ${CMAKE_SOURCE_DIR}/src/analizador_semantico.cpp
)
target_include_directories(test_semantico PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_semantico COMMAND test_semantico)

# Pruebas de generación de código C (Fase 5).
add_executable(test_codegen
  test_codegen.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/compiler.cpp
  ${CMAKE_SOURCE_DIR}/src/fn_binaria.cpp
  ${CMAKE_SOURCE_DIR}/src/recolector_variables.cpp
)
target_include_directories(test_codegen PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_codegen COMMAND test_codegen)

# Pruebas de tipado gradual opcional (Fase 27).
add_executable(test_tipado
  test_tipado.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
  ${CMAKE_SOURCE_DIR}/src/analizador_semantico.cpp
  ${CMAKE_SOURCE_DIR}/src/compiler.cpp
  ${CMAKE_SOURCE_DIR}/src/fn_binaria.cpp
  ${CMAKE_SOURCE_DIR}/src/recolector_variables.cpp
)
target_include_directories(test_tipado PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_tipado COMMAND test_tipado)

# Pruebas de Programación Orientada a Objetos: clases, estructuras,
# interfaces, herencia, `nuevo`, `este`, `base` y el operador `es`.
add_executable(test_poo
  test_poo.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
  ${CMAKE_SOURCE_DIR}/src/analizador_semantico.cpp
  ${CMAKE_SOURCE_DIR}/src/compiler.cpp
  ${CMAKE_SOURCE_DIR}/src/fn_binaria.cpp
  ${CMAKE_SOURCE_DIR}/src/recolector_variables.cpp
)
target_include_directories(test_poo PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_poo COMMAND test_poo)

# Pruebas de genéricos al estilo de Rust (PLAN_GENERICOS.md).
add_executable(test_genericos
  test_genericos.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
  ${CMAKE_SOURCE_DIR}/src/analizador_semantico.cpp
  ${CMAKE_SOURCE_DIR}/src/compiler.cpp
  ${CMAKE_SOURCE_DIR}/src/fn_binaria.cpp
  ${CMAKE_SOURCE_DIR}/src/recolector_variables.cpp
)
target_include_directories(test_genericos PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_genericos COMMAND test_genericos)

# Pruebas de resolución de módulos exportar/importar (PLAN_MODULOS.md, M3).
add_executable(test_modulos
  test_modulos.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/ast_impresor.cpp
  ${CMAKE_SOURCE_DIR}/src/analizador_semantico.cpp
  ${CMAKE_SOURCE_DIR}/src/resolutor_modulos.cpp
)
target_include_directories(test_modulos PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_modulos COMMAND test_modulos)

# Pruebas de generacion de codigo C para "externo"/"inseguro" (PLAN_FFI.md,
# F5/F7): marshalling de argumentos/retorno y preambulo (extern/#pragma
# comment/#include <stdint.h>). Mismo patron que test_codegen (Fase 5).
add_executable(test_ffi
  test_ffi.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast.cpp
  ${CMAKE_SOURCE_DIR}/src/compiler.cpp
  ${CMAKE_SOURCE_DIR}/src/fn_binaria.cpp
  ${CMAKE_SOURCE_DIR}/src/recolector_variables.cpp
)
target_include_directories(test_ffi PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME test_ffi COMMAND test_ffi)

# Pruebas del runtime de FFI con C (PLAN_FFI.md, F3): LAT_PUNTERO y
# lat_ffi_verificar_tipo. A diferencia de los tests anteriores (que
# compilan el compilador de Latino), este compila directamente contra el
# runtime en C -- todavía no hay integración con el compilador (llega en
# F4/F5/F6), así que no hay nada de Latino que parsear. No se usa el target
# "latino_runtime" (solo compila runtime/latino.c para verificar que sigue
# siendo válido -- ver su comentario en el CMakeLists.txt raíz): la propia
# runtime/latino.c ya requiere lat_paquete_llamar_args de
# runtime/libs/paquete.c (lat_obj_llamar_metodo, despacho dinámico), así que
# hace falta compilar ambos .c, igual que un programa Latino real enlaza el
# runtime completo.
add_executable(test_runtime_ffi
  test_runtime_ffi.cpp
  ${CMAKE_SOURCE_DIR}/runtime/latino.c
  ${CMAKE_SOURCE_DIR}/runtime/libs/paquete.c
)
target_include_directories(test_runtime_ffi PRIVATE
  ${CMAKE_SOURCE_DIR}/runtime
  ${CMAKE_SOURCE_DIR}/runtime/libs)

add_test(NAME test_runtime_ffi COMMAND test_runtime_ffi)

# Pruebas del mecanismo de ABI del backend LLVM (Fase L2 de
# input/PLAN_LLVM.md). Solo se registran si el build tiene LLVM habilitado;
# las máquinas sin LLVM instalado siguen corriendo el resto de la suite sin
# fallos por ausencia de LLVM (ver Decisión 1 del plan).
if(LATINO_LLVM_BACKEND)
  add_executable(test_codegen_llvm
    test_codegen_llvm.cpp
    ${CMAKE_SOURCE_DIR}/src/ast.cpp
    ${CMAKE_SOURCE_DIR}/src/compiler_llvm.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime_abi_llvm.cpp
    ${CMAKE_SOURCE_DIR}/src/fn_binaria.cpp
    ${CMAKE_SOURCE_DIR}/src/recolector_variables.cpp
  )
  target_include_directories(test_codegen_llvm PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/runtime
    ${CMAKE_BINARY_DIR}/src
    ${LLVM_INCLUDE_DIRS})
  separate_arguments(LATINO_LLVM_DEFS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
  target_compile_definitions(test_codegen_llvm PRIVATE ${LATINO_LLVM_DEFS})
  llvm_map_components_to_libnames(LATINO_LLVM_TEST_LIBS core support irreader)
  target_link_libraries(test_codegen_llvm PRIVATE ${LATINO_LLVM_TEST_LIBS})
  add_dependencies(test_codegen_llvm latino_runtime_abi)

  add_test(NAME test_codegen_llvm COMMAND test_codegen_llvm)
endif()

# Utilidad de depuración (no es una prueba): vuelca los tokens de un .lat.
add_executable(lexer_dump
  lexer_dump.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
)
target_include_directories(lexer_dump PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Pruebas extremo a extremo (E2E) (Fase 8)
add_executable(test_e2e test_e2e.cpp)
target_include_directories(test_e2e PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Obtener todos los ejemplos .lat
file(GLOB EJEMPLOS_LAT "${CMAKE_SOURCE_DIR}/ejemplos/*.lat")

foreach(EJEMPLO ${EJEMPLOS_LAT})
  get_filename_component(EJEMPLO_NOMBRE ${EJEMPLO} NAME_WE)

  # Directorio para alojar los ejecutables generados
  set(SALIDA_EXE "${CMAKE_CURRENT_BINARY_DIR}/e2e_${EJEMPLO_NOMBRE}")
  if(WIN32)
    set(SALIDA_EXE "${SALIDA_EXE}.exe")
  endif()

  add_test(
    NAME "e2e_${EJEMPLO_NOMBRE}"
    COMMAND test_e2e
            $<TARGET_FILE:latino>
            "${EJEMPLO}"
            "${SALIDA_EXE}"
            "${CMAKE_SOURCE_DIR}/runtime"
  )

  # Fase L11 (input/PLAN_LLVM.md): el mismo ejemplo, compilado y ejecutado
  # con --backend=llvm -- test_e2e compara ambos contra la misma anotación
  # '#salida:' del .lat, así que una coincidencia de las dos pruebas ya
  # implica paridad de salida entre los dos backends (no hace falta un
  # segundo binario "test_e2e_llvm" que los diffee directamente). Ruta de
  # salida distinta a la del backend C para que ambas puedan ejecutarse sin
  # pisarse el mismo .exe. Solo se registra si el build tiene LLVM
  # habilitado (Decisión 1 del plan: sin LLVM, la suite del backend C debe
  # seguir corriendo completa).
  if(LATINO_LLVM_BACKEND)
    set(SALIDA_EXE_LLVM "${CMAKE_CURRENT_BINARY_DIR}/e2e_${EJEMPLO_NOMBRE}_llvm")
    if(WIN32)
      set(SALIDA_EXE_LLVM "${SALIDA_EXE_LLVM}.exe")
    endif()

    add_test(
      NAME "e2e_${EJEMPLO_NOMBRE}_llvm"
      COMMAND test_e2e
              $<TARGET_FILE:latino>
              "${EJEMPLO}"
              "${SALIDA_EXE_LLVM}"
              "${CMAKE_SOURCE_DIR}/runtime"
              llvm
    )
  endif()
endforeach()

# -----------------------------------------------------------------------
# Fase 20 — Suites de prueba de cobertura completa por librería
#
# Cada suite compila fragmentos .lat en tiempo de prueba, ejecuta el
# binario generado y compara la salida con los valores esperados.
# Argumentos: <compilador> <runtime_dir> <directorio_temporal>
# -----------------------------------------------------------------------

macro(add_suite20 TARGET)
  add_executable(${TARGET} ${TARGET}.cpp)
  add_test(
    NAME ${TARGET}
    COMMAND ${TARGET}
            $<TARGET_FILE:latino>
            "${CMAKE_SOURCE_DIR}/runtime"
            "${CMAKE_CURRENT_BINARY_DIR}"
  )

  # Fase L11 (input/PLAN_LLVM.md): la misma suite (mismo ejecutable, ya
  # compilado arriba) contra --backend=llvm, vía el parámetro de backend
  # que test_harness.h acepta desde esa fase. Directorio temporal propio
  # (subcarpeta "llvm") para que los .lat/.exe que cada variante escribe con
  # el mismo nombre de caso no se pisen entre sí. Solo si LATINO_LLVM_BACKEND
  # está habilitado (Decisión 1 del plan).
  if(LATINO_LLVM_BACKEND)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/llvm")
    add_test(
      NAME ${TARGET}_llvm
      COMMAND ${TARGET}
              $<TARGET_FILE:latino>
              "${CMAKE_SOURCE_DIR}/runtime"
              "${CMAKE_CURRENT_BINARY_DIR}/llvm"
              llvm
    )
  endif()
endmacro()

add_suite20(test_funciones_base)
add_suite20(test_lib_cadena)
add_suite20(test_lib_lista)
add_suite20(test_lib_dic)
add_suite20(test_lib_mate)
add_suite20(test_lib_sis)
add_suite20(test_lib_archivo)

# Hallazgo de la auditoría de input/PLAN_LIBS.md: "paquete" (carga dinámica
# de bibliotecas nativas, Fase 16) no tenía ninguna prueba funcional --
# ejemplos/paquete_ejemplo.lat existe pero a propósito no llama
# paquete.cargar de verdad (no había ninguna .dll/.so real en el repo).
# Se agrega una biblioteca de prueba real (tests/fixtures/paquete_fixture.c)
# para cargar/llamar de punta a punta. No puede usar add_suite20 tal cual:
# necesita un argumento extra, la ruta de esa biblioteca ya compilada (ver
# tests/test_lib_paquete.cpp).
add_library(test_paquete_fixture SHARED
  fixtures/paquete_fixture.c
  ${CMAKE_SOURCE_DIR}/runtime/latino.c
  ${CMAKE_SOURCE_DIR}/runtime/libs/paquete.c
)
target_include_directories(test_paquete_fixture PRIVATE ${CMAKE_SOURCE_DIR}/runtime)
set_target_properties(test_paquete_fixture PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_executable(test_lib_paquete test_lib_paquete.cpp)
add_test(
  NAME test_lib_paquete
  COMMAND test_lib_paquete
          $<TARGET_FILE:latino>
          "${CMAKE_SOURCE_DIR}/runtime"
          "${CMAKE_CURRENT_BINARY_DIR}"
          $<TARGET_FILE:test_paquete_fixture>
)
if(LATINO_LLVM_BACKEND)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/llvm")
  add_test(
    NAME test_lib_paquete_llvm
    COMMAND test_lib_paquete
            $<TARGET_FILE:latino>
            "${CMAKE_SOURCE_DIR}/runtime"
            "${CMAKE_CURRENT_BINARY_DIR}/llvm"
            $<TARGET_FILE:test_paquete_fixture>
            llvm
  )
endif()

add_suite20(test_incluir)
add_suite20(test_modulos_e2e)
add_suite20(test_modulos_multi)
add_suite20(test_poo_e2e)
add_suite20(test_genericos_e2e)
add_suite20(test_ffi_e2e)

