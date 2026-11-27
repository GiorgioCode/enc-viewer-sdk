# Visualización de cartas S-57

Esta guía describe la composición de celdas, las reglas de presentación y la
navegación de [`enc::Viewer`](../include/enc/viewer.h). Las opciones pertenecen a
la biblioteca; los botones, atajos y diálogos pertenecen a los
[ejemplos de implementación](../examples/README.md).

## Mosaico y cobertura

`Presentation::automaticMosaic=true` ordena las celdas de generales a detalladas,
usando escala de compilación y propósito de navegación. Las coberturas `M_COVR`
con `CATCOV=1` delimitan la superficie utilizable; `CATCOV=2` y los anillos
interiores excluyen zonas sin datos. Cuando una celda detallada cubre un punto,
oculta allí la información de las celdas inferiores. El recorte se aplica al
dibujo y a la selección, para que el cursor consulte los objetos que se ven.

Si falta cobertura válida, se utiliza la extensión disponible como alternativa.
`visibleCells()` devuelve las celdas candidatas de la vista, sus escalas y el
factor de ampliación; no calcula porcentajes de cobertura visible. Desactivar el
mosaico muestra las celdas superpuestas, útil para inspeccionar los datos.

La cámara descarta celdas demasiado detalladas para una vista general mediante
`detailThreshold`, cuyo valor inicial es 8, cuando su cobertura puede obtenerse
de cartas generales. Conserva las celdas que aportan cobertura única y omite las
cartas generales totalmente tapadas en la vista. Esto regula la presentación de
los datos cargados. Para evitar también su lectura, combinarlo con la
[carga progresiva por región](carga-progresiva.md).

## Escala y cantidad de detalle

`scaleInfo()` proporciona el denominador, metros por píxel lógico y dimensiones
de una barra de escala. Su cálculo es local al centro de la vista. La propiedad
`millimetresPerPixel`, inicialmente 0.2645833333, relaciona píxeles lógicos con
tamaño físico: el anfitrión debe calibrarla si necesita una escala física precisa.
El tamaño del framebuffer HiDPI se informa por separado en `RenderTarget`.

Con `respectScale=true`, se aplican `SCAMIN` y `SCAMAX` a puntos, líneas y áreas.
Los objetos de categoría base se conservan; un objeto seleccionado puede superar
el filtro de escala. No se omiten por ello los filtros de categoría o de capa.
`showScaleBar` y `showOverscale` controlan los indicadores. El aviso de
sobreescala indica que una celda se amplía más allá de su escala de compilación;
no añade precisión a los datos.

```cpp
auto p = visor.presentation();
p.automaticMosaic = true;
p.respectScale = true;
p.displayMode = enc::DisplayMode::Standard;
p.textScale = 1.2;
p.labelDensity = 0.8;
p.preferNationalNames = true;
const auto estado = visor.setPresentation(p); // Comprobar Status antes de continuar.
const auto escala = visor.scaleInfo();
const auto celdas = visor.visibleCells();
```

Las categorías propias son `Base`, `Standard` y `Other`. `DisplayMode::Base`
conserva la primera; `Standard` incluye las dos primeras y `All` las tres.
La clasificación es un subconjunto del motor, sin equivalencia completa con
las categorías de una biblioteca de presentación S-52.

## Reglas configurables

[`PortrayalRule`](../include/enc/portrayal.h) permite cambiar categoría, prioridad,
color de relleno y línea, ancho y patrón por clase. Colores como `0x336699`
representan RGB. Las reglas se aplican a superficies y trazos; la interpretación
de atributos y los símbolos náuticos especializados siguen sus dibujantes.

```cpp
enc::PortrayalRule costa;
costa.objectClass = "COALNE";
costa.category = enc::DisplayCategory::Base;
costa.priority = 8;
costa.line = 0x365E69;
costa.lineWidth = 2;
costa.pattern = enc::LinePattern::Solid;
const auto estadoReglas = visor.setPortrayalRules({costa});
```

Cada llamada parte de `defaultPortrayalRules()` y reemplaza las clases indicadas;
no acumula cambios de llamadas anteriores. Un lote inválido o con clases repetidas
se rechaza completo. `setPortrayalRules({})` restaura las reglas predeterminadas.
Los colores personalizados también pasan por la transformación del tema nocturno.

## Profundidades y peligros

`shallowDepth`, `safetyDepth` y `deepDepth` controlan colores de profundidad,
ordenados de menor a mayor. `safetyContour` es una petición independiente: el
motor resalta, en cada celda, el contorno `DEPCNT/VALDCO` igual o inmediatamente
más profundo que el solicitado. Si no existe, esa celda informa un valor ausente.
`depthInfo()` devuelve el resultado por celda y un contorno común sólo cuando
coincide en todas las celdas candidatas.

Las sondas distinguen profundidades menores o iguales a `safetyDepth`.
Los objetos `WRECKS`, `OBSTRN` y `UWTROC` reciben un resaltado básico de peligro
según `VALSOU`, `WATLEV` y ese umbral; `FeatureInfo::danger` expone la misma
evaluación. No contempla calado, marea, margen bajo quilla o una comprobación
anticipada de ruta. Es una presentación condicional limitada.

## Texto y consultas comprensibles

La lectura solicita `RECODE_BY_DSSI=ON` a GDAL para convertir los atributos a
UTF-8 según la codificación declarada por la celda, como describe el
[controlador S-57 de GDAL](https://gdal.org/en/stable/drivers/vector/s57.html).
Las etiquetas se decodifican como UTF-8 y se dibujan con un atlas incorporado
derivado de DejaVu Sans. Se incluyen latín extendido, griego, cirílico y puntuación
general; los puntos de código no incluidos utilizan un glifo sustituto. No hay
dependencia de FreeType en ejecución, ni tipografía compleja, escritura de derecha
a izquierda o cobertura universal de idiomas.

`preferNationalNames` prioriza `NOBJNM`, con alternativa `OBJNAM`. `textScale`
admite 0.5–3 y `labelDensity` 0.1–4. El distribuidor reserva símbolos, prueba varias
posiciones y recuerda la posición elegida entre cuadros. Las etiquetas pueden
desplazarse u omitirse al cambiar escala o densidad; no se garantiza que todos los
nombres sean visibles. La selección mejora la prioridad del objeto consultado.

`pickAll(punto, tolerancia, limite)` obtiene varios objetos visibles bajo el cursor.
El anfitrión puede recorrerlos y llamar a `select(id)`, como hace Ctrl+clic en los
ejemplos. `description` y `decodedAttributes` de `FeatureInfo` exponen los nombres
y valores descritos por los diccionarios S-57 de GDAL. Se conserva el acrónimo
original; las descripciones disponibles provienen de GDAL y pueden estar en inglés.

## Proyección, giro e historial

```cpp
visor.rememberView(); // Guardar una sola vez al comenzar el gesto.
auto camara = visor.camera();
camara.projection = enc::Projection::Mercator;
camara.rotationDegrees = 30;
const auto estadoCamara = visor.setCamera(camara);
visor.previousView();
visor.nextView();
```

La proyección predeterminada es equirectangular local. Mercator admite centros
hasta ±85.05112878°; no representa los polos. `pixelsPerDegree` conserva su
significado en el eje longitudinal proyectado. La rotación es horaria en pantalla;
los símbolos geográficos, el radar y los contactos usan la misma transformación.
`zoomAt` mantiene el punto geográfico bajo el cursor también con giro.

`Bounds` con `west > east` representa una región que cruza el antimeridiano.
Las conversiones, la consulta y el encuadre admiten esa convención; las imágenes
GeoTIFF que crucen esa línea deben dividirse antes de cargarlas. `fitFeatures`
encuadra una selección de identificadores vigentes. El historial conserva hasta
100 vistas y se agrupa con llamadas explícitas a `rememberView()`.

Para experimentar con estas opciones, ver el
[ejemplo avanzado](../examples/advanced/README.md) y los
[contratos de la API](api.md).
