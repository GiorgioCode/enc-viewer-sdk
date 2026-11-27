# Recursos incorporados

El mundo y el atlas de etiquetas se incorporan a la biblioteca al compilar.
No hay que copiar este directorio junto al ejecutable. Sus licencias se conservan
también en el paquete instalado.

## Mapa mundial

`world-land.geojson` contiene polígonos de tierra Natural Earth 1:110m, versión
4.1.0, convertidos a GeoJSON WGS84 con cuatro decimales. Es un fondo generalizado;
el detalle de costa procede de las ENC cargadas.

- [Fuente Natural Earth](https://www.naturalearthdata.com/downloads/110m-physical-vectors/110m-land/).
- [Descarga de los polígonos](https://naciscdn.org/naturalearth/110m/physical/ne_110m_land.zip).
- [Condiciones: dominio público](https://www.naturalearthdata.com/about/terms-of-use/).

Para repetir la conversión con GDAL:

```sh
ogr2ogr -f GeoJSON world-land.geojson ne_110m_land.shp \
  -select featurecla -lco COORDINATE_PRECISION=4
```

## Atlas ENC Labels

`enc-labels.rle` almacena avances de glifo y una imagen alfa cuantizada, derivada
de DejaVu Sans. El atlas contiene 1297 entradas, incluyendo el glifo sustituto,
en una imagen de 640×984. Cubre U+0020–U+02FF, U+0370–U+052F y U+2000–U+206F:
latín extendido, griego, cirílico y puntuación. La licencia Bitstream/DejaVu se
conserva completa en [FONT-LICENSE.txt](FONT-LICENSE.txt).

Para regenerarlo, desde la raíz del repositorio, con Python 3, Pillow y la fuente
DejaVu Sans disponibles:

```sh
python3 tools/generate_text_atlas.py /ruta/a/DejaVuSans.ttf
cmake --build build/dev
```

El script utiliza FreeType a través de Pillow durante la generación; la biblioteca
sólo carga el recurso incorporado y crea su textura al renderizar. No necesita
Pillow, FreeType o el archivo TTF en ejecución. Cualquier cambio de repertorio,
tamaño o formato debe mantener sincronizados generador y el módulo
`src/detail/text_renderer.cpp` del repositorio.
