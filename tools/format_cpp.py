#!/usr/bin/env python3
"""Aplica o comprueba clang-format 18 sólo en las fuentes C++ del repositorio."""
import argparse
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Comprobar sin modificar archivos')
    parser.add_argument('--executable', default='clang-format-18')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        version = subprocess.check_output([args.executable, '--version'], text=True)
        if not re.search(r'version 18\.', version):
            parser.error('Se requiere clang-format 18 para un formato reproducible.')
        sources = sorted(p for folder in ('src', 'include', 'tests', 'examples')
                         for p in (root / folder).rglob('*') if p.suffix in ('.h', '.cpp'))
        mode = ['--dry-run', '--Werror'] if args.check else ['-i']
        return subprocess.call([args.executable, *mode, *map(str, sources)])
    except (OSError, subprocess.CalledProcessError) as error:
        print(f'No se pudo ejecutar clang-format: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
