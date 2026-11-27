# API y contratos

La cabecera única es [`<enc/enc.h>`](../include/enc/enc.h). Sus componentes públicos
no incluyen cabeceras de GDAL, OpenGL ni GLFW. Los detalles de implementación no
se instalan. Esta versión 0.1 es una API inicial: recompilar consumidores contra
la misma versión del paquete y su toolchain C++.

## Catálogo

| Operación | Comportamiento |
| --- | --- |
| `Catalog::scan(source, options)` | Recorre una carpeta o abre una base `.000`; prepara y confirma una instantánea ordenada. |
| `refresh(options)` | Repite el recorrido de la fuente anterior; no modifica una escena `Viewer` ya cargada. |
| `cells()` | Referencia a los metadatos del índice; se invalida al confirmar otro escaneo. |
| `query(bounds)` | Devuelve copias de las celdas cuya envolvente intersecta la región WGS84. |
| `saveInventory(path)` | Guarda JSON de rutas, edición, actualización, escala, tamaños y cobertura. |

`ScanOptions::recursive` vale `true`. No se siguen enlaces simbólicos de carpetas.
Las bases se deduplican por ruta canónica, sin confundir archivos homónimos de
carpetas diferentes. Las actualizaciones se ordenan por su número y se buscan
junto a la base con el mismo nombre. Los archivos de intercambio `CATALOG.031`
no se interpretan como celdas ni se cargan como actualizaciones de otras bases.

Con `inspectMetadata=true`, GDAL inspecciona DSID y cobertura, dejando
`CellInfo::readable=false` y diagnósticos en las entradas ilegibles. El escaneo
puede terminar correctamente y contener entradas que no se pueden cargar. Con
`inspectMetadata=false` sólo se descubre el inventario: `readable=false` y límites
inválidos indican que no se inspeccionaron los datos. `Viewer::loadFolder` utiliza
esta variante para evitar leer dos veces cada celda.

`modifiedTicks` es una marca opaca del reloj de archivos del toolchain, para
comparaciones en el mismo entorno; no representa una fecha UTC portable. El JSON
es un inventario informativo: no almacena geometrías ni se carga como caché.

## Carga y escena

| Operación | Comportamiento |
| --- | --- |
| `loadFolder(path, options)` | Descubre las bases y carga su colección. También admite una base individual. |
| `loadCatalog(catalog, options)` | Carga las rutas del catálogo; no necesita mantenerlo vivo después. |
| `loadCells(paths, options)` | Carga únicamente las rutas elegidas, deduplicando repeticiones. |
| `clearCharts()` | Descarga ENC e invalida selección e identificadores; conserva mundo, cámara, TIFF y preferencias. |
| `sceneInfo()` | Cantidades de celdas, capas, objetos y triángulos, más extensión WGS84. |
| `layers()` | Copias de las capas con celda, clase S-57, cantidad y visibilidad. |
| `setLayerVisible(id, visible)` | Control individual; el índice de capa corresponde a la carga actual. |
| `setChartsVisible(visible)` | Control maestro ENC; ocultar no descarga datos ni desactiva radar o imágenes. |

`LoadOptions::fitView` vale `true`; ponerlo en `false` mantiene la cámara.
`allowPartial=false` mantiene la colección anterior si alguna celda falla.
Con `allowPartial=true`, el resultado puede ser exitoso y contener diagnósticos
de error para las celdas descartadas. No se confirma una colección sin ninguna
celda con geometría utilizable.

`ProgressCallback` recibe `completed`, `total` y ruta. La llamada final puede
llevar ruta vacía. Devolver `false` cancela, incluso antes de la confirmación final.
No se interrumpe una operación GDAL a mitad de celda. Los callbacks son síncronos
en el hilo llamador; no deben reentrar a la misma instancia.

Una carga confirmada conserva el tema, presentación, TIFF y configuración del
radar. Reconstruye los ecos simulados para las nuevas cartas; los ecos externos
permanecen bajo control del anfitrión.

## Cámara, consulta y selección

| Operación | Contrato |
| --- | --- |
| `resize(width, height)` | Dimensiones lógicas del mapa; cero suspende el renderizado, valores negativos se rechazan. |
| `camera()` / `setCamera(camera)` | Centro WGS84, latitud de referencia y píxeles por grado local. |
| `fitCharts()` / `fitWorld()` | Encuadra las ENC o el mundo sin cambiar visibilidad ni datos. |
| `fitBounds(bounds)` | Encuadra una región válida que no cruce el antimeridiano. |
| `zoomAt(point, factor)` | Mantiene fijo el punto geográfico bajo el cursor; factor positivo. |
| `panBy(dx, dy)` | Desplazamiento del mouse en píxeles lógicos. |
| `screenToGeo` / `geoToScreen` | Conversión respecto de la cámara y tamaño lógico actuales. |
| `queryFeatures(region, class, limit)` | Copias de objetos espaciales, incluso ocultos; región inválida o clase vacía omiten ese filtro. |
| `pick(point)` | Primer objeto visible bajo un punto local; prioriza ayudas, líneas y áreas respetando huecos. |
| `feature(id)` / `select(id)` / `selected()` | Consulta y selección mediante identificadores que incluyen la generación de carga. |

`queryFeatures` limita por defecto a 1000 resultados. `FeatureInfo::position` es
el primer anclaje puntual disponible; `depth` corresponde a su profundidad Z,
cuando existe. Las geometrías completas permanecen privadas. Para objetos multipunto
esta consulta no constituye una exportación de todas sus sondas.

Un `FeatureId` queda inválido al sustituir o limpiar las ENC. Un `OverlayId`
permanece válido hasta retirar esa imagen; no se recicla durante la vida de la
instancia. Los índices de capa deben actualizarse después de cada carga.

## Presentación y símbolos

`setTheme(Day/Night)` modifica la presentación de carta, mundo y radar. El TIFF
se atenúa al dibujarlo de noche; sus píxeles y posición no cambian. El anfitrión
aplica su propia paleta a sus paneles, como hace el visor de ejemplo.

`Presentation` controla umbrales en metros (`shallowDepth <= safetyDepth <=
deepDepth`), escala de símbolos entre 0.25 y 4, etiquetas, sondas y sectores.
No sustituye el cálculo de seguridad ni la presentación condicional de un ECDIS.

`describeSymbol(class, attributes)` devuelve una interpretación consultable.
`symbolExamples()` contiene 44 ejemplos. `renderSymbolCatalogue(target)` permite
inspeccionarlos gráficamente con la paleta actual. El catálogo y las cartas
comparten el decodificador.

La interpretación utiliza BOYSHP, BCNSHP, COLOUR, COLPAT, CATCAM, TOPSHP, CATLMK,
WATLEV y SECTR1/SECTR2. Las ayudas se asocian por referencias LNAM_REFS a una
estructura cuando están disponibles y por posición en caso contrario, siempre
dentro de la misma celda identificada por ruta completa. Los sectores son arcos
esquemáticos en pantalla. Las demoras S-57 se miden desde el mar hacia la luz;
al dibujar desde la luz se aplica la dirección recíproca (+180°), según
[IHO S-57, apéndice A, capítulo 2](https://legacy.iho.int/iho_pubs/standard/S-57Ed3.1/31ApAch2.pdf).
Los sectores pueden cruzar 0°. No se infiere región IALA cuando falta el color lateral.

## Imágenes

| Operación | Contrato |
| --- | --- |
| `addGeoTiff(path, options)` | Devuelve ID; un fallo no retira las imágenes existentes. |
| `rasters()` | Copias ordenadas desde la imagen inferior hasta la superior, con esquinas ya colocadas. |
| `setRasterVisible` / `setRasterOpacity` | Visibilidad independiente y alfa entre 0 y 1. |
| `moveRaster` / `scaleRasterAt` / `resetRaster` | Ajuste en memoria, anclaje bajo cursor y restauración de georreferencia inicial. |
| `fitRaster` | Encuadra su huella colocada. |
| `moveRasterToTop` | La lleva arriba de las otras imágenes; el radar continúa encima. |
| `removeRaster` | Retira CPU y encola eliminación GPU; no necesita contexto en ese momento. |
| `addDemoGeoTiff` | Genera una imagen sintética temporal, la carga y elimina el archivo temporal. |

Se admiten RGB, RGBA, grises y paletas; alfa, máscaras de dataset y no-data.
Las bandas no byte se ajustan al rango válido muestreado. GeoTIFF proyectados,
rotados o con GCP se reproyectan a WGS84. La previsualización se reduce a un
máximo de 2048 píxeles por lado. No es un visor de ráster por teselas.

## Radar

`enableRadar(true)` ubica el origen inicial cerca del centro, usando geometría
DEPARE/DRGARE cuando existe. `configureRadar` define una configuración explícita.
La simulación toma ecos de costa y estructuras y agrega blancos sintéticos.
El modo `ExternalEchoes` dibuja exclusivamente los retornos recibidos mediante
`setRadarEchoes`; el barrido y los anillos siguen disponibles.

| Parámetro | Unidad y límites |
| --- | --- |
| `origin` | WGS84 longitud/latitud. |
| `rangeNm` | 0.125 a 48 millas náuticas. |
| `opacity`, intensidad de eco | 0 a 1. |
| `gain` | 0 a 10. |
| `rpm` | 1 a 120 revoluciones por minuto. |
| `advance(seconds)` | Tiempo transcurrido, finito y no negativo; se limita a 0.25 s por llamada. |

El tiempo avanza sólo con radar visible y sin pausa; `render` no lo modifica.
Los cambios de origen, alcance o modo limpian los ecos externos y requieren
reenviar la escena. El muestreo deduplica posiciones próximas y limita a 40000
ecos. No se implementan adquisición de sensores ni propagación física.

## Frontera gráfica

`render` y `renderSymbolCatalogue` requieren un contexto de escritorio compatible.
OpenGL ES y perfiles core se rechazan con diagnóstico. La biblioteca utiliza
funciones de compatibilidad y no trae un cargador moderno ni cambia el contexto.

Se conservan viewport, scissor, habilitaciones, colores, mezcla, matrices de
proyección/modelo/textura, programa shader, unidades de textura, sampler 0 cuando
está disponible y almacenamiento de píxeles, incluida PBO de subida. No se cambia
el framebuffer enlazado. El anfitrión debe entregar un destino framebuffer
completo, un contexto de compatibilidad sin errores pendientes y dimensiones
válidas. No llamar dentro de un `glBegin` del anfitrión.

`releaseGraphicsResources` requiere el contexto que posee las texturas y es
idempotente. Después se pueden reconstruir desde CPU en otro contexto. No hay
comprobación portable de identidad de contexto sin introducir una dependencia
del framework; el anfitrión debe garantizarla.

## Versión

`enc::version()` y las macros `ENC_VERSION_MAJOR`, `ENC_VERSION_MINOR`,
`ENC_VERSION_PATCH` y `ENC_VERSION_STRING` proceden de CMake. La cabecera
`<enc/version.h>` se genera al configurar y se instala junto a la API pública.
Ver la [política de compatibilidad](mantenimiento.md).
