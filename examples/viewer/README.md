# Visor de integración

Esta aplicación utiliza exclusivamente `<enc/enc.h>` y el target `enc::viewer`.
Posee ventana, controles, callbacks y reloj; toda la lectura y visualización de
cartas y superposiciones se resuelve mediante llamadas públicas de la biblioteca.

## Ejecución

Desde `enc-viewer-sdk/`, después de compilar el proyecto principal:

```sh
./build/dev/examples/viewer/enc-viewer-demo ./sample-charts --contacts --streaming --radar --demo-tiff
./build/dev/examples/viewer/enc-viewer-demo /datos/enc /datos/ortofoto.tif --day
```

`--contacts` incorpora nueve contactos sintéticos: AIS dormido, activo, peligroso,
perdido y sin orientación; radar seguido, en adquisición y peligroso; y un
contacto asociado AIS/radar. El ejemplo suministra también datos del buque propio.
`--streaming` utiliza carga por región con caché RAM; sin ese indicador, la carga
es síncrona. Ambos indicadores son independientes. `--night` inicia de noche.

Puede abrir varias imágenes y cambiar la imagen activa mediante **ANT/SIG**.
**Subir al tope** modifica el orden de las imágenes; radar y contactos siguen encima.

| Entrada | Acción |
| --- | --- |
| Rueda sobre mapa | Zoom alrededor del cursor. |
| Arrastre izquierdo | Desplazar la cámara. |
| Clic sobre objeto | Seleccionar e inspeccionar atributos. |
| Ctrl+clic | Recorrer los objetos ENC visibles bajo el mismo punto. |
| Clic sobre contacto | Seleccionar y consultar estado, fuente, edad y datos relativos disponibles. |
| `Q` / `E` | Girar el mapa 15° a izquierda/derecha. |
| `M` | Alternar proyección local/Mercator. |
| `H` / `Y` | Vista anterior / siguiente. |
| `D` | Cambiar categoría de presentación. |
| `F6` | Preferir nombres nacionales de la carta. |
| `F8` / `F9` | Reducir/aumentar tamaño de etiquetas. |
| `O` | Introducir carpeta ENC o base `.000`. |
| `F` / `W` | Encuadrar cartas / mundo. |
| `V` | Mostrar u ocultar todas las ENC. |
| `N` | Día/noche en carta, radar e interfaz. |
| `L` | Abrir/cerrar catálogo visual de símbolos. |
| `R` / `P` | Mostrar radar / pausar. |
| `B` | Mostrar/ocultar la capa de contactos. |
| `S` | Mostrar/ocultar AIS dormidos. |
| `U` | Alternar vectores verdaderos/relativos. |
| `A` | Alternar símbolo AIS/radar para contactos asociados. |
| `G` | Reconocer el contacto seleccionado. |
| `C`, clic en mapa | Colocar origen del PPI. |
| `[` / `]` | Reducir/aumentar alcance radar. |
| `T` | Introducir ruta TIFF. |
| `I`, `,`, `.` | Visibilidad / opacidad de imagen activa. |
| `J` / `K` | Restaurar / encuadrar imagen activa. |
| Shift + arrastre / rueda | Mover / escalar imagen activa. |
| Rueda sobre listas | Desplazar capas o atributos. |
| `F5` | Volver a indexar y cargar la colección. |
| `Esc` | Cerrar diálogo/catálogo o salir. |

Los diálogos admiten rutas UTF-8 y Ctrl+V. Arrastrar carpetas/celdas carga primero
las cartas; los TIFF se incorporan después. Los controles laterales permiten
cambiar ganancia/alfa radar y gestionar imágenes sin recordar atajos.

La escala gráfica, el aviso de sobreescala y el contorno de seguridad se calculan
en la biblioteca. Los símbolos de contactos siguen el subconjunto de presentación
descrito en [contactos y trazabilidad OMI](../../docs/contactos.md); no representan
una certificación. La selección ENC y la selección de contactos usan IDs distintos.

## Compilar como proyecto receptor independiente

Después de instalar ENC SDK, desde cualquier directorio:

```sh
cmake -S /ruta/enc-viewer-sdk/examples/viewer -B build-consumer -DCMAKE_PREFIX_PATH=/ruta/al/paquete
cmake --build build-consumer --parallel
./build-consumer/enc-viewer-demo /ruta/enc-viewer-sdk/sample-charts --contacts --streaming --radar --demo-tiff
```

Esta compilación utiliza el paquete instalado y las cabeceras públicas. El
framework GLFW se utiliza únicamente aquí y en el ejemplo mínimo de ventana.

## Recorrido del código

`main.cpp` crea el contexto, calcula el destino físico y ejecuta el bucle.
`app.cpp` traduce acciones a métodos públicos. `input.cpp` convierte eventos
GLFW a navegación y selección. `interface.cpp` presenta la información devuelta
por la biblioteca; `drawing.cpp` dibuja exclusivamente sus controles.
`contacts_demo.cpp` genera informes sintéticos y los entrega mediante
`upsertContacts`; no forma parte del motor de seguimiento del anfitrión.

Para una captura automática de pocos cuadros:

```sh
./enc-viewer-demo /datos/enc --radar --demo-tiff --smoke-frames 4 --screenshot vista.ppm
```

La [integración avanzada](../advanced/README.md) ofrece estas APIs en un consumidor
más compacto. Ver también [visualización](../../docs/visualizacion.md) y
[carga progresiva](../../docs/carga-progresiva.md).
