# Alcance de la prueba de concepto

Esta biblioteca permite integrar la visualización ENC en una aplicación anfitriona.
El repositorio incluye el motor, sus recursos y ejemplos de implementación.

## Implementado

Indexación explícita de carpetas y bases, metadatos DSID, actualizaciones GDAL,
consultas de cobertura, inventario JSON, lectura y teselación, selección y
atributos, cámara, simbología vectorial por atributos, presentación día/noche,
varias imágenes TIFF, radar simulado/externo y composición en contexto ajeno.
Incluye mosaico por cobertura, control de escala, reglas configurables, etiquetas
UTF-8, profundidad condicional básica, proyección Mercator/local con giro e
historial, carga progresiva con caché RAM y contactos múltiples AIS/radar.

La simbología utiliza identidad de celda por ruta completa, referencias LNAM_REFS
para asociar equipos y asociación por posición como alternativa. Incluye sectores
luminosos esquemáticos, rocas por nivel de agua, controles de presentación y
un catálogo de 44 ejemplos visuales.

## Límites para planificar la integración

| Área | Límite actual |
| --- | --- |
| OpenGL | Escritorio 2.1 o superior con compatibilidad. No OpenGL ES, WebGL ni core. |
| Proyección | Equirectangular local y Mercator hasta ±85.05112878°, con giro y antimeridiano. Sin proyección polar ni transformación geodésica de navegación. |
| Lectura | Síncrona o trabajador `ChartStreamer`; cancelación entre celdas, sin interrumpir GDAL dentro de una celda. API del anfitrión serializada. |
| Colecciones grandes | Caché LRU CPU con presupuesto estimado; escena y temporales consumen RAM adicional. Sin caché persistente ni motor por teselas. |
| Mosaico | Cobertura S-57 y preferencia por escala/propósito. Sin catálogo remoto, actualización automática ni validación de continuidad hidrográfica. |
| Índice | Recorrido y refresco explícitos; JSON informativo, sin caché persistente recargable ni vigilante automático. |
| Símbolos | Subconjunto visual propio, sin biblioteca IHO S-52 completa, presentación condicional completa ni auditoría de conformidad. |
| Texto | Atlas UTF-8 con latín extendido, griego, cirílico y puntuación; glifo alternativo para otros caracteres. Sin shaping, CJK ni escritura de derecha a izquierda. |
| Escala y seguridad | Escala local según calibración física aportada, SCAMIN/SCAMAX, contornos y peligros básicos. Sin marea, calado, margen bajo quilla o vigilancia de ruta. |
| Luces | Arcos esquemáticos y una luz asociada por estructura en el modelo actual; sin simulación de períodos o alcance luminoso físico. |
| GeoTIFF | Una previsualización de hasta 2048×2048 por imagen; WGS84 como destino. Dividir rasters que crucen antimeridiano o polos. |
| Radar | Un PPI por escena. Simulación visual, sin propagación, sombras de costa ni dinámica naval. El anfitrión aporta los ecos externos geográficos. |
| Contactos | AIS/radar/asociados con estados, vectores e historial; el anfitrión aporta tracking y asociación. Sin decodificador AIS/NMEA, ARPA, alertas sonoras ni gestión de alertas de puente. |
| CPA/TCPA | Aproximación local de velocidad constante; sin predicción de maniobra, curvas de giro o evaluación anticolisión validada. |
| Presentación OMI | Subconjunto de símbolos de contactos y buque propio, documentado por tabla de trazabilidad; sin conformidad completa ni ensayos/certificación. |
| Recursos gráficos | Una instancia usa un contexto a la vez y requiere liberación explícita antes de destruirlo o transferirlo. |
| Estado GL avanzado | Contrato de compatibilidad probado con FBO, shader, texturas y PBO. No usarlo dentro de transform feedback, listas o primitivas abiertas del anfitrión. |
| Distribución | Biblioteca estática y API C++17 inicial. No ABI C, bindings Python ni promesa de ABI binaria entre toolchains/versiones. |
| Plataformas | Verificado en Linux/Mesa; otros controladores y sistemas requieren validación de integración. |

Las cartas incluidas sirven como fixtures de prueba. La aplicación no ofrece
funciones de navegación certificada, cálculo de derrota o validación operativa
para reemplazar un sistema ECDIS.

Los contratos y opciones se detallan en [visualización](visualizacion.md),
[carga progresiva](carga-progresiva.md) y [contactos](contactos.md). El
[ejemplo avanzado](../examples/advanced/README.md) permite explorar su integración.

El código y la documentación propios se distribuyen bajo [MIT](../LICENSE).
Los datos externos conservan sus [avisos y condiciones](../THIRD_PARTY_NOTICES.md).
