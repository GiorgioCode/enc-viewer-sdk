# CMakeLists.txt — Contratos de CPU, renderizado opcional e integración de consumidores externos.
# GLFW no interviene; EGL sólo se busca cuando están habilitadas las pruebas gráficas.
add_executable(enc-core-test core_test.cpp)
target_link_libraries(enc-core-test PRIVATE enc::viewer)
add_test(NAME enc-cpu COMMAND enc-core-test "${enc_sdk_SOURCE_DIR}/sample-charts" "${CMAKE_CURRENT_BINARY_DIR}/cpu-data")

if(ENC_BUILD_RENDER_TESTS)
    find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
    add_executable(enc-render-test render_test.cpp test_support.cpp fixtures.cpp)
    target_link_libraries(enc-render-test PRIVATE enc::viewer GDAL::GDAL OpenGL::GL OpenGL::EGL Threads::Threads)
    add_test(NAME enc-render COMMAND enc-render-test "${enc_sdk_SOURCE_DIR}/sample-charts" "${CMAKE_CURRENT_BINARY_DIR}/captures")
    set_tests_properties(enc-render PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
endif()

# El hueco geométrico requiere inspeccionar la teselación privada; no necesita un contexto.
add_executable(enc-geometry-test geometry_test.cpp)
target_include_directories(enc-geometry-test PRIVATE "${enc_sdk_SOURCE_DIR}/src")
target_link_libraries(enc-geometry-test PRIVATE enc::viewer GDAL::GDAL OpenGL::GL)
add_test(NAME enc-geometry COMMAND enc-geometry-test)
foreach(consumer IN ITEMS package subproject)
    add_test(NAME enc-${consumer} COMMAND "${CMAKE_COMMAND}"
        "-DENC_SOURCE=${enc_sdk_SOURCE_DIR}" "-DENC_BINARY=${enc_sdk_BINARY_DIR}"
        "-DENC_COMPILER=${CMAKE_CXX_COMPILER}" "-DENC_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/${consumer}_test.cmake")
endforeach()

# Python sólo añade verificaciones de desarrollo, nunca dependencias al paquete exportado.
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND NOT MSVC)
    add_test(NAME enc-headers COMMAND "${Python3_EXECUTABLE}"
        "${enc_sdk_SOURCE_DIR}/tools/check_public_headers.py" "--compiler=${CMAKE_CXX_COMPILER}"
        "--include=${enc_sdk_SOURCE_DIR}/include" "--generated=${enc_sdk_BINARY_DIR}/generated/include")
    add_test(NAME enc-repository COMMAND "${Python3_EXECUTABLE}" "${enc_sdk_SOURCE_DIR}/tools/check_repository.py")
endif()
