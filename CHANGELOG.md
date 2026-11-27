# Registro de cambios

## 0.2.0 — en preparación

### Visualización y lectura

- Mosaico automático por cobertura S-57 y escala, recorte de solapamientos y
  consulta de las celdas candidatas.
- SCAMIN/SCAMAX, categorías, reglas configurables, escala gráfica y sobreescala.
- Contorno de seguridad por celda y presentación básica de sondas y peligros.
- Texto UTF-8 con atlas incorporado, tamaño/densidad, prioridad y nombres nacionales.
- Cámara Mercator/local, giro, antimeridiano, historial y encuadre de objetos.
- Consulta de múltiples objetos y diccionarios descriptivos de clases/atributos.
- `ChartStreamer`: lectura CPU por región, revisiones y caché LRU en RAM estimada.

### Contactos y ejemplos

- Contactos múltiples AIS, radar rastreado y asociados, independientes de las ENC.
- Estados, selección, pérdida por tiempo, reconocimiento, filtros, vectores e historial.
- Buque propio y evaluación CPA/TCPA local de velocidad constante.
- Presentación parcial referenciada a OMI, con tabla de trazabilidad y límites explícitos.
- Consumidor `enc-showcase` y opciones `--contacts`/`--streaming` del visor completo.
- Guías específicas de visualización, carga progresiva y contactos, diagramas y capturas.
- Pruebas de navegación, presentación, carga progresiva y contactos; avisos del atlas.

La API y las estructuras públicas se amplían: recompilar consumidores y solicitar
`find_package(EncSDK 0.2 REQUIRED CONFIG)`. No se promete compatibilidad binaria con 0.1.

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
