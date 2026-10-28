# Peachy (Studio Wiga) — VST3

Préampli saturé + fuzz au **format pédale double**, d'après une pédale « préampli + fuzz » au son des sessions
metal du milieu des années 90 (7 cordes accordées bas). Disposition des commandes reprise de la pédale d'origine,
habillage Studio Wiga d'après le visuel de l'utilisateur (cadre noir peint, bande noire rayée de rouge en haut, rouge en dégradé, inscriptions manuscrites). Usage personnel. Windows 11 et macOS
(binaire universel).

## Installation via GitHub

1. Le push sur `main` lance la compilation (onglet **Actions**) : Windows (`windows-2022`) et macOS.
2. Télécharger l'artefact :
   - **Windows** : copier `Peachy.vst3` dans `C:\Program Files\Common Files\VST3\`
   - **macOS** : dézipper puis copier `Peachy.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`
     (premier lancement : `xattr -cr ~/Library/Audio/Plug-Ins/VST3/Peachy.vst3` si macOS le bloque)
3. Relancer l'analyse des plugins. Le plugin apparaît comme **Peachy (Studio Wiga)**.

## Commandes

| Section | Commande | Rôle |
|---|---|---|
| PREAMP (footswitch gauche, LED gauche) | GAIN | 6 à 56 dB répartis sur deux étages à lampe (écrêtage asymétrique), sur-échantillonnage x4 |
| | MASTER | Volume de sortie du préampli (midi = niveau de référence, 0 = coupé) |
| | BASS / MID / TREBLE | Tone stack passif 3 bandes, fonction de transfert analogique exacte (valeurs d'une tête britannique), placé après la saturation ; BASS logarithmique |
| | PRESENCE (petit potard) | Plateau aigu 3,2 kHz, −10 / +10 dB, même pente des deux côtés de midi |
| FUZZ (footswitch droit, LED droite) | DISTORTION | Son des **captures NAM de la vraie pédale** (50 à 100 %), fondu entre deux captures voisines ; sous 50 % : moins d'attaque à l'entrée de la capture 50 % |
| | TONE | Filtre exact du TONE d'origine, mesuré sur les captures TONE 1 à 9 (agit après l'écrêtage) |
| | VOLUME | Volume de sortie du fuzz (midi = niveau des captures) |
| | OCTAVE (inverseur) | Octave grave analogique (bascule à fréquence moitié) ajoutée devant le fuzz, suit les cordes très graves |

Ordre du signal : entrée → FUZZ → PREAMP → sortie (le fuzz attaque le préampli). Les deux footswitchs sont
indépendants ; clic = on / off, appui maintenu (> 0,45 s) quand la section était coupée = momentané
(désactivable, clic droit > Settings). Noise gate intégré (clic droit > Noise gate : Off / Light par défaut / Medium / Strong).

Interprétation des potards du haut (non documentée par le fabricant) : MASTER / GAIN = préampli, VOLUME / DISTORTION
= fuzz (côté droit de la pédale, avec TONE et OCTAVE).

## Le fuzz : captures NAM

- 45 captures fournies (fuzz seul, DISTORTION 50 / 65 / 75 / 90 / 100 %, TONE 1 à 9, 48 kHz).
- Analyse : le TONE est un filtre linéaire **après** l'écrêtage (cohérence ≈ 1 entre sinus glissé et riff, même
  courbe quel que soit le gain). Il est reproduit par un filtre exact (rapport de deux polynômes du 2ᵉ ordre ajustés
  sur les 9 positions mesurées, erreur rms 0,4 dB) appliqué aux 5 captures TONE 5, qui tournent en réseau WaveNet
  (NeuralAmpModelerCore, licence MIT, dans `third_party/`).
- Vérification (riff de guitare synthétique, écart normalisé ESR après alignement) : DISTORTION 75 / TONE 5 = 0,0000 (identique),
  90 / 1 = 0,003, 65 / 3 = 0,006, 100 / 7 = 0,016, 75 / 9 = 0,05 — du même ordre que l'erreur des captures elles-mêmes.
- À une fréquence autre que 48 kHz : rééchantillonnage sinc (Kaiser) vers 48 kHz et retour, latence déclarée au DAW.
- Processeur : ~10 % d'un cœur avec une capture, ~20 % entre deux positions de capture (AVX2).

## Interface

- Fenêtre ouverte à **661 × 495** (taille de Ring Think), impossible de la réduire en dessous, proportions bloquées.
  Clic droit > *Window size* : défaut / 75 % / 100 % (993 × 743).
- **Barre de presets** : flèches = précédent / suivant, clic sur le nom = menu (10 presets d'usine, presets utilisateur 1 à 9).
- **Ligne d'état** : réglage de la commande que l'on bouge (potards en %), dans la police manuscrite des inscriptions de la pédale (Shadows Into Light).
- Potards : seul le repère blanc peint tourne, les reflets du biseau chromé restent fixes (lumière en haut à gauche).
- Footswitchs : celui d'Echoflex (référence Studio Wiga), bouton assombri tant qu'on appuie. LED rouges en fondu,
  la façade s'éclaire autour.
- Inverseur OCTAVE : levier nickel du kit Studio Wiga (haut = on), image intermédiaire pendant la bascule, molette possible.
- Jacks du kit sur les tranches (OUT à gauche, IN à droite), logo Studio Wiga noir en bas.
- **Anti-gel v2** : rendu logiciel forcé sous Windows, fenêtre opaque, timer 30 Hz qui ne redessine que ce qui change,
  caches d'images reconstruits par le timer seulement, chien de garde de redessin (1,5 s), jamais de `setSize` dans
  `resized()` ; côté audio, aucun appel à l'hôte ni allocation (MIDI en file vidée sur le thread de messages,
  paramètres lus par pointeurs mis en cache). JUCE 8.0.15.

## MIDI

| Message | Fonction |
|---|---|
| CC 20 à 28 | Master, Gain, Volume, Distortion, Bass, Mid, Treble, Tone, Presence |
| CC 52 à 60 | Poids faible 14 bits des CC 20 à 28 |
| CC 29 | Octave (≥ 64 on) |
| CC 30 / 31 | Footswitch PREAMP / FUZZ |
| Program Change 1 à 10 | Presets d'usine |

Footswitchs MIDI à accrochage (127 / 0, par défaut) ou momentanés (bascule à chaque appui). Pédale d'expression sans
saccades (rampe entre deux CC, course apprise automatiquement, zones mortes aux butées). Tout est automatisable.

## Presets d'usine

1. Indigo Crunch · 2. Seven String Chug · 3. Scooped Rhythm · 4. Woolly Fuzz · 5. Fuzz Octave Sludge ·
6. Bright Fuzz Lead · 7. Fuzz Into Preamp · 8. Wall of Sludge · 9. Gritty Boost · 10. Capture Reference G75 T5 (capture d'origine telle quelle)

## Fichiers

- `PeachyDSP.*` moteur audio · `PluginProcessor.*` / `PluginEditor.*` plugin · `Layout.h` repères (généré)
- `tools/make_art.py` génère tout le visuel (fond, inverseur, calques des LED, `Layout.h`) · `tools/render` captures
  de la vraie fenêtre sous Linux · `tests/test_dsp.cpp` banc d'essai audio
- `models/` captures NAM du fuzz (TONE 5) intégrées au plugin · `third_party/` NeuralAmpModelerCore (MIT), nlohmann/json (MIT)
- Police : Shadows Into Light (OFL) pour les inscriptions et la ligne d'état ; Poppins pour la notice
