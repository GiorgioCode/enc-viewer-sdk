# CMakeLists.txt — Biblioteca C++17 instalable y consumidores opcionales de ENC Viewer SDK.
# Los proyectos receptores sólo necesitan GDAL, OpenGL y GLU; GLFW/EGL se solicitan al activar ejemplos/pruebas.
cmake_minimum_required(VERSION 3.16)
project(enc_sdk VERSION 0.2.0 DESCRIPTION "Biblioteca integrable para visualizar ENC S-57"
    HOMEPAGE_URL "https://github.com/GiorgioCode/enc-viewer-sdk" LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Usar un directorio de compilación separado: cmake -S . -B build/library")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# La configuración predeterminada construye exclusivamente la biblioteca, también con add_subdirectory.
option(ENC_BUILD_EXAMPLES "Compilar los ejemplos de escritorio GLFW" OFF)
option(ENC_BUILD_TOOLS "Compilar el indexador de línea de comandos" OFF)
option(ENC_BUILD_TESTS "Compilar las pruebas de contrato e integración" OFF)
option(ENC_BUILD_RENDER_TESTS "Incluir las pruebas gráficas EGL cuando ENC_BUILD_TESTS está activo" ON)
find_package(GDAL 3.0 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
if(NOT TARGET OpenGL::GLU)
    message(FATAL_ERROR "Se requiere GLU para teselar geometrías. En Ubuntu/Debian instalar libglu1-mesa-dev.")
endif()

# Recurso mundial y versión generados: ninguna ruta del checkout se necesita durante la ejecución.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS assets/world-land.geojson assets/enc-labels.rle)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/world-land.geojson" ENC_WORLD_JSON)
configure_file(cmake/world_data.h.in generated/private/world_data.h @ONLY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/enc-labels.rle" ENC_FONT_BYTES HEX)
string(REGEX REPLACE "(..)" "0x\\1," ENC_FONT_BYTES "${ENC_FONT_BYTES}")
configure_file(cmake/font_data.h.in generated/private/font_data.h @ONLY)
configure_file(cmake/version.h.in generated/include/enc/version.h @ONLY)
add_library(enc-viewer STATIC
    src/contacts.cpp
    src/detail/contact_renderer.cpp
    src/portrayal.cpp
    src/navigation.cpp
    src/streaming.cpp
    src/detail/spatial_index.cpp
    src/detail/mosaic.cpp
    src/detail/text_renderer.cpp
    src/runtime.cpp
    src/catalog.cpp
    src/viewer.cpp
    src/overlays.cpp
    src/render.cpp
    src/symbols.cpp
    src/detail/camera.cpp
    src/detail/chart_reader.cpp
    src/detail/chart_renderer.cpp
    src/detail/chart_style.cpp
    src/detail/display_theme.cpp
    src/detail/geometry.cpp
    src/detail/nautical_symbols.cpp
    src/detail/overlay_renderer.cpp
    src/detail/picking.cpp
    src/detail/radar_overlay.cpp
    src/detail/radar_scene.cpp
    src/detail/raster_overlay.cpp
    src/detail/render_primitives.cpp
    src/detail/symbol_catalogue.cpp)
add_library(enc::viewer ALIAS enc-viewer)
set_target_properties(enc-viewer PROPERTIES
    EXPORT_NAME viewer
    POSITION_INDEPENDENT_CODE ON
    CXX_EXTENSIONS OFF)
target_compile_features(enc-viewer PUBLIC cxx_std_17)
target_include_directories(enc-viewer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated/private")
target_link_libraries(enc-viewer PRIVATE GDAL::GDAL OpenGL::GL OpenGL::GLU Threads::Threads)
if(MSVC)
    target_compile_options(enc-viewer PRIVATE /W4)
else()
    target_compile_options(enc-viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ENC_BUILD_TOOLS)
    add_executable(enc-index examples/minimal/index.cpp)
    target_link_libraries(enc-index PRIVATE enc::viewer)
endif()
if(ENC_BUILD_EXAMPLES)
    find_package(glfw3 3.3 REQUIRED)
    add_subdirectory(examples/viewer)
    add_subdirectory(examples/advanced)
    add_executable(enc-minimal examples/minimal/opengl.cpp)
    target_link_libraries(enc-minimal PRIVATE enc::viewer glfw OpenGL::GL)
endif()

# El paquete exportado resuelve el mismo target que add_subdirectory, sin GLFW ni rutas privadas.
install(TARGETS enc-viewer EXPORT EncSDKTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Development)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/include/enc/version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/enc COMPONENT Development)
install(EXPORT EncSDKTargets NAMESPACE enc:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EncSDK
    COMPONENT Development)
configure_package_config_file(cmake/EncSDKConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/EncSDKConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EncSDK)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/EncSDKConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/EncSDKConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/EncSDKConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EncSDK COMPONENT Development)
install(FILES assets/README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/enc-viewer-sdk
    RENAME WORLD-PROVENANCE.md COMPONENT Development)
install(FILES THIRD_PARTY_NOTICES.md docs/paquete-instalado.md assets/FONT-LICENSE.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/enc-viewer-sdk COMPONENT Development)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/enc-viewer-sdk COMPONENT Development)

if(ENC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# La documentación gráfica se genera sólo a pedido, sin añadir dependencias al consumidor.
find_program(ENC_PLANTUML plantuml)
if(ENC_PLANTUML)
    add_custom_target(enc-diagrams
        COMMAND ${CMAKE_COMMAND} -E env JAVA_TOOL_OPTIONS=-Djava.awt.headless=true
            ${ENC_PLANTUML} -charset UTF-8 -tsvg
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/diagrams/architecture.puml"
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/diagrams/loading.puml"
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/diagrams/rendering.puml"
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/diagrams/lifecycle.puml"
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/diagrams/streaming.puml"
        COMMENT "Generando diagramas de integración en SVG")
endif()

# CPack pertenece al proyecto principal; incluir el SDK no agrega targets de empaquetado al anfitrión.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CPACK_VERBATIM_VARIABLES YES)
    set(CPACK_PACKAGE_NAME enc-viewer-sdk)
    set(CPACK_PACKAGE_VENDOR GiorgioCode)
    set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_CHECKSUM SHA256)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Biblioteca C++17 para visualizar cartas ENC S-57")
    set(CPACK_GENERATOR TGZ)
    set(CPACK_SOURCE_GENERATOR TGZ)
    set(CPACK_PACKAGE_FILE_NAME "enc-viewer-sdk-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_SOURCE_PACKAGE_FILE_NAME "enc-viewer-sdk-${PROJECT_VERSION}-source")
    set(CPACK_SOURCE_IGNORE_FILES
        "/\\.git/;/build[^/]*/;/install[^/]*/;/\\.venv/;/__pycache__/;\\.pyc$;\\.ppm$;/CMakeUserPresets\\.json$;\\.tar\\.gz$")
    include(CPack)
endif()
