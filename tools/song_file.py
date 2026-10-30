#!/usr/bin/env python3
"""YAML <-> FM-1 song, and replace/extract one slot in a downloaded backup.
All conversions validate limits; import never truncates sections or chords.
No device access: use chordbot_backup.py separately to transfer the result.
"""
import argparse
from pathlib import Path
import struct
import sys
import yaml
import chordbot_backup as backup

DATA = Path(__file__).resolve().parents[1] / 'data'
ROOTS = ['Cb','C','C#','Db','D','D#','Eb','E','E#','Fb','F','F#','Gb','G','G#','Ab','A','A#','Bb','B','B#']
TYPES = [c['name'] for c in yaml.safe_load((DATA/'chords.yaml').read_text())['chords']]
IDS = {t['id'] for t in yaml.safe_load((DATA/'tracks.yaml').read_text())['tracks']}
MAGIC, SIZE = 0x43424631, 3812

def integer(v, lo, hi, what):
    if not isinstance(v, int) or not lo <= v <= hi:
        raise ValueError(f'{what}: expected {lo}..{hi}, got {v!r}')
    return v

def name(v, size):
    if not isinstance(v, str):
        raise ValueError('name must be a string')
    b=v.encode('ascii')
    if not b or len(b)>=size or any(c<32 or c>126 for c in b):
        raise ValueError(f'name {v!r}: use 1..{size-1} printable ASCII characters')
    return b+b'\0'*(size-len(b))

def encode(d):
    if d.get('version')!=1 or d.get('format') not in ('chordbot-linux','chordbot-fm1'):
        raise ValueError('expected chordbot-linux or chordbot-fm1 version 1 YAML')
    sections=d['sections']; integer(len(sections),1,16,'section count')
    out=bytearray(struct.pack('<IHH24sB3x',MAGIC,1,integer(d['tempo'],30,300,'tempo'),name(d['name'],24),len(sections)))
    for s in sections:
        chords=s['chords']; parts=s.get('parts',[])
        integer(len(chords),1,32,'chords in section'); integer(len(parts),0,12,'parts in section')
        out+=struct.pack('<22s6B',name(s['name'],22),len(chords),len(parts),integer(s.get('reverb',8),0,127,'reverb'),integer(s.get('chorus',4),0,127,'chorus'),integer(s.get('repeat',1),1,16,'repeat'),0)
        for p in parts:
            if p['track'] not in IDS: raise ValueError(f'unknown track ID {p["track"]}')
            out+=struct.pack('<HBB',p['track'],integer(p.get('volume',5),0,10,'volume'),int(bool(p.get('mute',False)))+2*int(bool(p.get('solo',False))))
        out+=bytes((12-len(parts))*4)
        for c in chords:
            out+=struct.pack('<BBBbb',ROOTS.index(c['root']),TYPES.index(c['type']),integer(c['beats'],1,32,'duration'),ROOTS.index(c['bass']) if c.get('bass') else -1,integer(c.get('inversion',-1),-1,35,'inversion'))
        out+=bytes((32-len(chords))*5)
    out+=bytes((16-len(sections))*236)
    assert len(out)==SIZE
    return bytes(out)

def decode(data, linux=False):
    if len(data)!=SIZE: raise ValueError(f'wrong song size: {len(data)}, expected {SIZE}')
    magic,ver,tempo,title,count=struct.unpack_from('<IHH24sB3x',data)
    if magic!=MAGIC or ver!=1: raise ValueError('not a ChordBot FM-1 song')
    def txt(raw):
        if raw[-1]: raise ValueError('unterminated name')
        return raw.split(b'\0',1)[0].decode('ascii')
    d=dict(format='chordbot-linux' if linux else 'chordbot-fm1',version=1,name=txt(title),tempo=tempo,sections=[])
    integer(count,1,16,'section count')
    for i in range(count):
        pos=36+i*236
        title,nc,np,rev,cho,repeat,_=struct.unpack_from('<22s6B',data,pos)
        integer(nc,1,32,'chord count'); integer(np,0,12,'part count'); integer(repeat,1,16,'repeat')
        s=dict(name=txt(title),reverb=rev,chorus=cho,parts=[],chords=[])
        for j in range(np):
            track,vol,flags=struct.unpack_from('<HBB',data,pos+28+j*4)
            integer(flags,0,3,'part flags')
            p=dict(track=track,volume=vol)
            if flags&1: p['mute']=True
            if flags&2: p['solo']=True
            s['parts'].append(p)
        for j in range(nc):
            root,kind,duration,bass,inv=struct.unpack_from('<BBBbb',data,pos+76+j*5)
            integer(root,0,20,'root'); integer(kind,0,len(TYPES)-1,'chord type'); integer(bass,-1,20,'bass')
            c=dict(root=ROOTS[root],type=TYPES[kind],beats=duration)
            if bass>=0: c['bass']=ROOTS[bass]
            if inv>=0: c['inversion']=inv
            else: integer(inv,-1,-1,'inversion')
            s['chords'].append(c)
        if linux:
            d['sections'] += [s]*repeat
        else:
            if repeat!=1: s['repeat']=repeat
            d['sections'].append(s)
    if linux and len(d['sections'])>32: raise ValueError('expanded repeats exceed the Linux limit of 32 sections')
    # Reuse the encoder for semantic checks; each Linux-expanded section separately.
    if linux:
        for s in d['sections']: encode(dict(d,sections=[s]))
    else: encode(d)
    return d

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    sub=ap.add_subparsers(dest='cmd',required=True)
    for cmd in ('pack','unpack'):
        p=sub.add_parser(cmd); p.add_argument('source',type=Path); p.add_argument('output',type=Path)
        if cmd=='unpack': p.add_argument('--linux',action='store_true')
    p=sub.add_parser('put',help='put YAML into one slot of an existing backup; preserve other slots')
    p.add_argument('backup',type=Path); p.add_argument('song',type=Path); p.add_argument('output',type=Path); p.add_argument('--slot',type=int,required=True,choices=range(1,17))
    p=sub.add_parser('get',help='extract a backup slot to YAML')
    p.add_argument('backup',type=Path); p.add_argument('output',type=Path); p.add_argument('--slot',type=int,required=True,choices=range(1,17)); p.add_argument('--linux',action='store_true')
    a=ap.parse_args()
    if a.cmd=='pack': a.output.write_bytes(encode(yaml.safe_load(a.source.read_text())))
    elif a.cmd=='unpack': a.output.write_text(yaml.safe_dump(decode(a.source.read_bytes(),a.linux),sort_keys=False))
    else:
        flags,objs=backup.parse(a.backup.read_bytes()); slot=a.slot+2
        if a.cmd=='get': a.output.write_text(yaml.safe_dump(decode(dict(objs)[slot],a.linux),sort_keys=False))
        else:
            payload=encode(yaml.safe_load(a.song.read_text()))
            a.output.write_bytes(backup.build([(o,payload if o==slot else data) for o,data in objs],flags))
    print(a.output)

if __name__=='__main__':
    try: main()
    except (ValueError,KeyError,IndexError,UnicodeError,backup.BackupError) as e:
        sys.exit(f'song_file: {e}')
