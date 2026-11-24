AtelierDSP Lo-Fi : 4 plugins VST3 originaux, dans un dépôt séparé des 3 premiers.
  - RetroDeck   : émulation cassette, vinyle et VHS
  - RetroDeck 2 : seconde version pour comparaison (hystérésis magnétique Jiles-Atherton, traçage géométrique
                  de la pointe sur le sillon avec RIAA, effets plus prononcés) ; s'installe à côté de RetroDeck
  - SampleGrade : grades d'échantillonnage d'un sampler de phrases de la fin des années 90
                  (Hi-Fi 31,25 kHz, Standard/Long 15,63 kHz, Lo-Fi 1 7,81 kHz, Lo-Fi 2 3,91 kHz)
  - WowLab      : pleurage et scintillement (wow & flutter) avec couleur bande, aléatoire, synchro tempo, mode HQ

Compilation : poussez ce dossier sur un nouveau dépôt GitHub ; l'onglet Actions produit l'artefact AtelierLoFi-macOS.
Installation macOS (14.4 ou plus) : copiez uniquement RetroDeck.vst3, "RetroDeck 2.vst3", SampleGrade.vst3 et WowLab.vst3 dans ~/Library/Audio/Plug-Ins/VST3/,
puis pour chacun : xattr -dr com.apple.quarantine <bundle> ; codesign --force --deep -s - <bundle>
Presets utilisateur : ~/Documents/AtelierDSP/RetroDeck et ~/Documents/AtelierDSP/SampleGrade
Tests du moteur (Linux/macOS) : g++ -std=c++17 -O2 -ISource tests/test_retro.cpp -o tr && ./tr

RetroDeck - Mix Mode :
  Morph (par défaut) : un seul trajet de signal ; Mix dose l'intensité de tous les étages, aucun phasage possible.
  Parallel, aligned  : vrai mélange dry/wet, le dry passe par le même transport (même délai, même pleurage).
  Parallel, classic  : mélange classique (seul le décalage fixe de 2 échantillons est compensé).
Page Response : quantités Réponse / Saturation / Compression de 0 à 100 % avec plages x1, x2, x4, x8,
pente des aigus 12/24/36 dB/oct, réglage des coupures haute et basse, fréquence et gain du head bump.

Windows : copiez les dossiers .vst3 dans C:\Program Files\Common Files\VST3\ (runtime Microsoft intégré, rien d'autre à installer).
Tests du moteur v2 : g++ -std=c++17 -O2 -ISource tests/test_retro2.cpp -o tr2 && ./tr2
