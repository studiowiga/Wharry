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
VERSION = "1.9.0"
TMP = os.path.dirname(OUT)

for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(ROOT, "fonts", f + ".ttf")))

RED = colors.HexColor("#B3141E"); DARK = colors.HexColor("#1D1B1C"); GREY = colors.HexColor("#6B6466"); LIGHT = colors.HexColor("#F4EEEE")

import numpy as np
# ---------- illustrations ----------
# SHOTS = captures du plugin (tools/render : wharry_100pc_milieu / haute / basse.png) ou, si elles manquent, dossier contenant
# les images déjà préparées : wharry_annotated.png, wharry_updown.png, wharry_cover_cutout.png (RGBA détourée).
if os.path.exists(os.path.join(SHOTS, "wharry_100pc_milieu.png")):
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
    pa, pb = a.crop((0, 0, int(560 * k), a.height)), b.crop((0, 0, int(560 * k), b.height))
    both = Image.new("RGB", (pa.width * 2 + 24, pa.height), (255, 255, 255)); both.paste(pa, (0, 0)); both.paste(pb, (pa.width + 24, 0))
    updown = os.path.join(TMP, "wharry_updown.png"); both.save(updown)
    # pédale détourée (masque de la silhouette généré par tools/design_art.py), plateau à plat
    shot = Image.open(os.path.join(SHOTS, "wharry_100pc_basse.png")).convert("RGB")
    msk = Image.open(os.path.join(ROOT, "design", "pedal_mask.png")).convert("L").resize(shot.size, Image.LANCZOS)
    cut = Image.new("RGBA", shot.size, (0, 0, 0, 0)); cut.paste(shot, (0, 0), msk)
    cut = cut.crop(cut.getbbox())
else:
    annotated = os.path.join(SHOTS, "wharry_annotated.png")
    updown = os.path.join(SHOTS, "wharry_updown.png")
    cut = Image.open(os.path.join(SHOTS, "wharry_cover_cutout.png")).convert("RGBA")
    cut = cut.crop(cut.getbbox())
# couverture : pédale détourée sur fond noir, léger reflet au sol et halo rouge très doux qui retombe à zéro avant les bords
cw, ch = cut.size
pad = int(cw * 0.10)
canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
gcx, gcy = canvas.width / 2, pad + ch * 0.45
glow = np.exp(-(((xx - gcx) / (cw * 0.48)) ** 2 + ((yy - gcy) / (ch * 0.42)) ** 2)) * 38
edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
halo = np.dstack([glow * 1.0, glow * 0.10, glow * 0.12, np.full_like(glow, 255)]).astype(np.uint8)
canvas = Image.alpha_composite(canvas, Image.fromarray(halo, "RGBA"))
refl = cut.transpose(Image.FLIP_TOP_BOTTOM)
ra = np.array(refl).astype(np.float64)
fade = np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22
ra[..., 3] *= fade[:, None]
canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
canvas.alpha_composite(cut, (pad, pad))
canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
cover = os.path.join(TMP, "wharry_cover.png"); canvas.convert("RGB").save(cover)

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
    "cover": ParagraphStyle("cover", fontName="PopL", fontSize=40, leading=46, textColor=colors.white, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="Pop", fontSize=12, leading=16, textColor=colors.HexColor("#BDB5B5"), alignment=TA_CENTER),
    "coverlang": ParagraphStyle("coverlang", fontName="PopM", fontSize=8.5, leading=11, textColor=colors.HexColor("#E0262F")),
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
    ("Pedal Steel Bend (2nd/3rd)", "2ND / 3RD", "Classic", 50, 85, 90), ("Whammy Octave Down", "OCT DN", "Classic", 25, 100, 100)]
FACT_DESC = {
    "en": ["The famous two-octave scream, fast and raw.", "The classic octave-up bend for leads and riffs.", "Rising fifth on power chords, clean in Chords mode.",
           "Instant half-step drop of the whole guitar, chord-friendly.", "Sub octave under chords: massive low end for the 7-string.",
           "Two octaves down, slow glide: doom / drone floor.", "Three-octave dive, short glide for a wild drop.", "Light chorus-like doubling, the pedal doses the detuned voice.",
           "Wide detune wash for clean arpeggios.", "Harmony sweeping from an octave below to an octave above.", "Fifth to seventh harmony bend, bluesy and dark.",
           "Twin-guitar harmony moving from a third to a fourth.", "Pedal steel style bend from a second to a third.", "The classic one-octave dive, toe down = one octave below."],
    "fr": ["Le fameux cri à deux octaves, rapide et brut.", "Le classique glissement d'une octave vers le haut pour solos et riffs.", "Quinte montante sur power chords, propre en mode Chords.",
           "Descente instantanée d'un demi-ton de toute la guitare, adaptée aux accords.", "Octave grave sous les accords : bas énorme pour la 7 cordes.",
           "Deux octaves plus bas, glissement lent : son doom / drone.", "Plongée de trois octaves, glissement court pour une chute sauvage.", "Doublage léger façon chorus, la pédale dose la voix désaccordée.",
           "Large nappe désaccordée pour arpèges en son clair.", "Harmonie qui balaie d'une octave en dessous à une octave au-dessus.", "Harmonie quinte vers septième, bluesy et sombre.",
           "Harmonie façon deux guitares, de la tierce à la quarte.", "Bend façon pedal steel, de la seconde à la tierce.", "La descente classique d'une octave : orteil enfoncé = une octave plus bas."]}

# ---------- textes ----------
T = {
"en": dict(
 lang="ENGLISH", sub="Pitch-shift expression pedal &nbsp;·&nbsp; VST3 for Windows and macOS", ver="User manual &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Introduction",
 intro=["<b>Wharry</b> is a Studio Wiga plugin that reproduces the behaviour of the DigiTech Whammy 5 pitch-shifting pedal: a rocker pedal that bends your notes up or down, adds a moving harmony, or a subtle detuned double.",
        "It keeps the original logic (21 programs, Classic / Chords switch, MIDI CC 11 control of the treadle, Program Change selection) and adds a few modern extras: GLIDE, TONE and MIX knobs, a live screen, a studio-quality chords engine, and a treadle that tilts on screen in real time with your MIDI expression pedal or DAW automation.",
        "Sound: Classic mode has the fast, slightly gritty character of the original single-note Whammy; Chords mode keeps chords in tune while you bend them."],
 inst_t="2. Installation",
 inst=[("Windows", "Download the <b>Wharry-VST3-Windows</b> artifact from the Actions tab of the GitHub repository studiowiga/wharry, unzip it and copy the <b>Wharry.vst3</b> folder into <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Download <b>Wharry-VST3-macOS</b>, unzip it and copy <b>Wharry.vst3</b> into <b>~/Library/Audio/Plug-Ins/VST3</b> (or /Library/Audio/Plug-Ins/VST3 for all users). If macOS blocks it, run <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Wharry.vst3</b> in the Terminal. The binary is universal (Apple Silicon and Intel)."),
       ("Reaper", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. The plugin appears as <b>Wharry (Studio Wiga)</b>. To control it with a MIDI pedal, make sure the track receives MIDI from your controller (record-arm or input monitoring with MIDI input set to your device).")],
 ov_t="3. Interface overview",
 ov_rows=[["#", "Control", "Role"],
   ["1", "Treadle", "Expression pedal. Toe up = 0 %, toe down = 100 %. Moves with the mouse (drag down to press the toe, wheel, double-click = full up / full down), with MIDI CC 11 (or any learnt CC) and with DAW automation."],
   ["2", "Program selector", "21 detents: 10 Whammy, 2 Detune, 9 Harmony programs. Drag up / down or use the mouse wheel."],
   ["3", "Screen", "Engine, ON / BYPASS, preset name (or family) and number, previous / current / next program, live pitch offset, treadle travel bars and the status line. Click the previous / next program to step through the programs, click anywhere else (PRESETS) to open the preset list."],
   ["4", "CLASSIC · CHORDS", "Pitch engine selector (single notes / chords)."],
   ["5", "GLIDE", "Treadle smoothing, 0 to 250 ms (default 30 ms)."],
   ["6", "TONE", "Low-pass filter on the shifted voice, 700 Hz to fully open (default 100 %)."],
   ["7", "MIX", "Amount of effect (default 100 %)."],
   ["8", "Footswitch", "Effect ON / OFF (short click-free crossfade); its cap goes down when pressed, by mouse or MIDI."],
   ["9", "LED", "Lit when the effect is on."],
   ["10", "IN", "Input jack (right side)."],
   ["11", "OUT", "Output jack (left side)."]],
 ctl_t="4. Controls in detail",
 ctl=[("Treadle and programs", "In <b>Whammy</b> programs the signal is 100 % shifted: toe up gives the original pitch, toe down gives the full interval. In <b>Harmony</b> programs your dry note is kept and a second voice glides between the two intervals of the program (toe up = first interval, toe down = second). In <b>Detune</b> programs a slightly detuned voice is added to the dry signal and the treadle sets its level."),
      ("GLIDE", "Smooths the treadle (and the MIDI controller). Low values feel immediate; higher values give slow, singing bends and hide the steps of 7-bit MIDI controllers. The treadle drawn on screen follows exactly the same smoothed position you hear."),
      ("TONE", "Darkens the shifted voice only: useful to tame the fizz of 2 OCT UP or to make octave-down voices sit behind the guitar. The dry signal is never filtered."),
      ("MIX", "Whammy programs: blends dry and shifted signal (100 % = original Whammy behaviour). Harmony / Detune programs: level of the added voice (0 % = dry only)."),
      ("CLASSIC / CHORDS", "<b>Classic</b>: single-note engine locked on the detected pitch (down to about 35 Hz, so a 7-string tuned to G or lower tracks), very fast, with the slightly grainy Whammy character on chords. <b>Chords</b>: polyphonic engine, chords stay in tune, no latency. Right-click &gt; <b>Chords engine</b> &gt; <b>Studio HQ</b> uses a phase-vocoder engine with the cleanest result, at the cost of about 170 ms latency (reported to the DAW): for re-amping and mixing, not for live playing.")],
 fs_t="5. Footswitch, LED and expression pedal",
 fs=["<b>Footswitch</b>: click to switch the effect on or off; the cap goes down while pressed (also when switched by MIDI) and the LED shows the state. Switching programs or engines uses a short crossfade (no clicks).",
     "<b>CLASSIC / CHORDS</b>: click the lever; it slides in its slot. The engine change is crossfaded.",
     "<b>Treadle look</b>: the drawing follows the tilt with light and shadow: raised toe catches the light and casts a shadow, a soft reflection slides along the rubber, and the toe darkens when pressed down.",
     "<b>Expression pedal</b>: <b>CC 11</b> moves the treadle directly, as on the original pedal. <b>The pedal direction is inverted by default</b> (right-click &gt; <b>Invert pedal direction</b> is ticked when the plugin opens): untick it if the treadle moves the wrong way with your pedal. Right-click the treadle or the enclosure: <b>MIDI learn pedal controller</b>, <b>Reset to CC 11 (Whammy standard)</b>, <b>Invert pedal direction</b>, <b>Calibrate pedal</b>, <b>Reset pedal calibration</b>.",
     "<b>Pedal range</b>: many expression pedals never send exactly 0 and 127 (an M-Audio EX-P on an M-Vave Chocolate Plus often sends about 2 to 116). Wharry learns the real range automatically the first time you press the pedal fully down and up, and adds a small dead zone at both ends: toe up is exactly 0, toe down exactly the full interval. If the pedal is linked through your DAW instead of CC 11, use <b>Calibrate pedal</b>: press it fully down and up; the calibration ends by itself (or choose <b>Finish pedal calibration</b>, or click the screen)."],
 midi_t="6. MIDI",
 midi=["<b>MIDI Learn on every control</b>: right-click a control (the treadle, the program selector, GLIDE, TONE, MIX, the CLASSIC / CHORDS switch or the footswitch). The top of the menu shows <b>MIDI Learn: &lt;Name&gt; (CC n / none)</b> with the current assignment, <b>Clear MIDI: &lt;Name&gt;</b> and, while a learn is waiting, <b>Cancel MIDI learn</b>. Choose MIDI Learn, then move a pedal, a knob or press a footswitch of your controller: the status line of the screen shows <b>MIDI LEARN: move a control for &lt;Name&gt;</b>, then <b>&lt;Name&gt; = CC n</b> for 2 seconds.",
       "Right-click outside the controls (for example on the enclosure) to open the usual menu with a <b>MIDI</b> submenu: <b>Learn</b> for each of the 7 controls (with its current assignment), <b>Cancel MIDI learn</b> and <b>Reset MIDI to defaults</b> (pedal CC 11, all others none). Choosing the pending item again also cancels the learn.",
       "A CC number can only drive one control: learning a CC that is already used removes it from the other control. Learn accepts CC 0 to 119; the CLASSIC / CHORDS switch and the footswitch can also learn a MIDI <b>note</b>. Wharry listens on all MIDI channels. <b>Assignments are saved with the project</b> (and with DAW presets); projects made with older versions keep their pedal and switch assignments."],
 midi_hdr=["Control", "Default CC", "Type"],
 midi_rows=[["Expression pedal (treadle)", "CC 11", "Continuous 0-127 (toe up to toe down), with auto range, calibration and inversion (on by default)"],
            ["Program selector", "none", "21-position selector: the 0-127 travel is split into 21 equal zones (programs 1 to 21)"],
            ["Glide", "none", "Continuous 0-127 = 0 to 250 ms"], ["Tone", "none", "Continuous 0-127 = 0-100 %"], ["Mix", "none", "Continuous 0-127 = 0-100 %"],
            ["CLASSIC / CHORDS switch", "none", "Momentary footswitch (CC or note): each press (CC 64 or more, or note on) toggles"],
            ["ON / OFF footswitch", "none", "Momentary footswitch (CC or note): each press (CC 64 or more, or note on) toggles"]],
 midi2=["<b>Program Change</b> (right-click &gt; <b>Respond to MIDI Program Change</b>, on by default), same logic as the Whammy 5: PC 1-21 = programs 1-21 in Classic, PC 22-42 = same programs bypassed, PC 43-63 = Chords, PC 64-84 = Chords bypassed.",
        "<b>M-Vave Chocolate Plus</b>: plug your expression pedal into its EXP input and set it to CC 11 (or MIDI-learn the treadle); learn one footswitch on the Wharry footswitch and another one on CLASSIC / CHORDS.",
        "Every control is also a DAW parameter: Program (selector), Expression Pedal, Classic / Chords (switch), Effect On (footswitch), Chords Studio HQ engine, Glide, Tone, Mix, Pedal Calibration Heel / Toe, MIDI Pedal Invert, Respond to MIDI Program Change; all can be automated in Reaper."],
 win_t="7. Window",
 win=["Wharry uses the double pedal format with the same window heights as Ring Think. Right-click &gt; <b>Plugin size</b>: <b>60 % (default)</b>, <b>75 %</b>, <b>100 %</b>. The window opens at its default size and can never be made smaller than it; you can also drag the bottom-right corner (proportions are kept).",
      "The <b>status line</b> at the bottom of the screen shows the live values: pedal position, GLIDE, TONE, MIX (and the latency in Studio HQ), or the instructions during a calibration or a MIDI learn."],
 pre_t="8. Presets",
 pre=["Click the screen (or use the preset menu of your DAW) to load one of the 21 original programs or one of the 14 Studio Wiga factory presets:"],
 prog_t="Original programs",
 prog_hdr=["#", "Family", "Program", "Toe up", "Toe down"],
 fac_hdr=["Preset", "Program", "Engine", "Glide / Tone / Mix", "Description"],
 tips_t="9. Tips",
 tips=["In Reaper place Wharry <b>before</b> your amp simulator (for example before Two Notes Genome): pitch shifting a clean guitar signal sounds far better than shifting distortion.",
       "7-string tuned to G or lower: Classic tracks single notes down to about 35 Hz; for low chords use Chords (live) or Studio HQ when re-amping.",
       "For dive bombs and octave sweeps raise GLIDE (60-120 ms); for rhythmic 'Morello' style switching keep it low (0-20 ms).",
       "Use TONE around 70-85 % on 2 OCT UP to soften the high octave.",
       "Learn the program selector on a knob of your controller to sweep through the 21 programs live."],
 tech_t="10. Specifications",
 tech=[["Format", "VST3 (Windows 64-bit, macOS universal), AU also built on macOS"], ["Version", VERSION],
       ["Channels", "Mono or stereo in, mono or stereo out (the input is summed to mono)"],
       ["Latency", "0 samples (Classic, Chords live) · about 170 ms (Chords Studio HQ, reported to the DAW)"], ["Pitch tracking", "about 35 Hz to 1500 Hz (Classic)"],
       ["Range", "up to +2 octaves / -3 octaves"],
       ["MIDI", "MIDI Learn on all 7 controls (saved with the project), expression pedal CC 11 by default (inverted by default), notes for the switches, Program Change 1-84 (Whammy 5 map), all channels"],
       ["Window", "60 % (default, also the minimum size), 75 %, 100 %; anti-freeze v2 on Windows (display caches rebuilt by a timer, watchdog that restarts drawing, no call to the host from the audio thread)"],
       ["CPU", "about 2 % of one core at 48 kHz"]],
 updown_cap="Treadle toe up (left) and toe down (right): the drawing follows the MIDI pedal and automation in real time.",
 spec_h=["", ""]),
"fr": dict(
 lang="FRANÇAIS", sub="Pédale d'expression de transposition &nbsp;·&nbsp; VST3 pour Windows et macOS", ver="Notice d'utilisation &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Présentation",
 intro=["<b>Wharry</b> est un plugin Studio Wiga qui reproduit le fonctionnement de la pédale de transposition DigiTech Whammy 5 : une pédale à bascule qui fait glisser vos notes vers le haut ou le bas, ajoute une harmonie mobile ou un léger doublage désaccordé.",
        "Il garde la logique d'origine (21 programmes, commutateur Classic / Chords, pilotage du plateau par le MIDI CC 11, sélection par Program Change) et ajoute quelques plus : boutons GLIDE, TONE et MIX, écran en direct, moteur d'accords qualité studio, et un plateau qui bascule à l'écran en temps réel avec votre pédale d'expression MIDI ou les automations du DAW.",
        "Son : le mode Classic a le caractère rapide et un peu granuleux de la Whammy d'origine sur notes seules ; le mode Chords garde les accords justes pendant le glissement."],
 inst_t="2. Installation",
 inst=[("Windows", "Téléchargez l'artefact <b>Wharry-VST3-Windows</b> dans l'onglet Actions du dépôt GitHub studiowiga/wharry, décompressez-le et copiez le dossier <b>Wharry.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Téléchargez <b>Wharry-VST3-macOS</b>, décompressez-le et copiez <b>Wharry.vst3</b> dans <b>~/Bibliothèque/Audio/Plug-Ins/VST3</b> (ou /Bibliothèque/Audio/Plug-Ins/VST3 pour tous les utilisateurs). Si macOS le bloque, tapez <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Wharry.vst3</b> dans le Terminal. Le binaire est universel (Apple Silicon et Intel)."),
       ("Reaper", "Options &gt; Préférences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Le plugin apparaît sous le nom <b>Wharry (Studio Wiga)</b>. Pour le piloter avec une pédale MIDI, la piste doit recevoir le MIDI de votre contrôleur (armement ou monitoring d'entrée avec l'entrée MIDI réglée sur votre appareil).")],
 ov_t="3. Vue d'ensemble de l'interface",
 ov_rows=[["#", "Commande", "Rôle"],
   ["1", "Plateau", "Pédale d'expression. Orteil levé = 0 %, orteil enfoncé = 100 %. Se déplace à la souris (tirer vers le bas = appuyer sur la pointe, molette, double-clic = tout en haut / tout en bas), avec le MIDI CC 11 (ou tout CC appris) et avec les automations du DAW."],
   ["2", "Sélecteur de programme", "21 crans : 10 programmes Whammy, 2 Detune, 9 Harmony. Glisser vers le haut / le bas ou utiliser la molette."],
   ["3", "Écran", "Moteur, ON / BYPASS, nom du preset (ou famille) et numéro, programme précédent / actuel / suivant, décalage en direct, barres de course du plateau et ligne d'état. Un clic sur le programme précédent / suivant change de programme ; un clic ailleurs (PRESETS) ouvre la liste des presets."],
   ["4", "CLASSIC · CHORDS", "Choix du moteur de transposition (notes seules / accords)."],
   ["5", "GLIDE", "Lissage du plateau, de 0 à 250 ms (30 ms par défaut)."],
   ["6", "TONE", "Filtre passe-bas sur la voix transposée, de 700 Hz à grand ouvert (100 % par défaut)."],
   ["7", "MIX", "Dosage de l'effet (100 % par défaut)."],
   ["8", "Footswitch", "Effet ON / OFF (fondu court sans clic) ; son chapeau s'enfonce à l'appui, à la souris comme en MIDI."],
   ["9", "LED", "Allumée quand l'effet est actif."],
   ["10", "IN", "Jack d'entrée (côté droit)."],
   ["11", "OUT", "Jack de sortie (côté gauche)."]],
 ctl_t="4. Les commandes en détail",
 ctl=[("Plateau et programmes", "Dans les programmes <b>Whammy</b>, le signal est transposé à 100 % : orteil levé = hauteur d'origine, orteil enfoncé = intervalle complet. Dans les programmes <b>Harmony</b>, la note sèche est conservée et une deuxième voix glisse entre les deux intervalles du programme (orteil levé = premier intervalle, orteil enfoncé = second). Dans les programmes <b>Detune</b>, une voix légèrement désaccordée s'ajoute au son sec et le plateau règle son niveau."),
      ("GLIDE", "Lisse le plateau (et le contrôleur MIDI). Valeurs faibles = réponse immédiate ; valeurs élevées = glissements lents et chantants, et disparition des « marches » des contrôleurs MIDI 7 bits. Le plateau dessiné suit exactement la même position lissée que celle que vous entendez."),
      ("TONE", "Assombrit uniquement la voix transposée : pratique pour adoucir le grésillement du 2 OCT UP ou placer une octave grave derrière la guitare. Le signal sec n'est jamais filtré."),
      ("MIX", "Programmes Whammy : mélange son sec / son transposé (100 % = comportement de la Whammy d'origine). Programmes Harmony / Detune : niveau de la voix ajoutée (0 % = son sec seul)."),
      ("CLASSIC / CHORDS", "<b>Classic</b> : moteur pour notes seules calé sur la hauteur détectée (jusqu'à environ 35 Hz, donc une 7 cordes accordée en G ou plus bas est suivie), très rapide, avec le caractère un peu granuleux de la Whammy sur les accords. <b>Chords</b> : moteur polyphonique, accords justes, sans latence. Clic droit &gt; <b>Chords engine</b> &gt; <b>Studio HQ</b> : moteur à vocodeur de phase, résultat le plus propre, avec environ 170 ms de latence (annoncée au DAW) : pour le ré-amping et le mixage, pas pour jouer en direct.")],
 fs_t="5. Footswitch, LED et pédale d'expression",
 fs=["<b>Footswitch</b> : un clic active ou coupe l'effet ; le chapeau s'enfonce pendant l'appui (aussi quand il est commandé en MIDI) et la LED indique l'état. Les changements de programme ou de moteur se font avec un fondu court (pas de clic).",
     "<b>CLASSIC / CHORDS</b> : cliquez sur le levier, il coulisse dans sa fente. Le changement de moteur se fait en fondu.",
     "<b>Rendu du plateau</b> : le dessin suit l'inclinaison avec lumière et ombres : la pointe levée prend la lumière et projette une ombre, un reflet doux glisse sur le caoutchouc, et la pointe s'assombrit quand elle est enfoncée.",
     "<b>Pédale d'expression</b> : le <b>CC 11</b> déplace directement le plateau, comme sur la pédale d'origine. <b>Le sens de la pédale est inversé par défaut</b> (clic droit &gt; <b>Invert pedal direction</b> est coché à l'ouverture du plugin) : décochez-le si le plateau part dans le mauvais sens avec votre pédale. Clic droit sur le plateau ou le boîtier : <b>MIDI learn pedal controller</b>, <b>Reset to CC 11 (Whammy standard)</b>, <b>Invert pedal direction</b>, <b>Calibrate pedal</b>, <b>Reset pedal calibration</b>.",
     "<b>Course de la pédale</b> : beaucoup de pédales d'expression n'envoient jamais exactement 0 et 127 (une M-Audio EX-P sur une M-Vave Chocolate Plus envoie souvent environ 2 à 116). Wharry apprend tout seul la course réelle dès que vous enfoncez puis relevez la pédale à fond, et ajoute une petite zone morte aux deux butées : pédale relevée = exactement 0, enfoncée = exactement l'intervalle complet. Si la pédale est liée par le DAW au lieu du CC 11, utilisez <b>Calibrate pedal</b> : enfoncez-la puis relevez-la à fond ; la calibration se termine seule (ou choisissez <b>Finish pedal calibration</b>, ou cliquez sur l'écran)."],
 midi_t="6. MIDI",
 midi=["<b>MIDI Learn sur toutes les commandes</b> : faites un clic droit sur une commande (le plateau, le sélecteur de programme, GLIDE, TONE, MIX, l'inverseur CLASSIC / CHORDS ou le footswitch). Le haut du menu affiche <b>MIDI Learn: &lt;Nom&gt; (CC n / none)</b> avec l'assignation actuelle, <b>Clear MIDI: &lt;Nom&gt;</b> et, tant qu'un apprentissage est en attente, <b>Cancel MIDI learn</b>. Choisissez MIDI Learn puis bougez une pédale, un bouton ou appuyez sur un footswitch de votre contrôleur : la ligne d'état de l'écran affiche <b>MIDI LEARN: move a control for &lt;Nom&gt;</b>, puis <b>&lt;Nom&gt; = CC n</b> pendant 2 secondes.",
       "Un clic droit hors des commandes (par exemple sur le boîtier) ouvre le menu habituel avec un sous-menu <b>MIDI</b> : <b>Learn</b> pour chacune des 7 commandes (avec son assignation actuelle), <b>Cancel MIDI learn</b> et <b>Reset MIDI to defaults</b> (pédale CC 11, toutes les autres sans CC). Choisir de nouveau la commande en attente annule aussi l'apprentissage.",
       "Un numéro de CC ne pilote qu'une seule commande : apprendre un CC déjà utilisé le retire de l'autre commande. L'apprentissage accepte les CC 0 à 119 ; l'inverseur CLASSIC / CHORDS et le footswitch peuvent aussi apprendre une <b>note</b> MIDI. Wharry écoute tous les canaux MIDI. <b>Les assignations sont sauvegardées avec le projet</b> (et avec les presets du DAW) ; les projets faits avec une version antérieure gardent leurs assignations de pédale et de commutateurs."],
 midi_hdr=["Commande", "CC par défaut", "Type"],
 midi_rows=[["Expression pedal (plateau)", "CC 11", "Continu 0-127 (orteil levé à orteil enfoncé), avec course automatique, calibration et inversion (active par défaut)"],
            ["Program selector", "aucun", "Sélecteur à 21 positions : la course 0-127 est découpée en 21 zones égales (programmes 1 à 21)"],
            ["Glide", "aucun", "Continu 0-127 = 0 à 250 ms"], ["Tone", "aucun", "Continu 0-127 = 0-100 %"], ["Mix", "aucun", "Continu 0-127 = 0-100 %"],
            ["CLASSIC / CHORDS switch", "aucun", "Footswitch momentané (CC ou note) : chaque appui (CC à 64 ou plus, ou note) bascule"],
            ["ON / OFF footswitch", "aucun", "Footswitch momentané (CC ou note) : chaque appui (CC à 64 ou plus, ou note) bascule"]],
 midi2=["<b>Program Change</b> (clic droit &gt; <b>Respond to MIDI Program Change</b>, actif par défaut), même logique que la Whammy 5 : PC 1-21 = programmes 1-21 en Classic, PC 22-42 = mêmes programmes en bypass, PC 43-63 = Chords, PC 64-84 = Chords en bypass.",
        "<b>M-Vave Chocolate Plus</b> : branchez votre pédale d'expression sur son entrée EXP et réglez-la sur le CC 11 (ou faites un MIDI Learn du plateau) ; apprenez un footswitch sur le footswitch de Wharry et un autre sur CLASSIC / CHORDS.",
        "Chaque commande est aussi un paramètre du DAW : Program (selector), Expression Pedal, Classic / Chords (switch), Effect On (footswitch), Chords Studio HQ engine, Glide, Tone, Mix, Pedal Calibration Heel / Toe, MIDI Pedal Invert, Respond to MIDI Program Change ; tous automatisables dans Reaper."],
 win_t="7. Fenêtre",
 win=["Wharry utilise le format pédale double avec les mêmes hauteurs de fenêtre que Ring Think. Clic droit &gt; <b>Plugin size</b> : <b>60 % (default)</b>, <b>75 %</b>, <b>100 %</b>. La fenêtre s'ouvre à sa taille par défaut et ne peut jamais être réduite en dessous ; on peut aussi tirer le coin en bas à droite (les proportions sont conservées).",
      "La <b>ligne d'état</b>, en bas de l'écran, affiche les valeurs en direct : position de la pédale, GLIDE, TONE, MIX (et la latence en Studio HQ), ou les consignes pendant une calibration ou un MIDI Learn."],
 pre_t="8. Presets",
 pre=["Cliquez sur l'écran (ou utilisez le menu de presets de votre DAW) pour charger l'un des 21 programmes d'origine ou l'un des 14 presets usine Studio Wiga :"],
 prog_t="Programmes d'origine",
 prog_hdr=["#", "Famille", "Programme", "Orteil levé", "Orteil enfoncé"],
 fac_hdr=["Preset", "Programme", "Moteur", "Glide / Tone / Mix", "Description"],
 tips_t="9. Conseils d'utilisation",
 tips=["Dans Reaper, placez Wharry <b>avant</b> votre simulateur d'ampli (par exemple avant Two Notes Genome) : transposer un son clair est bien plus propre que transposer une distorsion.",
       "7 cordes accordée en G ou plus bas : le mode Classic suit les notes seules jusqu'à environ 35 Hz ; pour les accords graves, utilisez Chords (live) ou Studio HQ en ré-amping.",
       "Pour les dive bombs et les balayages d'octave, montez GLIDE (60-120 ms) ; pour des changements rythmiques secs, gardez-le bas (0-20 ms).",
       "Réglez TONE vers 70-85 % sur 2 OCT UP pour adoucir l'octave haute.",
       "Apprenez le sélecteur de programme sur un bouton de votre contrôleur pour parcourir les 21 programmes en direct."],
 tech_t="10. Caractéristiques techniques",
 tech=[["Format", "VST3 (Windows 64 bits, macOS universel), AU également compilé sur macOS"], ["Version", VERSION],
       ["Canaux", "Entrée mono ou stéréo, sortie mono ou stéréo (l'entrée est sommée en mono)"],
       ["Latence", "0 échantillon (Classic, Chords live) · environ 170 ms (Chords Studio HQ, annoncée au DAW)"], ["Suivi de hauteur", "d'environ 35 Hz à 1500 Hz (Classic)"],
       ["Plage", "jusqu'à +2 octaves / -3 octaves"],
       ["MIDI", "MIDI Learn sur les 7 commandes (sauvegardé avec le projet), pédale d'expression CC 11 par défaut (inversée par défaut), notes pour les commutateurs, Program Change 1-84 (table de la Whammy 5), tous canaux"],
       ["Fenêtre", "60 % (par défaut, c'est aussi la taille minimale), 75 %, 100 % ; anti-gel v2 sous Windows (caches d'affichage refaits par un timer, chien de garde qui relance le dessin, aucun appel à l'hôte depuis le thread audio)"],
       ["Processeur", "environ 2 % d'un cœur à 48 kHz"]],
 updown_cap="Plateau orteil levé (à gauche) et orteil enfoncé (à droite) : le dessin suit la pédale MIDI et les automations en temps réel.",
 spec_h=["", ""]),
}

W = A4[0] - 36 * mm
def section(lang):
    t = T[lang]; f = []
    # couverture (page noire, pédale détourée)
    f.append(NextPageTemplate("p"))
    f.append(Spacer(1, 6 * mm))
    f.append(P(t["lang"], "coverlang"))
    f.append(Spacer(1, 6 * mm))
    f.append(img(cover, W * 0.92))
    f.append(Spacer(1, 6 * mm))
    f.append(P("W H A R R Y", "cover"))
    f.append(P(t["sub"], "coversub"))
    f.append(Spacer(1, 2 * mm))
    f.append(P(t["ver"], "coversub"))
    f.append(Spacer(1, 6 * mm))
    f.append(img(os.path.join(ROOT, "logo_studio_wiga_blanc.png"), 52 * mm))
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
    f.append(P(t["midi_t"], "h1")); f += [P(x) for x in t["midi"]]
    f.append(Spacer(1, 2 * mm)); f.append(KeepTogether([table([t["midi_hdr"]] + t["midi_rows"], [44 * mm, 24 * mm, W - 68 * mm])])); f.append(Spacer(1, 3 * mm))
    f += bullets(t["midi2"])
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
    f.append(table(rows, [40 * mm, 27 * mm, 17 * mm, 37 * mm, W - 121 * mm]))
    f.append(P(t["tips_t"], "h1")); f += bullets(t["tips"])
    f.append(KeepTogether([P(t["tech_t"], "h1"), table(t["tech"], [34 * mm, W - 34 * mm], header=False)]))
    f.append(PageBreak())
    return f

# logo Studio Wiga en foncé pour les pages blanches
lg = Image.open(os.path.join(ROOT, "logo_studio_wiga_blanc.png")).convert("RGBA")
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
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(RED); c.rect(0, A4[1] - 3 * mm, A4[0], 3 * mm, stroke=0, fill=1); c.restoreState()
fr_ = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")
doc.addPageTemplates([PageTemplate(id="cover", frames=[fr_], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f2")], onPage=deco)])
en = section("en"); fr = section("fr")
en[-1:] = [NextPageTemplate("cover"), PageBreak()]      # la partie française commence aussi par une couverture noire
story = en + fr[:-1]
doc.build(story)
print(OUT)
