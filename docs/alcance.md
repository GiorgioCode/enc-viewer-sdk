# Alcance de la prueba de concepto

Esta biblioteca permite integrar la visualización ENC en una aplicación anfitriona.
El repositorio incluye el motor, sus recursos y ejemplos de implementación.

## Implementado

Indexación explícita de carpetas y bases, metadatos DSID, actualizaciones GDAL,
consultas de cobertura, inventario JSON, lectura y teselación, selección y
atributos, cámara, simbología vectorial por atributos, presentación día/noche,
varias imágenes TIFF, radar simulado/externo y composición en contexto ajeno.

La simbología utiliza identidad de celda por ruta completa, referencias LNAM_REFS
para asociar equipos y asociación por posición como alternativa. Incluye sectores
luminosos esquemáticos, rocas por nivel de agua, controles de presentación y
un catálogo de 44 ejemplos visuales.

## Límites para planificar la integración

| Área | Límite actual |
| --- | --- |
| OpenGL | Escritorio 2.1 o superior con compatibilidad. No OpenGL ES, WebGL ni core. |
| Proyección | Aproximación equirectangular local. No Mercator global continuo ni cruce del antimeridiano. |
| Lectura | Síncrona; cancelación entre celdas. El anfitrión administra hilos y serialización. |
| Colecciones grandes | Se decodifican en memoria las celdas elegidas; no hay caché de geometría, paginación ni carga por teselas. |
| Índice | Recorrido y refresco explícitos; JSON informativo, sin caché persistente recargable ni vigilante automático. |
| Símbolos | Subconjunto visual propio, sin biblioteca IHO S-52 completa, presentación condicional completa ni auditoría de conformidad. |
| Luces | Arcos esquemáticos y una luz asociada por estructura en el modelo actual; sin simulación de períodos o alcance luminoso físico. |
| GeoTIFF | Una previsualización de hasta 2048×2048 por imagen; WGS84 como destino. Dividir rasters que crucen antimeridiano o polos. |
| Radar | Un PPI por escena. Simulación visual, sin propagación, sombras de costa ni dinámica naval. El anfitrión aporta los ecos externos geográficos. |
| Recursos gráficos | Una instancia usa un contexto a la vez y requiere liberación explícita antes de destruirlo o transferirlo. |
| Estado GL avanzado | Contrato de compatibilidad probado con FBO, shader, texturas y PBO. No usarlo dentro de transform feedback, listas o primitivas abiertas del anfitrión. |
| Distribución | Biblioteca estática y API C++17 inicial. No ABI C, bindings Python ni promesa de ABI binaria entre toolchains/versiones. |
| Plataformas | Verificado en Linux/Mesa; otros controladores y sistemas requieren validación de integración. |

Las cartas incluidas sirven como fixtures de prueba. La aplicación no ofrece
funciones de navegación certificada, cálculo de derrota o validación operativa
para reemplazar un sistema ECDIS.

El código y la documentación propios se distribuyen bajo [MIT](../LICENSE).
Los datos externos conservan sus [avisos y condiciones](../THIRD_PARTY_NOTICES.md).
