# Ejemplos de implementación

Cada ejemplo utiliza las cabeceras públicas y `enc::viewer`. Ejecutar los siguientes
comandos desde la raíz del repositorio después de compilar el perfil `dev`.

| Ejemplo | Objetivo | Ejecución |
| --- | --- | --- |
| [Indexador](minimal/index.cpp) | Indexar, exportar inventario y cargar geometrías sin ventana. | `./build/dev/enc-index ./sample-charts ./build/dev/inventory.json` |
| [Ventana mínima](minimal/opengl.cpp) | Contexto, bucle, navegación con el mouse y cierre. | `./build/dev/enc-minimal ./sample-charts` |
| [Visor completo](viewer/README.md) | Interfaz, selección, día/noche, radar y múltiples imágenes. | `./build/dev/examples/viewer/enc-viewer-demo ./sample-charts --radar --demo-tiff` |

El indexador requiere `ENC_BUILD_TOOLS=ON`; las ventanas, `ENC_BUILD_EXAMPLES=ON`
y GLFW. El visor completo también puede compilarse contra el paquete instalado.
La [guía de integración](../docs/integracion.md) describe responsabilidades y contratos.
