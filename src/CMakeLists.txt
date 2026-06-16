add_executable(latino
  main.cpp
  lexer.cpp
  ast.cpp
  ast_impresor.cpp
  parser.cpp
  analizador_semantico.cpp
  compiler.cpp
  interpreter.cpp
)

target_include_directories(latino PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)