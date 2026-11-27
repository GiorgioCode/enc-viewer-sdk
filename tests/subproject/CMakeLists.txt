# CMakeLists.txt — Anfitrión de prueba que integra el SDK con sus opciones predeterminadas.
# Detecta si add_subdirectory agrega herramientas, ejemplos, pruebas o empaquetado no solicitados.
cmake_minimum_required(VERSION 3.16)
project(enc_subproject_consumer LANGUAGES CXX)
add_subdirectory("${ENC_SOURCE}" sdk)
foreach(unwanted IN ITEMS enc-index enc-minimal enc-viewer-demo enc-core-test package)
    if(TARGET ${unwanted})
        message(FATAL_ERROR "La integración predeterminada agrega el target ajeno ${unwanted}")
    endif()
endforeach()
add_executable(subproject-consumer ../consumer/main.cpp)
target_link_libraries(subproject-consumer PRIVATE enc::viewer)
