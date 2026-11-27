# CMakeLists.txt — Consumidor avanzado compilable con el SDK o como proyecto independiente.
# La API pública resuelve cartas, presentación, contactos y caché; GLFW pertenece sólo al anfitrión.
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(enc_showcase_consumer LANGUAGES CXX)
    find_package(EncSDK 0.2 REQUIRED CONFIG)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 3.3 REQUIRED)
endif()
add_executable(enc-showcase main.cpp ../common/contact_scenario.cpp)
target_link_libraries(enc-showcase PRIVATE enc::viewer glfw OpenGL::GL)
if(NOT MSVC)
    target_compile_options(enc-showcase PRIVATE -Wall -Wextra -Wpedantic)
endif()
