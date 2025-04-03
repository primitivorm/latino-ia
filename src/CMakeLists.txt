add_executable(latino
  main.cpp
  lexer.cpp
  parser.cpp
  compiler.cpp
  interpreter.cpp
)

target_include_directories(latino PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)