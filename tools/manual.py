# Wharry (Studio Wiga) - notice PDF bilingue (anglais puis français), illustrée avec le rendu réel du plugin.
# Usage : python3 tools/manual.py <dossier des captures (tools/render)> <fichier PDF de sortie>
import os, sys
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, NextPageTemplate, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = sys.argv[1]
OUT = sys.argv[2]
VERSION = "1.4.0"
TMP = os.path.dirname(OUT)

for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(ROOT, "fonts", f + ".ttf")))

RED = colors.HexColor("#B3141E"); DARK = colors.HexColor("#1D1B1C"); GREY = colors.HexColor("#6B6466"); LIGHT = colors.HexColor("#F4EEEE")

# ---------- capture annotée ----------
src = Image.open(os.path.join(SHOTS, "wharry_100pc_milieu.png")).convert("RGB")
k = src.height / 928.0
marks = [  # (numéro, x, y) dans le repère 1020 x 928 du plugin
    (1, 306, 455), (2, 616, 315), (3, 818, 250), (4, 884, 120), (5, 620, 531), (6, 737, 531), (7, 855, 528),
    (8, 735, 746), (9, 840, 757), (10, 1000, 470), (11, 24, 470)]
ann = src.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(ROOT, "fonts", "Poppins-Bold.ttf"), 22)
for n, x, y in marks:
    cx, cy, r = x * k, y * k, 17
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(179, 20, 30), width=4)
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(179, 20, 30), anchor="mm")
annotated = os.path.join(TMP, "wharry_annotated.png"); ann.save(annotated)

# position haute / basse côte à côte
a = Image.open(os.path.join(SHOTS, "wharry_100pc_haute.png")).convert("RGB")
b = Image.open(os.path.join(SHOTS, "wharry_100pc_basse.png")).convert("RGB")
crop = (0, 0, int(450 * k * 928 / 928), a.height)
pa, pb = a.crop((0, 0, int(560 * k), a.height)), b.crop((0, 0, int(560 * k), b.height))
both = Image.new("RGB", (pa.width * 2 + 24, pa.height), (255, 255, 255)); both.paste(pa, (0, 0)); both.paste(pb, (pa.width + 24, 0))
updown = os.path.join(TMP, "wharry_updown.png"); both.save(updown)
cover = os.path.join(SHOTS, "wharry_100pc_milieu.png")

def img(path, width):
    im = Image.open(path); h = width * im.height / im.width
    return RLImage(path, width=width, height=h)

# ---------- styles ----------
st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=24, textColor=RED, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=13, leading=17, textColor=DARK, spaceAfter=4, spaceBefore=10, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=5),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
    "cover": ParagraphStyle("cover", fontName="PopL", fontSize=40, leading=46, textColor=DARK, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="Pop", fontSize=12, leading=16, textColor=GREY, alignment=TA_CENTER),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items): return [P("&bull;&nbsp;&nbsp;" + i) for i in items]
def table(rows, widths, header=True):
    data = [[P(c, "cellb" if (header and r == 0) else "cell") for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#DDD4D4")),
             ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, RED)]
    t.setStyle(TableStyle(style)); return t

# ---------- données (identiques au code livré) ----------
PROGRAMS = [
    ("Whammy", "2 OCT UP", "0", "+24"), ("Whammy", "OCT UP", "0", "+12"), ("Whammy", "5TH UP", "0", "+7"), ("Whammy", "4TH UP", "0", "+5"),
    ("Whammy", "2ND DN", "0", "-2"), ("Whammy", "4TH DN", "0", "-5"), ("Whammy", "5TH DN", "0", "-7"), ("Whammy", "OCT DN", "0", "-12"),
    ("Whammy", "2 OCT DN", "0", "-24"), ("Whammy", "DIVE BOMB", "0", "-36"),
    ("Detune", "SHALLOW", "+10 c", "+10 c"), ("Detune", "DEEP", "+25 c", "+25 c"),
    ("Harmony", "OCT DN / OCT UP", "-12", "+12"), ("Harmony", "5TH DN / 4TH DN", "-7", "-5"), ("Harmony", "5TH / 7TH", "+7", "+10"),
    ("Harmony", "5TH / 6TH", "+7", "+9"), ("Harmony", "4TH / 5TH", "+5", "+7"), ("Harmony", "3RD / 4TH", "+4", "+5"),
    ("Harmony", "b3RD / 3RD", "+3", "+4"), ("Harmony", "2ND / 3RD", "+2", "+4"), ("Harmony", "4TH DN / 3RD DN", "-5", "-4")]
FACTORY = [  # nom, programme, moteur, glide, tone, mix
    ("Octave Scream (2 Oct Up)", "2 OCT UP", "Classic", 20, 100, 100), ("Classic Octave Up", "OCT UP", "Classic", 25, 100, 100),
    ("Power Fifth Rise", "5TH UP", "Chords", 30, 90, 100), ("Half-Step Drop (2nd Dn chords)", "2ND DN", "Chords", 40, 100, 100),
    ("Seven-String Sub (Oct Dn chords)", "OCT DN", "Chords", 30, 80, 100), ("Doom Floor (2 Oct Dn chords)", "2 OCT DN", "Chords", 45, 70, 100),
    ("Dive Bomb", "DIVE BOMB", "Classic", 15, 100, 100), ("Shallow Chorus (Detune)", "SHALLOW", "Classic", 60, 85, 90),
    ("Deep Detune Wash", "DEEP", "Chords", 60, 80, 100), ("Octave Spread (Oct Dn/Oct Up)", "OCT DN / OCT UP", "Chords", 35, 90, 85),
    ("Bass Fifth Bend (5th/7th)", "5TH / 7TH", "Classic", 30, 90, 90), ("Twin Lead Thirds (3rd/4th)", "3RD / 4TH", "Classic", 25, 100, 90),
    ("Pedal Steel Bend (2nd/3rd)", "2ND / 3RD", "Classic", 50, 85, 90), ("Studio Octave Up (HQ, latency)", "OCT UP", "Chords Studio HQ", 25, 100, 100)]
FACT_DESC = {
    "en": ["The famous two-octave scream, fast and raw.", "The classic octave-up bend for leads and riffs.", "Rising fifth on power chords, clean in Chords mode.",
           "Instant half-step drop of the whole guitar, chord-friendly.", "Sub octave under chords: massive low end for the 7-string.",
           "Two octaves down, slow glide: doom / drone floor.", "Three-octave dive, short glide for a wild drop.", "Light chorus-like doubling, the pedal doses the detuned voice.",
           "Wide detune wash for clean arpeggios.", "Harmony sweeping from an octave below to an octave above.", "Fifth to seventh harmony bend, bluesy and dark.",
           "Twin-guitar harmony moving from a third to a fourth.", "Pedal steel style bend from a second to a third.", "Highest quality octave up for re-amping / mixing (adds latency)."],
    "fr": ["Le fameux cri à deux octaves, rapide et brut.", "Le classique glissement d'une octave vers le haut pour solos et riffs.", "Quinte montante sur power chords, propre en mode Chords.",
           "Descente instantanée d'un demi-ton de toute la guitare, adaptée aux accords.", "Octave grave sous les accords : bas énorme pour la 7 cordes.",
           "Deux octaves plus bas, glissement lent : son doom / drone.", "Plongée de trois octaves, glissement court pour une chute sauvage.", "Doublage léger façon chorus, la pédale dose la voix désaccordée.",
           "Large nappe désaccordée pour arpèges en son clair.", "Harmonie qui balaie d'une octave en dessous à une octave au-dessus.", "Harmonie quinte vers septième, bluesy et sombre.",
           "Harmonie façon deux guitares, de la tierce à la quarte.", "Bend façon pedal steel, de la seconde à la tierce.", "Octave haute en qualité maximale pour ré-amping / mixage (ajoute de la latence)."]}

# ---------- textes ----------
T = {
"en": dict(
 lang="ENGLISH", sub="Pitch-shift expression pedal &nbsp;·&nbsp; VST3 for Windows and macOS", ver="User manual &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Introduction",
 intro=["<b>Wharry</b> is a Studio Wiga plugin that reproduces the behaviour of the DigiTech Whammy 5 pitch-shifting pedal: a rocker pedal that bends your notes up or down, adds a moving harmony, or a subtle detuned double.",
        "It keeps the original logic (21 programs, Classic / Chords switch, MIDI CC 11 control of the treadle, Program Change selection) and adds a few modern extras: GLIDE, TONE and MIX knobs, a live screen, a studio-quality chords engine, and a treadle that tilts on screen in real time with your MIDI expression pedal or DAW automation.",
        "Sound: Classic mode has the fast, slightly gritty character of the original single-note Whammy; Chords mode keeps chords in tune while you bend them."],
 inst_t="2. Installation",
 inst=[("Windows", "Download the <b>Wharry-VST3-Windows</b> artifact from the GitHub Actions page of the Wharry repository, unzip it and copy the <b>Wharry.vst3</b> folder into <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Download <b>Wharry-VST3-macOS</b>, unzip it and copy <b>Wharry.vst3</b> into <b>~/Library/Audio/Plug-Ins/VST3</b> (or /Library/Audio/Plug-Ins/VST3 for all users). If macOS blocks it, run <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Wharry.vst3</b> in the Terminal. The binary is universal (Apple Silicon and Intel)."),
       ("Reaper", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. The plugin appears as <b>Wharry (Studio Wiga)</b>. To control it with a MIDI pedal, make sure the track receives MIDI from your controller (record-arm or input monitoring with MIDI input set to your device).")],
 ov_t="3. Interface overview",
 ov_rows=[["#", "Control", "Role"],
   ["1", "Treadle", "Expression pedal. Toe up = 0 %, toe down = 100 %. Moves with the mouse (drag down to press the toe, wheel, double-click = full up / full down), with MIDI CC 11 and with DAW automation."],
   ["2", "Program selector", "21 detents: 10 Whammy, 2 Detune, 9 Harmony programs. Drag up / down or use the mouse wheel."],
   ["3", "Screen", "Engine, ON / BYPASS, family and number, previous / current / next program, live pitch offset, treadle travel bars and the status line. Click it to open the preset menu."],
   ["4", "CLASSIC · CHORDS", "Pitch engine selector (single notes / chords)."],
   ["5", "GLIDE", "Treadle smoothing, 0 to 250 ms (default 30 ms)."],
   ["6", "TONE", "Low-pass filter on the shifted voice, 700 Hz to fully open (default 100 %)."],
   ["7", "MIX", "Amount of effect (default 100 %)."],
   ["8", "Footswitch", "Effect ON / OFF (true-bypass style crossfade)."],
   ["9", "LED", "Lit when the effect is on."],
   ["10", "IN", "Input jack (right side)."],
   ["11", "OUT", "Output jack (left side)."]],
 ctl_t="4. Controls in detail",
 ctl=[("Treadle and programs", "In <b>Whammy</b> programs the signal is 100 % shifted: toe up gives the original pitch, toe down gives the full interval. In <b>Harmony</b> programs your dry note is kept and a second voice glides between the two intervals of the program (toe up = first interval, toe down = second). In <b>Detune</b> programs a slightly detuned voice is added to the dry signal and the treadle sets its level."),
      ("GLIDE", "Smooths the treadle (and the MIDI controller). Low values feel immediate; higher values give slow, singing bends and hide the steps of 7-bit MIDI controllers. The treadle drawn on screen follows exactly the same smoothed position you hear."),
      ("TONE", "Darkens the shifted voice only: useful to tame the fizz of 2 OCT UP or to make octave-down voices sit behind the guitar. The dry signal is never filtered."),
      ("MIX", "Whammy programs: blends dry and shifted signal (100 % = original Whammy behaviour). Harmony / Detune programs: level of the added voice (0 % = dry only)."),
      ("CLASSIC / CHORDS", "<b>Classic</b>: single-note engine locked on the detected pitch (down to about 35 Hz, so a 7-string tuned to G or lower tracks), very fast, with the slightly grainy Whammy character on chords. <b>Chords</b>: polyphonic engine, chords stay in tune, no latency. Right-click &gt; <b>Chords engine</b> &gt; <b>Studio HQ</b> uses a phase-vocoder engine with the cleanest result, at the cost of about 170 ms latency (reported to the DAW): for re-amping and mixing, not for live playing.")],
 fs_t="5. Footswitch, MIDI and expression pedals",
 fs=["<b>Footswitch</b>: click to switch the effect on or off; the LED shows the state. Switching programs or engines uses a short crossfade (no clicks).",
     "<b>MIDI CC 11</b> moves the treadle directly, as on the original pedal (0 = toe up, 127 = toe down). Right-click the treadle: <b>MIDI learn pedal controller</b> (move your pedal), <b>Reset to CC 11</b>, <b>Invert pedal direction</b>, <b>Reset heel / toe calibration</b>. The heel / toe calibration parameters let a pedal that does not reach 0 or 127 cover the full range.",
     "Right-click the footswitch or the CLASSIC · CHORDS switch to <b>MIDI learn</b> a CC or note for them (or clear the assignment).",
     "<b>Program Change</b> (can be disabled in the right-click menu), same logic as the Whammy 5: PC 1-21 = program 1-21 in Classic, PC 22-42 = same programs bypassed, PC 43-63 = Chords, PC 64-84 = Chords bypassed.",
     "Every control is also a DAW parameter: Program (selector), Expression Pedal, Classic / Chords, Effect On, Chords Studio HQ engine, Glide, Tone, Mix, MIDI pedal calibration heel / toe, MIDI pedal invert, Respond to MIDI Program Change. In Reaper you can also link them to any controller with <i>Parameter modulation / MIDI link</i>."],
 win_t="6. Window",
 win=["The window opens at its default size, <b>544 x 495 pixels</b> (same height as Ring Think), and can never be made smaller than that. Right-click &gt; <b>Plugin size</b>: Default, 75 % (557 px high), 100 % (743 px high); you can also drag the bottom-right corner (proportions are kept).",
      "The <b>status line</b> at the bottom of the screen shows the live values: pedal position, GLIDE, TONE, MIX (and the latency in Studio HQ), or the instructions during a MIDI learn."],
 pre_t="7. Presets",
 pre=["Click the screen (or use the preset menu of your DAW) to load one of the 21 original programs or one of the 14 Studio Wiga factory presets:"],
 prog_t="Original programs",
 prog_hdr=["#", "Family", "Program", "Toe up", "Toe down"],
 fac_hdr=["Preset", "Program", "Engine", "Glide / Tone / Mix", "Description"],
 tips_t="8. Tips",
 tips=["In Reaper place Wharry <b>before</b> your amp simulator (for example before Two Notes Genome): pitch shifting a clean guitar signal sounds far better than shifting distortion.",
       "7-string tuned to G or lower: Classic tracks single notes down to about 35 Hz; for low chords use Chords (live) or Studio HQ when re-amping.",
       "For dive bombs and octave sweeps raise GLIDE (60-120 ms); for rhythmic 'Morello' style switching keep it low (0-20 ms).",
       "Use TONE around 70-85 % on 2 OCT UP to soften the high octave."],
 tech_t="9. Specifications and version history",
 tech=[["Format", "VST3 (Windows 64-bit, macOS universal), AU also built on macOS"], ["Channels", "Mono or stereo in, mono or stereo out (the input is summed to mono)"],
       ["Latency", "0 samples (Classic, Chords live) · about 170 ms (Chords Studio HQ, reported to the DAW)"], ["Pitch tracking", "about 35 Hz to 1500 Hz (Classic)"],
       ["Range", "up to +2 octaves / -3 octaves"], ["CPU", "about 2 % of one core at 48 kHz"]],
 hist=[["1.0.0", "First version: Whammy 5 engine, MIDI, temporary visual"], ["1.1.0", "User design, treadle animated in perspective, GLIDE / TONE / MIX, live screen"],
       ["1.2.0", "Program LEDs and labels removed, programs shown on the screen"], ["1.3.0", "Larger screen, selector centred on its left"],
       ["1.4.0", "Pedal fills the window (544 x 495), smaller jacks, IN / OUT engraved on the sides, more realistic treadle tilt synchronised with MIDI / automation, status line in the screen"]],
 updown_cap="Treadle toe up (left) and toe down (right): the drawing follows the MIDI pedal and automation in real time.",
 ver_h=["Version", "Changes"], spec_h=["", ""]),
"fr": dict(
 lang="FRANÇAIS", sub="Pédale d'expression de transposition &nbsp;·&nbsp; VST3 pour Windows et macOS", ver="Notice d'utilisation &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Présentation",
 intro=["<b>Wharry</b> est un plugin Studio Wiga qui reproduit le fonctionnement de la pédale de transposition DigiTech Whammy 5 : une pédale à bascule qui fait glisser vos notes vers le haut ou le bas, ajoute une harmonie mobile ou un léger doublage désaccordé.",
        "Il garde la logique d'origine (21 programmes, commutateur Classic / Chords, pilotage du plateau par le MIDI CC 11, sélection par Program Change) et ajoute quelques plus : boutons GLIDE, TONE et MIX, écran en direct, moteur d'accords qualité studio, et un plateau qui bascule à l'écran en temps réel avec votre pédale d'expression MIDI ou les automations du DAW.",
        "Son : le mode Classic a le caractère rapide et un peu granuleux de la Whammy d'origine sur notes seules ; le mode Chords garde les accords justes pendant le glissement."],
 inst_t="2. Installation",
 inst=[("Windows", "Téléchargez l'artefact <b>Wharry-VST3-Windows</b> dans l'onglet Actions du dépôt GitHub Wharry, décompressez-le et copiez le dossier <b>Wharry.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Téléchargez <b>Wharry-VST3-macOS</b>, décompressez-le et copiez <b>Wharry.vst3</b> dans <b>~/Bibliothèque/Audio/Plug-Ins/VST3</b> (ou /Bibliothèque/Audio/Plug-Ins/VST3 pour tous les utilisateurs). Si macOS le bloque, tapez <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Wharry.vst3</b> dans le Terminal. Le binaire est universel (Apple Silicon et Intel)."),
       ("Reaper", "Options &gt; Préférences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Le plugin apparaît sous le nom <b>Wharry (Studio Wiga)</b>. Pour le piloter avec une pédale MIDI, la piste doit recevoir le MIDI de votre contrôleur (armement ou monitoring d'entrée avec l'entrée MIDI réglée sur votre appareil).")],
 ov_t="3. Vue d'ensemble de l'interface",
 ov_rows=[["#", "Commande", "Rôle"],
   ["1", "Plateau", "Pédale d'expression. Orteil levé = 0 %, orteil enfoncé = 100 %. Se déplace à la souris (tirer vers le bas = appuyer sur la pointe, molette, double-clic = tout en haut / tout en bas), avec le MIDI CC 11 et avec les automations du DAW."],
   ["2", "Sélecteur de programme", "21 crans : 10 programmes Whammy, 2 Detune, 9 Harmony. Glisser vers le haut / le bas ou utiliser la molette."],
   ["3", "Écran", "Moteur, ON / BYPASS, famille et numéro, programme précédent / actuel / suivant, décalage en direct, barres de course du plateau et ligne d'état. Un clic ouvre le menu des presets."],
   ["4", "CLASSIC · CHORDS", "Choix du moteur de transposition (notes seules / accords)."],
   ["5", "GLIDE", "Lissage du plateau, de 0 à 250 ms (30 ms par défaut)."],
   ["6", "TONE", "Filtre passe-bas sur la voix transposée, de 700 Hz à grand ouvert (100 % par défaut)."],
   ["7", "MIX", "Dosage de l'effet (100 % par défaut)."],
   ["8", "Footswitch", "Effet ON / OFF (fondu court, façon true bypass)."],
   ["9", "LED", "Allumée quand l'effet est actif."],
   ["10", "IN", "Jack d'entrée (côté droit)."],
   ["11", "OUT", "Jack de sortie (côté gauche)."]],
 ctl_t="4. Les commandes en détail",
 ctl=[("Plateau et programmes", "Dans les programmes <b>Whammy</b>, le signal est transposé à 100 % : orteil levé = hauteur d'origine, orteil enfoncé = intervalle complet. Dans les programmes <b>Harmony</b>, la note sèche est conservée et une deuxième voix glisse entre les deux intervalles du programme (orteil levé = premier intervalle, orteil enfoncé = second). Dans les programmes <b>Detune</b>, une voix légèrement désaccordée s'ajoute au son sec et le plateau règle son niveau."),
      ("GLIDE", "Lisse le plateau (et le contrôleur MIDI). Valeurs faibles = réponse immédiate ; valeurs élevées = glissements lents et chantants, et disparition des « marches » des contrôleurs MIDI 7 bits. Le plateau dessiné suit exactement la même position lissée que celle que vous entendez."),
      ("TONE", "Assombrit uniquement la voix transposée : pratique pour adoucir le grésillement du 2 OCT UP ou placer une octave grave derrière la guitare. Le signal sec n'est jamais filtré."),
      ("MIX", "Programmes Whammy : mélange son sec / son transposé (100 % = comportement de la Whammy d'origine). Programmes Harmony / Detune : niveau de la voix ajoutée (0 % = son sec seul)."),
      ("CLASSIC / CHORDS", "<b>Classic</b> : moteur pour notes seules calé sur la hauteur détectée (jusqu'à environ 35 Hz, donc une 7 cordes accordée en G ou plus bas est suivie), très rapide, avec le caractère un peu granuleux de la Whammy sur les accords. <b>Chords</b> : moteur polyphonique, accords justes, sans latence. Clic droit &gt; <b>Chords engine</b> &gt; <b>Studio HQ</b> : moteur à vocodeur de phase, résultat le plus propre, avec environ 170 ms de latence (annoncée au DAW) : pour le ré-amping et le mixage, pas pour jouer en direct.")],
 fs_t="5. Footswitch, MIDI et pédales d'expression",
 fs=["<b>Footswitch</b> : un clic active ou coupe l'effet ; la LED indique l'état. Les changements de programme ou de moteur se font avec un fondu court (pas de clic).",
     "Le <b>MIDI CC 11</b> déplace directement le plateau, comme sur la pédale d'origine (0 = orteil levé, 127 = orteil enfoncé). Clic droit sur le plateau : <b>MIDI learn pedal controller</b> (bougez votre pédale), <b>Reset to CC 11</b>, <b>Invert pedal direction</b>, <b>Reset heel / toe calibration</b>. Les paramètres de calibration talon / pointe permettent à une pédale qui n'atteint pas 0 ou 127 de couvrir toute la course.",
     "Clic droit sur le footswitch ou sur l'inverseur CLASSIC · CHORDS : <b>MIDI learn</b> d'un CC ou d'une note (ou suppression de l'assignation).",
     "<b>Program Change</b> (désactivable dans le menu clic droit), même logique que la Whammy 5 : PC 1-21 = programmes 1-21 en Classic, PC 22-42 = mêmes programmes en bypass, PC 43-63 = Chords, PC 64-84 = Chords en bypass.",
     "Chaque commande est aussi un paramètre du DAW : Program (selector), Expression Pedal, Classic / Chords, Effect On, Chords Studio HQ engine, Glide, Tone, Mix, MIDI Pedal Calibration Heel / Toe, MIDI Pedal Invert, Respond to MIDI Program Change. Dans Reaper, vous pouvez aussi les lier à n'importe quel contrôleur via <i>Parameter modulation / MIDI link</i>."],
 win_t="6. Fenêtre",
 win=["La fenêtre s'ouvre à sa taille par défaut, <b>544 x 495 pixels</b> (même hauteur que Ring Think), et ne peut jamais être réduite en dessous. Clic droit &gt; <b>Plugin size</b> : Default, 75 % (557 px de haut), 100 % (743 px de haut) ; on peut aussi tirer le coin en bas à droite (les proportions sont conservées).",
      "La <b>ligne d'état</b>, en bas de l'écran, affiche les valeurs en direct : position de la pédale, GLIDE, TONE, MIX (et la latence en Studio HQ), ou les consignes pendant un MIDI learn."],
 pre_t="7. Presets",
 pre=["Cliquez sur l'écran (ou utilisez le menu de presets de votre DAW) pour charger l'un des 21 programmes d'origine ou l'un des 14 presets usine Studio Wiga :"],
 prog_t="Programmes d'origine",
 prog_hdr=["#", "Famille", "Programme", "Orteil levé", "Orteil enfoncé"],
 fac_hdr=["Preset", "Programme", "Moteur", "Glide / Tone / Mix", "Description"],
 tips_t="8. Conseils d'utilisation",
 tips=["Dans Reaper, placez Wharry <b>avant</b> votre simulateur d'ampli (par exemple avant Two Notes Genome) : transposer un son clair est bien plus propre que transposer une distorsion.",
       "7 cordes accordée en G ou plus bas : le mode Classic suit les notes seules jusqu'à environ 35 Hz ; pour les accords graves, utilisez Chords (live) ou Studio HQ en ré-amping.",
       "Pour les dive bombs et les balayages d'octave, montez GLIDE (60-120 ms) ; pour des changements rythmiques secs, gardez-le bas (0-20 ms).",
       "Réglez TONE vers 70-85 % sur 2 OCT UP pour adoucir l'octave haute."],
 tech_t="9. Caractéristiques techniques et historique des versions",
 tech=[["Format", "VST3 (Windows 64 bits, macOS universel), AU également compilé sur macOS"], ["Canaux", "Entrée mono ou stéréo, sortie mono ou stéréo (l'entrée est sommée en mono)"],
       ["Latence", "0 échantillon (Classic, Chords live) · environ 170 ms (Chords Studio HQ, annoncée au DAW)"], ["Suivi de hauteur", "d'environ 35 Hz à 1500 Hz (Classic)"],
       ["Plage", "jusqu'à +2 octaves / -3 octaves"], ["Processeur", "environ 2 % d'un cœur à 48 kHz"]],
 hist=[["1.0.0", "Première version : moteur Whammy 5, MIDI, visuel provisoire"], ["1.1.0", "Design de l'utilisateur, plateau animé en perspective, GLIDE / TONE / MIX, écran en direct"],
       ["1.2.0", "LED et noms des programmes supprimés, programmes affichés sur l'écran"], ["1.3.0", "Écran agrandi, sélecteur centré à sa gauche"],
       ["1.4.0", "Pédale agrandie dans la fenêtre (544 x 495), jacks plus petits, IN / OUT gravés sur les tranches, bascule du plateau plus réaliste et synchronisée avec le MIDI / les automations, ligne d'état dans l'écran"]],
 updown_cap="Plateau orteil levé (à gauche) et orteil enfoncé (à droite) : le dessin suit la pédale MIDI et les automations en temps réel.",
 ver_h=["Version", "Changements"], spec_h=["", ""]),
}

W = A4[0] - 36 * mm
def section(lang):
    t = T[lang]; f = []
    # couverture
    f.append(Spacer(1, 12 * mm))
    f.append(P(t["lang"], "small"))
    f.append(Spacer(1, 4 * mm))
    f.append(img(cover, W * 0.86))
    f.append(Spacer(1, 6 * mm))
    f.append(P("W H A R R Y", "cover"))
    f.append(P(t["sub"], "coversub"))
    f.append(Spacer(1, 2 * mm))
    f.append(P(t["ver"], "coversub"))
    f.append(Spacer(1, 6 * mm))
    f.append(img(os.path.join(TMP, "logo_dark.png"), 52 * mm))
    f.append(PageBreak())
    f.append(P(t["intro_t"], "h1")); f += [P(x) for x in t["intro"]]
    f.append(P(t["inst_t"], "h1"))
    for h, x in t["inst"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(PageBreak())
    f.append(P(t["ov_t"], "h1")); f.append(img(annotated, W * 0.82)); f.append(Spacer(1, 3 * mm))
    f.append(table(t["ov_rows"], [10 * mm, 38 * mm, W - 48 * mm]))
    f.append(PageBreak())
    f.append(P(t["ctl_t"], "h1"))
    for h, x in t["ctl"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(KeepTogether([Spacer(1, 3 * mm), img(updown, W * 0.56), P(t["updown_cap"], "small")]))
    f.append(P(t["fs_t"], "h1")); f += bullets(t["fs"])
    f.append(P(t["win_t"], "h1")); f += [P(x) for x in t["win"]]
    f.append(PageBreak())
    f.append(P(t["pre_t"], "h1")); f += [P(x) for x in t["pre"]]
    f.append(P(t["prog_t"], "h2"))
    rows = [t["prog_hdr"]] + [[str(i + 1), fam, nm, up, dn] for i, (fam, nm, up, dn) in enumerate(PROGRAMS)]
    f.append(table(rows, [10 * mm, 24 * mm, 46 * mm, 30 * mm, 30 * mm]))
    f.append(Spacer(1, 2 * mm)); f.append(P("st = semitones / demi-tons · c = cents", "small"))
    f.append(PageBreak())
    f.append(P("Studio Wiga factory presets" if lang == "en" else "Presets usine Studio Wiga", "h2"))
    rows = [t["fac_hdr"]] + [[n, pr, eng, f"{g} ms · {to} % · {mx} %", FACT_DESC[lang][i]] for i, (n, pr, eng, g, to, mx) in enumerate(FACTORY)]
    f.append(table(rows, [40 * mm, 27 * mm, 21 * mm, 31 * mm, W - 119 * mm]))
    f.append(P(t["tips_t"], "h1")); f += bullets(t["tips"])
    f.append(KeepTogether([P(t["tech_t"], "h1"), table(t["tech"], [34 * mm, W - 34 * mm], header=False)])); f.append(Spacer(1, 4 * mm))
    f.append(table([t["ver_h"]] + t["hist"], [20 * mm, W - 20 * mm]))
    f.append(PageBreak())
    return f

# logo Studio Wiga en foncé pour les pages blanches
lg = Image.open(os.path.join(ROOT, "logo_studio_wiga_blanc.png")).convert("RGBA")
import numpy as np
la = np.array(lg); la[..., :3] = (29, 27, 28); Image.fromarray(la).save(os.path.join(TMP, "logo_dark.png"))

def deco(c, doc):
    c.saveState()
    c.setFillColor(RED); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Wharry (Studio Wiga) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="Wharry - Manual / Notice v" + VERSION, author="Studio Wiga")
doc.addPageTemplates([PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
story = section("en") + section("fr")
story = story[:-1]
doc.build(story)
print(OUT)
