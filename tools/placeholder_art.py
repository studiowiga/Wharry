# Génère le visuel PROVISOIRE de Wharry (remplacé par le design de l'utilisateur) :
#   background.png  : boîtier sans plateau, sans potard / LED / footswitch (1240 x 928)
#   treadle_up.png  : plateau seul, orteil levé  (calque RGBA plein cadre)
#   treadle_down.png: plateau seul, orteil enfoncé (calque RGBA plein cadre)
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont
W, H = 1240, 928
F = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
def font(s): return ImageFont.truetype(F, s)
out = sys.argv[1] if len(sys.argv) > 1 else "."

bg = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(bg)
# ombre portée + boîtier
sh = Image.new("RGBA", (W, H), (0, 0, 0, 0)); ImageDraw.Draw(sh).rounded_rectangle((118, 34, 1128, 912), 44, fill=(0, 0, 0, 150))
bg.alpha_composite(sh.filter(ImageFilter.GaussianBlur(10)))
d.rounded_rectangle((110, 20, 1130, 904), 42, fill=(122, 18, 24, 255), outline=(60, 8, 10, 255), width=4)
d.rounded_rectangle((126, 36, 1114, 888), 34, outline=(170, 50, 52, 255), width=2)
# panneau supérieur
d.rounded_rectangle((140, 44, 1100, 300), 22, fill=(28, 26, 28, 255), outline=(10, 10, 10, 255), width=3)
# cuvette du plateau
d.rounded_rectangle((292, 318, 948, 872), 26, fill=(40, 10, 12, 255), outline=(20, 4, 6, 255), width=3)
# jacks (kit Studio Wiga) : OUT à gauche, IN à droite
jo = Image.open("embase_jack_gauche_OUT.png"); ji = Image.open("embase_jack_droite_IN.png")
jo = jo.resize((int(jo.width * 0.62), int(jo.height * 0.62))); ji = ji.resize((int(ji.width * 0.62), int(ji.height * 0.62)))
bg.alpha_composite(jo, (110 - jo.width + 4, 520 - jo.height // 2)); bg.alpha_composite(ji, (1130 - 4, 520 - ji.height // 2))
d.text((150, 506), "OUT", font=font(20), fill=(240, 220, 220, 255)); d.text((1046, 506), "IN", font=font(20), fill=(240, 220, 220, 255))
# sérigraphie : liste des programmes (LED dessinées par le plugin)
import json
names = ["2 OCT UP","OCT UP","5TH UP","4TH UP","2ND DN","4TH DN","5TH DN","OCT DN","2 OCT DN","DIVE BOMB","SHALLOW","DEEP",
         "OCT DN/OCT UP","5TH DN/4TH DN","4TH DN/3RD DN","5TH/7TH","5TH/6TH","4TH/5TH","3RD/4TH","b3RD/3RD","2ND/3RD"]
for i, nm in enumerate(names):
    col = 0 if i < 12 else 1
    row = i if i < 12 else i - 12
    x = 410 if col == 0 else 600
    y = 60 + row * 19
    d.text((x + 16, y - 8), nm, font=font(13), fill=(235, 235, 235, 255))
d.text((396, 286 - 18), "", font=font(12))
d.text((410 - 8, 48 - 2), "", font=font(11))
d.text((600 + 16, 60 + 9 * 19 + 4), "", font=font(11))
d.text((405, 278), "WHAMMY  ·  DETUNE", font=font(11), fill=(200, 60, 60, 255))
d.text((610, 238), "HARMONY", font=font(11), fill=(200, 60, 60, 255))
# switch CLASSIC / CHORDS
d.text((838, 64), "CLASSIC", font=font(14), fill=(235, 235, 235, 255)); d.text((986, 64), "CHORDS", font=font(14), fill=(235, 235, 235, 255))
d.rounded_rectangle((920, 60, 980, 86), 13, fill=(12, 12, 12, 255), outline=(90, 90, 90, 255), width=2)
# titre + logo
d.text((840, 146), "WHARRY", font=font(54), fill=(245, 245, 245, 255))
logo = Image.open("logo_studio_wiga_blanc.png"); logo = logo.resize((230, int(230 * logo.height / logo.width)))
bg.alpha_composite(logo, (856, 232))
# footswitchs : étiquettes
d.text((140, 832), "CLASSIC / CHORDS", font=font(15), fill=(240, 220, 220, 255))
d.text((1010, 832), "ON / OFF", font=font(15), fill=(240, 220, 220, 255))
d.text((300, 896 - 20), "", font=font(12))
wm = Image.new("RGBA", (W, H), (0, 0, 0, 0)); ImageDraw.Draw(wm).text((478, 4), "VISUEL PROVISOIRE - design à venir", font=font(13), fill=(255, 255, 255, 140))
bg.alpha_composite(wm)
bg.save(f"{out}/background.png")

def treadle(up):
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dd = ImageDraw.Draw(im)
    hingeY, hx0, hx1 = 856, 320, 920
    if up:   top, tx0, tx1, lip = 300, 300, 940, 34   # orteil levé : avant relevé, plus proche -> plus large, tranche visible
    else:    top, tx0, tx1, lip = 352, 318, 922, 8
    # ombre portée sous le plateau
    s = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(s).polygon([(tx0 + 10, top + lip + 30 if up else top + 14), (tx1 - 10, top + lip + 30 if up else top + 14), (hx1, hingeY), (hx0, hingeY)], fill=(0, 0, 0, 170 if up else 90))
    im.alpha_composite(s.filter(ImageFilter.GaussianBlur(14 if up else 5)))
    # tranche avant (visible quand la pointe est levée)
    dd.polygon([(tx0, top), (tx1, top), (tx1 - 4, top + lip), (tx0 + 4, top + lip)], fill=(70, 70, 74, 255))
    # dessus du plateau (métal) + caoutchouc
    dd.polygon([(tx0, top), (tx1, top), (hx1, hingeY), (hx0, hingeY)], fill=(150, 150, 156, 255), outline=(60, 60, 64, 255))
    ins = 26
    def lerp(a, b, t): return a + (b - a) * t
    rub_top, rub_bot = top + ins, hingeY - ins
    def xat(y, left):
        t = (y - top) / (hingeY - top)
        return lerp(tx0, hx0, t) + ins if left else lerp(tx1, hx1, t) - ins
    dd.polygon([(xat(rub_top, True), rub_top), (xat(rub_top, False), rub_top), (xat(rub_bot, False), rub_bot), (xat(rub_bot, True), rub_bot)], fill=(30, 30, 32, 255))
    n = 15
    for i in range(n):
        y0 = lerp(rub_top + 12, rub_bot - 12, i / (n - 1)); hh = 10 if up else 8
        dd.rounded_rectangle((xat(y0, True) + 14, y0 - hh / 2, xat(y0, False) - 14, y0 + hh / 2), 4, fill=(58, 58, 62, 255))
    # reflet : plus clair quand le plateau est incliné vers la lumière (orteil levé)
    gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(gl).polygon([(tx0, top), (tx1, top), (lerp(tx1, hx1, .35), lerp(top, hingeY, .35)), (lerp(tx0, hx0, .35), lerp(top, hingeY, .35))], fill=(255, 255, 255, 46 if up else 14))
    im.alpha_composite(gl)
    return im
treadle(True).save(f"{out}/treadle_up.png"); treadle(False).save(f"{out}/treadle_down.png")
print("ok")
