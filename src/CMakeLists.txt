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
