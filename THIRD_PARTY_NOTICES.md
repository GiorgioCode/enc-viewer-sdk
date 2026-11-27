# Recursos y dependencias de terceros

La licencia [MIT](LICENSE) del repositorio se aplica a su código y documentación
propios. Los recursos cartográficos y dependencias conservan sus condiciones de
origen; no se les asigna una licencia nueva.

## Mapa mundial incorporado

`assets/world-land.geojson` procede de Natural Earth 1:110m, versión 4.1.0,
convertido a WGS84. Se incorpora a la biblioteca durante la compilación.
Natural Earth publica sus datos en dominio público. La procedencia, descarga y
conversión se conservan en `assets/README.md` y en el paquete instalado como
`WORLD-PROVENANCE.md`.

- Fuente: https://www.naturalearthdata.com/downloads/110m-physical-vectors/110m-land/
- Condiciones: https://www.naturalearthdata.com/about/terms-of-use/

## Celdas de ejemplo

`sample-charts/` contiene tres paquetes ENC NOAA conservados con sus archivos de
actualización y avisos originales `README.TXT` y `USERAGREEMENT.TXT`. Su procedencia
se documenta en `sample-charts/README.md`. Son datos de prueba; no forman parte de
la biblioteca estática ni del paquete binario de desarrollo. El archivo fuente
distribuido por CPack sí incluye los datos y sus avisos.

- Fuente: https://charts.noaa.gov/ENCs/ENCs.shtml

## Atlas de etiquetas

`assets/enc-labels.rle` es un atlas rasterizado y cuantizado derivado de DejaVu
Sans, identificado como **ENC Labels**. El aviso completo Bitstream/DejaVu se
conserva en `assets/FONT-LICENSE.txt` y se instala como `FONT-LICENSE.txt` junto a
los demás avisos del paquete. El atlas queda incorporado al archivo estático.
No se distribuye el archivo TTF ni se utiliza FreeType en ejecución.

- Fuente: https://dejavu-fonts.github.io/
- Regeneración: `tools/generate_text_atlas.py`, con Pillow y la fuente DejaVu Sans.

## Bibliotecas enlazadas

GDAL con soporte GEOS, OpenGL, GLU y los componentes del entorno gráfico son dependencias externas
resueltas por CMake. GLFW se utiliza sólo en los ejemplos y EGL/Mesa en las pruebas
gráficas. No se copian sus implementaciones dentro de este repositorio ni dentro
del paquete de desarrollo; quien redistribuya sus binarios debe conservar sus
respectivos avisos.

- GDAL: https://github.com/OSGeo/gdal/blob/master/LICENSE.TXT
- GLFW: https://www.glfw.org/license.html
- Mesa: https://docs.mesa3d.org/license.html
- GEOS: https://libgeos.org/usage/license/

## Referencias de simbología

El catálogo contiene dibujos vectoriales propios, informados por NOAA Chart No. 1
y los atributos S-57. No se incluyen una biblioteca IHO S-52 ni certificados de
conformidad.
Los símbolos de contactos AIS/radar y buque propio son dibujos propios tomando
como referencia las tablas 1–4 del anexo 1 de OMI SN.1/Circ.243/Rev.2 y Corr.1.
El alcance parcial y sus diferencias se describen en `docs/contactos.md`; no se
reproduce el documento ni se afirma conformidad completa.

- NOAA Chart No. 1: https://nauticalcharts.noaa.gov/publications/us-chart-1.html
- Catálogo de atributos S-57: https://legacy.iho.int/iho_pubs/standard/S-57Ed3.1/31ApAch2.pdf
- Presentación de símbolos OMI: https://wwwcdn.imo.org/localresources/en/OurWork/Safety/Documents/IMO%20Documents%20related%20to/SN.1-Circ.243-Rev.2%20%2B%20Corr.1.pdf
