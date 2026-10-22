#!/usr/bin/env python3
"""Build docs/generated/*.md from each engine's canonical README.

Run from any working directory. This script does not modify the original READMEs.
Relative references to local source files become links to those files on GitHub;
links between indexed README pages are kept within the documentation website.
"""
from pathlib import Path
import re
import posixpath
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs' / 'generated'
GITHUB = 'https://github.com/CEID-HPCLAB/pi4u'
RAW = 'https://raw.githubusercontent.com/CEID-HPCLAB/pi4u/main/'

SOURCES = {
    'tmcmc': 'inference/TMCMC/README.md',
    'dram': 'inference/DRAM/README.md',
    'abc-subsim': 'inference/ABC_SubSim/README.md',
    'cmaes': 'singleopt/cmaes/README.md',
    'amalgam': 'singleopt/AMaLGaM/README.md',
    'nsga2': 'multiobj/nsga2/README.md',
    'moead': 'multiobj/moead/README.md',
    'mocmaes': 'multiobj/mocmaes/README.md',
    'subset': 'rare/subset/README.md',
    'derivatives': 'derivatives/tools/README.md',
    'external-coupling': 'coupling/README.md',
    'tools': 'tools/README.md',
    'postprocessing': 'postprocessing_tools/README.md',
    'covariance-scripts': 'scripts/README.md',
}
INDEX = {path: slug for slug, path in SOURCES.items()}
# Handles ordinary Markdown links and images; complex HTML/Markdown links
# may still require review after the site is built.
LINK = re.compile(r'(!?\[[^\]]*\]\()([^)]*)(\))')


def adapt_link(match, src):
    before, url, after = match.groups()
    url = url.strip()
    if not url or re.match(r'^(?:[a-z][a-z0-9+.-]*:|//|#|/)', url, re.I):
        return match.group(0)
    if any(c.isspace() for c in url):
        return match.group(0)
    parts = urlsplit(url)
    if not parts.path:
        return match.group(0)
    target = posixpath.normpath(posixpath.join(posixpath.dirname(src), parts.path))
    if target.startswith('../'):
        return match.group(0)
    suffix = ('?' + parts.query if parts.query else '') + ('#' + parts.fragment if parts.fragment else '')
    if target in INDEX and not before.startswith('!'):
        new_url = INDEX[target] + '.md' + suffix
    elif before.startswith('!'):
        new_url = RAW + target + suffix
    else:
        path_is_dir = parts.path.endswith('/') or Path(ROOT / target).is_dir()
        new_url = GITHUB + ('/tree/main/' if path_is_dir else '/blob/main/') + target + suffix
    return before + new_url + after


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for slug, src in SOURCES.items():
        f = ROOT / src
        if not f.is_file():
            raise SystemExit(f'Missing source README: {src}')
        content = f.read_text(encoding='utf-8')
        content = LINK.sub(lambda m: adapt_link(m, src), content)
        (OUT / (slug + '.md')).write_text(content, encoding='utf-8')
        print(f'{src} -> docs/generated/{slug}.md')


if __name__ == '__main__':
    main()
