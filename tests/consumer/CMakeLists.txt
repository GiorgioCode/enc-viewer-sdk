# CMakeLists.txt — Consumidor mínimo externo para probar la exportación del paquete instalado.
# No recibe rutas a fuentes privados ni depende de GLFW; sólo conoce enc::viewer.
cmake_minimum_required(VERSION 3.16)
project(enc_package_consumer LANGUAGES CXX)
find_package(EncSDK 0.2 REQUIRED CONFIG)
add_executable(package-consumer main.cpp)
target_link_libraries(package-consumer PRIVATE enc::viewer)
