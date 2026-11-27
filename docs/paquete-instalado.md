# Usar el paquete instalado de ENC Viewer SDK

Este directorio acompaña la biblioteca estática y sus cabeceras públicas. La API
requiere C++17, GDAL 3.0 o superior con controlador S57 y soporte GEOS, OpenGL de escritorio y GLU.
Para dibujar, la aplicación aporta un contexto OpenGL 2.1 o superior de compatibilidad.

El paquete no contiene ni instala las bibliotecas de terceros. GLFW y EGL no son
dependencias del SDK instalado. GDAL y sus datos de proyección deben estar disponibles
en el entorno donde se utilice la biblioteca.

## CMake

```cmake
cmake_minimum_required(VERSION 3.16)
project(mi_aplicacion LANGUAGES CXX)
find_package(EncSDK 0.2 REQUIRED CONFIG)
add_executable(mi_aplicacion main.cpp)
target_link_libraries(mi_aplicacion PRIVATE enc::viewer)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/ruta/al/prefijo
cmake --build build --parallel
```

El target exportado incorpora las dependencias transitivas del archivo estático.
No agregar rutas privadas ni enlazar manualmente archivos de implementación.

## C++

```cpp
#include <enc/enc.h>

enc::Viewer viewer;
viewer.resize(1024, 768);
const auto loaded = viewer.loadFolder("/datos/enc");
// Comprobar loaded, loaded.message y loaded.diagnostics.
viewer.setTheme(enc::Theme::Night);

// Con el contexto vigente, en el bucle del anfitrión:
viewer.advance(dt);
const auto drawn = viewer.render({0, 0, framebufferWidth, framebufferHeight});
// Comprobar drawn. El anfitrión presenta los buffers.

// Antes de destruir o reemplazar el contexto:
viewer.releaseGraphicsResources();
```

La biblioteca incorpora el mapa mundial y el atlas de texto; no necesita el árbol
de fuentes, fuentes instaladas ni FreeType en ejecución.
Los archivos ENC y las imágenes del anfitrión se proporcionan como rutas externas.
La biblioteca previsualiza cartas y capas, y no constituye un sistema de navegación certificado.

Documentación, ejemplos y fuentes completos:
https://github.com/GiorgioCode/enc-viewer-sdk

El código propio utiliza la licencia MIT, incluida como `LICENSE`. Los datos
incorporados mantienen los avisos de `THIRD_PARTY_NOTICES.md`, `WORLD-PROVENANCE.md`
y `FONT-LICENSE.txt`.
Los archivos estáticos se deben utilizar con un entorno de compilación compatible;
la versión 0.2 aún no ofrece estabilidad ABI entre versiones o toolchains.

La API incluye `ChartStreamer` para lectura progresiva con caché RAM y
`Contact`/`ContactSettings` para AIS y radar rastreado. Las llamadas a `poll(viewer)`
se realizan desde el hilo anfitrión; una confirmación de celdas invalida los
identificadores de objetos ENC, pero conserva contactos e imágenes. Los informes
de contactos usan el reloj local `contactTime()`; el SDK no recibe protocolos de
sensores ni acredita una presentación náutica certificada. Consultar las guías
de visualización, carga progresiva y contactos del repositorio para sus contratos.
