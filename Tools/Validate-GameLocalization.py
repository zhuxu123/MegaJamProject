"""Validate all game-visible Chinese has English text; exercise dynamic format captures.
Run from any directory: python Tools/Validate-GameLocalization.py
Use --probes to write runtime cases consumed by -GameLocalizationAudit.
"""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HAN = re.compile(r'[\u3400-\u9fff]')
TOKEN = re.compile(r'\{[^{}]+\}|%(?:[-+0 #]*\d*(?:\.\d+)?)?[dsf]')
FIELDS = {'title','description','name','label','objective','client_statement','summary','headlines','paper_lead','note','evidence'}


def visible_texts():
    files = list((ROOT/'Script/Marketing').glob('*.as')) + list((ROOT/'Source/MegaJamProject').glob('*.cpp'))
    for path in files:
        if 'Tests' in path.name: continue
        for line in path.read_text(encoding='utf-8-sig').splitlines():
            if line.lstrip().startswith('//'): continue
            # Source strings, not identifiers, comments or asset filenames.
            for value in re.findall(r'"((?:\\.|[^"\\])*)"',line):
                if HAN.search(value): yield value, str(path.relative_to(ROOT))
    def walk(data, field=''):
        if isinstance(data, dict):
            for key,value in data.items(): yield from walk(value,key)
        elif isinstance(data,list):
            for value in data: yield from walk(value,field)
        elif isinstance(data,str) and field in FIELDS and HAN.search(data): yield data
    for name in ['story.json','levels.json','Audio/music.json']:
        for value in walk(json.loads((ROOT/'Content/SuperstarStory'/name).read_text(encoding='utf-8-sig'))):
            yield value,name


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--probes',action='store_true');args=parser.parse_args()
    entries=json.loads((ROOT/'Content/Localization/en.json').read_text(encoding='utf-8'))['entries']
    catalog={e['source']:e for e in entries}
    assert len(catalog)==len(entries),'Duplicate source keys'
    missing=[(src,path) for src,path in visible_texts() if src not in catalog]
    assert not missing,'Missing translations:\n'+'\n'.join(f'{p}: {s}' for s,p in missing)
    probes=[]
    for e in entries:
        source,en=e['source'],e['en']
        assert en.strip() and not HAN.search(en),source
        assert bool(TOKEN.search(source)) == ('pattern' in e), 'Format pattern missing: '+source
        if 'pattern' in e:
            expected_args={str(i) for i in range(e['args'])}
            assert set(re.findall(r'\{(\d+)\}',en))==expected_args,source
            tokens=list(TOKEN.finditer(source));assert len(tokens)==e['args'],source
            values=['林晚' if m.group().endswith('s') or 'Title' in m.group() or 'BeatLabel' in m.group() else str(7+i) for i,m in enumerate(tokens)]
            index=iter(values);sample=TOKEN.sub(lambda m:next(index),source)
            match=re.fullmatch(e['pattern'],sample);assert match and list(match.groups())==values,source
            expected=en
            for i,v in reversed(list(enumerate(values))):expected=expected.replace('{'+str(i)+'}',catalog.get(v,{}).get('en',v))
            probes.append({'source':sample,'en':expected})
        else:probes.append({'source':source,'en':en})
    # Runtime concatenation + translated variable inside a pattern.
    first_objective=json.loads((ROOT/'Content/SuperstarStory/levels.json').read_text(encoding='utf-8'))['levels'][0]['objective']
    probes += [
        {'source':'任务：'+first_objective,'en':'Objective: '+catalog[first_objective]['en']},
        {'source':'BGM · 镜头背后','en':'BGM · Behind the Lens'},
    ]
    if args.probes:
        path=ROOT/'Saved/Localization/probes.json';path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(json.dumps(probes,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'PASS: {len(entries)} translations cover all visible source/JSON text; {len(probes)} exact, formatted and compound cases.')

if __name__=='__main__':main()
