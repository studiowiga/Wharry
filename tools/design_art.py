# Wharry : prépare les images du plugin à partir du design de l'utilisateur (design/wharry_design_source.png, 1390 x 1132)
#   background.png   : fond 1020 x 928 (textes réécrits, LED / écran vidés, logo Studio Wiga, jacks Studio Wiga)
#   treadle_down.png : calque du plateau à plat (orteil enfoncé) ; le plugin calcule les autres positions en perspective
#   fs_on.png / fs_off.png, toggle_lever.png : sprites des commandes (le levier de l'inverseur coulisse)
#   tools/layout.txt : coordonnées des commandes dans le repère 1020 x 928 (reprises dans PluginEditor.cpp)
#   design/apercu_treadle_up.png : aperçu de la position orteil levé
import os, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = Image.open(os.path.join(ROOT, "design", "wharry_design_source.png")).convert("RGB")
A = np.array(src)

# fond recadré au plus près de la pédale : la pédale occupe toute la hauteur de la fenêtre
W, H = 1020, 928
CX0, CY0, CX1, CY1 = 160, 45, 1215, 1058          # contour de la pédale dans le design
OY = 30.0                                          # marge haute : place pour la pointe du plateau levée
S = (H - OY - 8.0) / (CY1 - CY0)
OX = (W - (CX1 - CX0) * S) / 2.0
def C(x, y): return (OX + (x - CX0) * S, OY + (y - CY0) * S)

# ---------- 0) éléments déplacés ----------
KNOB_OLD, KNOB_NEW, KNOB_RS = (821, 258), (808, 378), 76          # sélecteur : centré verticalement sur l'écran, à sa gauche
SCR_OUT = (905, 228, 1177, 528)                                   # écran agrandi (cadre extérieur)
kyy, kxx = np.mgrid[0:2 * KNOB_RS, 0:2 * KNOB_RS]
ka = np.clip((KNOB_RS - 5.0 - np.hypot(kxx - KNOB_RS + 0.5, kyy - KNOB_RS + 0.5)) / 2.0, 0, 1)
knob_rgb = A[KNOB_OLD[1] - KNOB_RS:KNOB_OLD[1] + KNOB_RS, KNOB_OLD[0] - KNOB_RS:KNOB_OLD[0] + KNOB_RS]
knob_sprite = np.dstack([knob_rgb, (ka * 255).astype(np.uint8)])

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
reg2 = (1000, 810, 1130, 940)                      # halo de la LED du footswitch
R1 = A[reg2[1]:reg2[3], reg2[0]:reg2[2]].astype(np.int32)
g2 = (((R1[..., 1] - R1[..., 2]) > 4) & (R1[..., 0] > 120)).astype(np.uint8) * 255
mask[reg2[1]:reg2[3], reg2[0]:reg2[2]] |= cv2.dilate(g2, np.ones((13, 13), np.uint8))
inpainted = cv2.inpaint(A, mask, 6, cv2.INPAINT_TELEA)

# zone du sélecteur (anciennes LED, halos, étiquettes) : panneau brossé reconstruit =
# dégradé de couleur du panneau (plan ajusté sur les pixels propres) + grain brossé pris dans une zone propre
ZX0, ZY0, ZX1, ZY1 = 717, 140, 1016, 515
yy3, xx3 = np.mgrid[ZY0:ZY1, ZX0:ZX1]
rk = np.hypot(xx3 - 822, yy3 - 256)
knob = np.zeros_like(rk, bool)                           # l'ancien emplacement du sélecteur est entièrement refait
oldk = rk < 96                                           # sélecteur d'origine + son ombre + reflets des LED
vline = (np.abs(xx3 - 812) < 9) & (yy3 > 330) & (yy3 < 492)   # trait vertical sous le sélecteur d'origine
zone_mask = (mask[ZY0:ZY1, ZX0:ZX1] > 0) | oldk | vline
zone_mask = cv2.dilate(zone_mask.astype(np.uint8), np.ones((5, 5), np.uint8)).astype(bool) & ~knob
Z = A[ZY0:ZY1, ZX0:ZX1].astype(np.float32)
clean_px = ~zone_mask & ~(rk < 104) & (np.abs(xx3 - 813) > 6)
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

# sélecteur recollé à sa nouvelle place, avec son ombre portée (lumière en haut à gauche)
base = inpainted.astype(np.float32)
R2 = KNOB_RS + 20
cx, cy = KNOB_NEW
shm = np.zeros(A.shape[:2], np.float32)
cv2.circle(shm, (cx + 5, cy + 9), KNOB_RS - 4, 1.0, -1)
shm = cv2.GaussianBlur(shm, (0, 0), 8) * 0.55
base *= (1 - shm[..., None])
spr = knob_sprite.astype(np.float32)
sl = (slice(cy - KNOB_RS, cy + KNOB_RS), slice(cx - KNOB_RS, cx + KNOB_RS))
al = spr[..., 3:4] / 255.0
base[sl] = spr[..., :3] * al + base[sl] * (1 - al)
inpainted = np.clip(base, 0, 255).astype(np.uint8)

# écran agrandi : cadre noir en creux + vitre (le plugin dessine l'affichage dans la vitre)
scr_img = Image.fromarray(inpainted)
sd = ImageDraw.Draw(scr_img, "RGBA")
x0, y0, x1, y1 = SCR_OUT
shadow = Image.new("L", scr_img.size, 0); ImageDraw.Draw(shadow).rounded_rectangle((x0 - 2, y0 - 1, x1 + 3, y1 + 5), 16, fill=170)
shadow = shadow.filter(ImageFilter.GaussianBlur(5))
scr_img = Image.composite(Image.new("RGB", scr_img.size, (25, 4, 6)), scr_img, shadow)
sd = ImageDraw.Draw(scr_img, "RGBA")
sd.rounded_rectangle((x0, y0, x1, y1), 15, fill=(14, 12, 13), outline=(70, 22, 26), width=2)
sd.rounded_rectangle((x0 + 3, y0 + 3, x1 - 3, y1 - 3), 13, outline=(36, 32, 34), width=1)
G = 9
sd.rounded_rectangle((x0 + G, y0 + G, x1 - G, y1 - G), 9, fill=(8, 15, 22))
sd.line((x0 + G + 8, y0 + G + 1, x1 - G - 8, y0 + G + 1), fill=(120, 130, 140, 120), width=1)   # reflet sur la vitre
inpainted = np.array(scr_img)
SCR = (x0 + G, y0 + G, x1 - G, y1 - G)
# ---------- 1b) coins de la pédale : léger arrondi au lieu des pans coupés ----------
# Le pan coupé et sa pièce de chant sont supprimés ; le coin est refait comme un vrai coin usiné arrondi :
#  - cadre (noir, filet chromé, chant) = profil réel du bord droit voisin, enroulé autour de l'arc ;
#  - panneau = même ligne de pixels prise plus loin sur la même rangée (le brossage horizontal reste continu).
def profile(img, ox, oy, sx, sy, horizontal, depth, at, n=10):
    """profil depuis le bord extérieur vers l'intérieur, pris sur la partie droite du bord, à 'at' px du coin"""
    acc = []
    for k in range(n):
        o = at + k
        row = []
        for dd in range(depth):
            x = ox + sx * (o if horizontal else dd); y = oy + sy * (dd if horizontal else o)
            row.append(img[int(y), int(x)].astype(np.float32))
        acc.append(row)
    return np.median(np.array(acc), 0)

def samp(prof, d):
    d = min(max(d, 0.0), len(prof) - 1.001); i = int(d); f = d - i
    return prof[i] * (1 - f) + prof[i + 1] * f

def round_corner(img, ox, oy, sx, sy, L, R, F, xmax=None, keep=None, row_copy=True, inpaint_panel=False, frame=None):
    """ox, oy : coin extérieur virtuel ; sx, sy : sens vers l'intérieur ; L : jambe du pan coupé ;
    R : rayon de l'arrondi ; F : épaisseur du cadre (jusqu'au panneau)"""
    src = img.astype(np.float32); out = src.copy()
    depth = int(F + 8)
    ph = profile(img, ox, oy, sx, sy, True, depth, int(L + F + 10))      # bord horizontal (haut / bas)
    pv = profile(img, ox, oy, sx, sy, False, depth, int(L + F + 10))     # bord vertical (gauche / droite)
    K = int(L + F * 1.6 + 24)
    lim = L + F * 1.42 + 14                                      # au-delà : image d'origine intacte
    pmask = np.zeros(img.shape[:2], np.uint8)
    for v in range(K):
        for u in range(K):
            x = int(ox + sx * u + (0 if sx > 0 else -1)); y = int(oy + sy * v + (0 if sy > 0 else -1))
            if xmax is not None and x >= xmax: continue
            if keep is not None and keep(x, y): continue
            uc, vc = u + 0.5, v + 0.5
            if uc + vc > lim + 6: continue
            if uc < R and vc < R:
                d = R - math.hypot(R - uc, R - vc); w = math.atan2(R - vc, R - uc) / (math.pi / 2)
            else:
                d = min(uc, vc); w = 1.0 / (1.0 + math.exp(-(uc - vc) / 5.0))   # raccord d'onglet adouci
            if d < -0.6: continue                                  # dehors : fond d'origine
            FR = F if frame is None else frame                     # épaisseur réelle du cadre (le reste est du panneau)
            if d < FR + 2:
                val = samp(pv, d) * (1 - w) + samp(ph, d) * w
            elif inpaint_panel:
                pmask[y, x] = 255; val = src[y, x]
            else:
                if row_copy: xs, ys = x + sx * int(lim + 4), y
                else:        xs, ys = x, y + sy * int(lim + 4)
                val = src[int(ys), int(xs)]
                if d < FR + 4: val = val * ((d - FR - 2) / 2) + (samp(pv, d) * (1 - w) + samp(ph, d) * w) * (1 - (d - FR - 2) / 2)
            blend = min(1.0, max(0.0, (lim + 6 - (uc + vc)) / 6.0))   # raccord progressif vers l'original
            a = min(1.0, d + 0.6) * blend
            out[y, x] = val * a + src[y, x] * (1 - a)
    res = np.clip(out, 0, 255).astype(np.uint8)
    if inpaint_panel and pmask.any(): res = cv2.inpaint(res, pmask, 9, cv2.INPAINT_TELEA)
    return res

RC = 20                                                      # rayon des coins (pixels du design)
img = inpainted
img = round_corner(img, 1211, 46, -1, +1, 62, RC, 14)                               # haut droit (panneau)
img = round_corner(img, 1211, 1057, -1, -1, 72, RC, 30, row_copy=False, frame=16)                             # bas droit (pièce d'angle supprimée)
screw = lambda x, y: math.hypot(x - 212, y - 1002) < 15
img = round_corner(img, 162, 1057, +1, -1, 56, RC, 30, keep=screw, inpaint_panel=True)                  # bas gauche (boîtier)
img = round_corner(img, 164, 70, +1, +1, 50, RC, 16, xmax=204, row_copy=False)      # haut gauche (boîtier, sous le plateau)
img = round_corner(img, 204, 51, +1, +1, 16, 12, 22)                                # coins hauts du plateau
img = round_corner(img, 708, 51, -1, +1, 16, 12, 22)
inpainted = img

# ---------- 1c) retouches : halo de la LED, fin du trait, fente de l'inverseur vide, vis ajoutées ----------
TEX = A[365:530, 828:878].astype(np.float32); TEX = TEX - cv2.GaussianBlur(TEX, (0, 0), 6)
def panel_rebuild(img, region_mask, ring):
    """panneau brossé reconstruit = dégradé de couleur ajusté sur l'anneau propre 'ring' + grain brossé"""
    ys, xs = np.nonzero(ring)
    X = np.stack([xs, ys, np.ones_like(xs)], 1).astype(np.float64)
    coef = np.linalg.lstsq(X, img[ys, xs].astype(np.float64), rcond=None)[0]
    yy, xx = np.mgrid[0:img.shape[0], 0:img.shape[1]]
    out = img.astype(np.float32)
    sel = region_mask > 0.001
    py, px = yy[sel], xx[sel]
    plane = np.stack([px, py, np.ones_like(px)], 1).astype(np.float64) @ coef
    tex = TEX[py % TEX.shape[0], px % TEX.shape[1]]
    synth = np.clip(plane + tex, 0, 255)
    a = region_mask[sel][:, None]
    out[sel] = synth * a + out[sel] * (1 - a)
    return np.clip(out, 0, 255).astype(np.uint8)

yy0, xx0 = np.mgrid[0:A.shape[0], 0:A.shape[1]]
# halo de la LED du footswitch (visible quand elle est éteinte) : disque de panneau neuf, bord fondu
def panel_copy(img, region_mask, dx, ring):
    """panneau repris sur la même rangée, décalé de dx (brossage continu), avec correction du dégradé de couleur"""
    ys, xs = np.nonzero(ring)
    X = np.stack([xs, ys, np.ones_like(xs)], 1).astype(np.float64)
    coef = np.linalg.lstsq(X, img[ys, xs].astype(np.float64), rcond=None)[0]
    out = img.astype(np.float32)
    sel = region_mask > 0.001
    py, px = np.nonzero(sel)
    plane = lambda x, y: np.stack([x, y, np.ones_like(x)], 1).astype(np.float64) @ coef
    val = img[py, px + dx].astype(np.float64) - plane(px + dx, py) + plane(px, py)
    a = region_mask[py, px][:, None]
    out[py, px] = np.clip(val, 0, 255) * a + out[py, px] * (1 - a)
    return np.clip(out, 0, 255).astype(np.uint8)
rL = np.hypot(xx0 - 1063, yy0 - 873)
red_ = (inpainted[..., 0].astype(int) - inpainted[..., 1].astype(int)) > 25          # pixels de panneau rouge uniquement
inpainted = panel_copy(inpainted, np.clip((50 - rL) / 12.0, 0, 1).astype(np.float32), 72, (rL > 52) & (rL < 70) & red_)
# fin du trait décoratif du bas (il venait buter dans le coin) : effacé sous la future vis
lm = np.zeros(A.shape[:2], np.float32); lm[1018:1036, 1050:1188] = 1.0; lm = cv2.GaussianBlur(lm, (0, 0), 2)
ring = np.zeros(A.shape[:2], bool); ring[1008:1016, 1050:1188] = True; ring[1040:1046, 1050:1188] = True
inpainted = panel_rebuild(inpainted, lm, ring)

# inverseur CLASSIC / CHORDS : la fente est vidée, le levier devient un sprite qui coulisse (éclairage conservé)
LEV = (1094, 145, 1131, 177)                       # levier (chapeau strié + flanc + base) dans le design
SLOT_C, TRAVEL = 1114.0, 9.0                       # centre de la fente, course de part et d'autre
lev = A[LEV[1]:LEV[3], LEV[0]:LEV[2]]
lmask = np.zeros(A.shape[:2], np.uint8); lmask[LEV[1]:LEV[3], LEV[0]:LEV[2]] = 255
inpainted = cv2.inpaint(inpainted, lmask, 5, cv2.INPAINT_TELEA)
# intérieur de la fente : nuance verticale reprise des bords libres (gauche / droite) pour une fente régulière
for y in range(LEV[1], LEV[3]):
    a_, b_ = inpainted[y, 1086:1092].astype(np.float32).mean(0), inpainted[y, 1136:1142].astype(np.float32).mean(0)
    for x in range(LEV[0], LEV[2]):
        t = (x - LEV[0]) / (LEV[2] - LEV[0]); inpainted[y, x] = (a_ * (1 - t) + b_ * t).astype(np.uint8)
la_ = np.ones(lev.shape[:2], np.float32); la_[:, 0] = 0.5; la_[:, -1] = 0.5; la_[0, :] *= 0.6; la_[-1, :] *= 0.4
Image.fromarray(np.dstack([lev, (la_ * 255).astype(np.uint8)]), "RGBA").resize(
    (round((LEV[2] - LEV[0]) * S), round((LEV[3] - LEV[1]) * S)), Image.LANCZOS).save(os.path.join(ROOT, "toggle_lever.png"))

# footswitch : ombre de contact sur le panneau (lumière en haut à gauche) + chapeau mobile en sprite
FSC, FS_R, CAP_R = (945, 859), 76, 45
rF = np.hypot(xx0 - FSC[0] - 5, yy0 - FSC[1] - 8)
shF = cv2.GaussianBlur(((rF < FS_R)).astype(np.float32), (0, 0), 7) * 0.55
shF[np.hypot(xx0 - FSC[0], yy0 - FSC[1]) < FS_R - 1] = 0
inpainted = np.clip(inpainted.astype(np.float32) * (1 - shF[..., None]), 0, 255).astype(np.uint8)
Rc = CAP_R + 2
cap = A[FSC[1] - Rc:FSC[1] + Rc, FSC[0] - Rc:FSC[0] + Rc]
yyc, xxc = np.mgrid[-Rc:Rc, -Rc:Rc]
ca = np.clip((CAP_R - np.hypot(xxc + 0.5, yyc + 0.5)) / 1.5 + 0.5, 0, 1)
Image.fromarray(np.dstack([cap, (ca * 255).astype(np.uint8)]), "RGBA").resize((round(2 * Rc * S), round(2 * Rc * S)), Image.LANCZOS).save(os.path.join(ROOT, "fs_cap.png"))

# vis : copie de la vis du haut à droite (même taille, même éclairage), en bas à droite du panneau
#       et sous le plateau (bas du plateau, aux mêmes abscisses que ses deux vis du haut)
def paste_screw(img, src_c, dst_c, r, dim=None):
    sx_, sy_ = src_c; dx_, dy_ = dst_c; R_ = r + 7
    if dim is None:                                  # même rapport de luminosité que le panneau autour
        rs = np.hypot(xx0 - sx_, yy0 - sy_); rd = np.hypot(xx0 - dx_, yy0 - dy_)
        ringS = (rs > R_ + 2) & (rs < R_ + 10) & ((A[..., 0].astype(int) - A[..., 1].astype(int)) > 25)
        ringD = (rd > R_ + 2) & (rd < R_ + 10) & ((img[..., 0].astype(int) - img[..., 1].astype(int)) > 25)
        dim = float(np.clip(img[ringD][:, 0].mean() / max(1.0, A[ringS][:, 0].mean()), 0.55, 1.0))
    spr = A[sy_ - R_:sy_ + R_, sx_ - R_:sx_ + R_].astype(np.float32)
    redpx = ((spr[..., 0] - spr[..., 1]) > 25)[..., None]
    spr = spr * np.where(redpx, dim, max(dim, 0.82))      # la cuvette rouge suit le panneau, la tête métal reste brillante
    yy_, xx_ = np.mgrid[-R_:R_, -R_:R_]
    al = np.clip((R_ - 1.5 - np.hypot(xx_ + 0.5, yy_ + 0.5)) / 3.0, 0, 1)[..., None]
    out = img.astype(np.float32)
    sl_ = (slice(dy_ - R_, dy_ + R_), slice(dx_ - R_, dx_ + R_))
    out[sl_] = spr * al + out[sl_] * (1 - al)
    return np.clip(out, 0, 255).astype(np.uint8)
inpainted = paste_screw(inpainted, (1130, 92), (1130, 1004), 20)   # rapport de luminosité automatique
inpainted = paste_screw(inpainted, (258, 100), (258, 979), 21)
inpainted = paste_screw(inpainted, (658, 100), (658, 979), 21)

clean = Image.fromarray(inpainted)

# ---------- 2) mise à l'échelle sur le fond ----------
# le fond texturé du design est prolongé par miroir pour remplir toute la fenêtre (pas de cadre visible)
PAD = 200
ext = cv2.copyMakeBorder(np.array(clean), PAD, PAD, PAD, PAD, cv2.BORDER_REFLECT)
sx0 = CX0 - OX / S; sy0 = CY0 - OY / S
box = (round(sx0 + PAD), round(sy0 + PAD), round(sx0 + PAD + W / S), round(sy0 + PAD + H / S))
canvas = Image.fromarray(ext).crop(box).resize((W, H), Image.LANCZOS).convert("RGBA")

jo = Image.open(os.path.join(ROOT, "embase_jack_gauche_OUT.png")); ji = Image.open(os.path.join(ROOT, "embase_jack_droite_IN.png"))
js = 0.40
jo = jo.resize((round(jo.width * js), round(jo.height * js)), Image.LANCZOS); ji = ji.resize((round(ji.width * js), round(ji.height * js)), Image.LANCZOS)
lx, _ = C(166, 0); rx, _ = C(1209, 0); _, jy = C(0, 560)
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
# IN / OUT : gravés sur la tranche de la façade, juste à côté de chaque jack (lecture de bas en haut)
def side_label(x, y, s_):
    f = ImageFont.truetype(FM, round(24 * S))
    tw = int(f.getlength(s_)) + 8; th = round(34 * S)
    im = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
    ImageDraw.Draw(im).text((tw / 2, th / 2), s_, font=f, fill=INK, anchor="mm")
    im = im.rotate(90, expand=True, resample=Image.BICUBIC)
    px, py = C(x, y)
    canvas.alpha_composite(im, (round(px - im.width / 2), round(py - im.height / 2)))
side_label(191, 560, "OUT")
side_label(1171, 560, "IN")
canvas.save(os.path.join(ROOT, "background.png"))

# ---------- 4) calque du plateau ----------
def arc(cx, cy, r, a0, a1, n=8): return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
PL = arc(216, 63, 12, 180, 270) + arc(696, 63, 12, 270, 360) + [(708, 1000), (700, 1012), (210, 1012), (203, 1000)]
pm = Image.new("L", (W, H), 0); ImageDraw.Draw(pm).polygon([C(*p) for p in PL], fill=255)
pm = pm.filter(ImageFilter.GaussianBlur(0.7))
down = canvas.copy(); down.putalpha(pm)
down.save(os.path.join(ROOT, "treadle_down.png"))

# aperçu orteil levé (même calcul que le plugin)
top_y = C(0, 51)[1]; hinge_y = C(0, 1012)[1]; xl = C(203, 0)[0]; xr = C(708, 0)[0]; cxm = (xl + xr) / 2
rise, widen = 26.0, 1.015
M = cv2.getPerspectiveTransform(np.float32([[xl, top_y], [xr, top_y], [xr, hinge_y], [xl, hinge_y]]),
                                np.float32([[cxm - (cxm - xl) * widen, top_y - rise], [cxm + (xr - cxm) * widen, top_y - rise], [xr, hinge_y], [xl, hinge_y]]))
up = cv2.warpPerspective(np.array(down), M, (W, H), flags=cv2.INTER_CUBIC)
prev = canvas.copy(); prev.alpha_composite(Image.fromarray(up, "RGBA"))
prev.convert("RGB").save(os.path.join(ROOT, "design", "apercu_treadle_up.png"))

# masque de la silhouette (pédale détourée pour la couverture de la notice)
mk = Image.new("L", (W, H), 0); md = ImageDraw.Draw(mk)
def CR(x0, y0, x1, y1): a = C(x0, y0); b = C(x1, y1); return (a[0], a[1], b[0], b[1])
md.rounded_rectangle(CR(162, 70, 1211, 1057), RC * S, fill=255)
md.rounded_rectangle(CR(700, 46, 1211, 400), RC * S, fill=255)
md.polygon([C(*p) for p in PL], fill=255)
for (im_, x_) in ((jo, lx - jo.width + 3), (ji, rx - 3)):
    md.rectangle((x_, jy - im_.height / 2, x_ + im_.width, jy + im_.height / 2), fill=255)
mk.filter(ImageFilter.GaussianBlur(0.8)).save(os.path.join(ROOT, "design", "pedal_mask.png"))

# ---------- 5) sprites ----------
def sprite(box, name, flip=False):
    x0, y0, x1, y1 = box
    im = Image.fromarray(A).crop(box).resize((round((x1 - x0) * S), round((y1 - y0) * S)), Image.LANCZOS)
    if flip: im = im.transpose(Image.FLIP_LEFT_RIGHT)
    im.save(os.path.join(ROOT, name))
FS = (868, 784, 1020, 936)

TG = (1082, 140, 1144, 184)


with open(os.path.join(ROOT, "tools", "layout.txt"), "w") as fo:
    def P(n, x, y): px, py = C(x, y); fo.write(f"{n} {px:.1f} {py:.1f}\n")
    for i, (x, y) in enumerate(LEDS): P(f"led{i}", x, y)
    P("knob", KNOB_NEW[0] + 1, KNOB_NEW[1] - 8); fo.write(f"knobR {70 * S:.1f} capR {52 * S:.1f}\n")
    for n, (x, y) in (("glide", (813, 615)), ("tone", (946, 615)), ("mix", (1080, 612))): P(n, x, y)
    fo.write(f"smallR {48 * S:.1f} smallCapR {37 * S:.1f}\n")
    P("screen0", SCR[0], SCR[1]); P("screen1", SCR[2], SCR[3])
    P("fs0", FS[0], FS[1]); P("fs1", FS[2], FS[3]); P("fsled", 1063, 873)
    P("tg0", TG[0], TG[1]); P("tg1", TG[2], TG[3])
    P("fsc", FSC[0], FSC[1]); fo.write(f"capR {CAP_R * S:.2f} capSprR {(CAP_R + 2) * S:.2f}\n")
    P("lev0", LEV[0], LEV[1]); P("lev1", LEV[2], LEV[3]); P("slotC", SLOT_C, 0); fo.write(f"travel {TRAVEL * S:.2f}\n")
    P("plateTL", 203, 51); P("plateBR", 708, 1012)
    fo.write(f"ledR {7.5 * S:.1f} S {S}\n")
print("ok")
