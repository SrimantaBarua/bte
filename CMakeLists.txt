cmake_minimum_required(VERSION 3.0)

project(bte)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(FC REQUIRED fontconfig)
pkg_check_modules(FT2 REQUIRED freetype2)

include_directories(include)
file(GLOB SOURCES src/*.c)
add_executable(bte ${SOURCES})
target_include_directories(bte PUBLIC ${GLFW_INCLUDE_DIRS} ${FC_INCLUDE_DIRS} ${FT2_INCLUDE_DIRS})
target_link_libraries(bte ${GLFW_LIBRARIES} ${FC_LIBRARIES} ${FT2_LIBRARIES} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
target_compile_options(bte PUBLIC ${GLFW_CFLAGS_OTHER} ${FC_CFLAGS_OTHER} ${FT2_CFLAGS_OTHER} -g -O3)
