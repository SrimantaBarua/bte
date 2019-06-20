cmake_minimum_required(VERSION 3.0)

project(bte)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

include_directories(include)
file(GLOB SOURCES src/*.c)
add_executable(bte ${SOURCES})
target_include_directories(bte PUBLIC ${GLFW_INCLUDE_DIRS})
target_link_libraries(bte ${GLFW_LIBRARIES} ${FC_LIBRARIES} ${CMAKE_DL_LIBS})
target_compile_options(bte PUBLIC ${GLFW_CFLAGS_OTHER} -g)
