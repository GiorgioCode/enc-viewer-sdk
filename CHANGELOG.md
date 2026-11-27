# Registro de cambios

## 0.1.0 — en preparación

Versión inicial de la biblioteca y sus ejemplos de integración.

### Biblioteca

- API C++17 `enc::viewer`, sin dependencia de GLFW.
- Índice recursivo, metadatos y actualizaciones S-57, consultas e inventario JSON.
- Carga transaccional con progreso, cancelación y modo parcial explícito.
- Mapa mundial incorporado, cámara, selección y atributos de objetos.
- Catálogo vectorial por atributos, variantes de rocas y sectores luminosos.
- Presentación día/noche y parámetros de profundidad y simbología.
- Varias capas GeoTIFF y PPI simulado o alimentado por ecos externos.
- Integración en framebuffer ajeno y gestión explícita de recursos GPU.

### Integración y mantenimiento

- Configuración predeterminada que compila sólo la biblioteca.
- Target instalable `enc::viewer`, paquete `EncSDK` relocatable y versión generada desde CMake.
- Ejemplos de lectura, ventana mínima y visor completo.
- Presets de biblioteca, desarrollo, CI y pruebas sin EGL.
- GitHub Actions para GCC/Clang, validación de formato, enlaces y empaquetado.
- Guías de integración, contribución, mantenimiento, diagramas y capturas reales.
- Licencia MIT para el código propio y avisos separados de datos y dependencias.
