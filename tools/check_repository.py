#!/usr/bin/env python3
"""Comprueba enlaces locales, encabezados y separación entre API, motor y ejemplos."""
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]


def main():
    errors = []
    # Sólo se recorren directorios fuente: una compilación o instalación local no forma documentación.
    files = [p for p in ROOT.iterdir() if p.is_file() and p.suffix == '.md']
    files += [p for folder in ('docs', 'examples', 'assets', 'sample-charts')
              for p in (ROOT / folder).rglob('*.md')]
    for path in files:
        text = path.read_text(encoding='utf-8')
        prose = re.sub(r'```.*?```', '', text, flags=re.S)
        for match in re.finditer(r'!?\[[^\]]*\]\(([^\s)]+)(?:\s+"[^"]*")?\)', prose):
            link = unquote(match.group(1).strip('<>'))
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            if not (path.parent / parsed.path).exists():
                errors.append(f'{path.relative_to(ROOT)}: enlace inexistente: {link}')
        if re.search(r'/home/[^\s`]+', text):
            errors.append(f'{path.relative_to(ROOT)}: referencia externa al repositorio')
    for folder in ('src', 'include', 'tests', 'examples'):
        for path in (ROOT / folder).rglob('*'):
            if path.suffix not in ('.cpp', '.h'):
                continue
            text = path.read_text(encoding='utf-8')
            if not text.lstrip().startswith(('//', '/*')):
                errors.append(f'{path.relative_to(ROOT)}: falta encabezado explicativo')
            includes = re.findall(r'^\s*#include\s*[<"]([^">]+)', text, re.M)
            if folder in ('src', 'include') and any('GLFW' in name for name in includes):
                errors.append(f'{path.relative_to(ROOT)}: el motor depende de GLFW')
            if folder == 'include' and any(
                    re.search(r'^(?:GL/|EGL/|gdal|ogr|cpl_)|(?:^|/)detail/', name, re.I)
                    for name in includes):
                errors.append(f'{path.relative_to(ROOT)}: dependencia privada en API pública')
            if folder == 'examples' and any('detail/' in name for name in includes):
                errors.append(f'{path.relative_to(ROOT)}: ejemplo dependiente de cabecera privada')
    try:
        json.loads((ROOT / 'CMakePresets.json').read_text(encoding='utf-8'))
    except (ValueError, OSError) as error:
        errors.append(str(error))
    for error in errors:
        print(error, file=sys.stderr)
    if not errors:
        print(f'Enlaces de {len(files)} documentos, encabezados y fronteras de módulos correctos.')
    return bool(errors)


if __name__ == '__main__':
    sys.exit(main())
