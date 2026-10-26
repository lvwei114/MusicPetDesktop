from pathlib import Path
from PIL import Image,ImageDraw
import shutil,struct,zlib
r=Path(__file__).resolve().parent/'motion-fixtures'
r.mkdir(exist_ok=True)
frames=[]
for i in range(4):
    im=Image.new('RGBA',(96,64),(0,0,0,0));d=ImageDraw.Draw(im)
    d.rectangle((5+i*15,8,20+i*15,50),fill=(255,40*i,30,255 if i%2==0 else 128));frames.append(im)
frames[0].save(r/'fixture.gif',save_all=True,append_images=frames[1:],duration=[200]*4,loop=0,disposal=2,transparency=0,optimize=True)
import io
def chunk(kind,data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
def pngdata(im):
    f=io.BytesIO();im.save(f,format='PNG');b=f.getvalue();parts=[];pos=8
    while pos<len(b):
        n=struct.unpack_from('>I',b,pos)[0];parts.append((b[pos+4:pos+8],b[pos+8:pos+8+n]));pos+=n+12
    return dict(parts)[b'IHDR'],b''.join(data for kind,data in parts if kind==b'IDAT')
# Explicitly build APNG chunks: partial rectangles, half alpha, both blend
# operations and all disposal modes. Reference composition uses Pillow's
# alpha_composite, independently of its animated-PNG plugin's blend behavior.
canvas=Image.new('RGBA',(96,64));sequence=0
header,_=pngdata(frames[0]);apng=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',header)+chunk(b'acTL',struct.pack('>II',4,0))
specs=[(frames[0],0,0,0,0),(Image.new('RGBA',(30,40),(0,220,80,128)),12,4,1,1),(Image.new('RGBA',(20,30),(30,70,255,180)),45,12,2,1),(frames[3],0,0,0,0)]
for i,(im,x,y,dispose,blend) in enumerate(specs):
    control=struct.pack('>IIIIIHHBB',sequence,im.width,im.height,x,y,200,1000,dispose,blend);sequence+=1
    apng+=chunk(b'fcTL',control);_,compressed=pngdata(im)
    if i==0:apng+=chunk(b'IDAT',compressed)
    else:apng+=chunk(b'fdAT',struct.pack('>I',sequence)+compressed);sequence+=1
    before=canvas.copy()
    if blend==0:canvas.paste(im,(x,y))
    else:canvas.alpha_composite(im,(x,y))
    rgba=canvas.tobytes();out=bytearray()
    for p in range(0,len(rgba),4):
        rr,g,b,a=rgba[p:p+4];out.extend(((b*a+127)//255,(g*a+127)//255,(rr*a+127)//255,a))
    (r/f'golden-{i}.bgra').write_bytes(out)
    if dispose==1:canvas.paste((0,0,0,0),(x,y,x+im.width,y+im.height))
    elif dispose==2:canvas=before
apng+=chunk(b'IEND',b'');(r/'fixture.apng').write_bytes(apng)
shutil.copyfile(r/'fixture.apng',r/'fixture.png')
b=bytearray((r/'fixture.apng').read_bytes());b[-1]^=1;(r/'corrupt.apng').write_bytes(b)
(r/'invalid.apng').write_bytes(b'\x89PNG\r\n\x1a\n'+struct.pack('>I',0x7fffffff)+b'fcTL')
many=[Image.new('RGB',(16,16),(i,255-i,(i*17)%256)) for i in range(130)]
many[0].save(r/'cache-limit.gif',save_all=True,append_images=many[1:],duration=20,loop=0,optimize=False)
layout=[]
for i in range(4):
    im=Image.new('RGBA',(96,64));ImageDraw.Draw(im).rectangle((20,10,70,54),fill=(40+i*40,150,70,255));layout.append(im)
layout[0].save(r/'layout-idle.png')
layout[0].save(r/'layout.apng',format='PNG',save_all=True,append_images=layout[1:],duration=150,disposal=0,blend=0,loop=0)
