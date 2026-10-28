"""
PEACHY (Studio Wiga) - fond du plugin a partir du visuel fourni par l'utilisateur (design/source/peachy_visuel_v2.png).

Le visuel (1168 x 874, meme geometrie que le repere 1240 x 928 du plugin) est la reference : sa lumiere
(en haut a gauche) et sa texture de peinture sont conservees. Ce script :
  1. efface les reperes blancs des potards (ils tournent dans le plugin), la ligne d'etat et la barre de presets
     (dessinees par le plugin), les LED, les footswitchs chromes, l'inverseur et les jacks des tranches ;
     les zones rouges sont refaites avec les basses frequences des alentours + le grain de la peinture du visuel ;
  2. agrandit le visuel au cadre 2480 x 1856 (2 px par unite) ;
  3. pose les nouvelles pieces rendues en 3D avec la meme lumiere : footswitchs NOIRS (ecrou hexagonal anodise,
     bague moletee, bouton tourne), LED a bague chromee (eteintes dans le fond, allumees en sprite), jacks EN HAUT
     du boitier comme sur l'original (OUT a gauche, IN a droite, plus petits) avec inscriptions out / in ;
  4. calcule l'eclairage de la facade par chaque LED, l'inverseur OCTAVE (kit Studio Wiga) et ecrit Layout.h.
Usage : python3 tools/make_background.py
"""
import os, sys, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import art3d as A3
from art3d import Canvas, normals, soft_shadow, env_chrome, finish, panel_shadow

SRC = os.path.join(ROOT, 'design', 'source', 'peachy_visuel_v3.png')
AS = 2
UW, UH = 1240, 928
W, H = UW * AS, UH * AS
S0 = 1.0 / AS                                # px (visuel ramene au cadre 2x) -> unites

# ------------------------------------------------------------------ reperes (unites ; identiques au rendu v1, le visuel est aligne dessus)
KNOBS = [(292.63, 202.88), (499.68, 202.88), (693.71, 202.88), (988.56, 202.88),
         (187.48, 396.27), (388.02, 396.27), (598.32, 396.27), (872.57, 396.27)]
KNOB_R, KNOB_TOP = 50.0, 42.0                # potard du visuel : moletage exterieur / dessus noir
PRES, PRES_R, PRES_TOP = (509.43, 545.60), 21.0, 16.5
OCT_OLD = (1014.58, 343.23)                  # position du visuel (effacee)
OCT = (1014.58, 396.27)                      # inverseur aligne sur le potard TONE (demande de l'utilisateur)
OCT_LABEL = (1014.58, 324.0)                 # inscription « octave » alignee sur « tone »
LED_L, LED_R = (314.31, 584.77), (914.85, 584.77)
FS_L, FS_R = (261.20, 759.39), (988.56, 759.39)
STATUS = (421.0, 648.0, 397.0, 52.0)          # cadres dessines par le plugin (a effacer du visuel)
PRESET = (440.0, 722.0, 357.0, 42.0)
BOX_TOP = 55.0                               # bord superieur du boitier (unites)
PX0, PX1 = 81.7, 1157.1                      # bords gauche / droit du boitier
JACK_OUT_X, JACK_IN_X = PX0 + 0.21 * (PX1 - PX0), PX0 + 0.82 * (PX1 - PX0)

def u2p(x, y): return (x / S0, y / S0)       # unites -> px du visuel

# ------------------------------------------------------------------ 1. nettoyage dans le repere du visuel
A = cv2.cvtColor(cv2.imread(SRC), cv2.COLOR_BGR2RGB).astype(np.float32)
A = cv2.resize(A, (W, H), interpolation=cv2.INTER_AREA if A.shape[1] > W else cv2.INTER_LANCZOS4)   # meme geometrie que le repere
Hs, Ws = A.shape[:2]
YY, XX = np.mgrid[0:Hs, 0:Ws].astype(np.float32)
lum = lambda a: a.mean(-1)
rng = np.random.default_rng(3)

def disc(c, r):
    cx, cy = u2p(*c)
    return (np.hypot(XX - cx, YY - cy) <= r / S0)

def knob_clean(c, rtop):
    """efface le repere blanc du dessus du potard ; la matiere noire est reprise par inpainting a l'interieur du dessus"""
    global A
    cx, cy = u2p(*c)
    inside = np.hypot(XX - cx, YY - cy) <= (rtop - 1.0) / S0
    m = ((lum(A) > 95) & ((A.max(-1) - A.min(-1)) < 45) & inside).astype(np.uint8)
    m = cv2.dilate(m, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9))) * inside
    tmp = A.copy()
    dark = np.median(A[inside & (lum(A) < 70)], 0)
    tmp[~inside] = dark
    out = cv2.inpaint(np.clip(tmp, 0, 255).astype(np.uint8), m.astype(np.uint8), 4, cv2.INPAINT_TELEA).astype(np.float32)
    # grain fin du plastique noir (sinon la zone reprise est trop lisse)
    g = rng.normal(0, 1.6, A.shape).astype(np.float32)
    A = np.where(m[..., None] > 0, out + g * m[..., None], A)

for c in KNOBS: knob_clean(c, KNOB_TOP)
knob_clean(PRES, PRES_TOP)

# grain de la peinture rouge (zones propres du visuel)
DONORS = [(1274, 951, 1571, 1062), (1359, 1104, 1614, 1274), (701, 1242, 849, 1359), (255, 1019, 488, 1189)]
redish = ((A[..., 0] > 70) & (A[..., 0] > 1.6 * A[..., 1])).astype(np.float32)
redish2 = ((A[..., 0] > 22) & (A[..., 0] > 1.35 * A[..., 1] + 4)).astype(np.float32)   # rouge, y compris dans l'ombre
# interieur de la facade rouge (fermeture large : les ombres sombres posees sur le rouge en font partie, pas le cadre noir)
face_in = cv2.morphologyEx((redish > 0).astype(np.uint8), cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (121, 121))).astype(np.float32)
face_in = cv2.erode(face_in, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9)))
def red_fill(mask, soft=3.0, sigma=30.0):
    """remplit le masque : basses frequences des alentours (moyenne normalisee hors masque) + grain de la peinture"""
    global A
    keep = (mask < 0.5).astype(np.float32) * redish2
    # basses frequences multi-echelles, calculees sur une image reduite (rapide) : l'echelle fine la ou il y a assez
    # de peinture propre autour, sinon une echelle plus large (jamais de trou sombre au centre d'une grande zone)
    D = 8
    ks = cv2.resize(keep, (Ws // D, Hs // D), interpolation=cv2.INTER_AREA)
    As = cv2.resize(A * keep[..., None], (Ws // D, Hs // D), interpolation=cv2.INTER_AREA)
    low = None
    for sg in (sigma / D, sigma * 3.0 / D, sigma * 9.0 / D):
        wsum = cv2.GaussianBlur(ks, (0, 0), sg)
        est = cv2.GaussianBlur(As, (0, 0), sg) / (wsum[..., None] + 1e-6)
        if low is None: low = est; okw = wsum
        else:
            t = np.clip((0.25 - okw) / 0.25, 0, 1)[..., None]
            low = low * (1 - t) + est * t; okw = np.maximum(okw, wsum)
    low = cv2.resize(low, (Ws, Hs), interpolation=cv2.INTER_CUBIC)
    # grain : morceaux de peinture propre, places au hasard (pas de motif repete visible)
    grain = np.zeros_like(A)
    tiles = []
    for (x0, y0, x1, y1) in DONORS:
        d = A[y0:y1, x0:x1]
        tiles.append(d - cv2.GaussianBlur(d, (0, 0), 3.0))
    ys, xs = np.where(mask > 0)
    if len(ys) == 0: return
    by0, by1, bx0, bx1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    y = by0
    while y < by1:
        x = bx0
        while x < bx1:
            t = tiles[rng.integers(len(tiles))]
            th, tw = t.shape[:2]
            h_ = min(th, by1 - y); w_ = min(tw, bx1 - x)
            grain[y:y + h_, x:x + w_] = t[:h_, :w_]
            x += tw
        y += th
    mm = cv2.GaussianBlur(mask.astype(np.float32), (0, 0), soft)[..., None]
    A = A * (1 - mm) + (low + grain) * mm

def rect_mask(r, pad=4.0, rad=10.0):
    x, y, w, h = r
    m = np.zeros((Hs, Ws), np.uint8)
    x0, y0 = u2p(x - pad, y - pad); x1, y1 = u2p(x + w + pad, y + h + pad)
    cv2.rectangle(m, (int(x0), int(y0)), (int(math.ceil(x1)), int(math.ceil(y1))), 1, -1)
    return m

# barre de presets du visuel (le plugin la dessine) et son ombre
red_fill(rect_mask((420.0, 700.0, 400.0, 86.0), pad=0.0), soft=5.0, sigma=50.0)
# LED du visuel (allumees, avec halo) : remplacees par les LED rendues
for c in (LED_L, LED_R):
    red_fill((disc(c, 50.0) & (redish2 > 0) | disc(c, 16.0)).astype(np.uint8), soft=5.0, sigma=40.0)
# footswitchs du visuel et leurs ombres bizarres : effaces (le plugin dessine les footswitchs noirs, enfoncables)
for c in (FS_L, FS_R):
    core = disc(c, 54.0)
    halo = (disc(c, 96.0) | disc((c[0] + 30, c[1] + 42), 82.0) | disc((c[0] - 10, c[1] + 40), 70.0)) & (face_in > 0)
    red_fill((core | halo).astype(np.uint8), soft=4.0, sigma=40.0)
# ancien emplacement de l'inverseur OCTAVE et de son inscription
m = np.zeros((Hs, Ws), np.uint8)
x0, y0 = u2p(958, 262); x1, y1 = u2p(1072, 302)
cv2.rectangle(m, (int(x0), int(y0)), (int(x1), int(y1)), 1, -1)
cx, cy = u2p(*OCT_OLD)
cv2.ellipse(m, (int(cx + 4), int(cy + 8)), (58, 58), 0, 0, 360, 1, -1)
cv2.rectangle(m, (int(cx - 20), int(cy - 76)), (int(cx + 24), int(cy)), 1, -1)
red_fill(m, soft=3.0, sigma=24.0)

# ------------------------------------------------------------------ 2. agrandissement au cadre 2x
img = np.clip(A, 0, 255)                      # le visuel est deja au cadre 2x

def over(dst, rgba, x0, y0):
    h, w = rgba.shape[:2]
    X0, Y0 = int(round(x0)), int(round(y0))
    sx0, sy0 = max(0, -X0), max(0, -Y0); ex, ey = min(w, W - X0), min(h, H - Y0)
    a = rgba[sy0:ey, sx0:ex, 3:4] / 255.0
    dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex] = dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex] * (1 - a) + rgba[sy0:ey, sx0:ex, :3] * a

L, HV = A3.L, A3.HV
def spec_terms(n, vis, shin):
    ndl = np.clip(n @ L, 0, 1) * (0.25 + 0.75 * vis)
    ndh = np.clip(n @ HV, 0, 1)
    return ndl, ndh ** shin * (0.35 + 0.65 * vis)

# ------------------------------------------------------------------ 3a. footswitch NOIR (ecrou hexagonal anodise, bague moletee, bouton tourne)
FS_W = 170.0
FS_FLATS, CAP_R, KNURL_R = 96.0, 32.0, 40.0
def footswitch_black(pressed=False):
    """footswitch noir ; pressed = bouton enfonce (2,6 unites plus bas : son ombre portee raccourcit, le dessus s'assombrit)"""
    c = Canvas(FS_W, FS_W, FS_W / 2, FS_W / 2)
    x, y = c.x, c.y
    r = np.hypot(x, y); th = np.arctan2(y, x)
    ap = FS_FLATS / 2
    a_ = th + math.pi / 2
    hexr = r * np.cos(((a_ + math.pi / 6) % (math.pi / 3)) - math.pi / 6)
    nut = np.minimum(6.0, (ap - hexr) * 1.6)                       # chanfrein du bord
    nut = np.minimum(nut, 6.0 - np.clip(r - (ap * 1.06), 0, None) * 1.4)   # coins arrondis
    c.put(np.clip(nut, 0, None), (hexr < ap) & (nut > 0), 1)
    c.put(np.full_like(r, 6.8), r < KNURL_R + 3.0, 4)              # rondelle crantee sous la bague
    kn = 10.0 + 0.55 * np.cos(th * 96) * np.clip((r - KNURL_R + 4.0) / 2.0, 0, 1)
    c.put(kn - np.clip(r - (KNURL_R - 0.8), 0, None) * 2.5, r < KNURL_R, 2)   # bague moletee
    drop = 4.5 if pressed else 0.0
    capz = 21.0 - drop + 0.8 * (1 - (r / CAP_R) ** 2) - np.clip(r - (CAP_R - 2.2), 0, None) ** 2 * 0.9
    c.put(capz, r < CAP_R, 3)
    vis = soft_shadow(c, samples=16, spread=0.09, max_u=40)
    n = normals(c)
    m = c.mat
    e, _ = env_chrome(n, gloss=0.55)
    ndl, sp_wide = spec_terms(n, vis, 22)
    _, sp_sharp = spec_terms(n, vis, 160)
    base = (6 + 24 * ndl + 0.13 * e * (0.5 + 0.5 * vis))
    col = base[..., None] * np.array([1.0, 1.0, 1.04]) + (55 * sp_wide + 170 * sp_sharp)[..., None]
    lathe = 0.97 + 0.03 * np.sin(r * 1.1 + 0.6 * np.sin(th * 3))
    capk = 0.70 if pressed else 1.08
    col = np.where((m == 3)[..., None], col * lathe[..., None] * capk, col)
    side = np.clip(1 - n[..., 2], 0, 1) ** 1.2
    col = col + (side * 26)[..., None] * np.array([1.0, 0.12, 0.12])
    col = np.where((m == 4)[..., None], col * 0.7, col)
    # ombre portee nette et courte (lumiere en haut a gauche) + ombre de contact ; pas de halo flou autour
    sh = panel_shadow(c, vis, amount=0.55)
    sh = cv2.GaussianBlur(sh, (0, 0), 0.8 * A3.PXU)
    # ombre douce portee par tout le footswitch (vers le bas a droite, longueur selon la hauteur du bouton)
    obj = (c.mat > 0).astype(np.float64)
    off = (4.5 if pressed else 12.0) * A3.PXU
    drop_sh = cv2.warpAffine(obj, np.float32([[1, 0, off * 0.6], [0, 1, off * 0.8]]), (c.W, c.H))
    drop_sh = cv2.GaussianBlur(drop_sh, (0, 0), (2.5 if pressed else 4.5) * A3.PXU) * (0.55 if pressed else 0.50)
    sh = np.maximum(sh, drop_sh * (c.mat == 0))
    return finish(c, col, sh, (round(FS_W * AS), round(FS_W * AS)))

# ------------------------------------------------------------------ 3b. LED : bague chromee + lentille en dome (eteinte / allumee)
LED_BEZ, LED_LENS = 12.0, 7.6
LED_S = 40.0
def led_sprite(on):
    c = Canvas(LED_S, LED_S, LED_S / 2, LED_S / 2)
    r = np.hypot(c.x, c.y)
    ring = (r < LED_BEZ) & (r > LED_LENS + 0.6)
    prof = np.sqrt(np.clip(1 - ((r - (LED_LENS + LED_BEZ) / 2) / ((LED_BEZ - LED_LENS) / 2)) ** 2, 0, 1))
    c.put(1.2 + 2.2 * prof, ring, 2)
    gap = (r <= LED_LENS + 0.6) & (r >= LED_LENS)
    c.put(np.full_like(r, 1.0), gap, 4)
    dome = 1.5 + 3.4 * np.sqrt(np.clip(1 - (r / LED_LENS) ** 2, 0, 1))
    c.put(dome, r < LED_LENS, 3)
    vis = soft_shadow(c, samples=10, spread=0.12, max_u=12)
    n = normals(c)
    e, rb = env_chrome(n)
    ndl, sp = spec_terms(n, vis, 90)
    chrome = (e * (0.55 + 0.45 * vis) + 255 * sp * 0.9)[..., None] * np.array([0.98, 0.98, 1.0]) + rb[..., None] * np.array([1.0, 0.15, 0.15])
    # lentille : verre rouge ; fresnel sur le bord, reflet net de la lumiere, cuvette reflechissante au fond
    _, glint = spec_terms(n, vis, 400)
    fres = np.clip(1 - n[..., 2], 0, 1) ** 2.5
    u = np.clip(r / LED_LENS, 0, 1)
    cup = np.exp(-((u - 0.55) / 0.16) ** 2)
    if not on:
        # LED blanche eteinte : verre clair laiteux, on devine la cuvette reflechissante et la puce
        body = np.dstack([150 - 40 * u + 30 * cup, 150 - 40 * u + 30 * cup, 156 - 40 * u + 32 * cup]) * (0.55 + 0.45 * ndl)[..., None]
        body = body * (1 - 0.35 * np.exp(-(u / 0.18) ** 2))[..., None]       # puce sombre au centre
    else:
        # LED blanche allumee : coeur eblouissant, verre diffusant legerement froid, bord un peu plus sombre
        core = np.exp(-(u / 0.40) ** 2)
        body = np.dstack([228 + 27 * core, 232 + 23 * core, 245 + 10 * core]) * (1 - 0.22 * u ** 2)[..., None]
    lens = body + (255 * glint * 1.1 + 70 * fres)[..., None] * np.array([1.0, 0.92, 0.92])
    m = c.mat[..., None]
    col = np.where(m == 2, chrome, np.where(m == 3, lens, 18.0))
    sh = panel_shadow(c, vis, amount=0.55)
    return finish(c, col, sh, (round(LED_S * AS), round(LED_S * AS)))

print('rendu des pieces...')
FS_UP, FS_DOWN = footswitch_black(False), footswitch_black(True)
LED_OFF, LED_ON = led_sprite(False), led_sprite(True)
for (x, y) in (LED_L, LED_R):
    over(img, LED_OFF, (x - LED_S / 2) * AS, (y - LED_S / 2) * AS)
# les footswitchs ne sont plus dans le fond : le plugin dessine fs_up.png / fs_down.png (bouton enfonce a l'appui)
Image.fromarray(np.clip(FS_UP, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'fs_up.png'))
Image.fromarray(np.clip(FS_DOWN, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'fs_down.png'))

# inscription « octave » alignee sur « tone » (ecriture manuscrite noire comme les autres inscriptions du rouge)
FONT = os.path.join(ROOT, 'assets', 'fonts', 'ShadowsIntoLight.ttf')
lab = Image.new('L', (W * 2, H * 2), 0)
ImageDraw.Draw(lab).text((OCT_LABEL[0] * AS * 2, OCT_LABEL[1] * AS * 2), "octave", font=ImageFont.truetype(FONT, int(40 * AS * 2)), fill=255, anchor='mm')
la = cv2.resize(np.asarray(lab, np.float32) / 255.0, (W, H), interpolation=cv2.INTER_AREA)
img = img * (1 - la[..., None]) + np.array([16.0, 12.0, 12.0]) * la[..., None]

# ------------------------------------------------------------------ 4. inverseur OCTAVE (kit Studio Wiga : levier nickel, haut = ON)
TOG_K = 0.95
TOG_S = 124.0
def toggle_frame(fn, ly0, ly1):
    s = np.asarray(Image.open(os.path.join(ROOT, 'assets', 'kit', fn)).convert('RGBA')).astype(np.float32)
    hh, ww = s.shape[:2]
    yy, xx = np.mgrid[0:hh, 0:ww].astype(np.float32)
    cx, cy = 93.0, 97.0
    rr = np.hypot(xx - cx, yy - cy)
    Lm = s[..., :3].mean(-1)
    lever = (np.abs(xx - cx) < 17) & (yy > ly0) & (yy < ly1) & (Lm > 30)
    objm = ((rr < 41) | lever) & (s[..., 3] > 128) & ~((rr > 33) & (Lm < 40)) & ~(lever & (rr > 41) & (Lm < 45))
    nl, lab_, st, _ = cv2.connectedComponentsWithStats(objm.astype(np.uint8))
    objm = lab_ == (1 + int(np.argmax(st[1:, cv2.CC_STAT_AREA])))
    objm = cv2.GaussianBlur(objm.astype(np.float32), (0, 0), 0.8)
    n = round(TOG_S * AS); sc = TOG_K
    M = np.float32([[sc, 0, n / 2 - cx * sc], [0, sc, n / 2 - cy * sc]])
    rgb = cv2.warpAffine(s[..., :3], M, (n, n), flags=cv2.INTER_LANCZOS4)
    a = np.clip(cv2.warpAffine(objm, M, (n, n), flags=cv2.INTER_LINEAR), 0, 1)
    def shift(im, dx, dy): return cv2.warpAffine(im, np.float32([[1, 0, dx], [0, 1, dy]]), (n, n))
    cast = cv2.GaussianBlur(shift(a, 4.0 * AS, 6.0 * AS), (0, 0), 3.5 * AS) * 0.6
    contact = cv2.GaussianBlur(shift(a, 0.8 * AS, 1.2 * AS), (0, 0), 1.2 * AS) * 0.5
    sh = np.clip(cast + contact, 0, 0.8) * (1 - a)
    alpha = np.clip(a + sh, 0, 1)
    col = np.where(alpha[..., None] > 1e-4, rgb * 1.05 * (a / np.maximum(alpha, 1e-4))[..., None], 0)
    return np.dstack([np.clip(col, 0, 255), alpha * 255]).astype(np.uint8)
frames = [toggle_frame('switch_levier_ON.png', 8, 97), toggle_frame('switch_levier_milieu.png', 97, 97), toggle_frame('switch_levier_OFF.png', 97, 150)]
Image.fromarray(np.concatenate(frames, 0), 'RGBA').save(os.path.join(ROOT, 'toggle_strip.png'))

# ------------------------------------------------------------------ 5. sauvegarde du fond, sprite LED allumee, calques d'eclairage
Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(os.path.join(ROOT, 'background.jpg'), quality=94, subsampling=0)
Image.fromarray(np.clip(LED_ON, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'led_on.png'))

GR = 120.0
def glow(c, fn):
    x0p, y0p = round((c[0] - GR) * AS), round((c[1] - GR) * AS)
    size = round(2 * GR * AS)
    reg = img[y0p:y0p + size, x0p:x0p + size].astype(np.float64)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    d = np.hypot((xx + 0.5) / AS - GR, (yy + 0.5) / AS - GR)
    h1, h2 = 9.0, 32.0
    E = 1.0 * h1 ** 3 / (d ** 2 + h1 ** 2) ** 1.5 + 0.16 * h2 ** 3 / (d ** 2 + h2 ** 2) ** 1.5
    E *= np.clip((GR - d) / 30.0, 0, 1)
    E[d < LED_BEZ] = 0.0                          # la LED elle-meme : sprite allume
    col = np.array([1.0, 0.97, 0.94])              # lumiere blanche (legerement chaude sur la peinture)
    lu = reg.mean(2, keepdims=True)
    lit = reg + (reg * 1.6 + 120.0 * np.clip(lu / 60.0, 0.5, 1.4)) * E[..., None] * col
    add = lit - reg
    a = np.clip(np.abs(add).max(-1) / 12.0, 0, 1)
    rgb = np.where(a[..., None] > 1e-3, reg + add / np.maximum(a, 1e-3)[..., None], reg)
    Image.fromarray(np.dstack([np.clip(rgb, 0, 255), a * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, fn))
    return (x0p / AS, y0p / AS, size / AS, size / AS)
GL = glow(LED_L, 'glow_l.png'); GRr = glow(LED_R, 'glow_r.png')

def f_(v): return f"{v:.2f}f"
with open(os.path.join(ROOT, 'Layout.h'), 'w') as fo:
    fo.write("// genere par tools/make_background.py - ne pas modifier a la main (repere 1240 x 928, fond 2x)\n#pragma once\n#include <juce_graphics/juce_graphics.h>\nnamespace layout\n{\n")
    fo.write("    // ordre : master, gain, volume, distortion, bass, mid, treble, tone\n")
    fo.write("    const juce::Point<float> knobPos[8] = { " + ", ".join("{ %s, %s }" % (f_(x), f_(y)) for x, y in KNOBS) + " };\n")
    fo.write(f"    const float knobR = {f_(KNOB_R)}, knobTopR = {f_(KNOB_TOP)};\n")
    fo.write(f"    const juce::Point<float> presPos {{ {f_(PRES[0])}, {f_(PRES[1])} }}; const float presR = {f_(PRES_R)}, presTopR = {f_(PRES_TOP)};\n")
    fo.write(f"    const juce::Point<float> octPos {{ {f_(OCT[0])}, {f_(OCT[1])} }}; const float toggleS = {f_(TOG_S)};\n")
    fo.write(f"    const juce::Point<float> ledL {{ {f_(LED_L[0])}, {f_(LED_L[1])} }}, ledR {{ {f_(LED_R[0])}, {f_(LED_R[1])} }}; const float ledS = {f_(LED_S)};\n")
    fo.write(f"    const juce::Rectangle<float> glowL {{ {f_(GL[0])}, {f_(GL[1])}, {f_(GL[2])}, {f_(GL[3])} }}, glowR {{ {f_(GRr[0])}, {f_(GRr[1])}, {f_(GRr[2])}, {f_(GRr[3])} }};\n")
    fo.write(f"    const juce::Point<float> fsL {{ {f_(FS_L[0])}, {f_(FS_L[1])} }}, fsR {{ {f_(FS_R[0])}, {f_(FS_R[1])} }}; const float footR = {f_(KNURL_R + 4.0)}, fsS = {f_(FS_W)};   // zone d'appui / sprite\n")
    fo.write("}\n")
print('ok')
