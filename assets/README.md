# World basemap

`world-land.geojson` contains Natural Earth 1:110m physical land polygons,
version 4.1.0, converted to WGS84 GeoJSON with four decimal places. It is a
generalized world backdrop; detailed coastlines come from the loaded ENCs.

Source: https://www.naturalearthdata.com/downloads/110m-physical-vectors/110m-land/

Download: https://naciscdn.org/naturalearth/110m/physical/ne_110m_land.zip

Natural Earth data is public domain:
https://www.naturalearthdata.com/about/terms-of-use/

Reproduce the conversion with GDAL:

```sh
ogr2ogr -f GeoJSON world-land.geojson ne_110m_land.shp \
  -select featurecla -lco COORDINATE_PRECISION=4
```
