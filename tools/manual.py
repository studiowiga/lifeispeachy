# PEACHY (Studio Wiga) - notice PDF bilingue (anglais puis francais), illustree avec le rendu reel du plugin.
# Usage : python3 tools/manual.py <dossier des captures (tools/render)> <fichier PDF de sortie>
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, CondPageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = sys.argv[1]
OUT = sys.argv[2]
VERSION = "1.5.0"
TMP = os.path.dirname(os.path.abspath(OUT))
FONTS = os.path.join(ROOT, "assets", "fonts")
for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FONTS, f + ".ttf")))
pdfmetrics.registerFont(TTFont("Hand", os.path.join(FONTS, "ShadowsIntoLight.ttf")))

ACC = colors.HexColor("#D8262C"); ACC_L = colors.HexColor("#FF5A50")
DARK = colors.HexColor("#1A1516"); GREY = colors.HexColor("#6B6466"); LIGHT = colors.HexColor("#F6E9E8")

# ---------- geometrie (Layout.h, repere 1240 x 928) ----------
import re
LAY = open(os.path.join(ROOT, "Layout.h")).read()
def pt(name): return [float(v) for v in re.search(name + r" \{ ([\d.]+)f, ([\d.]+)f", LAY).groups()]
KP = [(float(a), float(b)) for a, b in re.findall(r"\{ ([\d.]+)f, ([\d.]+)f \}", LAY.split("knobPos[8] =")[1].split(";")[0])]
PRES, OCT, LEDL, LEDR, FSL, FSR = pt("presPos"), pt("octPos"), pt("ledL"), pt("ledR"), pt("fsL"), pt("fsR")
PX0, PY0, PW, PH = [float(v) for v in re.search(r'boxRect \{ ([\d.]+)f, ([\d.]+)f, ([\d.]+)f, ([\d.]+)f', LAY).groups()]
PR = 43.0
JACK_OUT_X = float(re.search(r'jackOutX = ([\d.]+)f', LAY).group(1)); JACK_IN_X = float(re.search(r'jackInX = ([\d.]+)f', LAY).group(1))
JACK_Y = float(re.search(r'jackY = ([\d.]+)f', LAY).group(1))

src = Image.open(os.path.join(SHOTS, "peachy_status.png")).convert("RGB")
k = src.height / 928.0

# ---------- capture annotee ----------
marks = [(1, *KP[0]), (2, *KP[1]), (3, *KP[2]), (4, *KP[3]), (5, *KP[4]), (6, *KP[5]), (7, *KP[6]), (8, *KP[7]),
         (9, *PRES), (10, *OCT), (11, *LEDL), (12, *LEDR), (13, *FSL), (14, *FSR), (15, 620, 693), (16, 620, 767),
         (17, JACK_IN_X, JACK_Y + 24), (18, JACK_OUT_X, JACK_Y + 24)]
off = {1: (-70, 0), 2: (-70, 0), 3: (-70, 0), 4: (-70, 0), 5: (-70, 0), 6: (-70, 0), 7: (-70, 0), 8: (-70, 0), 9: (-45, 0),
       10: (48, 0), 11: (0, -36), 12: (0, -36), 13: (-80, 0), 14: (80, 0), 15: (-252, 0), 16: (-232, 0), 17: (78, 0), 18: (-78, 0)}
ann = src.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(FONTS, "Poppins-Bold.ttf"), int(26 * k))
for n, x, y in marks:
    ox, oy = off[n]
    cx, cy, r = (x + ox) * k, (y + oy) * k, 20 * k
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(216, 38, 44), width=int(4 * k))
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(26, 21, 22), anchor="mm")
annotated = os.path.join(TMP, "peachy_annotated.png"); ann.save(annotated)

def crop_u(path, box):
    im = Image.open(os.path.join(SHOTS, path)).convert("RGB")
    kk = im.height / 928.0
    return im.crop(tuple(int(v * kk) for v in box))
gap = 18
def strip(ims, name):
    s = Image.new("RGB", (sum(m.width for m in ims) + gap * (len(ims) - 1), ims[0].height), (255, 255, 255))
    x = 0
    for m in ims: s.paste(m, (x, 0)); x += m.width + gap
    p = os.path.join(TMP, name); s.save(p); return p
# octave off / on ; LED + footswitch off / on
ob = (OCT[0] - 80, OCT[1] - 84, OCT[0] + 80, OCT[1] + 60)
oct_img = strip([crop_u("peachy_off.png", ob), crop_u("peachy_both.png", ob)], "peachy_octave.png")
lb = (LEDL[0] - 110, LEDL[1] - 60, LEDL[0] + 110, FSL[1] + 70)
led_img = strip([crop_u("peachy_off.png", lb), crop_u("peachy_both.png", lb)], "peachy_leds.png")
bar = crop_u("peachy_status.png", (380, 650, 860, 808))
bar_img = os.path.join(TMP, "peachy_bar.png"); bar.save(bar_img)

# ---------- couverture : pedale detouree, reflet au sol, halo rouge tres doux ----------
shot = Image.open(os.path.join(SHOTS, "peachy_both.png")).convert("RGB")
kk = shot.height / 928.0
mk = Image.new("L", shot.size, 0); md = ImageDraw.Draw(mk)
md.rounded_rectangle((PX0 * kk, PY0 * kk, (PX0 + PW) * kk, (PY0 + PH) * kk), radius=PR * kk, fill=255)
for jx in (JACK_OUT_X, JACK_IN_X):
    md.rectangle(((jx - 58) * kk, max(0.0, JACK_Y - 16) * kk, (jx + 58) * kk, PY0 * kk + 2), fill=255)
arr = np.asarray(shot).astype(np.float32).mean(-1)
m = np.asarray(mk).astype(np.float32)
body = Image.new("L", shot.size, 0); ImageDraw.Draw(body).rounded_rectangle((PX0 * kk, PY0 * kk, (PX0 + PW) * kk, (PY0 + PH) * kk), radius=PR * kk, fill=255)
bm = np.asarray(body).astype(np.float32)
m = np.where(bm > 0, bm, m * (arr > 60))          # jacks : seulement le metal (pas le fond sombre)
from PIL import ImageFilter
mk = Image.fromarray(m.astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))
cut = Image.new("RGBA", shot.size, (0, 0, 0, 0)); cut.paste(shot, (0, 0), mk)
cut = cut.crop(cut.getbbox())
cw, ch = cut.size
pad = int(cw * 0.14)
canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
gcx, gcy = canvas.width / 2, pad + ch * 0.45
glow = np.exp(-(((xx - gcx) / (cw * 0.62)) ** 2 + ((yy - gcy) / (ch * 0.48)) ** 2)) * 40
edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
halo = np.dstack([glow * 0.95, glow * 0.12, glow * 0.12, np.full_like(glow, 255)]).astype(np.uint8)
canvas = Image.alpha_composite(canvas, Image.fromarray(halo, "RGBA"))
refl = cut.transpose(Image.FLIP_TOP_BOTTOM)
ra = np.array(refl).astype(np.float64)
fade = np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22
ra[..., 3] *= fade[:, None]
canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
canvas.alpha_composite(cut, (pad, pad))
canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
cover = os.path.join(TMP, "peachy_cover_cutout.png"); canvas.convert("RGB").save(cover)

def img(path, width):
    im = Image.open(path); h = width * im.height / im.width
    return RLImage(path, width=width, height=h)

st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=24, textColor=ACC, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=13, leading=17, textColor=DARK, spaceAfter=4, spaceBefore=10, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=5),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
    "cover": ParagraphStyle("cover", fontName="PopL", fontSize=40, leading=48, textColor=colors.white, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="Pop", fontSize=12, leading=16, textColor=colors.HexColor("#C9BFBF"), alignment=TA_CENTER),
    "coverlang": ParagraphStyle("coverlang", fontName="PopM", fontSize=8.5, leading=11, textColor=ACC_L),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items): return [P("&bull;&nbsp;&nbsp;" + i) for i in items]
def table(rows, widths, header=True):
    data = [[P(c, "cellb" if (header and r == 0) else "cell") for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#E3D2D0")),
             ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]
    t.setStyle(TableStyle(style)); return t

# ---------- donnees (identiques a PluginProcessor.cpp) ----------
FACTORY = [  # nom, master, gain, vol, dist, bass, mid, treb, tone, pres, oct, pre, fuzz
    ("Indigo Crunch",            5.0, 6.5, 5.0, 7.5, 6.0, 4.0, 6.5, 5.0, 6.0, 0, 1, 0),
    ("Seven String Chug",        5.0, 7.8, 5.0, 7.5, 5.5, 3.2, 6.2, 5.0, 6.8, 0, 1, 0),
    ("Scooped Rhythm",           5.2, 8.5, 5.0, 7.5, 7.0, 2.0, 7.0, 5.0, 6.0, 0, 1, 0),
    ("Woolly Fuzz",              5.0, 6.0, 5.0, 7.5, 5.0, 5.0, 5.0, 5.0, 5.0, 0, 0, 1),
    ("Fuzz Octave Sludge",       5.0, 6.0, 5.0, 9.0, 5.0, 5.0, 5.0, 3.5, 5.0, 1, 0, 1),
    ("Bright Fuzz Lead",         5.0, 6.0, 5.3, 10., 5.0, 5.0, 5.0, 8.0, 5.0, 0, 0, 1),
    ("Fuzz Into Preamp",         4.6, 5.0, 4.0, 7.0, 5.5, 4.5, 6.0, 4.5, 5.5, 0, 1, 1),
    ("Wall of Sludge",           4.6, 6.0, 4.0, 9.0, 6.5, 3.5, 6.0, 4.0, 6.0, 1, 1, 1),
    ("Gritty Boost",             6.0, 2.2, 5.0, 7.5, 5.0, 6.0, 6.0, 5.0, 5.5, 0, 1, 0),
    ("Capture Reference G75 T5", 5.0, 6.0, 5.0, 7.5, 5.0, 5.0, 5.0, 5.0, 5.0, 0, 0, 1)]
DESC = {
 "en": ["Preamp only: tight, saturated crunch with a light mid scoop, the starting point.",
        "More gain, deeper scoop and more presence for low-tuned 7-string riffs and palm mutes.",
        "High gain, bass up, mids down, treble up: the classic scooped rhythm wall.",
        "Fuzz only, exactly as captured at noon tone: woolly, thick, long sustain.",
        "Octave doubler into the fuzz and a darker tone: huge, slow, sludgy riffs.",
        "Full distortion and a bright tone: a cutting fuzz lead.",
        "Fuzz feeding a moderate preamp: the fuzz blends into the preamp drive.",
        "Everything on, with the octave: the densest wall the pedal can make.",
        "Low preamp gain with a bit more level: an edgy boost for an amp that is already crunchy.",
        "The original capture (distortion 75 %, tone 5) played back untouched, to compare."],
 "fr": ["Préampli seul : crunch serré et saturé, médiums légèrement creusés, le point de départ.",
        "Plus de gain, médiums plus creusés et plus de présence pour les riffs et palm mutes en 7 cordes accordée bas.",
        "Beaucoup de gain, graves montés, médiums baissés, aigus montés : le mur rythmique creusé classique.",
        "Fuzz seul, exactement comme la capture tone à midi : laineux, épais, long sustain.",
        "Octave doubler dans le fuzz et un tone plus sombre : riffs énormes, lents, sludge.",
        "Distortion à fond et tone brillant : un solo fuzz qui tranche.",
        "Le fuzz attaque un préampli modéré : le fuzz se mélange au drive du préampli.",
        "Tout enclenché, avec l'octave : le mur le plus dense que la pédale sait faire.",
        "Peu de gain de préampli, un peu plus de niveau : un boost mordant devant un ampli déjà crunch.",
        "La capture d'origine (distortion 75 %, tone 5) restituée telle quelle, pour comparer."]}
def pc(v): return f"{round(v * 10)} %"

T = {
"en": dict(
 lang="ENGLISH", sub="Preamp + fuzz &nbsp;·&nbsp; VST3 for Windows and macOS", ver="User manual &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Introduction",
 intro=["<b>PEACHY</b> is a Studio Wiga plugin modelled on the KoRn <b>Indigo Ranch</b> signature pedal, a double pedal that combines a "
        "high-gain <b>preamp</b> and a separate <b>fuzz</b> with a low <b>octave</b>, voiced after the guitar sounds of the band's mid-90s "
        "records (tight, saturated crunch for 7-string riffs; woolly, sustain-heavy fuzz). Studio Wiga is not affiliated with the band or the maker of the pedal.",
        "The <b>fuzz is the real pedal</b>: it is played by 45 NAM captures of the fuzz section (distortion 50 to 100 %, tone 1 to 9). "
        "Analysis showed that the TONE knob acts as a linear filter after the clipping, so Peachy runs the five TONE 5 captures as neural "
        "networks (NeuralAmpModelerCore), fades between the two nearest ones when you turn DISTORTION, and applies an exact model of the TONE "
        "filter measured on the nine captured positions. Checked against the captures, the difference is of the same order as the error of the captures themselves.",
        "The <b>preamp</b> was not captured: it is modelled as a high-gain tube preamp (two clipping stages with an asymmetric triode curve, "
        "low end tightened before the clipping, passive 3-band tone stack computed exactly, presence at the output), with 4x oversampling.",
        "Signal path: input &gt; OCTAVE &gt; FUZZ &gt; PREAMP &gt; output; the fuzz drives the preamp, as on the pedal."],
 inst_t="2. Installation",
 inst=[("Windows", "Download the <b>Peachy-VST3-Windows</b> artifact from the Actions tab of the GitHub repository studiowiga/lifeispeachy, unzip it and copy the <b>Peachy.vst3</b> folder into <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Download <b>Peachy-VST3-macOS</b>, unzip it and copy <b>Peachy.vst3</b> into <b>~/Library/Audio/Plug-Ins/VST3</b>. If macOS blocks it, run <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Peachy.vst3</b> in the Terminal. The binary is universal (Apple Silicon and Intel)."),
       ("Reaper", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. The plugin appears as <b>Peachy (Studio Wiga)</b>. To control it from MIDI footswitches and expression pedals, the track must receive MIDI from your controller (e.g. the M-Vave Chocolate Plus).")],
 ov_t="3. Interface overview",
 ov_rows=[["#", "Control", "Role"],
   ["1", "MASTER", "Preamp output level."], ["2", "GAIN", "Preamp gain."], ["3", "VOLUME", "Fuzz output level."],
   ["4", "DISTORTION", "Fuzz gain."], ["5", "BASS", "Preamp bass."], ["6", "MID", "Preamp mids."], ["7", "TREBLE", "Preamp treble."],
   ["8", "TONE", "Fuzz tone (dark / bright)."], ["9", "PRESENCE", "Preamp presence (small knob)."],
   ["10", "OCTAVE", "Octave doubler of Semitones (lever up = on), works with or without the fuzz."],
   ["11", "LED left", "Preamp on."], ["12", "LED right", "Fuzz on."],
   ["13", "Left footswitch", "Preamp on / off."], ["14", "Right footswitch", "Fuzz on / off."],
   ["15", "Status line", "Setting of the control you just moved."], ["16", "Preset bar", "Arrows = previous / next, name = preset menu."],
   ["17", "IN", "Input jack (top right)."], ["18", "OUT", "Output jack (top left)."]],
 ctl_t="4. Controls in detail",
 ctl=[("GAIN (preamp)", "From about 6 dB to 56 dB, spread over two tube-like stages. Low settings give an edgy, gritty boost; from noon the sound becomes the tight, saturated crunch of the pedal; the top of the travel adds sustain and density. The low end is tightened before the clipping so low-tuned riffs stay defined."),
      ("BASS · MID · TREBLE (preamp)", "Passive tone stack after the clipping, as in a British amp head (exact analog transfer function). The three controls interact like on an amp: MID at minimum gives the deep scoop, BASS follows a logarithmic taper, TREBLE adds bite. All three only act on the preamp."),
      ("PRESENCE (preamp)", "High shelf at 3.2 kHz, about −10 to +10 dB, same slope on both sides of noon: more attack and air, or a softer top end."),
      ("MASTER (preamp) · VOLUME (fuzz)", "Output level of each section. Noon = reference level (for the fuzz: the level of the captures), full = about +10 dB, 0 = muted. The two sections can be levelled independently so that switching between them does not jump."),
      ("DISTORTION (fuzz)", "From 50 % to 100 % the sound comes straight from the captures of the real pedal, with a smooth fade between the two nearest captured positions (50, 65, 75, 90, 100 %). Below 50 % the 50 % capture is driven by a lower input level: less saturation, more of the pick attack, as when turning the real knob down."),
      ("TONE (fuzz)", "Exact model of the pedal's TONE filter, measured on the nine captured positions (rms error 0.4 dB): counter-clockwise rolls off the highs (dark, woolly), clockwise cuts the lows and lifts the highs (thin, bright, cutting). Knob position 0 to 10 covers the captured range 1 to 9; noon = 5."),
      ("OCTAVE", "Switches on exactly the Octave doubler (-12) preset of our Semitones pedal, placed in front of Peachy: the same pitch-shifting engine transposes your guitar one octave down and blends it 50 / 50 with your dry signal (equal power, so the level stays the same). The doubled signal then goes into the fuzz and the preamp when they are on, and straight to the output when both footswitches are off.")],
 oct_cap="OCTAVE switch off (lever down) and on (lever up).",
 fs_t="5. Footswitches, LEDs and noise gate",
 fs=["<b>Footswitches</b>: black footswitches. Left = PREAMP, right = FUZZ, each independent. They switch as soon as you press them; while you hold the mouse button the cap stays pushed down (it sinks, gets darker and its shadow shortens). Hold for more than 0.45 s after turning a section on and it works as a momentary switch (it turns off when you release it); right-click &gt; <b>Settings</b> to disable.",
     "<b>LEDs</b>: white, left = preamp on, right = fuzz on, with a fade, a white halo and the panel lit around them.",
     "<b>Bypass</b>: each section passes the dry signal when off, time-aligned with the latency reported to the DAW (no clicks, crossfaded).",
     "<b>Noise gate</b> (right-click &gt; Noise gate): Off, <b>Light</b> (default), Medium or Strong. It listens to the input, opens instantly on the next note and lets notes fade naturally; it only acts when a section is on.",
     "Everything can also be automated in the DAW (all knobs, Octave, Preamp Footswitch, Fuzz Footswitch)."],
 midi_t="6. MIDI",
 midi_txt=["<b>MIDI Learn on every control</b>: right-click a control (any of the 9 knobs, the OCTAVE switch or one of the footswitches). The top of the menu shows <b>MIDI Learn: &lt;Name&gt; (CC n / none)</b> with the current assignment, <b>Clear MIDI: &lt;Name&gt;</b> and, while a learn is waiting, <b>Cancel MIDI learn</b>. Choose MIDI Learn, then move a pedal or knob or press a footswitch of your controller: the status line shows <b>MIDI LEARN: move a control for &lt;Name&gt;</b>, then <b>&lt;Name&gt; = CC n</b> for 2 seconds.",
           "Every right-click menu (on a control or anywhere else on the pedal) also has the <b>MIDI map</b> submenu: <b>Learn</b> for each of the 12 controls (with its current assignment), <b>Cancel MIDI learn</b> and <b>Reset MIDI to defaults (CC 20-31)</b>, plus a reminder of the map. Choosing the pending item again also cancels the learn.",
           "A CC number can only drive one control: learning a CC that is already used removes it from the other control. Learn accepts CC 0 to 119; Peachy listens on all MIDI channels. <b>Assignments are saved with the project</b> (and with DAW presets)."],
 midi_hdr=["Control", "Default CC", "Type"],
 midi=[["Master", "CC 20", "Continuous"], ["Gain", "CC 21", "Continuous"], ["Volume (fuzz)", "CC 22", "Continuous"], ["Distortion", "CC 23", "Continuous"],
       ["Bass", "CC 24", "Continuous"], ["Mid", "CC 25", "Continuous"], ["Treble", "CC 26", "Continuous"], ["Tone (fuzz)", "CC 27", "Continuous"], ["Presence", "CC 28", "Continuous"],
       ["Octave", "CC 29", "Switch: 64 and above = on, below 64 = off"],
       ["Preamp footswitch", "CC 30", "Footswitch: latching (127 = on / 0 = off) or momentary (toggle on each press)"],
       ["Fuzz footswitch", "CC 31", "Footswitch: latching (127 = on / 0 = off) or momentary (toggle on each press)"]],
 midi2=["<b>Knobs</b>: 0-127 covers the whole travel; a knob on CC 0-31 also accepts the 14-bit fine part on CC n+32 (CC 52 to 60 with the default map). Expression pedals are smooth: ramps between messages and automatic learning of your pedal's real travel (0 and 100 % are always reached).",
        "<b>MIDI footswitches</b>: right-click &gt; <b>Settings</b> &gt; <b>MIDI footswitches latching (127 = on / 0 = off)</b> (default) or <b>MIDI footswitches momentary (toggle each press)</b>.",
        "<b>Program Change</b> 1-10 loads the factory presets."],
 led_cap="Preamp section off, then on (LED lit).",
 win_t="7. Window",
 win=["Peachy uses the double pedal format of Ring Think. Right-click &gt; <b>Window size</b>: <b>60 % (default)</b>, <b>75 %</b>, <b>100 %</b>. The window opens at its default size and can never be made smaller; you can also drag the bottom-right corner to enlarge it (proportions kept).",
      "The <b>status line</b> shows the MIDI Learn messages, otherwise only the setting of the control you just moved (knobs in %, e.g. <i>distortion 82 %</i>, <i>fuzz on</i>) in the handwriting of the pedal and fades out after 2.5 s. The <b>preset bar</b> under it always shows the current preset."],
 bar_cap="Status line and preset bar.",
 pre_t="8. Presets",
 pre=["Use the arrows of the preset bar, click its name, right-click anywhere, or the preset menu of your DAW. Right-click &gt; <b>Save user preset</b> stores up to 9 presets of your own. Presets include the footswitch and octave states."],
 fac_hdr=["PC", "Preset", "Preamp: master / gain · bass / mid / treble · presence", "Fuzz: volume / distortion / tone", "On", "Description"],
 tips_t="9. Tips",
 tips=["In Reaper, place Peachy <b>before</b> your amp simulator (for example before Two Notes Genome) with a fairly clean amp, as with the real pedal; the preamp can also go straight into a power amp / cabinet model.",
       "For a 7-string tuned to G or lower: keep BASS around noon, raise PRESENCE a little, and add the OCTAVE for extra weight.",
       "Learn DISTORTION or GAIN on an expression pedal to push the saturation with your foot during a riff.",
       "Fuzz into preamp: lower the fuzz VOLUME and the preamp GAIN together; the fuzz already brings a lot of saturation.",
       "Between two captured DISTORTION positions Peachy runs two networks at once (about 20 % of one core at 48 kHz); at a captured position (50, 65, 75, 90 or 100 %) only one runs."],
 tech_t="10. Specifications",
 tech=[["Format", "VST3 (Windows 64-bit, macOS universal)"], ["Channels", "Mono or stereo (processed in mono, like the pedal)"],
       ["Latency", "32 samples at 48 kHz (preamp oversampling), more at other rates (resampling to the 48 kHz of the captures); reported to the DAW"],
       ["Fuzz", "5 NAM WaveNet captures (distortion 50 / 65 / 75 / 90 / 100 %), exact TONE filter"], ["Octave", "Semitones engine (pitch-synchronous splicing, YIN tracking down to 31 Hz), -12 semitones, 50 / 50 equal-power blend"],
       ["Preamp", "6 to 56 dB gain, 4x oversampling, passive 3-band tone stack, presence ±10 dB"],
       ["MIDI", "MIDI Learn on all 12 controls (saved with the project), defaults CC 20-31 (14-bit on CC 52-60), Program Change 1-10, all channels"],
       ["Window", "60 % (default, also the minimum size), 75 %, 100 %; anti-freeze v2 on Windows (software rendering, display caches rebuilt by a timer, redraw watchdog, no call to the host from the audio thread)"],
       ["Version", VERSION], ["CPU", "about 3 % (preamp), 10 to 20 % (fuzz) of one core at 48 kHz"]]),
"fr": dict(
 lang="FRANÇAIS", sub="Préampli + fuzz &nbsp;·&nbsp; VST3 pour Windows et macOS", ver="Notice d'utilisation &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Présentation",
 intro=["<b>PEACHY</b> est un plugin Studio Wiga inspiré de la pédale signature KoRn <b>Indigo Ranch</b>, une pédale double qui réunit un "
        "<b>préampli</b> high gain et un <b>fuzz</b> séparé avec une <b>octave</b> grave, au son des guitares des disques du groupe du milieu des années 90 "
        "(crunch serré et saturé pour les riffs en 7 cordes ; fuzz laineux à gros sustain). Studio Wiga n'a aucun lien avec le groupe ni avec le fabricant de la pédale.",
        "Le <b>fuzz, c'est la vraie pédale</b> : il est joué par 45 captures NAM de la section fuzz (distortion 50 à 100 %, tone 1 à 9). "
        "L'analyse a montré que le TONE agit comme un filtre linéaire après l'écrêtage : Peachy fait tourner les cinq captures TONE 5 en réseaux "
        "de neurones (NeuralAmpModelerCore), passe en fondu entre les deux plus proches quand on tourne DISTORTION, et applique un modèle exact du "
        "filtre TONE mesuré sur les neuf positions capturées. Comparé aux captures, l'écart est du même ordre que l'erreur des captures elles-mêmes.",
        "Le <b>préampli</b> n'a pas été capturé : il est modélisé comme un préampli à lampes high gain (deux étages d'écrêtage à courbe de triode asymétrique, "
        "grave serré avant l'écrêtage, tone stack passif 3 bandes calculé exactement, présence en sortie), avec un sur-échantillonnage x4.",
        "Trajet du signal : entrée &gt; OCTAVE &gt; FUZZ &gt; PREAMP &gt; sortie ; le fuzz attaque le préampli, comme sur la pédale."],
 inst_t="2. Installation",
 inst=[("Windows", "Téléchargez l'artefact <b>Peachy-VST3-Windows</b> dans l'onglet Actions du dépôt GitHub studiowiga/lifeispeachy, décompressez-le et copiez le dossier <b>Peachy.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Téléchargez <b>Peachy-VST3-macOS</b>, décompressez-le et copiez <b>Peachy.vst3</b> dans <b>~/Bibliothèque/Audio/Plug-Ins/VST3</b>. Si macOS le bloque, tapez <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Peachy.vst3</b> dans le Terminal. Le binaire est universel (Apple Silicon et Intel)."),
       ("Reaper", "Options &gt; Préférences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Le plugin apparaît sous le nom <b>Peachy (Studio Wiga)</b>. Pour le piloter avec des footswitchs et pédales d'expression MIDI, la piste doit recevoir le MIDI de votre contrôleur (par exemple la M-Vave Chocolate Plus).")],
 ov_t="3. Vue d'ensemble de l'interface",
 ov_rows=[["#", "Commande", "Rôle"],
   ["1", "MASTER", "Volume de sortie du préampli."], ["2", "GAIN", "Gain du préampli."], ["3", "VOLUME", "Volume de sortie du fuzz."],
   ["4", "DISTORTION", "Gain du fuzz."], ["5", "BASS", "Graves du préampli."], ["6", "MID", "Médiums du préampli."], ["7", "TREBLE", "Aigus du préampli."],
   ["8", "TONE", "Tonalité du fuzz (sombre / brillant)."], ["9", "PRESENCE", "Présence du préampli (petit potard)."],
   ["10", "OCTAVE", "Octave doubler de Semitones (levier haut = on), avec ou sans fuzz."],
   ["11", "LED gauche", "Préampli enclenché."], ["12", "LED droite", "Fuzz enclenché."],
   ["13", "Footswitch gauche", "Préampli on / off."], ["14", "Footswitch droit", "Fuzz on / off."],
   ["15", "Ligne d'état", "Réglage de la commande que vous venez de bouger."], ["16", "Barre de presets", "Flèches = précédent / suivant, nom = menu des presets."],
   ["17", "IN", "Jack d'entrée (en haut à droite)."], ["18", "OUT", "Jack de sortie (en haut à gauche)."]],
 ctl_t="4. Les commandes en détail",
 ctl=[("GAIN (préampli)", "D'environ 6 dB à 56 dB, répartis sur deux étages façon lampe. En bas de course, un boost mordant et granuleux ; dès midi, le crunch serré et saturé de la pédale ; le haut de la course ajoute du sustain et de la densité. Le grave est resserré avant l'écrêtage pour que les riffs accordés bas restent définis."),
      ("BASS · MID · TREBLE (préampli)", "Tone stack passif après l'écrêtage, comme dans une tête d'ampli britannique (fonction de transfert analogique exacte). Les trois commandes interagissent comme sur un ampli : MID au minimum donne le gros creux de médiums, BASS suit une courbe logarithmique, TREBLE ajoute du mordant. Elles n'agissent que sur le préampli."),
      ("PRESENCE (préampli)", "Plateau d'aigus à 3,2 kHz, environ −10 à +10 dB, même pente des deux côtés de midi : plus d'attaque et d'air, ou un haut du spectre plus doux."),
      ("MASTER (préampli) · VOLUME (fuzz)", "Volume de sortie de chaque section. Midi = niveau de référence (pour le fuzz : le niveau des captures), à fond = environ +10 dB, 0 = coupé. Les deux sections se règlent séparément pour passer de l'une à l'autre sans saut de volume."),
      ("DISTORTION (fuzz)", "De 50 % à 100 %, le son vient directement des captures de la vraie pédale, avec un fondu entre les deux positions capturées les plus proches (50, 65, 75, 90, 100 %). Sous 50 %, la capture 50 % reçoit un niveau d'entrée plus faible : moins de saturation, plus d'attaque du médiator, comme en baissant le vrai potard."),
      ("TONE (fuzz)", "Modèle exact du filtre TONE de la pédale, mesuré sur les neuf positions capturées (erreur rms 0,4 dB) : vers la gauche, les aigus s'adoucissent (sombre, laineux) ; vers la droite, le grave diminue et les aigus montent (fin, brillant, tranchant). La course 0 à 10 couvre les positions capturées 1 à 9 ; midi = 5."),
      ("OCTAVE", "Enclenche exactement le preset Octave doubler (-12) de notre pédale Semitones, placé devant Peachy : le même moteur de transposition descend votre guitare d'une octave et la mélange à 50 / 50 avec le son direct (à puissance égale, le niveau ne change pas). Le signal doublé entre ensuite dans le fuzz et le préampli s'ils sont enclenchés, et va droit en sortie deux footswitchs coupés.")],
 oct_cap="Inverseur OCTAVE coupé (levier en bas) et enclenché (levier en haut).",
 fs_t="5. Footswitchs, LED et noise gate",
 fs=["<b>Footswitchs</b> : footswitchs noirs. Gauche = PREAMP, droit = FUZZ, indépendants. Ils basculent dès l'appui ; tant qu'on garde le bouton de la souris enfoncé, le bouton reste enfoncé (il descend, s'assombrit et son ombre se raccourcit). Maintenu plus de 0,45 s après avoir enclenché une section, il fonctionne en momentané (la section se coupe au relâchement) ; clic droit &gt; <b>Settings</b> pour désactiver.",
     "<b>LED</b> : blanches, gauche = préampli actif, droite = fuzz actif, avec fondu, halo blanc et façade éclairée autour.",
     "<b>Bypass</b> : chaque section coupée laisse passer le signal sec, aligné sur la latence annoncée au DAW (sans clic, en fondu).",
     "<b>Noise gate</b> (clic droit &gt; Noise gate) : Off, <b>Light</b> (par défaut), Medium ou Strong. Il écoute l'entrée, s'ouvre instantanément à la note suivante et laisse les notes s'éteindre naturellement ; il n'agit que si une section est enclenchée.",
     "Tout est aussi automatisable dans le DAW (tous les potards, Octave, Preamp Footswitch, Fuzz Footswitch)."],
 midi_t="6. MIDI",
 midi_txt=["<b>MIDI Learn sur toutes les commandes</b> : faites un clic droit sur une commande (l'un des 9 potards, l'inverseur OCTAVE ou l'un des footswitchs). Le haut du menu affiche <b>MIDI Learn: &lt;Nom&gt; (CC n / none)</b> avec l'assignation actuelle, <b>Clear MIDI: &lt;Nom&gt;</b> et, tant qu'un apprentissage est en attente, <b>Cancel MIDI learn</b>. Choisissez MIDI Learn puis bougez une pédale, un bouton ou appuyez sur un footswitch de votre contrôleur : la ligne d'état affiche <b>MIDI LEARN: move a control for &lt;Nom&gt;</b>, puis <b>&lt;Nom&gt; = CC n</b> pendant 2 secondes.",
           "Chaque menu clic droit (sur une commande ou ailleurs sur la pédale) contient aussi le sous-menu <b>MIDI map</b> : <b>Learn</b> pour chacune des 12 commandes (avec son assignation actuelle), <b>Cancel MIDI learn</b> et <b>Reset MIDI to defaults (CC 20-31)</b>, plus un rappel de la table. Choisir de nouveau la commande en attente annule aussi l'apprentissage.",
           "Un numéro de CC ne pilote qu'une seule commande : apprendre un CC déjà utilisé le retire de l'autre commande. L'apprentissage accepte les CC 0 à 119 ; Peachy écoute tous les canaux MIDI. <b>Les assignations sont sauvegardées avec le projet</b> (et avec les presets du DAW)."],
 midi_hdr=["Commande", "CC par défaut", "Type"],
 midi=[["Master", "CC 20", "Continu"], ["Gain", "CC 21", "Continu"], ["Volume (fuzz)", "CC 22", "Continu"], ["Distortion", "CC 23", "Continu"],
       ["Bass", "CC 24", "Continu"], ["Mid", "CC 25", "Continu"], ["Treble", "CC 26", "Continu"], ["Tone (fuzz)", "CC 27", "Continu"], ["Presence", "CC 28", "Continu"],
       ["Octave", "CC 29", "Bascule : 64 et plus = on, moins de 64 = off"],
       ["Preamp footswitch", "CC 30", "Footswitch : à accrochage (127 = on / 0 = off) ou momentané (bascule à chaque appui)"],
       ["Fuzz footswitch", "CC 31", "Footswitch : à accrochage (127 = on / 0 = off) ou momentané (bascule à chaque appui)"]],
 midi2=["<b>Potards</b> : 0-127 couvre toute la course ; un potard sur un CC de 0 à 31 accepte aussi la partie fine 14 bits sur le CC n+32 (CC 52 à 60 avec la table par défaut). Les pédales d'expression sont fluides : rampes entre les messages et apprentissage automatique de la course réelle de la pédale (0 et 100 % toujours atteints).",
        "<b>Footswitchs MIDI</b> : clic droit &gt; <b>Settings</b> &gt; <b>MIDI footswitches latching (127 = on / 0 = off)</b> (par défaut) ou <b>MIDI footswitches momentary (toggle each press)</b>.",
        "<b>Program Change</b> 1 à 10 : presets usine."],
 led_cap="Section préampli coupée, puis enclenchée (LED allumée).",
 win_t="7. Fenêtre",
 win=["Peachy utilise le format pédale double de Ring Think. Clic droit &gt; <b>Window size</b> : <b>60 % (default)</b>, <b>75 %</b>, <b>100 %</b>. La fenêtre s'ouvre à sa taille par défaut et ne peut jamais être réduite en dessous ; on peut aussi tirer le coin en bas à droite pour l'agrandir (proportions conservées).",
      "La <b>ligne d'état</b> affiche les messages du MIDI Learn, sinon seulement le réglage de la commande que vous venez de bouger (potards en %, par ex. <i>distortion 82 %</i>, <i>fuzz on</i>) dans l'écriture manuscrite de la pédale, et s'efface au bout de 2,5 s. La <b>barre de presets</b> en dessous affiche en permanence le preset en cours."],
 bar_cap="Ligne d'état et barre de presets.",
 pre_t="8. Presets",
 pre=["Utilisez les flèches de la barre de presets, cliquez sur son nom, faites un clic droit n'importe où, ou passez par le menu de presets du DAW. Clic droit &gt; <b>Save user preset</b> enregistre jusqu'à 9 presets personnels. Les presets incluent l'état des footswitchs et de l'octave."],
 fac_hdr=["PC", "Preset", "Préampli : master / gain · bass / mid / treble · presence", "Fuzz : volume / distortion / tone", "Actif", "Description"],
 tips_t="9. Conseils d'utilisation",
 tips=["Dans Reaper, placez Peachy <b>avant</b> votre simulateur d'ampli (par exemple avant Two Notes Genome) avec un ampli assez clair, comme avec la vraie pédale ; le préampli peut aussi aller directement dans un modèle d'ampli de puissance / baffle.",
       "Pour une 7 cordes accordée en Sol ou plus bas : gardez BASS vers midi, montez un peu PRESENCE, et ajoutez l'OCTAVE pour plus de poids.",
       "Apprenez DISTORTION ou GAIN sur une pédale d'expression pour pousser la saturation au pied pendant un riff.",
       "Fuzz dans le préampli : baissez ensemble le VOLUME du fuzz et le GAIN du préampli ; le fuzz apporte déjà beaucoup de saturation.",
       "Entre deux positions capturées de DISTORTION, Peachy fait tourner deux réseaux à la fois (environ 20 % d'un cœur à 48 kHz) ; sur une position capturée (50, 65, 75, 90 ou 100 %), un seul."],
 tech_t="10. Caractéristiques techniques",
 tech=[["Format", "VST3 (Windows 64 bits, macOS universel)"], ["Canaux", "Mono ou stéréo (traité en mono, comme la pédale)"],
       ["Latence", "32 échantillons à 48 kHz (sur-échantillonnage du préampli), davantage aux autres fréquences (rééchantillonnage vers les 48 kHz des captures) ; annoncée au DAW"],
       ["Fuzz", "5 captures NAM WaveNet (distortion 50 / 65 / 75 / 90 / 100 %), filtre TONE exact"], ["Octave", "Moteur de Semitones (raccords synchronisés sur la période, suivi YIN jusqu'à 31 Hz), -12 demi-tons, mélange 50 / 50 à puissance égale"],
       ["Préampli", "gain 6 à 56 dB, sur-échantillonnage x4, tone stack passif 3 bandes, présence ±10 dB"],
       ["MIDI", "MIDI Learn sur les 12 commandes (sauvegardé avec le projet), CC 20 à 31 par défaut (14 bits sur CC 52 à 60), Program Change 1 à 10, tous canaux"],
       ["Fenêtre", "60 % (par défaut, c'est aussi la taille minimale), 75 %, 100 % ; anti-gel v2 sous Windows (rendu logiciel, caches d'affichage refaits par un timer, chien de garde de redessin, aucun appel à l'hôte depuis le thread audio)"],
       ["Version", VERSION], ["Processeur", "environ 3 % (préampli), 10 à 20 % (fuzz) d'un cœur à 48 kHz"]]),
}

W = A4[0] - 36 * mm
def section(lang):
    t = T[lang]; f = []
    f.append(NextPageTemplate("p"))
    f.append(Spacer(1, 2 * mm)); f.append(P(t["lang"], "coverlang")); f.append(Spacer(1, 4 * mm))
    f.append(img(cover, W * 0.80)); f.append(Spacer(1, 3 * mm))
    f.append(P("P E A C H Y", "cover")); f.append(P(t["sub"], "coversub")); f.append(Spacer(1, 2 * mm))
    f.append(P(t["ver"], "coversub")); f.append(Spacer(1, 8 * mm))
    f.append(img(os.path.join(ROOT, "assets", "logo_studio_wiga_blanc.png"), 52 * mm))
    f.append(PageBreak())
    f.append(P(t["intro_t"], "h1")); f += [P(x) for x in t["intro"]]
    f.append(P(t["inst_t"], "h1"))
    for h, x in t["inst"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(PageBreak())
    f.append(P(t["ov_t"], "h1"))
    f.append(img(annotated, W))
    f.append(Spacer(1, 3 * mm))
    f.append(table(t["ov_rows"], [10 * mm, 38 * mm, W - 48 * mm]))
    f.append(PageBreak())
    f.append(P(t["ctl_t"], "h1"))
    for h, x in t["ctl"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(KeepTogether([Spacer(1, 3 * mm), img(oct_img, W * 0.42), P(t["oct_cap"], "small")]))
    f.append(P(t["fs_t"], "h1")); f += bullets(t["fs"])
    f.append(KeepTogether([Spacer(1, 2 * mm), img(led_img, W * 0.46), P(t["led_cap"], "small")]))
    f.append(P(t["midi_t"], "h1")); f += [P(x) for x in t["midi_txt"]]
    f.append(Spacer(1, 2 * mm)); f.append(KeepTogether([table([t["midi_hdr"]] + t["midi"], [38 * mm, 24 * mm, W - 62 * mm])])); f.append(Spacer(1, 3 * mm))
    f += bullets(t["midi2"])
    f.append(P(t["win_t"], "h1")); f.append(P(t["win"][0]))
    wt = Table([[[P(t["win"][1])], [img(bar_img, 70 * mm), P(t["bar_cap"], "small")]]], colWidths=[W - 74 * mm, 74 * mm])
    wt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (0, 0), 0)]))
    f.append(wt)
    f.append(CondPageBreak(150 * mm))
    f.append(P(t["pre_t"], "h1")); f += [P(x) for x in t["pre"]]
    rows = [t["fac_hdr"]]
    for i, (n, ma, ga, vo, di, ba, mi, tr, to, pr, oc, pon, fon) in enumerate(FACTORY):
        on = " + ".join([x for x, b in (("preamp", pon), ("fuzz", fon), ("octave", oc)) if b])
        rows.append([str(i + 1), "<b>" + n + "</b>", f"{pc(ma)} / {pc(ga)} · {pc(ba)} / {pc(mi)} / {pc(tr)} · {pc(pr)}",
                     f"{pc(vo)} / {pc(di)} / {pc(to)}", on, DESC[lang][i]])
    f.append(table(rows, [9 * mm, 28 * mm, 34 * mm, 24 * mm, 20 * mm, W - 115 * mm]))
    f.append(KeepTogether([P(t["tips_t"], "h1")] + bullets(t["tips"]) + [P(t["tech_t"], "h1"), table(t["tech"], [30 * mm, W - 30 * mm], header=False)]))
    f.append(Spacer(1, 4 * mm))
    f.append(PageBreak())
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Peachy (Studio Wiga) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 3 * mm, A4[0], 3 * mm, stroke=0, fill=1); c.restoreState()
doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="PEACHY - Manual / Notice v" + VERSION, author="Studio Wiga")
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
en = section("en"); fr = section("fr")
en[-1:] = [NextPageTemplate("cover"), PageBreak()]
doc.build(en + fr[:-1])
print(OUT)
