"""Outils de rendu 3D (champ de hauteurs + lumiere en haut a gauche) des commandes Studio Wiga - repris de make_art.py."""
import math
import numpy as np, cv2
AS = 2
L = np.array([-0.46, -0.62, 0.64]); L /= np.linalg.norm(L)
V = np.array([0.0, 0.0, 1.0]); HV = (L + V) / np.linalg.norm(L + V)
SSAA = 3
PXU = AS * SSAA

class Canvas:
    def __init__(self, w_u, h_u, cx_u=None, cy_u=None):
        cx_u = w_u / 2 if cx_u is None else cx_u; cy_u = h_u / 2 if cy_u is None else cy_u
        self.W, self.H = round(w_u * PXU), round(h_u * PXU)
        yy, xx = np.mgrid[0:self.H, 0:self.W].astype(np.float64)
        self.x = (xx + 0.5) / PXU - cx_u
        self.y = (yy + 0.5) / PXU - cy_u
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

