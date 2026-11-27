# package_test.cmake — Instala, mueve el prefijo y compila un consumidor sin acceso a fuentes privadas.
# La mudanza detecta rutas absolutas de instalación que romperían la redistribución del paquete.
function(check_step)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Falló el consumidor del paquete: ${ARGV}\n${output}\n${errors}")
    endif()
    message(STATUS "${output}")
endfunction()
set(config_args)
if(ENC_CONFIG)
    list(APPEND config_args --config "${ENC_CONFIG}")
endif()
set(prefix "${ENC_BINARY}/tests/package-prefix")
set(relocated "${ENC_BINARY}/tests/package-relocated")
set(consumer "${ENC_BINARY}/tests/package-consumer")
file(REMOVE_RECURSE "${prefix}" "${relocated}" "${consumer}")
check_step("${CMAKE_COMMAND}" --install "${ENC_BINARY}" --prefix "${prefix}" ${config_args})
file(RENAME "${prefix}" "${relocated}")
check_step("${CMAKE_COMMAND}" -S "${ENC_SOURCE}/tests/consumer" -B "${consumer}"
    "-DCMAKE_PREFIX_PATH=${relocated}" "-DCMAKE_CXX_COMPILER=${ENC_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${ENC_CONFIG}")
check_step("${CMAKE_COMMAND}" --build "${consumer}" ${config_args} --parallel 2)
check_step("${consumer}/package-consumer" "${ENC_SOURCE}/sample-charts")
