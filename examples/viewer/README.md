# Visor de integración

Esta aplicación utiliza exclusivamente `<enc/enc.h>` y el target `enc::viewer`.
Posee ventana, controles, callbacks y reloj; toda la lectura y visualización de
cartas y superposiciones se resuelve mediante llamadas públicas de la biblioteca.

## Ejecución

Desde `enc-viewer-sdk/`, después de compilar el proyecto principal:

```sh
./build/dev/examples/viewer/enc-viewer-demo ./sample-charts --radar --demo-tiff --night
./build/dev/examples/viewer/enc-viewer-demo /datos/enc /datos/ortofoto.tif --day
```

Puede abrir varias imágenes y cambiar la imagen activa mediante **ANT/SIG**.
**Subir al tope** modifica el orden de las imágenes; el radar sigue encima.

| Entrada | Acción |
| --- | --- |
| Rueda sobre mapa | Zoom alrededor del cursor. |
| Arrastre izquierdo | Desplazar la cámara. |
| Clic sobre objeto | Seleccionar e inspeccionar atributos. |
| `O` | Introducir carpeta ENC o base `.000`. |
| `F` / `W` | Encuadrar cartas / mundo. |
| `V` | Mostrar u ocultar todas las ENC. |
| `N` | Día/noche en carta, radar e interfaz. |
| `L` | Abrir/cerrar catálogo visual de símbolos. |
| `R` / `P` | Mostrar radar / pausar. |
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

## Compilar como proyecto receptor independiente

Después de instalar ENC SDK, desde cualquier directorio:

```sh
cmake -S /ruta/enc-viewer-sdk/examples/viewer -B build-consumer -DCMAKE_PREFIX_PATH=/ruta/al/paquete
cmake --build build-consumer --parallel
./build-consumer/enc-viewer-demo /ruta/enc-viewer-sdk/sample-charts --radar --demo-tiff
```

Esta compilación utiliza el paquete instalado y las cabeceras públicas. El
framework GLFW se utiliza únicamente aquí y en el ejemplo mínimo de ventana.

## Recorrido del código

`main.cpp` crea el contexto, calcula el destino físico y ejecuta el bucle.
`app.cpp` traduce acciones a métodos públicos. `input.cpp` convierte eventos
GLFW a navegación y selección. `interface.cpp` presenta la información devuelta
por la biblioteca; `drawing.cpp` dibuja exclusivamente sus controles.

Para una captura automática de pocos cuadros:

```sh
./enc-viewer-demo /datos/enc --radar --demo-tiff --smoke-frames 4 --screenshot vista.ppm
```
