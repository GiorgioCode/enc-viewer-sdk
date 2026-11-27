# Integración avanzada: enc-showcase

Este consumidor compacto utiliza exclusivamente `<enc/enc.h>` y `enc::viewer`.
Reúne carga progresiva, mosaico, Mercator y giro, reglas de presentación, imágenes,
PPI y múltiples contactos AIS/radar. La integración está en [main.cpp](main.cpp);
el sensor sintético compartido está en
[`contact_scenario.cpp`](../common/contact_scenario.cpp).

## Compilar y ejecutar

Desde la raíz del repositorio, con las dependencias de la biblioteca y GLFW:

```sh
cmake --preset dev
cmake --build --preset dev
./build/dev/examples/advanced/enc-showcase ./sample-charts
```

Acepta una carpeta de celdas o una base `.000`. Sin argumento, busca
`sample-charts` en el directorio de trabajo. No necesita servicios externos: el
TIFF, el PPI y los informes de contactos se generan para la demostración.

## Qué muestra cada parte

| Función | Contrato que utiliza |
| --- | --- |
| Índice y carga por región | `Catalog`, `ChartStreamer::requestView`, `poll` y `StreamInfo`. |
| Mosaico y presentación | `Presentation`, categorías y regla de ejemplo para `COALNE`. |
| Navegación | `Camera`, Mercator/local, rotación, zoom y vistas anteriores/siguientes. |
| Consulta múltiple | `pickAll`, `select`, `FeatureInfo::decodedAttributes`. |
| Imágenes y vídeo radar | `addDemoGeoTiff`, `configureRadar` y `advance`. |
| Contactos | `upsertContacts`, buque propio, vectores, filtros, reconocimiento y selección. |
| Cierre | Destrucción del trabajador y `releaseGraphicsResources` con contexto vigente. |

La transferencia de una nueva colección puede invalidar los identificadores de
objetos ENC. El ejemplo conserva los contactos y obtiene otra vez la información
de cartas cuando se confirma la revisión. Los sensores sintéticos pertenecen al
ejemplo; el motor recibe los mismos informes que enviaría un adaptador real.

## Controles

| Entrada | Acción |
| --- | --- |
| Arrastre izquierdo / rueda | Desplazar / acercar o alejar alrededor del cursor. |
| Clic / Ctrl+clic | Seleccionar contacto u objeto / recorrer los objetos ENC del punto. |
| `Q` / `E` | Girar el mapa. |
| `M` | Alternar Mercator y proyección local. |
| `H` / `Y` | Vista anterior / siguiente. |
| `D` | Alternar categorías Base, Standard y All. |
| `N` | Alternar día/noche. |
| `F6` | Preferir nombres nacionales. |
| `F8` / `F9` | Reducir/aumentar etiquetas. |
| `B` / `S` | Mostrar contactos / filtrar AIS dormidos. |
| `U` | Alternar vectores verdaderos/relativos. |
| `A` | Elegir símbolo AIS/radar para contactos asociados. |
| `G` | Reconocer el contacto seleccionado. |
| `I` / `R` | Mostrar u ocultar TIFF / PPI. |
| `F` | Encuadrar las cartas cargadas; usa la primera celda del catálogo si la escena está vacía. |
| `Esc` | Cerrar. |

Para una comprobación automática de pocos cuadros:

```sh
./build/dev/examples/advanced/enc-showcase ./sample-charts \
    --smoke-frames 4 --screenshot ./build/dev/showcase.ppm
```

También puede compilarse como consumidor del paquete instalado:

```sh
cmake -S examples/advanced -B build/showcase-consumer -DCMAKE_PREFIX_PATH=/ruta/al/paquete
cmake --build build/showcase-consumer --parallel 2
./build/showcase-consumer/enc-showcase ./sample-charts
```

El directorio hermano `examples/common` forma parte del código del ejemplo;
mantenerlo al trasladar este consumidor a otra carpeta.

El ejemplo explica la integración. Los detalles de comportamiento están en
[visualización](../../docs/visualizacion.md),
[carga progresiva](../../docs/carga-progresiva.md) y
[contactos](../../docs/contactos.md). Para un consumidor con paneles, diálogos y
catálogo gráfico, usar el [visor completo](../viewer/README.md).
