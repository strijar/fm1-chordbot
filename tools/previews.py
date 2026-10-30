#!/usr/bin/env python3
"""Convert simulator screen captures to PNG; make a contact sheet for review."""
from pathlib import Path
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parents[1]
out=root/'build/previews'
out.mkdir(parents=True,exist_ok=True)
names=['main','chords','inversion','mixer','instruments','sections','presets']
sheet=Image.new('RGB',(720,810),'#d6dad5')
draw=ImageDraw.Draw(sheet)
for i,name in enumerate(names):
    p=root/'build/tests/smoke'/f'{name}.ppm'
    im=Image.open(p)
    im.save(out/f'{name}.png')
    x=i%3*240; y=i//3*270
    draw.text((x+5,y+4),name,fill='#304e5c')
    sheet.paste(im,(x,y+25))
sheet.save(out/'overview.png')
print(out/'overview.png')
