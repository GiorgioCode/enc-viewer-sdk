# Mantenimiento y distribución

El proyecto se distribuye como biblioteca estática C++17. Los ejemplos son
consumidores de su API pública; las pruebas verifican integración por fuentes y
por paquete instalado. [CONTRIBUTING.md](../CONTRIBUTING.md) describe el entorno.

## Opciones y perfiles

| Opción | Predeterminado | Efecto |
| --- | --- | --- |
| `ENC_BUILD_EXAMPLES` | `OFF` | Ejemplos de escritorio; requiere GLFW. |
| `ENC_BUILD_TOOLS` | `OFF` | Indexador sin ventana `enc-index`. |
| `ENC_BUILD_TESTS` | `OFF` | Pruebas de CPU, integración y verificaciones disponibles. |
| `ENC_BUILD_RENDER_TESTS` | `ON` | Añade EGL a las pruebas si `ENC_BUILD_TESTS=ON`. |

Los perfiles CMake ≥ 3.21 son `library` (sólo biblioteca Release), `dev` (Debug
completo), `ci` (Release completo) y `cpu` (pruebas sin GLFW ni EGL). La compilación
manual admite CMake ≥ 3.16. Los perfiles usan Makefiles en directorios separados
bajo `build/`; las preferencias personales van en `CMakeUserPresets.json`, ignorado por Git.

## Verificación de cambios

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
python3 tools/format_cpp.py --check
python3 tools/check_repository.py
```

La [matriz CI](../.github/workflows/ci.yml) ejecuta GCC y Clang sobre Ubuntu,
pruebas EGL con Mesa, el ejemplo GLFW sobre Xvfb y empaquetado. Un trabajo separado
compila sólo la biblioteca. Su ejecución remota depende de que el workflow esté
publicado y habilitado en GitHub; los comandos anteriores permiten verificar localmente.

Las dependencias nativas se resuelven desde el sistema, sin descargarlas durante
CMake. Al actualizar GDAL, Mesa o un compilador, ejecutar las pruebas y comprobar
los contextos del anfitrión. Dependabot propone actualizaciones de GitHub Actions;
no modifica bibliotecas del sistema. El formato utiliza clang-format 18.

## Versión y compatibilidad

`project(... VERSION ...)` en [CMakeLists.txt](../CMakeLists.txt) es la única fuente
de versión. CMake genera `<enc/version.h>` y `enc::version()` usa esa definición.
Actualizar el [registro de cambios](../CHANGELOG.md) al cambiar de versión.

Durante la serie `0.x`, `find_package(EncSDK 0.2 REQUIRED CONFIG)` acepta versiones
de la misma serie menor, por ejemplo `0.2.1`, pero no `0.3.0`. Esto describe la
selección del paquete; no promete ABI estable entre compiladores, dependencias o
versiones. Recompilar el anfitrión con una configuración compatible.

## Crear paquetes para revisión

Desde la raíz del repositorio:

```sh
cmake --preset library
cmake --build --preset library
cpack --config build/library/CPackConfig.cmake -B build/library/dist
cpack --config build/library/CPackSourceConfig.cmake -B build/library/dist
```

CPack genera archivos `.tar.gz` y sus sumas SHA-256 para comprobar integridad.

El paquete binario contiene la biblioteca, cabeceras, configuración CMake y avisos
MIT/terceros y la licencia del atlas de texto; no incluye dependencias del sistema ni cartas de prueba. La
[guía instalada](paquete-instalado.md) explica su consumo. Debe identificarse el
sistema, arquitectura y toolchain con que se construyó antes de compartirlo.

El archivo de fuentes contiene código, ejemplos, pruebas, documentos, recursos y
cartas de ejemplo con sus avisos. Excluye `.git`, compilaciones e instalaciones
locales. Probar una extracción limpia antes de adjuntarlo a una versión.

Para preparar una publicación: ejecutar la validación completa, revisar los avisos
de los datos, confirmar la versión y su registro de cambios y verificar ambos
paquetes. Etiquetas y publicaciones se realizan explícitamente por los mantenedores;
el workflow no publica versiones automáticamente.

## Recursos incorporados

El mundo y el atlas se convierten en cabeceras privadas durante CMake. La
aplicación no requiere rutas al checkout para utilizarlos. Para modificar el
repertorio de texto, ver [`generate_text_atlas.py`](../tools/generate_text_atlas.py)
y la [procedencia de recursos](../assets/README.md). Regenerar el atlas requiere
Pillow con soporte FreeType y la fuente original; esas dependencias no pertenecen a la biblioteca en
ejecución. Conservar siempre [`FONT-LICENSE.txt`](../assets/FONT-LICENSE.txt).

Los cambios a mosaico, presentación o contactos deben acompañarse de contratos
actualizados y verificaciones CPU/gráficas pertinentes. La trazabilidad normativa
de [contactos](contactos.md) describe el subconjunto implementado; no ampliarla
sólo por semejanza de una captura.
