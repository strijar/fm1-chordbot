#!/usr/bin/env python3
"""Compile the recovered ChordBot data into const flash tables. No YAML on the FM-1."""
import json
from pathlib import Path
import yaml

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/gen'
ROOTS = ['Cb','C','C#','Db','D','D#','Eb','E','E#','Fb','F','F#','Gb','G','G#','Ab','A','A#','Bb','B','B#']
def load(name):
    return yaml.safe_load((ROOT / 'data' / name).read_text())
def q(v):
    return json.dumps(str(v), ensure_ascii=True)
def seq(xs):
    return '{' + ','.join(map(str, xs)) + '}'
def part(t, v, flags=0):
    assert 0 <= v <= 10
    return seq([t, v, flags])

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    tracks = load('tracks.yaml')['tracks']
    types = load('chords.yaml')['chords']
    presets = load('presets.yaml')['presets']
    ids = {t['id'] for t in tracks}
    lines = ['/* Generated from the YAML catalog; do not edit. */']
    for i,t in enumerate(tracks):
        assert 1 <= len(t['patterns']) <= 8 and t.get('delay',0) == 0
        assert 24 % t.get('delta',6) == 0
        ps = []
        for p in t['patterns']:
            assert -128 <= p[0] <= 127 and 0 < len(p[1]) <= 256
            ps.append(seq([p[0],int(p[2]) if len(p)>2 else 0,len(p[1]),q(p[1])]))
        lines.append(f'static const cb_pattern patterns_{i}[] = '+seq(ps)+';')
    lines.append('const cb_track cb_tracks[] = {')
    for i,t in enumerate(tracks):
        mode=['chords','bass','guitar','drums'].index(t.get('type','chords'))
        lines.append(seq([q(t['name']),q(t.get('time','4/4')),f'patterns_{i}',t['id'],t.get('leading',0),t['category'],t.get('instrument',0),t.get('delta',6),len(t['patterns']),mode,int(t.get('invert',True)),int(t.get('sustain',False)),int(mode==3 or t.get('continous',False)),t.get('transpose',0),t.get('volume',0)])+',')
    lines.append('};\nconst cb_type cb_types[] = {')
    for t in types:
        assert len(t['notes']) <= 12
        lines.append(seq([q(t['name']),len(t['notes']),seq(t['notes'] or [0])])+',')
    lines.append('};\nconst cb_preset cb_presets[] = {')
    for p in presets:
        assert len(p['tracks']) <= 12 and all(x[0] in ids for x in p['tracks'])
        lines.append(seq([q(p['name']),q(p.get('time','4/4')),p['id'],len(p['tracks']),p.get('reverb',0),p.get('chorus',0),int(p.get('legacy',False)),seq([part(*x) for x in p['tracks']])])+',')
    lines.append('};\nconst cb_song cb_demos[] = {')
    names=[x['name'] for x in types]
    for path in sorted((ROOT/'data/demos').glob('*.yaml')):
        d=yaml.safe_load(path.read_text()); ss=[]
        assert len(d['sections']) <= 16
        for s in d['sections']:
            assert len(s['chords']) <= 32 and len(s['parts']) <= 12
            cc=[]
            for c in s['chords']:
                cc.append(seq([ROOTS.index(c['root']),names.index(c['type']),c['beats'],ROOTS.index(c['bass']) if c.get('bass') else -1,c.get('inversion',-1)]))
            pp=[part(p['track'],p.get('volume',5),int(p.get('mute',False))+2*int(p.get('solo',False))) for p in s['parts']]
            ss.append(seq([q(s['name'][:21]),len(cc),len(pp),s.get('reverb',8),s.get('chorus',4),1,0,seq(pp),seq(cc)]))
        lines.append(seq(['0x43424631u',1,d['tempo'],q(d['name'][:23]),len(ss),'{0}',seq(ss)])+',')
    lines.append('};')
    for name in ['tracks','types','presets','demos']:
        lines.append(f'const unsigned cb_n{name} = sizeof(cb_{name})/sizeof(cb_{name}[0]);')
    (OUT/'catalog.inc').write_text('\n'.join(lines)+'\n')
    print(f'catalog: {len(tracks)} tracks, {len(presets)} presets, {len(types)} chord types')
if __name__ == '__main__':
    main()
