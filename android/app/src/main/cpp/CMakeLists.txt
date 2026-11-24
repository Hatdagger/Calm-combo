cmake_minimum_required(VERSION 3.22.1)
project(calm C)

# the portrait front end, and the Neo Geo CD's rule files built unchanged
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../core)
set(NG ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../neogeo-cd/src)

add_library(calm SHARED
    jni.c
    ${CORE}/calm.c ${CORE}/gfx.c ${CORE}/scenery.c
    ${NG}/board.c ${NG}/puzzle.c)
target_include_directories(calm PRIVATE ${CORE} ${NG})
set_property(TARGET calm PROPERTY C_STANDARD 99)
target_compile_options(calm PRIVATE -O2 -Wall)
target_link_libraries(calm jnigraphics log android)
