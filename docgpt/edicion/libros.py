"""Render the docgpt Markdown collection into self-contained HTML and optional PDF."""
from pathlib import Path
from html import escape, unescape
from urllib.parse import urlsplit, unquote
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile

import markdown

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
READ = ROOT / 'lectura'
PDF = ROOT / 'pdf'
ORDER = ['cycle','scroll','blinds','stripes','wobble','bounce','ripple','mosaic','zoom','rotozoom']
SPANISH_DEMOS = ['demos/README.md','demos/BASES.md']+[f'demos/{n}/doc/README.md' for n in ORDER]
ENGLISH_DEMOS = ['demos/README-EN.md','demos/BASES-EN.md']+[f'demos/{n}/doc/README-EN.md' for n in ORDER]
APPENDICES = ['LABS.md','GUIA.md','APIS.md','CUADERNO.md','GLOSARIO.md','FUENTES.md','ERRATAS.md','PDFS.md']
SUPPORT = {20:'ch20-paso-a-paso.md',25:'ch25-paso-a-paso.md'}

def lessons(first,last):
    files=[]
    for number in range(first,last+1):
        if number in SUPPORT:
            files.append(SUPPORT[number])
        files.append(f'ch{number:02d}.md')
    return files

# Tighter spacing prevents short exercise answers from occupying a page alone.
COMPACT = {'ch04.md','ch07.md','ch16.md','ch17.md','LABS.md','CUADERNO.md','demos/wobble/doc/README-EN.md'}
BOOKS = {
    'curso_completo': {
        'title':'Construye un juego para MS-DOS',
        'subtitle':'De la primera variable a un mundo compartido',
        'strap':'32 lecciones · 2 guías desde cero · 10 efectos',
        'description':'Un curso desde cero sobre C, VGA, sonido, IPX, mapas y cámara. Incluye el taller de demos y todos los apéndices en español.',
        'lang':'es', 'files':['README.md']+lessons(0,31)+SPANISH_DEMOS+APPENDICES,
    },
    'camara_y_mapas': {
        'title':'Cámara y mapas, por dentro',
        'subtitle':'Del mundo completo al píxel que aparece en pantalla',
        'strap':'8 lecciones · guía desde cero · laboratorio resuelto',
        'description':'Coordenadas, stride, memoria huge, clipping, zona muerta, máscara, niveles y radar. Un recorrido completo por las lecciones 21–28.',
        'lang':'es', 'files':lessons(21,28)+['LABS.md'],
    },
    'camara_paso_a_paso': {
        'title':'La cámara, paso a paso',
        'subtitle':'Una tira de papel, una ventana y una idea que se puede ver',
        'strap':'Guía 25A · ejemplos de consola · ejercicios resueltos',
        'description':'La guía extra de cámara en un documento independiente: coordenadas, seguimiento y límites, desde un ejemplo pequeño hasta su aplicación al juego.',
        'lang':'es', 'files':['ch25-paso-a-paso.md'],
    },
    'checksum_paso_a_paso': {
        'title':'Checksum, paso a paso',
        'subtitle':'Dos cuadernos, una suma y el momento correcto para comparar',
        'strap':'Guía 20A · ejemplos de consola · ejercicios resueltos',
        'description':'La guía extra de checksum en un documento independiente: inventarios, resúmenes, colisiones, frames e historial circular, antes de recorrer el juego.',
        'lang':'es', 'files':['ch20-paso-a-paso.md'],
    },
    'demos': {
        'title':'Diez trucos de una intro',
        'subtitle':'De la paleta al rotozoom, una transformación cada vez',
        'strap':'Bases matemáticas · 10 efectos · ejemplos resueltos',
        'description':'Una versión ampliada del taller de demos: fórmulas deducidas, números concretos, límites y experimentos con resultados esperados.',
        'lang':'es', 'files':SPANISH_DEMOS,
    },
    'demos_en': {
        'title':'Ten tricks behind an intro',
        'subtitle':'From colour cycling to rotozoom, one transform at a time',
        'strap':'Shared foundations · 10 effects · worked examples',
        'description':'An expanded workshop on indexed colour, fixed point, row transforms, inverse sampling and visible spans.',
        'lang':'en', 'files':ENGLISH_DEMOS,
    },
}

CSS = r'''
@page {size:A4; margin:18mm 17mm 19mm;
  @top-left {content:"CUTREGAME / MSDOS";font:7.1pt "DejaVu Sans";color:#65808a;letter-spacing:.9pt;vertical-align:bottom;padding-bottom:4mm}
  @top-right {content:"DOCGPT · EDICIÓN DE ESTUDIO";font:7.1pt "DejaVu Sans";color:#65808a;vertical-align:bottom;padding-bottom:4mm}
  @bottom-left {content:"C · VGA · SONIDO · IPX · MAPAS";font:6.9pt "DejaVu Sans";color:#65808a;padding-top:3mm}
  @bottom-right {content:counter(page);font:8pt "DejaVu Sans";color:#173548;padding-top:3mm}
}
@page :first {margin:0;@top-left{content:none}@top-right{content:none}@bottom-left{content:none}@bottom-right{content:none}}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;color:#243746;font:10pt/1.54 "DejaVu Sans",sans-serif;background:white;-webkit-print-color-adjust:exact;print-color-adjust:exact}
h1,h2,h3{font-family:"DejaVu Sans",sans-serif;color:#173548;line-height:1.22;break-after:avoid;page-break-after:avoid}h1{font-size:26pt;letter-spacing:-.8pt;margin:0 0 8mm;font-weight:700}h2{font-size:15pt;margin:8mm 0 3mm}h3{font-size:11.5pt;margin:6mm 0 2mm}p{margin:0 0 3.8mm;orphans:3;widows:3}a{color:#087f73;text-decoration:none}strong{color:#173548}em{font-style:italic}ul,ol{padding-left:6mm;margin:3mm 0 5mm}li{margin:0 0 1.8mm;orphans:2;widows:2}hr{border:0;border-top:1px solid #d4e2df;margin:6mm 0}
code{font-family:"DejaVu Sans Mono",monospace;font-size:.88em;background:#edf3f2;padding:.1em .24em;border-radius:3px;overflow-wrap:anywhere}pre{font:8.1pt/1.48 "DejaVu Sans Mono",monospace;background:#eef4f3;color:#173548;border-left:3px solid #159884;padding:4mm;margin:4mm 0 5mm;white-space:pre-wrap;overflow-wrap:anywhere;tab-size:4;break-inside:avoid}pre code{font:inherit;background:none;padding:0;white-space:inherit}blockquote{margin:4mm 0;padding:3mm 5mm;border-left:3px solid #c9a051;background:#fbf5e8}
table{border-collapse:collapse;width:100%;font-size:8.5pt;line-height:1.43;margin:4mm 0 6mm;table-layout:auto}thead{display:table-header-group;background:#173548;color:white}th{text-align:left;font-weight:600;padding:2.4mm 2.1mm;color:white}th strong{color:white}td{border-bottom:.3mm solid #dce6e4;padding:2.3mm 2.1mm;vertical-align:top;overflow-wrap:anywhere}tbody tr:nth-child(even){background:#f1f6f4}tr{break-inside:avoid}td code,th code{font-size:7.9pt;background:transparent;padding:0}thead a{color:white}img,svg{max-width:100%}
.chapter{break-before:page;page-break-before:always;position:relative}.chapter .kicker{font-size:8pt;text-transform:uppercase;letter-spacing:1.2pt;color:#148f82;border-top:1mm solid #148f82;padding-top:4mm;margin-bottom:6mm}.chapter>h1{max-width:98%}.chapter>p:first-of-type{font-size:10.4pt}.source-ref{color:#4c646e}.figure{break-inside:avoid;margin:5mm 0 6mm}.figure svg{width:100%;height:auto;display:block}.figure figcaption{font-size:8pt;line-height:1.43;color:#537078;margin:2.4mm 2mm 0}.figure figcaption em{font-style:normal}
.cover{height:297mm;position:relative;background:#173548;color:#eef9f3;padding:22mm 20mm;break-after:page;page-break-after:always;overflow:hidden}.cover .series{font-size:10pt;letter-spacing:2.8pt;text-transform:uppercase;color:#72d9c1;margin:0 0 20mm}.cover h1{font-size:42pt;line-height:1.06;letter-spacing:-1.5pt;color:white;margin:0 0 10mm;max-width:165mm}.cover .subtitle{font-size:17pt;line-height:1.4;color:#b6e7da;max-width:150mm}.cover-art{margin:12mm -1mm 10mm;width:170mm;height:79mm}.cover .strap{font-size:10pt;font-weight:600;color:#e9c887}.cover .description{font-size:10.4pt;line-height:1.65;max-width:157mm}.cover .edition{position:absolute;bottom:18mm;left:20mm;font-size:8pt;color:#9cbabf;letter-spacing:1pt}
.contents{break-before:page;break-after:page}.contents h1{margin-top:4mm}.contents .intro{color:#55717a;font-size:10pt}.contents ul{list-style:none;margin:6mm 0;padding:0}.contents li{margin:0;border-bottom:1px solid #e1e9e7;padding:2.1mm 0;break-inside:avoid}.contents a{display:flex;gap:4mm;align-items:baseline;color:#173548;font-size:9.2pt}.contents .toc-num{color:#148f82;font:8pt "DejaVu Sans Mono";width:8mm;flex:none}.contents .group{font-size:9pt;color:#148f82;letter-spacing:.6pt;font-weight:700;margin:8mm 0 1mm;break-after:avoid}.screen-toolbar{display:none}.broken-reference{color:#ae4c39}.file-note{font-size:8pt;color:#61777f}
@media screen {body{background:#e8efec}.cover{margin:24px auto;max-width:210mm;box-shadow:0 10px 40px #17354822}.contents,.chapter{background:white;width:min(100% - 32px,210mm);margin:24px auto;padding:18mm 17mm;box-shadow:0 10px 40px #17354810}.contents{min-height:240mm}.screen-toolbar{display:flex;position:sticky;top:0;z-index:10;padding:12px 24px;background:#173548;gap:24px;justify-content:center;flex-wrap:wrap}.screen-toolbar a{font:13px "DejaVu Sans";color:#daf4e9}.chapter{scroll-margin-top:70px}a:hover{text-decoration:underline}}
@media(max-width:650px){.cover{height:auto;min-height:100vh;padding:26px}.cover h1{font-size:34px}.cover .series{margin-bottom:35px}.cover .edition{position:static;margin-top:25px}.cover-art{width:100%;height:auto}.contents,.chapter{padding:24px}pre{font-size:10px}table{font-size:10px}.screen-toolbar{gap:12px}}
@media print{body{line-height:1.44}p{margin-bottom:2.8mm}h2{margin-top:6mm}.chapter>p:first-of-type{font-size:10pt}.figure svg{max-height:74mm}.chapter-nav,.chapter>hr:last-of-type{display:none}.figure{margin:4mm 0 5mm}.contents li{padding:1.2mm 0}.contents a{font-size:9pt;line-height:1.35}}
@media print{.compact p{margin-bottom:2mm}.compact h2{margin-top:5mm}.compact .kicker{margin-bottom:4mm}.compact .figure{margin:3mm 0 4mm}.compact .figure svg{max-height:70mm}.compact td{padding-top:2mm;padding-bottom:2mm}}
@media print{#doc-ch07-md>h1,#doc-ch17-md>h1{margin-bottom:4mm}#doc-ch07-md .figure svg,#doc-ch17-md .figure svg{max-height:64mm}}
'''

def stable_id(path):
    return 'doc-'+re.sub(r'[^a-z0-9]+','-',path.lower()).strip('-')

def title_of(path):
    line=next(line for line in (ROOT/path).read_text().splitlines() if line.startswith('# '))
    return line[2:]

def source_repository():
    try:
        remote=subprocess.check_output(['git','config','--get','remote.origin.url'],cwd=REPO,text=True).strip()
        commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO,text=True).strip()
        match=re.search(r'github\.com[:/]([^/]+/[^/]+?)(?:\.git)?$',remote)
        if match:
            return 'https://github.com/'+match.group(1),commit
    except subprocess.CalledProcessError:
        pass
    return None,None

REMOTE,COMMIT=source_repository()

def inline_svg(path):
    svg=path.read_text()
    prefix='fig-'+path.stem+'-'
    svg=re.sub(r'id="([^"]+)"',lambda m:'id="'+prefix+m.group(1)+'"',svg)
    svg=re.sub(r'url\(#([^)]*)\)',lambda m:'url(#'+prefix+m.group(1)+')',svg)
    svg=svg.replace('aria-labelledby="title desc"',f'aria-labelledby="{prefix}title {prefix}desc"')
    return svg

def chapter_html(rel,book):
    path=ROOT/rel
    text=path.read_text()
    content=markdown.markdown(text,extensions=['tables','fenced_code','sane_lists','toc'],output_format='html')
    prefix=stable_id(rel)
    content=re.sub(r'id="([^"]+)"',lambda m:f'id="{prefix}-{m.group(1)}"',content)
    current_files=set(book['files'])
    def link(match):
        raw=unescape(match.group(1)); label=match.group(2)
        parsed=urlsplit(raw)
        if parsed.scheme or raw.startswith('//'):
            return match.group(0)
        destination=(path.parent/unquote(parsed.path)).resolve() if parsed.path else path
        fragment=unquote(parsed.fragment)
        if destination.is_relative_to(ROOT):
            target=destination.relative_to(ROOT).as_posix()
            if target in current_files:
                ref=stable_id(target)
                if fragment: ref+='-'+fragment
                return f'<a href="#{escape(ref)}">{label}</a>'
            if target.endswith('.md'):
                other='demos_en' if target in ENGLISH_DEMOS else 'curso_completo'
                return f'<a href="../pdf/{other}.pdf#nameddest={stable_id(target)}">{label}</a>'
            return f'<a href="../{escape(target)}">{label}</a>'
        if destination.is_relative_to(REPO) and REMOTE:
            repo_path=destination.relative_to(REPO).as_posix()
            kind='tree' if destination.is_dir() else 'blob'
            return f'<a href="{REMOTE}/{kind}/{COMMIT}/{escape(repo_path)}">{label}</a>'
        return f'<span class="source-ref">{label}</span>'
    content=re.sub(r'<a href="([^"]*)">(.*?)</a>',link,content,flags=re.S)
    def image(match):
        tag=match.group(0)
        src=re.search(r'src="([^"]+)"',tag)
        if not src: return tag
        target=(path.parent/unquote(unescape(src.group(1)))).resolve()
        if target.suffix=='.svg' and target.is_file():
            return inline_svg(target)
        raise ValueError('Unresolved illustration: '+str(target))
    content=re.sub(r'<img\b[^>]*>',image,content)
    content=re.sub(r'<p>(<svg.*?</svg>)</p>\s*<p><em>(.*?)</em></p>',r'<figure class="figure">\1<figcaption>\2</figcaption></figure>',content,flags=re.S)
    content=re.sub(r'<p>(<svg.*?</svg>)</p>',r'<figure class="figure">\1</figure>',content,flags=re.S)
    paragraphs=list(re.finditer(r'<p>(.*?)</p>',content,re.S))
    if paragraphs and any(word in paragraphs[-1].group(1) for word in ['Anterior','Índice','Previous','Index','Volver al índice','Volver al curso']):
        last=paragraphs[-1]
        content=content[:last.start()]+last.group(0).replace('<p>','<p class="chapter-nav">',1)+content[last.end():]
    # Readable wrap points in long function names, excluding preformatted code.
    chunks=re.split(r'(<pre>.*?</pre>)',content,flags=re.S)
    for i in range(0,len(chunks),2):
        chunks[i]=re.sub(r'<code>(.*?)</code>',lambda m:'<code>'+m.group(1).replace('_','_<wbr>')+'</code>',chunks[i],flags=re.S)
    content=''.join(chunks)
    kicker='TALLER DE DEMOS' if rel.startswith('demos/') else 'CURSO / LECCIONES' if rel.startswith('ch') else 'GUÍAS Y MATERIAL DE APOYO'
    if book['lang']=='en': kicker='DEMO WORKSHOP / STUDY EDITION'
    class_name='chapter compact' if rel in COMPACT else 'chapter'
    return f'<article class="{class_name}" id="{prefix}" data-source="{escape(rel)}"><div class="kicker">{kicker}</div>{content}</article>'

def cover_art():
    return '''<svg class="cover-art" viewBox="0 0 1000 460" xmlns="http://www.w3.org/2000/svg" aria-label="World map and a camera window">
<defs><pattern id="cover-grid" width="40" height="40" patternUnits="userSpaceOnUse"><path d="M40 0H0V40" fill="none" stroke="#416274" stroke-width="1"/></pattern></defs>
<rect x="10" y="10" width="650" height="420" rx="10" fill="#234758"/>
<rect x="10" y="10" width="650" height="420" fill="url(#cover-grid)"/>
<path d="M50 65H225V185H120V345H305V235H460V115H610" fill="none" stroke="#5b7882" stroke-width="22"/>
<rect x="180" y="145" width="320" height="200" rx="3" fill="#66cdb1" fill-opacity=".16" stroke="#83e4c8" stroke-width="4"/>
<path d="M500 145L735 100M500 345L735 285" stroke="#82ccb9" stroke-width="2" stroke-dasharray="8 7"/>
<rect x="735" y="100" width="250" height="185" rx="6" fill="#58b69e"/>
<rect x="758" y="130" width="205" height="135" fill="#1b574e"/>
<g fill="#edc773"><rect x="304" y="229" width="8" height="36"/><rect x="332" y="229" width="8" height="36"/><rect x="312" y="237" width="20" height="24"/><rect x="320" y="225" width="4" height="23"/><rect x="835" y="180" width="8" height="36"/><rect x="863" y="180" width="8" height="36"/><rect x="843" y="188" width="20" height="24"/><rect x="851" y="176" width="4" height="23"/></g>
<text x="745" y="345" fill="#bde9db" font-family="DejaVu Sans" font-size="31">320 × 200</text><text x="745" y="390" fill="#83b8b3" font-family="DejaVu Sans Mono" font-size="21">world − camera</text>
</svg>'''

def build(name,book):
    en=book['lang']=='en'
    toc=[]
    previous=None
    for i,rel in enumerate(book['files']):
        group='Demos' if rel.startswith('demos/') else 'Lecciones' if re.match(r'ch\d',rel) else 'Orientación y apéndices'
        if en: group='Demo workshop'
        if group!=previous:
            if previous is not None: toc.append('</ul>')
            toc.append(f'<div class="group">{escape(group)}</div><ul>');previous=group
        chapter_number=re.match(r'ch(\d+)(-paso-a-paso)?\.md',rel)
        if chapter_number:
            number=chapter_number.group(1)
            if chapter_number.group(2): number+='A'
        elif rel.startswith('demos/'):
            effect=rel.split('/')[1]
            number='D'+str(ORDER.index(effect)+1) if effect in ORDER else 'D0' if 'BASES' in rel else 'D'
        else:
            number='A'+str(APPENDICES.index(rel)+1) if rel in APPENDICES else 'I'
        toc.append(f'<li><a href="#{stable_id(rel)}"><span class="toc-num">{number}</span><span>{escape(title_of(rel))}</span></a></li>')
    toc.append('</ul>')
    chapters='\n'.join(chapter_html(rel,book) for rel in book['files'])
    language_css=''
    if en:
        language_css='@page{@top-right{content:"DOCGPT · STUDY EDITION"}@bottom-left{content:"PALETTES · FIXED POINT · TRANSFORMS"}}'
    contents_title='Contents' if en else 'Índice navegable'
    intro='Select a lesson to jump directly to it. Text and vector figures remain searchable and scalable.' if en else 'Pulsa una lección para ir directamente a ella. Puedes buscar texto, ampliar los esquemas y volver a las explicaciones desde sus enlaces.'
    page=f'''<!doctype html><html lang="{book['lang']}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="description" content="{escape(book['description'])}"><title>{escape(book['title'])} · docgpt</title><style>{CSS}{language_css}</style></head><body>
<nav class="screen-toolbar"><a href="#contents">{contents_title}</a><a href="../pdf/{name}.pdf">PDF</a><a href="../camara.html">Laboratorio de cámara</a><a href="../README.md">Markdown</a></nav>
<header class="cover"><p class="series">CutreGame MS-DOS / docgpt</p><h1>{escape(book['title'])}</h1><p class="subtitle">{escape(book['subtitle'])}</p>{cover_art()}<p class="strap">{escape(book['strap'])}</p><p class="description">{escape(book['description'])}</p><p class="edition">EDICIÓN DE ESTUDIO · SEPTIEMBRE 2026</p></header>
<nav class="contents" id="contents"><h1>{contents_title}</h1><p class="intro">{intro}</p>{''.join(toc)}</nav>{chapters}</body></html>'''
    (READ/(name+'.html')).write_text(page)
    return {'html':'lectura/'+name+'.html','pdf':'pdf/'+name+'.pdf','language':book['lang'],'sources':[{'path':rel,'sha256':hashlib.sha256((ROOT/rel).read_bytes()).hexdigest()} for rel in book['files']]}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pdf',action='store_true',help='Also invoke an installed Chrome to create PDFs')
    args=parser.parse_args()
    READ.mkdir(exist_ok=True);PDF.mkdir(exist_ok=True)
    manifest={'edition':'2026-09','source_commit':COMMIT,'books':{name:build(name,b) for name,b in BOOKS.items()}}
    covered={s['path'] for book in manifest['books'].values() for s in book['sources']}
    existing={p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*.md')}
    manifest['markdown_coverage']={'total':len(existing),'included':len(covered),'missing':sorted(existing-covered)}
    if manifest['markdown_coverage']['missing']:
        raise ValueError('Markdown omitted from books: '+repr(manifest['markdown_coverage']['missing']))
    (ROOT/'edicion'/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    print(f'Created {len(BOOKS)} HTML books; {len(covered)}/{len(existing)} Markdown files included.')
    if args.pdf:
        chrome=shutil.which('google-chrome') or shutil.which('chromium') or shutil.which('chromium-browser')
        if not chrome: raise SystemExit('Chrome/Chromium was not found in PATH.')
        for name in BOOKS:
            with tempfile.TemporaryDirectory(prefix='docgpt-print-') as profile:
                cmd=[chrome,'--headless','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--disable-background-networking','--no-first-run','--no-default-browser-check',f'--user-data-dir={profile}','--no-pdf-header-footer',f'--print-to-pdf={PDF/(name+".pdf")}',(READ/(name+'.html')).as_uri()]
                subprocess.run(cmd,check=True,timeout=180)
            print('Created',PDF/(name+'.pdf'))

if __name__=='__main__':
    main()
