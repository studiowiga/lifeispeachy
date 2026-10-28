# apercu rapide hors plugin : fond + inverseur + reperes + LED allumees
import re, math, sys
from PIL import Image, ImageDraw
L=open('Layout.h').read()
bg=Image.open('background.jpg').convert('RGBA')
g=lambda k: [float(x) for x in re.search(k+r' \{ ([\d.]+)f, ([\d.]+)f',L).groups()]
oct_=g('octPos'); strip=Image.open('toggle_strip.png'); fs=strip.width
bg.alpha_composite(strip.crop((0,0,fs,fs)),(int(oct_[0]*2-fs/2),int(oct_[1]*2-fs/2)))
for k,fn in (('glowL','glow_l.png'),('glowR','glow_r.png')):
    r=g(k); bg.alpha_composite(Image.open(fn),(int(r[0]*2),int(r[1]*2)))
d=ImageDraw.Draw(bg)
kp=re.findall(r'\{ ([\d.]+)f, ([\d.]+)f \}', L.split('knobPos[8] =')[1].split(';')[0])
krin=float(re.search(r'knobTopR = ([\d.]+)',L).group(1))
for (x,y),v in zip(kp,[0.7,0.6,0.5,0.75,0.6,0.4,0.55,0.5]):
    x=float(x)*2;y=float(y)*2;a=math.radians(-150+300*v); R=krin*2
    d.line([(x+math.sin(a)*R*0.15,y-math.cos(a)*R*0.15),(x+math.sin(a)*R*0.9,y-math.cos(a)*R*0.9)],fill=(235,235,235),width=int(R*0.09))
p=g('presPos'); prin=float(re.search(r'presTopR = ([\d.]+)',L).group(1))*2
d.line([(p[0]*2,p[1]*2-prin*0.2),(p[0]*2,p[1]*2-prin*0.9)],fill=(235,235,235),width=4)
for k in ('ledL','ledR'):
    c=g(k); d.ellipse([c[0]*2-11,c[1]*2-11,c[0]*2+11,c[1]*2+11],fill=(255,70,50))
bg.convert('RGB').resize((1240,928),Image.LANCZOS).save(sys.argv[1])
