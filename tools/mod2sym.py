"""Convert a single-module library XML export into a symbol XML export.

Usage: python mod2sym.py <module.xml> <symbol.xml>

Takes the module's embedded <SYMBOLDEF> (details, fields and drawing) plus the
fonts/styles it uses and writes it as a <SYMBOL> that File > Import symbols
accepts. Module-level details, extra objects and pin net hints are dropped.
"""
import sys
src, dst = sys.argv[1], sys.argv[2]
L = open(src, encoding='utf-8').read().split('\n')
def idx(pred, start=0):
    for i in range(start, len(L)):
        if pred(L[i].strip()): return i
    raise SystemExit('not found')
top = idx(lambda s: s == '<TinyCAD>')
sd = idx(lambda s: s.startswith('<SYMBOLDEF'), top)
inner = idx(lambda s: s == '<TinyCAD>', sd)
inner_end = idx(lambda s: s == '</TinyCAD>', inner)
resources = L[top+1:sd]                         # FONT/STYLE/FILL shared by the module
header = [s for s in L[sd+1:inner] if not s.strip().startswith('<REF_POINT')]
body = L[inner+1:inner_end]
def dedent(lines, n): return [l[n:] if l.startswith('\t'*n) else l.lstrip('\t') for l in lines]
out = L[:L.index('<Library>')+1]
out += ['\t<SYMBOL>', '\t\t<PPP>1</PPP>', '\t\t<ORIENTATION>0</ORIENTATION>', '\t\t<DETAILS>']
out += ['\t\t\t' + l for l in dedent([h for h in header if not h.strip().startswith(('<PPP>','<GUID>'))], 4)]
out += ['\t\t</DETAILS>', '\t\t<TinyCAD>']
out += ['\t\t\t' + l for l in dedent(resources, 3)]
out += ['\t\t\t' + l for l in dedent(body, 5)]
out += ['\t\t</TinyCAD>', '\t</SYMBOL>', '</Library>', '']
open(dst, 'w', encoding='utf-8', newline='\r\n' if '\r' in open(src,'rb').read().decode() else '\n').write('\n'.join(l.rstrip('\r') for l in out))
