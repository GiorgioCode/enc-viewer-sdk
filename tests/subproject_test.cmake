# subproject_test.cmake — Compila una aplicación anfitriona con add_subdirectory y ejecuta la API pública.
# Usa un árbol independiente para que las opciones del desarrollo no oculten los valores por defecto.
function(check_step)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Falló add_subdirectory: ${ARGV}\n${output}\n${errors}")
    endif()
    message(STATUS "${output}")
endfunction()
set(consumer "${ENC_BINARY}/tests/subproject-consumer")
file(REMOVE_RECURSE "${consumer}")
check_step("${CMAKE_COMMAND}" -S "${ENC_SOURCE}/tests/subproject" -B "${consumer}"
    "-DENC_SOURCE=${ENC_SOURCE}" "-DCMAKE_CXX_COMPILER=${ENC_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${ENC_CONFIG}")
set(config_args)
if(ENC_CONFIG)
    list(APPEND config_args --config "${ENC_CONFIG}")
endif()
check_step("${CMAKE_COMMAND}" --build "${consumer}" ${config_args} --parallel 2)
check_step("${consumer}/subproject-consumer" "${ENC_SOURCE}/sample-charts")
