"""
PEACHY (Studio Wiga) - visuel du plugin, entierement synthetise (design original noir & rouge).

Disposition des commandes reprise de la pedale d'origine (preampli + fuzz, format double) ; habillage original :
facade rouge serigraphiee, encre noire (coulures, peche en trame, eclaboussures), nom « Peachy », logo Studio Wiga
noir en bas. Une seule lumiere pour toute la pedale : en haut a gauche (ombres portees vers le bas a droite).

Repere du plugin : 1240 x 928 unites (comme Ring Think), images en 2x (2480 x 1856).
Sorties (racine du depot) :
  background.jpg        fond complet (boitier, facade, potards fixes, footswitchs, LED eteintes, jacks, logo)
  toggle_strip.png      inverseur OCTAVE : 3 images carrees empilees (haut = ON, intermediaire, bas = OFF)
  glow_l.png / glow_r.png  facade eclairee par chaque LED (canal alpha = seule la lumiere ajoutee)
  Layout.h              reperes des commandes
Habillage sans motif central ni nom (demande de l'utilisateur) : coulures en haut, rouge qui fonce vers les coins bas.
Usage : python3 tools/make_art.py
"""
import os, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
AS = 2                                   # pixels par unite (images 2x)
UW, UH = 1240, 928
W, H = UW * AS, UH * AS
rng = np.random.default_rng(7)

L = np.array([-0.46, -0.62, 0.64]); L /= np.linalg.norm(L)        # vers la lumiere (x droite, y bas, z vers l'oeil)
V = np.array([0.0, 0.0, 1.0]); HV = (L + V) / np.linalg.norm(L + V)

# ------------------------------------------------------------------ geometrie (unites)
PX0, PY0, PW, PH, PR = 78.0, 56.0, 1084.0, 816.0, 44.0          # boitier
def P(u, v): return (PX0 + u * PW, PY0 + v * PH)                 # coordonnees relatives (mesurees sur la pedale d'origine)

KNOB_R = 52.0
KNOBS = {   # id : (u, v, label)
    'master':     (0.198, 0.180, 'master'),
    'gain':       (0.389, 0.180, 'gain'),
    'volume':     (0.568, 0.180, 'volume'),
    'distortion': (0.840, 0.180, 'distortion'),
    'bass':       (0.101, 0.417, 'bass'),
    'mid':        (0.286, 0.417, 'mid'),
    'treble':     (0.480, 0.417, 'treble'),
    'tone':       (0.733, 0.417, 'tone'),
}
PRES = P(0.398, 0.600); PRES_R = 22.0
OCT = P(0.864, 0.352)
LED_L, LED_R = P(0.218, 0.648), P(0.772, 0.648)
FS_L, FS_R = P(0.169, 0.862), P(0.840, 0.862)

# ------------------------------------------------------------------ outils
YY, XX = np.mgrid[0:H, 0:W].astype(np.float32)
UX, UY = (XX + 0.5) / AS, (YY + 0.5) / AS

def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)

def rrect_sd(x, y, x0, y0, w, h, r):
    """distance signee (unites, negative a l'interieur) a un rectangle arrondi"""
    cx, cy = x0 + w / 2, y0 + h / 2
    qx = np.abs(x - cx) - (w / 2 - r); qy = np.abs(y - cy) - (h / 2 - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r

def fbm(shape, scales, amps, seed=0):
    r = np.random.default_rng(seed); out = np.zeros(shape, np.float32)
    for s, a in zip(scales, amps):
        n = r.standard_normal((max(2, shape[0] // s + 2), max(2, shape[1] // s + 2))).astype(np.float32)
        out += a * cv2.resize(n, (shape[1], shape[0]), interpolation=cv2.INTER_CUBIC)
    return out

def over(dst, rgba, x0, y0):
    """compose un sprite RGBA (float) sur dst a la position pixel (x0, y0)"""
    h, w = rgba.shape[:2]
    X0, Y0 = int(round(x0)), int(round(y0))
    sx0, sy0 = max(0, -X0), max(0, -Y0)
    ex, ey = min(w, W - X0), min(h, H - Y0)
    a = rgba[sy0:ey, sx0:ex, 3:4] / 255.0
    reg = dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex]
    dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex] = reg * (1 - a) + rgba[sy0:ey, sx0:ex, :3] * a

# ------------------------------------------------------------------ 1. surface autour de la pedale
img = np.zeros((H, W, 3), np.float32)
base = 13.0 + 7.0 * np.exp(-(((UX - 300) / 900) ** 2 + ((UY - 150) / 700) ** 2))
img[:] = (base + fbm((H, W), [3, 40], [1.2, 1.0], 1))[..., None] * np.array([1.0, 0.98, 0.97])

sd = rrect_sd(UX, UY, PX0, PY0, PW, PH, PR)
body = np.clip(0.5 - sd * AS, 0, 1)                     # couverture anti-crenelee

# ombre portee du boitier (lumiere en haut a gauche) + ombre de contact
sh_m = cv2.GaussianBlur((rrect_sd(UX - 16, UY - 22, PX0, PY0, PW, PH, PR) < 0).astype(np.float32), (0, 0), 22 * AS)
ct_m = cv2.GaussianBlur((rrect_sd(UX - 3, UY - 4, PX0, PY0, PW, PH, PR) < 0).astype(np.float32), (0, 0), 4 * AS)
img *= (1 - 0.72 * sh_m - 0.35 * ct_m)[..., None]

# ------------------------------------------------------------------ 2. boitier : alu peint noir satine, arete arrondie
EDGE = 9.0
d_in = np.clip(-sd, 0, None)
hgt = np.where(d_in < EDGE, np.sqrt(np.clip(EDGE ** 2 - (EDGE - d_in) ** 2, 0, None)), EDGE)
gy, gx = np.gradient(cv2.GaussianBlur(hgt, (0, 0), 1.0), 1.0 / AS)
nrm = np.dstack([-gx, -gy, np.ones_like(hgt)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
ndl = np.clip(nrm @ L, 0, 1); ndh = np.clip(nrm @ HV, 0, 1)
paint_black = np.array([19.0, 18.0, 20.0])
encl = paint_black * (0.55 + 0.75 * ndl)[..., None] + (70 * ndh ** 40 + 18 * ndh ** 6)[..., None]
encl += fbm((H, W), [2], [1.5], 3)[..., None]
img = img * (1 - body[..., None]) + encl * body[..., None]

# ------------------------------------------------------------------ 3. facade rouge serigraphiee
FI = 15.0                                                  # retrait de l'impression par rapport au bord
fsd = rrect_sd(UX, UY, PX0 + FI, PY0 + FI, PW - 2 * FI, PH - 2 * FI, PR - FI + 4)
edge_noise = fbm((H, W), [3, 14], [0.5, 0.9], 11)
face = np.clip(0.5 - (fsd + edge_noise * 0.9) * AS, 0, 1)
red = np.array([166.0, 22.0, 27.0])
mott = 1.0 + 0.05 * fbm((H, W), [60, 180], [0.6, 0.8], 5) + 0.03 * fbm((H, W), [4], [1.0], 6)
light = 1.0 + 0.07 * np.clip(1 - (UX - PX0) / PW * 0.6 - (UY - PY0) / PH * 0.6, -1, 1)   # leger degrade vers la lumiere
sheen = 20 * np.exp(-(((UX - 330) / 420) ** 2 + ((UY - 170) / 300) ** 2))               # satin : reflet tres large
redc = red[None, None, :] * (mott * light)[..., None] + sheen[..., None] * np.array([1.0, 0.6, 0.6])
# usure : quelques micro-rayures plus claires
scr = np.zeros((H, W), np.float32)
for _ in range(70):
    x0, y0 = rng.uniform(PX0 + 30, PX0 + PW - 30) * AS, rng.uniform(PY0 + 30, PY0 + PH - 30) * AS
    a = rng.uniform(0, math.pi); l = rng.uniform(6, 40) * AS
    cv2.line(scr, (int(x0), int(y0)), (int(x0 + l * math.cos(a)), int(y0 + l * math.sin(a))), float(rng.uniform(0.2, 0.6)), 1, cv2.LINE_AA)
redc += cv2.GaussianBlur(scr, (0, 0), 0.6)[..., None] * np.array([40.0, 22.0, 22.0])
dark = np.clip((UY - (PY0 + 0.42 * PH)) / (0.58 * PH), 0, 1) ** 1.4
dark *= np.clip(np.abs(UX - (PX0 + PW / 2)) / (PW * 0.42), 0, 1) ** 1.2 * 0.9 + 0.12
redc *= (1 - 0.84 * np.clip(dark, 0, 1))[..., None]
img = img * (1 - face[..., None]) + redc * face[..., None]

# ------------------------------------------------------------------ 4. encre noire : masque (sur-echantillonne 2x)
SS = 2
INK = Image.new('L', (W * SS, H * SS), 0)
dr = ImageDraw.Draw(INK)
K = AS * SS
def U2(x, y): return (x * K, y * K)

# keep-out : zones des commandes (l'encre y est permise, mais pas les eclaboussures)
keep = [(P(u, v), KNOB_R + 18) for (u, v, _) in KNOBS.values()] + [(PRES, 40), (OCT, 45), (LED_L, 30), (LED_R, 30), (FS_L, 70), (FS_R, 70)]
def free(x, y, r):
    for (cx, cy), rr in keep:
        if math.hypot(x - cx, y - cy) < rr + r: return False
    return True

# 4a. peinture noire versee depuis le haut : bande epaisse ondulee + grosses coulures qui descendent loin
#     (elles passent derriere les potards ; les inscriptions qu'elles croisent sont reservees en rouge)
topy = PY0 + FI
def band_y(x):
    return topy + 26 + 9 * math.sin(x * 0.021 + 0.7) + 6 * math.sin(x * 0.057 + 2.0) + 3 * math.sin(x * 0.13)
pts = [U2(PX0, topy - 14)] + [U2(x, band_y(x)) for x in np.linspace(PX0, PX0 + PW, 400)] + [U2(PX0 + PW, topy - 14)]
dr.polygon(pts, fill=255)
# (x, longueur sous la bande, largeur)
DRIPS = [(112, 255, 24), (160, 92, 13), (205, 40, 10), (228, 175, 26), (372, 58, 12), (408, 26, 9),
         (441, 470, 23), (558, 30, 11), (592, 150, 20), (618, 52, 10), (782, 560, 32),
         (828, 175, 16), (866, 70, 12), (884, 150, 20), (1106, 385, 27), (1142, 150, 15)]
def drip(x, ln, w0):
    y0 = band_y(x) - 6
    n = 160
    left, right = [], []
    ph = x * 0.37
    for i in range(n + 1):
        t = i / n
        y = y0 + ln * t
        # col evase sous la bande, corps qui s'affine puis se renfle avant la goutte
        w = w0 * (0.78 + 0.55 * math.exp(-t * 14) - 0.22 * t + 0.10 * math.sin(t * 7 + ph))
        cx = x + 2.2 * math.sin(t * 4.0 + ph) * (w0 / 20)
        left.append(U2(cx - w / 2, y)); right.append(U2(cx + w / 2, y))
    dr.polygon(left + right[::-1], fill=255)
    # goutte terminale (un peu plus large, en forme de larme)
    bw = w0 * (0.78 - 0.22 + 0.10 * math.sin(7 + ph)) * 1.32
    cx = x + 2.2 * math.sin(4.0 + ph) * (w0 / 20)
    yb = y0 + ln
    dr.ellipse([U2(cx - bw / 2, yb - bw * 0.55), U2(cx + bw / 2, yb + bw * 0.62)], fill=255)
    # petite goutte detachee sous les longues coulures
    if ln > 200:
        r_ = w0 * 0.22
        dr.ellipse([U2(cx - r_, yb + bw * 0.62 + w0 * 0.55), U2(cx + r_, yb + bw * 0.62 + w0 * 0.55 + 2.4 * r_)], fill=255)
for x, ln, w0 in DRIPS:
    drip(x, ln, w0)
# accumulations de peinture dans la bande (petites bosses vers le bas entre les coulures)
for x in np.linspace(PX0 + 30, PX0 + PW - 30, 26):
    r_ = rng.uniform(6, 13)
    dr.ellipse([U2(x - r_ * 1.6, band_y(x) - r_), U2(x + r_ * 1.6, band_y(x) + r_ * 0.75)], fill=255)

FONT_LAB = os.path.join(ROOT, 'assets', 'fonts', 'SpecialElite-Regular.ttf')

# 4d. inscriptions des commandes (Special Elite, minuscules)
labs = Image.new('L', INK.size, 0); dlab = ImageDraw.Draw(labs)
flab = ImageFont.truetype(FONT_LAB, int(31 * K))
LABEL_POS = {}
for kid, (u, v, txt) in KNOBS.items():
    x, y = P(u, v)
    LABEL_POS[kid] = (x, y - KNOB_R - 22)
    dlab.text(U2(x, y - KNOB_R - 22), txt, font=flab, fill=255, anchor='mm')
dlab.text(U2(OCT[0], OCT[1] - 62), "octave", font=flab, fill=255, anchor='mm')
dlab.text(U2(PRES[0] + 36, PRES[1] + 26), "presence", font=ImageFont.truetype(FONT_LAB, int(25 * K)), fill=255, anchor='lm')
lab_ink = np.asarray(labs, np.float32) / 255.0

# 4e. eclaboussures (hors commandes) : quelques gerbes autour de la peche et du nom, brume fine pres des footswitchs
spl = Image.new('L', INK.size, 0); dsp = ImageDraw.Draw(spl)
def splash(cx, cy, n, spread, rmax):
    for _ in range(n):
        a = rng.uniform(0, 2 * math.pi); d = abs(rng.normal(0, spread))
        r = rmax * math.exp(-d / spread) * rng.uniform(0.25, 1.0)
        x, y = cx + d * math.cos(a), cy + d * math.sin(a)
        if r < 0.7 or not free(x, y, r): continue
        dsp.ellipse([U2(x - r, y - r), U2(x + r, y + r)], fill=255)
        if r > 2.5 and rng.uniform() < 0.4:          # trainee
            for t in np.linspace(0, 1, 8):
                rr = r * (1 - 0.8 * t) * 0.55
                xx, yy = x + math.cos(a) * r * 3 * t, y + math.sin(a) * r * 3 * t
                dsp.ellipse([U2(xx - rr, yy - rr), U2(xx + rr, yy + rr)], fill=255)
splash(430, 690, 60, 50, 7)
splash(800, 700, 60, 45, 6)
splash(1080, 480, 50, 40, 5)
splash(190, 560, 50, 45, 5)
splash(250, 300, 25, 30, 4)
for _ in range(1800):                                   # brume (aerographe) dans les coins bas
    side = rng.integers(2)
    cx = PX0 + (40 if side == 0 else PW - 40); cy = PY0 + PH - 30
    x = cx + rng.normal(0, 90) * (1 if side == 0 else -1) * 0.8; y = cy - abs(rng.normal(0, 70))
    r = rng.uniform(0.4, 1.6)
    if free(x, y, r) and rrect_sd(np.float32(x), np.float32(y), PX0 + FI, PY0 + FI, PW - 2 * FI, PH - 2 * FI, PR - FI) < -2:
        dsp.ellipse([U2(x - r, y - r), U2(x + r, y + r)], fill=255)
spl_ink = np.asarray(spl, np.float32) / 255.0

paint2 = np.asarray(INK, np.float32) / 255.0                          # peinture versee (relief brillant)
paint = cv2.resize(paint2, (W, H), interpolation=cv2.INTER_AREA)
lab_mask = cv2.resize(lab_ink, (W, H), interpolation=cv2.INTER_AREA)
spl_mask = cv2.resize(spl_ink, (W, H), interpolation=cv2.INTER_AREA)
# inscriptions : noires sur le rouge, reservees (rouges) dans la peinture noire
ink = np.maximum(paint * (1 - lab_mask), lab_mask * (1 - paint))
ink = np.maximum(ink, spl_mask * (1 - lab_mask))
# serigraphie usee : micro-manques dans les aplats imprimes (pas dans la peinture epaisse)
wear = cv2.GaussianBlur((fbm((H, W), [1, 3], [1.0, 0.6], 21) > 2.05).astype(np.float32), (0, 0), 0.5)
ink = ink * (1 - wear * (1 - lab_mask) * (1 - paint) * 0.85)
ink *= face
ink_col = np.array([13.0, 10.0, 11.0]) * (0.9 + 0.2 * light)[..., None] + (sheen * 0.3)[..., None]
# peinture epaisse et brillante : relief tire de la distance au bord, reflet de la lumiere en haut a gauche
dist = cv2.distanceTransform((paint > 0.5).astype(np.uint8), cv2.DIST_L2, 5) / AS
hp = np.sqrt(np.clip(dist, 0, 6.0) / 6.0) * 3.2
hp = cv2.GaussianBlur(hp, (0, 0), 1.2 * AS)
gy_, gx_ = np.gradient(hp, 1.0 / AS)
n_ = np.dstack([-gx_, -gy_, np.ones_like(hp)]); n_ /= np.linalg.norm(n_, axis=2, keepdims=True)
spec = np.clip(n_ @ HV, 0, 1) ** 60 * 150 + np.clip(n_ @ HV, 0, 1) ** 8 * 18
rim = np.clip(-(n_ @ L) + 0.55, 0, 1) * 0.35                          # bord oppose a la lumiere un peu plus sombre
gloss = (spec * paint)[..., None] * np.array([1.0, 0.93, 0.95])
pcol = ink_col * (1 - rim[..., None] * paint[..., None]) + gloss
col = np.where((paint > 0.02)[..., None] & (lab_mask < 0.5)[..., None], pcol, ink_col)
img = img * (1 - ink[..., None]) + col * ink[..., None]
# ombre portee tres legere de la peinture en relief sur la facade
psh = cv2.GaussianBlur(cv2.warpAffine(paint, np.float32([[1, 0, 2.0 * AS], [0, 1, 2.8 * AS]]), (W, H)), (0, 0), 2.0 * AS)
img *= (1 - 0.35 * psh * (1 - paint) * face)[..., None]

# ------------------------------------------------------------------ 5. logo Studio Wiga (noir), tout en bas
logo = np.asarray(Image.open(os.path.join(ROOT, 'assets', 'logo_studio_wiga_blanc.png')).convert('RGBA')).astype(np.float32)
LOGO_W = 250.0
lw, lh = int(LOGO_W * AS), int(LOGO_W * AS * logo.shape[0] / logo.shape[1])
la = cv2.resize(logo[..., 3], (lw, lh), interpolation=cv2.INTER_AREA) / 255.0
LOGO_C = (620.0, PY0 + PH - FI - 26.0)
lx0, ly0 = int(LOGO_C[0] * AS - lw / 2), int(LOGO_C[1] * AS - lh / 2)
reg = img[ly0:ly0 + lh, lx0:lx0 + lw]
img[ly0:ly0 + lh, lx0:lx0 + lw] = reg * (1 - la[..., None]) + np.array([12.0, 10.0, 11.0]) * la[..., None]

# ------------------------------------------------------------------ 6. commandes en relief (champ de hauteurs + lumiere)
SSAA = 3
PXU = AS * SSAA

class Canvas:
    def __init__(self, w_u, h_u):
        self.W, self.H = round(w_u * PXU), round(h_u * PXU)
        yy, xx = np.mgrid[0:self.H, 0:self.W].astype(np.float64)
        self.x = (xx + 0.5) / PXU - w_u / 2
        self.y = (yy + 0.5) / PXU - h_u / 2
        self.h = np.zeros((self.H, self.W)); self.mat = np.zeros((self.H, self.W), np.int32)
    def put(self, hgt, mask, mid):
        m = mask & (hgt > self.h - 1e-9); self.h[m] = hgt[m]; self.mat[m] = mid

def normals(c):
    gy, gx = np.gradient(c.h, 1.0 / PXU)
    n = np.dstack([-gx, -gy, np.ones_like(c.h)]); return n / np.linalg.norm(n, axis=2, keepdims=True)

def soft_shadow(c, samples=10, spread=0.10, max_u=40.0):
    vis = np.zeros_like(c.h); r = np.random.default_rng(1)
    step = 1.0 / PXU * 2; nsteps = int(max_u / step)
    for s in range(samples):
        d = L + r.normal(0, spread, 3) * np.array([1, 1, 0.5]); d /= np.linalg.norm(d)
        hz = math.hypot(d[0], d[1]); tanE = d[2] / hz; dx, dy = d[0] / hz, d[1] / hz
        occl = np.zeros_like(c.h, bool)
        for k in range(1, nsteps):
            t = k * step
            sx, sy = int(round(dx * t * PXU)), int(round(dy * t * PXU))
            if abs(sx) >= c.W or abs(sy) >= c.H: break
            sh = np.full_like(c.h, -1.0)
            ys0, ys1 = max(0, -sy), min(c.H, c.H - sy); xs0, xs1 = max(0, -sx), min(c.W, c.W - sx)
            sh[ys0:ys1, xs0:xs1] = c.h[ys0 + sy:ys1 + sy, xs0 + sx:xs1 + sx]
            occl |= sh > c.h + t * tanE + 0.05
        vis += ~occl
    return vis / samples

def env_chrome(n, gloss=1.0):
    nz = n[..., 2]
    r = np.dstack([2 * nz * n[..., 0], 2 * nz * n[..., 1], 2 * nz * nz - 1])
    k = np.clip(r @ L, -1, 1)
    sky = 0.5 - 0.5 * r[..., 1]
    base_ = 34 + 120 * np.clip(sky, 0, 1) ** 1.6
    box = 215 * np.clip((k - 0.55) / 0.45, 0, 1) ** (1.5 / gloss)
    rim = 34 * np.clip(-r[..., 0] * 0.6 - r[..., 1] * 0.3, 0, 1)
    redbounce = 18 * np.clip(-r[..., 2] * 0.0 + (1 - nz) * 0.8, 0, 1)        # reflet rouge de la facade sur les flancs
    return base_ + box + rim, redbounce

def shade(n, vis, tint, kind, rough=0.35):
    ndl = np.clip(n @ L, 0, 1) * (0.25 + 0.75 * vis)
    ndh = np.clip(n @ HV, 0, 1)
    shin = 12 + 220 * (1 - rough) ** 2
    spec = ndh ** shin * (0.35 + 0.65 * vis)
    if kind == 'chrome':
        e, rb = env_chrome(n)
        col = (e * (0.55 + 0.45 * vis) + 255 * spec * 0.9)[..., None] * np.asarray(tint) + rb[..., None] * np.array([1.0, 0.15, 0.15])
    elif kind == 'black':
        col = (34 * ndl + 9)[..., None] * np.asarray(tint) + (110 * spec)[..., None]
        col = col + (10 * (1 - n[..., 2]))[..., None] * np.array([1.0, 0.2, 0.2])
    elif kind == 'lens':
        col = (18 * ndl + 8)[..., None] * np.asarray(tint) + (250 * spec)[..., None]
    return col

def finish(c, rgb, shadow_on_panel, size_px):
    obj = cv2.GaussianBlur((c.mat > 0).astype(np.float64), (0, 0), 0.35 * SSAA)
    alpha = np.clip(obj + shadow_on_panel * (1 - obj), 0, 1)
    rgba = np.dstack([rgb * obj[..., None], alpha * 255])
    small = cv2.resize(rgba, size_px, interpolation=cv2.INTER_AREA)
    a = small[..., 3:4] / 255.0
    col = np.where(a > 1e-4, small[..., :3] / np.maximum(a, 1e-4), 0)
    return np.dstack([np.clip(col, 0, 255), np.clip(small[..., 3], 0, 255)]).astype(np.float32)

def panel_shadow(c, vis, amount=0.62):
    sh = (1 - vis) * amount
    occ = cv2.GaussianBlur((c.mat > 0).astype(np.float64), (0, 0), 1.6 * PXU) * 0.35
    return np.clip(sh + occ, 0, 0.85) * (c.mat == 0)

def knob_sprite(R):
    """potard noir moletee, biseau chrome poli au sommet (comme la pedale d'origine) ; le repere blanc est dessine par le plugin"""
    S_ = R * 2.9
    c = Canvas(S_, S_)
    r = np.hypot(c.x, c.y); th = np.arctan2(c.y, c.x)
    Rc = R
    Rk = Rc - 0.022 * R * (0.5 + 0.5 * np.cos(th * 40))           # cannelures
    Hc = 0.40 * R
    side = r < Rk
    # dessus : biseau chrome entre Rc - 0.18 R et Rc, centre noir tres legerement bombe
    rin = Rc - 0.17 * R
    top = np.where(r < rin, Hc + 0.02 * R * (1 - (r / rin) ** 2),
                   Hc - 0.11 * R * np.clip((r - rin) / (Rc - rin), 0, None) ** 1.6)
    c.put(top, side, 1)
    c.mat[side & (r >= rin - 0.01 * R) & (r < Rk - 0.035 * R)] = 2       # biseau poli
    vis = soft_shadow(c, samples=12, spread=0.11, max_u=R * 0.9)
    n = normals(c)
    blk = shade(n, vis, (1.0, 1.0, 1.04), 'black', rough=0.45)
    chr_ = shade(n, vis, (0.98, 0.98, 1.0), 'chrome')
    m = c.mat[..., None]
    rgb = np.where(m == 2, chr_, blk)
    sh = panel_shadow(c, vis, amount=0.66)
    return finish(c, rgb, sh, (round(S_ * AS), round(S_ * AS))), S_, rin

def led_sprite(R=9.0):
    """bague chromee + lentille rouge eteinte (dome translucide sombre)"""
    S_ = R * 3.4
    c = Canvas(S_, S_)
    r = np.hypot(c.x, c.y)
    ring = (r < R)
    c.put(np.where(ring, 3.0 + 1.6 * np.sqrt(np.clip(1 - ((r - 0.80 * R) / (0.2 * R)) ** 2, 0, 1)), 0), ring & (r > 0.6 * R), 2)
    lens = r < 0.62 * R
    c.put(2.2 + 2.4 * np.sqrt(np.clip(1 - (r / (0.62 * R)) ** 2, 0, 1)), lens, 3)
    gap = (r >= 0.60 * R) & (r < 0.66 * R)
    c.put(np.full_like(r, 2.0), gap, 4)
    vis = soft_shadow(c, samples=10, spread=0.12, max_u=10)
    n = normals(c)
    chr_ = shade(n, vis, (0.97, 0.97, 1.0), 'chrome')
    lensc = shade(n, vis, (2.4, 0.42, 0.42), 'lens', rough=0.05)
    gapc = np.full(chr_.shape, 16.0)
    m = c.mat[..., None]
    rgb = np.where(m == 2, chr_, np.where(m == 3, lensc, gapc))
    return finish(c, rgb, panel_shadow(c, vis, 0.55), (round(S_ * AS), round(S_ * AS))), S_

print('rendu potards...')
KN, KS, KRIN = knob_sprite(KNOB_R)
MK, MS, MRIN = knob_sprite(PRES_R)
LD, LS = led_sprite(9.0)
for kid, (u, v, _) in KNOBS.items():
    x, y = P(u, v)
    over(img, KN, (x - KS / 2) * AS, (y - KS / 2) * AS)
over(img, MK, (PRES[0] - MS / 2) * AS, (PRES[1] - MS / 2) * AS)
for (x, y) in (LED_L, LED_R):
    over(img, LD, (x - LS / 2) * AS, (y - LS / 2) * AS)

# ------------------------------------------------------------------ 7. footswitchs : celui d'ECHOFLEX (photo reelle)
EF_PTS = [(1.7, -56.7), (51.7, -26.7), (51.7, 31.7), (5.0, 58.3), (-48.3, 30.0), (-49.3, -26.7)]
EF_CENTROID = (1.5, 1.5); EF_FLATS = 100.5
FS_FLATS = 96.0
FS_K = FS_FLATS / EF_FLATS
FS_W = 190.0
def footswitch():
    src = np.asarray(Image.open(os.path.join(ROOT, 'assets', 'echoflex_footswitch_source.png')).convert('RGB')).astype(np.float64)
    n = round(FS_W * AS); sc = FS_K * AS / 2.0
    M = np.float32([[sc, 0, n / 2 - (150 + 2 * EF_CENTROID[0]) * sc], [0, sc, n / 2 - (150 + 2 * EF_CENTROID[1]) * sc]])
    rgb = cv2.warpAffine(src, M, (n, n), flags=cv2.INTER_LANCZOS4, borderMode=cv2.BORDER_REFLECT)
    pts = np.array([[n / 2 + (x - EF_CENTROID[0]) * FS_K * AS * 0.992, n / 2 + (y - EF_CENTROID[1]) * FS_K * AS * 0.992] for x, y in EF_PTS])
    m = np.zeros((n * 4, n * 4), np.uint8); cv2.fillPoly(m, [np.round(pts * 4).astype(np.int32)], 255)
    a = cv2.GaussianBlur(cv2.resize(m, (n, n), interpolation=cv2.INTER_AREA).astype(np.float64) / 255.0, (0, 0), 0.6)
    # leger reflet rouge de la facade sur les flancs de l'ecrou (le metal reflete son environnement)
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float64)
    rr = np.hypot(xx - n / 2, yy - n / 2) / (n / 2)
    rgb = rgb + (np.clip((rr - 0.42) / 0.2, 0, 1) * (rgb.mean(2) / 255.0) * 26)[..., None] * np.array([1.0, 0.1, 0.1])
    def shift(im, dx, dy): return cv2.warpAffine(im, np.float32([[1, 0, dx], [0, 1, dy]]), (n, n))
    cast = cv2.GaussianBlur(shift(a, 5.5 * AS, 8.0 * AS), (0, 0), 5.0 * AS) * 0.6
    contact = cv2.GaussianBlur(shift(a, 1.0 * AS, 1.5 * AS), (0, 0), 1.6 * AS) * 0.5
    sh = np.clip(cast + contact, 0, 0.82) * (1 - a)
    alpha = np.clip(a + sh, 0, 1)
    col = np.where(alpha[..., None] > 1e-4, rgb * (a / np.maximum(alpha, 1e-4))[..., None], 0)
    return np.dstack([np.clip(col, 0, 255), alpha * 255]).astype(np.float32)
FSPR = footswitch()
for (x, y) in (FS_L, FS_R):
    over(img, FSPR, (x - FS_W / 2) * AS, (y - FS_W / 2) * AS)

# ------------------------------------------------------------------ 8. jacks du kit sur les tranches (OUT a gauche, IN a droite)
JK = 0.78
JACK_Y = PY0 + 0.40 * PH
for fn, side in (('jack_out_gauche.png', -1), ('jack_in_droite.png', 1)):
    j = np.asarray(Image.open(os.path.join(ROOT, 'assets', 'kit', fn)).convert('RGBA')).astype(np.float32)
    jw, jh = int(j.shape[1] * JK), int(j.shape[0] * JK)
    j = cv2.resize(j, (jw, jh), interpolation=cv2.INTER_AREA)
    x0 = (PX0 * AS - jw + 6) if side < 0 else ((PX0 + PW) * AS - 6)
    # ombre portee du jack sur la surface (vers le bas a droite)
    sh = np.zeros((H, W), np.float32)
    sh[int(JACK_Y * AS - jh / 2):int(JACK_Y * AS - jh / 2) + jh, int(x0):int(x0) + jw] = j[..., 3] / 255.0
    sh = cv2.GaussianBlur(cv2.warpAffine(sh, np.float32([[1, 0, 10], [0, 1, 14]]), (W, H)), (0, 0), 8) * (1 - body)
    img *= (1 - 0.6 * sh)[..., None]
    over(img, j, x0, JACK_Y * AS - jh / 2)

# ------------------------------------------------------------------ 9. inverseur OCTAVE : levier nickel du kit (haut = ON / milieu / bas = OFF)
TOG_K = 0.95
TOG_S = 124.0                                       # sprite carre (unites)
def toggle_frame(fn, ly0, ly1):
    s = np.asarray(Image.open(os.path.join(ROOT, 'assets', 'kit', fn)).convert('RGBA')).astype(np.float32)
    hh, ww = s.shape[:2]
    yy, xx = np.mgrid[0:hh, 0:ww].astype(np.float32)
    cx, cy = 93.0, 97.0                                    # centre de la bague dans le kit (px)
    rr = np.hypot(xx - cx, yy - cy)
    Lm = s[..., :3].mean(-1)
    lever = (np.abs(xx - cx) < 17) & (yy > ly0) & (yy < ly1) & (Lm > 30)
    objm = ((rr < 41) | lever) & (s[..., 3] > 128) & ~((rr > 33) & (Lm < 40)) & ~(lever & (rr > 41) & (Lm < 45))
    nl, lab_, st, _ = cv2.connectedComponentsWithStats(objm.astype(np.uint8))
    objm = lab_ == (1 + int(np.argmax(st[1:, cv2.CC_STAT_AREA])))          # une seule piece (pas de residus)
    objm = cv2.GaussianBlur(objm.astype(np.float32), (0, 0), 0.8)
    n = round(TOG_S * AS)
    sc = TOG_K
    M = np.float32([[sc, 0, n / 2 - cx * sc], [0, sc, n / 2 - cy * sc]])
    rgb = cv2.warpAffine(s[..., :3], M, (n, n), flags=cv2.INTER_LANCZOS4)
    a = np.clip(cv2.warpAffine(objm, M, (n, n), flags=cv2.INTER_LINEAR), 0, 1)
    def shift(im, dx, dy): return cv2.warpAffine(im, np.float32([[1, 0, dx], [0, 1, dy]]), (n, n))
    cast = cv2.GaussianBlur(shift(a, 4.0 * AS, 6.0 * AS), (0, 0), 3.5 * AS) * 0.6
    contact = cv2.GaussianBlur(shift(a, 0.8 * AS, 1.2 * AS), (0, 0), 1.2 * AS) * 0.5
    sh = np.clip(cast + contact, 0, 0.8) * (1 - a)
    alpha = np.clip(a + sh, 0, 1)
    rgb = rgb * 1.05 + (np.clip(1 - a, 0, 1) * 0)[..., None]
    col = np.where(alpha[..., None] > 1e-4, rgb * (a / np.maximum(alpha, 1e-4))[..., None], 0)
    return np.dstack([np.clip(col, 0, 255), alpha * 255]).astype(np.uint8)
frames = [toggle_frame('switch_levier_ON.png', 8, 97), toggle_frame('switch_levier_milieu.png', 97, 97), toggle_frame('switch_levier_OFF.png', 97, 150)]
Image.fromarray(np.concatenate(frames, 0), 'RGBA').save(os.path.join(ROOT, 'toggle_strip.png'))

# ------------------------------------------------------------------ 10. sauvegarde du fond
bg8 = np.clip(img, 0, 255).astype(np.uint8)
Image.fromarray(bg8).save(os.path.join(ROOT, 'background.jpg'), quality=94, subsampling=0)

# ------------------------------------------------------------------ 11. facade eclairee par chaque LED (calque alpha : seule la lumiere ajoutee)
GR = 120.0
def glow(c, fn):
    x0p, y0p = round((c[0] - GR) * AS), round((c[1] - GR) * AS)
    size = round(2 * GR * AS)
    reg = img[y0p:y0p + size, x0p:x0p + size].astype(np.float64)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    d = np.hypot((xx + 0.5) / AS - GR, (yy + 0.5) / AS - GR)
    h1, h2 = 8.0, 30.0
    E = 1.0 * h1 ** 3 / (d ** 2 + h1 ** 2) ** 1.5 + 0.16 * h2 ** 3 / (d ** 2 + h2 ** 2) ** 1.5
    E *= np.clip((GR - d) / 30.0, 0, 1)
    E[d < 9.0] *= 0.0                                    # la LED elle-meme est dessinee par le plugin
    col = np.array([1.0, 0.16, 0.10])
    lum = reg.mean(2, keepdims=True)
    lit = reg + (reg * 2.8 + 10.0 * np.clip(lum / 50.0, 0.3, 1.6)) * E[..., None] * col
    add = lit - reg
    a = np.clip(np.abs(add).max(-1) / 12.0, 0, 1)
    rgb = np.where(a[..., None] > 1e-3, reg + add / np.maximum(a, 1e-3)[..., None], reg)
    Image.fromarray(np.dstack([np.clip(rgb, 0, 255), a * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, fn))
    return (x0p / AS, y0p / AS, size / AS, size / AS)
GL = glow(LED_L, 'glow_l.png'); GRr = glow(LED_R, 'glow_r.png')

# ------------------------------------------------------------------ 12. Layout.h
def f(v): return f"{v:.2f}f"
with open(os.path.join(ROOT, 'Layout.h'), 'w') as fo:
    fo.write("// genere par tools/make_art.py - ne pas modifier a la main (repere 1240 x 928, fond 2x)\n#pragma once\n#include <juce_graphics/juce_graphics.h>\nnamespace layout\n{\n")
    order = ['master', 'gain', 'volume', 'distortion', 'bass', 'mid', 'treble', 'tone']
    fo.write("    // ordre : master, gain, volume, distortion, bass, mid, treble, tone\n")
    fo.write("    const juce::Point<float> knobPos[8] = { " + ", ".join("{ %s, %s }" % (f(P(KNOBS[k][0], KNOBS[k][1])[0]), f(P(KNOBS[k][0], KNOBS[k][1])[1])) for k in order) + " };\n")
    fo.write(f"    const float knobR = {f(KNOB_R)}, knobTopR = {f(KRIN)};      // rayon du potard / du disque noir du dessus (repere)\n")
    fo.write(f"    const juce::Point<float> presPos {{ {f(PRES[0])}, {f(PRES[1])} }}; const float presR = {f(PRES_R)}, presTopR = {f(MRIN)};\n")
    fo.write(f"    const juce::Point<float> octPos {{ {f(OCT[0])}, {f(OCT[1])} }}; const float toggleS = {f(TOG_S)};\n")
    fo.write(f"    const juce::Point<float> ledL {{ {f(LED_L[0])}, {f(LED_L[1])} }}, ledR {{ {f(LED_R[0])}, {f(LED_R[1])} }}; const float ledLensR = {f(9.0 * 0.62)};\n")
    fo.write(f"    const juce::Rectangle<float> glowL {{ {f(GL[0])}, {f(GL[1])}, {f(GL[2])}, {f(GL[3])} }}, glowR {{ {f(GRr[0])}, {f(GRr[1])}, {f(GRr[2])}, {f(GRr[3])} }};\n")
    fo.write(f"    const juce::Point<float> fsL {{ {f(FS_L[0])}, {f(FS_L[1])} }}, fsR {{ {f(FS_R[0])}, {f(FS_R[1])} }}; const float footR = {f(36.0 * FS_K * 1.0)};   // bouton du footswitch\n")
    fo.write("}\n")
print('ok')
