# Wharry (Studio Wiga)

Plugin VST3 (Windows / macOS, + AU sur Mac) reproduisant le fonctionnement de la pédale DigiTech Whammy 5,
au format **pédale double** (fenêtre aux hauteurs de Ring Think : 544 x 495 par défaut et au minimum, puis 75 % et 100 %).

- 21 programmes : 10 Whammy, 2 Detune, 9 Harmony (ordre du sélecteur = Program Change 1 à 21)
- **CLASSIC** (notes seules, réactif, caractère Whammy) / **CHORDS** (accords, sans latence) ; option CHORDS **Studio HQ** (vocodeur de phase, latence annoncée au DAW)
- Pédale d'expression : paramètre automatisable + **CC 11 direct** (MIDI learn possible), lissage réglable, calibration talon/pointe, inversion
- Program Change façon Whammy 5 : 1-21 Classic, 22-42 Classic bypass, 43-63 Chords, 64-84 Chords bypass
- Inverseur CLASSIC / CHORDS, footswitch ON / OFF (MIDI learn par clic droit)
- Boutons GLIDE (lissage de la pédale), TONE (filtre de la voix traitée), MIX (dosage de l'effet)
- Sélecteur 21 crans sans LED : l'écran affiche famille, numéro, programmes voisins, programme choisi, décalage en direct et course de la pédale ; clic sur l'écran = menu des presets
- Suivi des notes jusqu'à ~35 Hz (7 cordes en G et plus bas)

v1.8.1 : pédale d'expression MIDI inversée par défaut (option « Invert pedal direction » cochée à l'ouverture)
v1.8.0 : anti-gel v2 (JUCE 8.0.15, caches refaits par le timer, chien de garde, aucun appel à l'hôte depuis le thread audio)

## Visuel
Design de l'utilisateur : `design/wharry_design_source.png`. `python3 tools/design_art.py` génère `background.png`,
`treadle_down.png` (plateau à plat), les sprites et `tools/layout.txt` (coordonnées reprises dans `PluginEditor.cpp`).
Le plugin calcule la bascule du plateau en perspective (orteil levé -> enfoncé).

## Build
GitHub Actions (`.github/workflows/build.yml`) : artefacts `Wharry-VST3-Windows` et `Wharry-VST3-macOS`.
Tests du moteur audio : `g++ -O2 -std=c++17 tests/test_dsp.cpp -o test_dsp && ./test_dsp`
