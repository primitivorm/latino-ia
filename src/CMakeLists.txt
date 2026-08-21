# Valores horneados para que el driver localice el compilador de C y el runtime.
if(MSVC)
  set(LATINO_CC_ESTILO "msvc")
  # CMAKE_GENERATOR_INSTANCE es la ruta de la instancia de VS (generador VS).
  set(LATINO_VSDEVCMD "${CMAKE_GENERATOR_INSTANCE}/Common7/Tools/VsDevCmd.bat")
else()
  set(LATINO_CC_ESTILO "gnu")
  set(LATINO_VSDEVCMD "")
endif()
set(LATINO_CC "${CMAKE_C_COMPILER}")
set(LATINO_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/runtime")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

add_executable(latino
  main.cpp
  lexer.cpp
  ast.cpp
  ast_impresor.cpp
  parser.cpp
  analizador_semantico.cpp
  compiler.cpp
  invocador_c.cpp
  interpreter.cpp
)

target_include_directories(latino PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_BINARY_DIR})

# Backend LLVM (ver input/PLAN_LLVM.md). Solo se agrega si el CMakeLists raíz
# encontró una instalación de LLVM 17.x compatible (LATINO_LLVM_BACKEND).
if(LATINO_LLVM_BACKEND)
  target_sources(latino PRIVATE compiler_llvm.cpp invocador_llvm.cpp)
  target_compile_definitions(latino PRIVATE LATINO_CON_LLVM)

  # Componentes mínimos para la Fase L1 (construir un Module y emitir un
  # objeto nativo vía TargetMachine). Fases futuras añadirán componentes:
  # 'irreader' en la Fase L2 (importar runtime_abi.ll), 'orcjit' en la
  # Fase L10 (modo --jit).
  llvm_map_components_to_libnames(LATINO_LLVM_LIBS
    core support nativecodegen native mc object target)

  target_include_directories(latino PRIVATE ${LLVM_INCLUDE_DIRS})
  separate_arguments(LATINO_LLVM_DEFS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
  target_compile_definitions(latino PRIVATE ${LATINO_LLVM_DEFS})
  target_link_libraries(latino PRIVATE ${LATINO_LLVM_LIBS})
endif()
