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
endmacro()

add_suite20(test_funciones_base)
add_suite20(test_lib_cadena)
add_suite20(test_lib_lista)
add_suite20(test_lib_dic)
add_suite20(test_lib_mate)
add_suite20(test_lib_sis)
add_suite20(test_lib_archivo)
add_suite20(test_incluir)
add_suite20(test_poo_e2e)

