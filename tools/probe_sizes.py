import sys, re, collections
G = collections.Counter(); S = []
for ln in open(sys.argv[1]):
    p = ln.split()
    if len(p) < 5 or p[-3] not in ('.text', '.rodata') and not p[-3].startswith('.text') and not p[-3].startswith('.rodata'):
        continue
    try: sz = int(p[-2], 16)
    except ValueError: continue
    name = p[-1]
    if not sz: continue
    S.append((sz, name))
    m = re.match(r'_ZN\d+(\w+?)\d|([a-zA-Z]+?)_', name)
    if name.startswith('_ZN') or name.startswith('_ZL') or name.startswith('_ZZ'): key = 'C++:' + (re.findall(r'\d+([A-Za-z]\w*)', name) or ['?'])[0][:14]
    else: key = (name.split('_')[0] or name)[:14]
    G[key] += sz
print('total sized', sum(s for s, _ in S))
for k, v in G.most_common(120): print(f'{v:8d} {k}')
print('--- biggest')
for sz, n in sorted(S, reverse=True)[:150]: print(f'{sz:8d} {n}')

import fnmatch
for pat in ('sp_*','ann_*','uh_*','usbh*','px_*','fm6*','FM6*','d9*','drum*','help*','speak*','mo_*','or_*','cz*','harm*','arp*','sid*'):
    print(pat, sum(sz for sz, n in S if fnmatch.fnmatch(n, pat)))
