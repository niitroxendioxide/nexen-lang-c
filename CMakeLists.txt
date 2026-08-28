cmake_minimum_required(VERSION 3.15)
project(LanguageVM C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(src)

# glob_recurse para que busque dentro de las carpetas de src tmb :v
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")

add_executable(nexen ${SOURCES})

