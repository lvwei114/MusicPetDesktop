from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
root=Path(r'E:\Claude works\MusicPet')
image=Image.new('RGBA',(256,256),(0,0,0,0))
draw=ImageDraw.Draw(image)
draw.rounded_rectangle((4,4,252,252),radius=72,fill=(66,139,133,255))
# A single connected eighth note, independent of installed fonts.
draw.ellipse((70,146,125,181),fill='white')
draw.rounded_rectangle((113,61,127,165),radius=6,fill='white')
draw.polygon([(120,60),(169,81),(176,109),(165,128),(157,127),(162,107),(149,96),(120,85)],fill='white')
image.save(root/'MusicPet.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
