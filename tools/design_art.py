# Wharry : prépare les images du plugin à partir du design de l'utilisateur (design/wharry_design_source.png, 1390 x 1132)
#   background.png   : fond 1240 x 928 (textes réécrits, LED / écran vidés, logo Studio Wiga, jacks Studio Wiga)
#   treadle_down.png : calque du plateau à plat (orteil enfoncé) ; le plugin calcule les autres positions en perspective
#   fs_on.png / fs_off.png, toggle_r.png / toggle_l.png : sprites des commandes
#   tools/layout.txt : coordonnées des commandes dans le repère 1240 x 928 (reprises dans PluginEditor.cpp)
#   design/apercu_treadle_up.png : aperçu de la position orteil levé
import os, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = Image.open(os.path.join(ROOT, "design", "wharry_design_source.png")).convert("RGB")
A = np.array(src)

W, H = 1240, 928
CX0, CY0, CX1, CY1 = 158, 40, 1226, 1062          # zone utile du design
S = 0.856
OX = (W - (CX1 - CX0) * S) / 2.0
OY = 6.0
def C(x, y): return (OX + (x - CX0) * S, OY + (y - CY0) * S)

# ---------- 1) nettoyage (inpainting) ----------
mask = np.zeros(A.shape[:2], np.uint8)
def text_mask(x0, y0, x1, y1, thr=40, grow=2):
    sub = cv2.cvtColor(A[y0:y1, x0:x1], cv2.COLOR_RGB2GRAY).astype(np.float32)
    med = np.median(sub)
    m = ((sub - med) > thr).astype(np.uint8) * 255
    m = cv2.dilate(m, np.ones((2 * grow + 1, 2 * grow + 1), np.uint8))
    mask[y0:y1, x0:x1] |= m
def circ_mask(x, y, r): cv2.circle(mask, (int(round(x)), int(round(y))), int(r), 255, -1)

for t in [(722, 82, 895, 120),            # WHAMMY (haut)
          (286, 84, 378, 120),            # W·CC sur le plateau
          (895, 162, 1015, 492),          # étiquettes de la liste
          (1088, 184, 1140, 210), (1150, 186, 1194, 236),   # AUX + pictogramme
          (765, 674, 862, 704), (910, 674, 982, 704), (1052, 674, 1108, 704),   # TONE LE / TONE / MIX
          (742, 950, 990, 995)]:          # WHAMMY (bas)
    text_mask(*t)

ARC = [(770, 348), (745, 327), (733, 298), (729, 264), (733, 230), (745, 198), (764, 179), (788, 167), (814, 163), (842, 167), (869, 179)]
LIST = [(897, 209), (911, 242), (913, 274), (909, 306), (896, 340), (893, 373), (893, 406), (893, 440), (893, 473), (893, 506)]
LEDS = ARC + LIST                                  # LED et étiquettes du design effacées : le programme s'affiche sur l'écran
for i, (x, y) in enumerate(LEDS): circ_mask(x, y, 15)
circ_mask(1063, 873, 30)                           # LED du footswitch (dessinée par le plugin)
# halos orangés des LED allumées (à gauche du sélecteur) : détectés par leur vert élevé, puis reconstruits
reg = (717, 150, 800, 385)
gmed = np.median(A[reg[1]:reg[3], reg[0]:reg[2], 1])
R0 = A[reg[1]:reg[3], reg[0]:reg[2]].astype(np.int32)
glow = (((R0[..., 1] - R0[..., 2]) > 6) & (R0[..., 0] > 175)).astype(np.uint8) * 255   # orangé : vert > bleu (le panneau rouge a bleu >= vert)
yy2, xx2 = np.mgrid[reg[1]:reg[3], reg[0]:reg[2]]
glow = cv2.dilate(glow, np.ones((11, 11), np.uint8))
glow[np.hypot(xx2 - 822, yy2 - 250) < 77] = 0           # épargne le bouton
mask[reg[1]:reg[3], reg[0]:reg[2]] |= glow
inpainted = cv2.inpaint(A, mask, 6, cv2.INPAINT_TELEA)

# zone du sélecteur (anciennes LED, halos, étiquettes) : panneau brossé reconstruit =
# dégradé de couleur du panneau (plan ajusté sur les pixels propres) + grain brossé pris dans une zone propre
ZX0, ZY0, ZX1, ZY1 = 717, 140, 1016, 515
yy3, xx3 = np.mgrid[ZY0:ZY1, ZX0:ZX1]
rk = np.hypot(xx3 - 822, yy3 - 256)
knob = rk < 78                                           # bouton + son socle (un peu plus bas que le chapeau)
halo = (rk >= 78) & (rk < 93) & (yy3 < 292)               # reflet des anciennes LED autour du bouton (garde l'ombre portée du bas)
zone_mask = ((mask[ZY0:ZY1, ZX0:ZX1] > 0) | halo) & ~knob
zone_mask = cv2.dilate(zone_mask.astype(np.uint8), np.ones((5, 5), np.uint8)).astype(bool) & ~knob
Z = A[ZY0:ZY1, ZX0:ZX1].astype(np.float32)
clean_px = ~zone_mask & ~(rk < 100) & (np.abs(xx3 - 813) > 4)
X = np.stack([xx3[clean_px], yy3[clean_px], np.ones(clean_px.sum())], 1).astype(np.float64)
coef = np.linalg.lstsq(X, Z[clean_px].astype(np.float64), rcond=None)[0]
plane = (np.stack([xx3, yy3, np.ones_like(xx3)], -1).astype(np.float64) @ coef).astype(np.float32)
tex_src = A[365:530, 828:878].astype(np.float32)
tex = tex_src - cv2.GaussianBlur(tex_src, (0, 0), 6)
th, tw = tex.shape[:2]
tile = np.tile(tex, ((ZY1 - ZY0) // th + 1, (ZX1 - ZX0) // tw + 1, 1))[:ZY1 - ZY0, :ZX1 - ZX0]
synth = np.clip(plane + tile, 0, 255)
soft = cv2.GaussianBlur(zone_mask.astype(np.float32), (0, 0), 4.5)[..., None]
soft[knob] = 0
region = inpainted[ZY0:ZY1, ZX0:ZX1].astype(np.float32)
inpainted[ZY0:ZY1, ZX0:ZX1] = (synth * soft + region * (1 - soft)).astype(np.uint8)

# écran vidé : le plugin y dessine l'affichage
SCR = (1024, 284, 1166, 484)
h, w = SCR[3] - SCR[1], SCR[2] - SCR[0]
yy = np.linspace(0, 1, h)[:, None, None]
inpainted[SCR[1]:SCR[3], SCR[0]:SCR[2]] = np.broadcast_to(np.array([8, 16, 24]) * (1 - 0.3 * yy), (h, w, 3)).astype(np.uint8)
clean = Image.fromarray(inpainted)

# ---------- 2) mise à l'échelle sur le fond ----------
# le fond texturé du design est prolongé par miroir pour remplir toute la fenêtre (pas de cadre visible)
PAD = 200
ext = cv2.copyMakeBorder(np.array(clean), PAD, PAD, PAD, PAD, cv2.BORDER_REFLECT)
sx0 = CX0 - OX / S; sy0 = CY0 - OY / S
box = (round(sx0 + PAD), round(sy0 + PAD), round(sx0 + PAD + W / S), round(sy0 + PAD + H / S))
canvas = Image.fromarray(ext).crop(box).resize((W, H), Image.LANCZOS).convert("RGBA")

jo = Image.open(os.path.join(ROOT, "embase_jack_gauche_OUT.png")); ji = Image.open(os.path.join(ROOT, "embase_jack_droite_IN.png"))
js = 0.62
jo = jo.resize((round(jo.width * js), round(jo.height * js)), Image.LANCZOS); ji = ji.resize((round(ji.width * js), round(ji.height * js)), Image.LANCZOS)
lx, _ = C(166, 0); rx, _ = C(1212, 0); _, jy = C(0, 560)
canvas.alpha_composite(jo, (round(lx - jo.width + 3), round(jy - jo.height / 2)))
canvas.alpha_composite(ji, (round(rx - 3), round(jy - ji.height / 2)))

# ---------- 3) inscriptions (même style que le design : sans fin, blanc cassé) ----------
FR = os.path.join(ROOT, "fonts", "Poppins-Regular.ttf"); FL = os.path.join(ROOT, "fonts", "Poppins-Light.ttf"); FM = os.path.join(ROOT, "fonts", "Poppins-Medium.ttf")
INK = (238, 230, 228, 255)
d = ImageDraw.Draw(canvas)
def txt(x, y, s, size, font=FR, anchor="lm", spacing=0.0):
    f = ImageFont.truetype(font, round(size * S))
    px, py = C(x, y)
    if spacing:
        # lettres espacées (comme le titre du design)
        total = sum(f.getlength(ch) for ch in s) + spacing * S * (len(s) - 1)
        cx = px - (total / 2 if anchor[0] == "m" else 0)
        for ch in s:
            d.text((cx, py), ch, font=f, fill=INK, anchor="l" + anchor[1]); cx += f.getlength(ch) + spacing * S
        return
    d.text((px, py), s, font=f, fill=INK, anchor=anchor)

txt(1113, 197, "CLASSIC · CHORDS", 13, FR, "mm")
txt(813, 689, "GLIDE", 18, FR, "mm"); txt(946, 689, "TONE", 18, FR, "mm"); txt(1080, 689, "MIX", 18, FR, "mm")
txt(750, 974, "WHARRY", 36, FL, "lm", spacing=13)
logo = Image.open(os.path.join(ROOT, "logo_studio_wiga_blanc.png")).convert("RGBA")
lw = round(172 * S); logo = logo.resize((lw, round(lw * logo.height / logo.width)), Image.LANCZOS)
la = np.array(logo).astype(np.float32); la[..., :3] = INK[:3]; logo = Image.fromarray(la.astype(np.uint8))
px, py = C(730, 101); canvas.alpha_composite(logo, (round(px), round(py - logo.height / 2)))
txt(185, 560, "OUT", 15, FM, "mm")
txt(1180, 560, "IN", 15, FM, "mm")
canvas.save(os.path.join(ROOT, "background.png"))

# ---------- 4) calque du plateau ----------
PL = [(206, 64), (219, 51), (694, 51), (708, 64), (708, 1000), (700, 1012), (210, 1012), (203, 1000)]
pm = Image.new("L", (W, H), 0); ImageDraw.Draw(pm).polygon([C(*p) for p in PL], fill=255)
pm = pm.filter(ImageFilter.GaussianBlur(0.7))
down = canvas.copy(); down.putalpha(pm)
down.save(os.path.join(ROOT, "treadle_down.png"))

# aperçu orteil levé (même calcul que le plugin)
top_y = C(0, 51)[1]; hinge_y = C(0, 1012)[1]; xl = C(203, 0)[0]; xr = C(708, 0)[0]; cxm = (xl + xr) / 2
rise, widen = 14.0, 1.06
M = cv2.getPerspectiveTransform(np.float32([[xl, top_y], [xr, top_y], [xr, hinge_y], [xl, hinge_y]]),
                                np.float32([[cxm - (cxm - xl) * widen, top_y - rise], [cxm + (xr - cxm) * widen, top_y - rise], [xr, hinge_y], [xl, hinge_y]]))
up = cv2.warpPerspective(np.array(down), M, (W, H), flags=cv2.INTER_CUBIC)
prev = canvas.copy(); prev.alpha_composite(Image.fromarray(up, "RGBA"))
prev.convert("RGB").save(os.path.join(ROOT, "design", "apercu_treadle_up.png"))

# ---------- 5) sprites ----------
def sprite(box, name, flip=False):
    x0, y0, x1, y1 = box
    im = Image.fromarray(A).crop(box).resize((round((x1 - x0) * S), round((y1 - y0) * S)), Image.LANCZOS)
    if flip: im = im.transpose(Image.FLIP_LEFT_RIGHT)
    im.save(os.path.join(ROOT, name))
FS = (868, 784, 1020, 936)
sprite(FS, "fs_on.png"); sprite(FS, "fs_off.png")          # footswitch métal (l'état est donné par la LED)
TG = (1082, 140, 1144, 184)
sprite(TG, "toggle_r.png"); sprite(TG, "toggle_l.png", flip=True)

with open(os.path.join(ROOT, "tools", "layout.txt"), "w") as fo:
    def P(n, x, y): px, py = C(x, y); fo.write(f"{n} {px:.1f} {py:.1f}\n")
    for i, (x, y) in enumerate(LEDS): P(f"led{i}", x, y)
    P("knob", 822, 250); fo.write(f"knobR {70 * S:.1f} capR {52 * S:.1f}\n")
    for n, (x, y) in (("glide", (813, 615)), ("tone", (946, 615)), ("mix", (1080, 612))): P(n, x, y)
    fo.write(f"smallR {48 * S:.1f} smallCapR {37 * S:.1f}\n")
    P("screen0", SCR[0], SCR[1]); P("screen1", SCR[2], SCR[3])
    P("fs0", FS[0], FS[1]); P("fs1", FS[2], FS[3]); P("fsled", 1063, 873)
    P("tg0", TG[0], TG[1]); P("tg1", TG[2], TG[3])
    P("plateTL", 203, 51); P("plateBR", 708, 1012)
    fo.write(f"ledR {7.5 * S:.1f} S {S}\n")
print("ok")
