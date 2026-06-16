cmake_minimum_required(VERSION 3.12)
project(latino)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

# Runtime en C que usan los programas Latino generados. Se compila aquí para
# verificar que sigue siendo válido; los programas generados se enlazan con él.
add_library(latino_runtime STATIC runtime/latino.c)
target_include_directories(latino_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/runtime)

enable_testing()
add_subdirectory(tests)