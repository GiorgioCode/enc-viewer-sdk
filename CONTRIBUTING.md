# Contribuir a ENC Viewer SDK

El objetivo es facilitar que otros proyectos integren la visualización ENC mediante
una API pequeña, sin depender de la ventana o los controles de la demostración.
Los comentarios y la documentación del proyecto se mantienen en español.

## Preparar el entorno

En Ubuntu/Debian, instalar las dependencias indicadas en el [README](README.md).
Para desarrollar con CMake 3.21 o superior:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

El preset `cpu` permite probar indexación, lectura y empaquetado sin GLFW ni EGL.
La [guía de validación](docs/validacion.md) describe los contratos comprobados.

## Cambios en la biblioteca

- Exponer operaciones mediante `include/enc`; mantener tipos GDAL, OpenGL, GLFW
  y cabeceras privadas fuera de la API pública.
- Mantener el contexto y el bucle bajo control del anfitrión. El destructor de
  `Viewer` no debe ejecutar OpenGL.
- Documentar unidades, propiedad de recursos, errores e invalidación de identificadores
  cuando cambie un contrato.
- Agregar verificaciones de comportamiento cuando cambien contratos o algoritmos.
  Para documentación y cambios de formato, ejecutar las comprobaciones pertinentes.
- Mantener encabezados y comentarios en español que expliquen las decisiones importantes.

Para orientarse, consultar [arquitectura](docs/arquitectura.md) y
[API](docs/api.md). Las mejoras de simbología deben incluir un ejemplo consultable
y una comprobación que contraste atributos y representación.

## Formato y documentación

```sh
python3 tools/format_cpp.py --check --executable clang-format-18
python3 tools/check_repository.py
```

Para aplicar formato, usar el mismo comando sin `--check`. Se utiliza clang-format 18
para obtener resultados reproducibles. Los diagramas se editan en PlantUML y se
regeneran con `cmake --build build/dev --target enc-diagrams` cuando PlantUML está instalado.

## Pull requests

Describir el problema y el comportamiento resultante, junto con la validación
realizada. Si cambia la API pública, actualizar la guía de integración y el registro
de cambios. Los ejemplos deben seguir compilando como consumidores independientes.

Para informar fallos, indicar compilador, GDAL, versión/perfil OpenGL y un ejemplo
reproducible. Si el problema es de seguridad, seguir [SECURITY.md](SECURITY.md).

Las contribuciones de código propio se incorporan bajo la licencia [MIT](LICENSE).
Los datos externos deben conservar su procedencia y sus condiciones de distribución.
