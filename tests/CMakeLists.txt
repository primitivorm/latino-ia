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

# Utilidad de depuración (no es una prueba): vuelca los tokens de un .lat.
add_executable(lexer_dump
  lexer_dump.cpp
  ${CMAKE_SOURCE_DIR}/src/lexer.cpp
)
target_include_directories(lexer_dump PRIVATE ${CMAKE_SOURCE_DIR}/include)
