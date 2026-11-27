#!/usr/bin/env python3
"""Compila cada cabecera pública por separado sin rutas de GDAL, OpenGL o GLFW."""
import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='c++')
    parser.add_argument('--include', type=Path, required=True)
    parser.add_argument('--generated', type=Path, required=True)
    args = parser.parse_args()
    headers = sorted((args.include / 'enc').glob('*.h'))
    headers += sorted((args.generated / 'enc').glob('*.h'))
    if not headers or not (args.generated / 'enc/version.h').is_file():
        parser.error('Configurar CMake primero para generar enc/version.h.')
    for header in headers:
        # Cada unidad empieza vacía para descubrir dependencias de inclusión accidentales.
        result = subprocess.run(
            [args.compiler, '-std=c++17', '-fsyntax-only', '-I', str(args.include),
             '-I', str(args.generated), '-x', 'c++', '-'],
            input=f'#include <enc/{header.name}>\n', text=True, capture_output=True)
        if result.returncode:
            print(f'{header}:\n{result.stderr}', file=sys.stderr)
            return result.returncode
    print(f'{len(headers)} cabeceras públicas compiladas individualmente.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
