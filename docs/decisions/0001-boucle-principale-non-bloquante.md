# 0001 — Boucle principale non bloquante : rendu OLED étalé sur ses 8 bandes

- **Statut :** accepted
- **Date :** 2026-08-20
- **Remplace :** —
- **Remplacé par :** —

## Contexte

Le PRD §12 a décidé de **réutiliser l'objet d'affichage de libGravity**,
`gravity.display`, de type `U8G2_SSD1306_128X64_NONAME_1_HW_I2C`, plutôt que
d'instancier un second objet U8g2. Le mode `_1_` n'alloue que **128 octets** de
tampon là où l'écran fait 128 × 64 pixels, soit 1024 octets.

Conséquence directe de ce choix : U8g2 rend l'image en **8 bandes horizontales**
successives via `firstPage()` / `nextPage()`. La fonction de dessin s'exécute
donc 8 fois par rafraîchissement et doit rester pure — contrainte déjà
documentée dans `include/flexseq/PatternScreen.h`.

Faits mesurés ou vérifiés dans les sources :

- Le bus I2C tourne à **400 kHz**. Le descripteur SSD1306 de U8g2 déclare
  `i2c_bus_clock_100kHz = 4` (`u8x8_d_ssd1306_128x64_noname.c:348`) et le
  backend Arduino applique cette valeur à chaque transfert (`U8x8lib.cpp:1361`
  et `:1367`). Ni libGravity ni FlexSeq ne la fixent : `initDisplay()` se
  contente de `display.begin()`.
- Une image pleine représente 1024 octets de données d'affichage, soit de
  l'ordre de **25 ms** de temps de bus à 400 kHz.
- Les 8 bandes s'enchaînaient **dans un seul appel** (`src/main.cpp`) : la
  boucle principale était bloquée pendant l'image entière. C'est ce que cette
  décision remplace ; elle est implémentée depuis le 2026-08-20.
- Le rendu est limité à une image toutes les 40 ms et seulement si l'écran a
  changé (`UI_MIN_INTERVAL_MS`). Cette limite réduit la **fréquence** des
  blocages, pas leur **durée**.
- La boucle principale est l'endroit où les ticks sont drainés et les triggers
  émis, et une sortie ne peut être réarmée qu'une fois par drainage.
- **`Gravity::Process()` ne touche ni au display ni à `Wire`** — seulement les
  boutons, l'encodeur, les CV (`analogRead`) et les sorties (`libGravity.cpp`).
  C'est ce qui rend l'entrelacement sûr : entre deux bandes, rien d'autre
  n'utilise l'objet d'affichage ni le bus I2C, donc aucune image ne peut être
  corrompue par ce qui tourne entre deux passages.
- **Non mesuré à la date de la décision :** rendu et chronométrage n'avaient
  jamais tourné ensemble. `src/simavr_main.cpp` ne contient aucun rendu, donc
  les durées de step validées en simavr l'ont été sans affichage.
  *Mise à jour du 2026-08-20 :* le firmware complet, rendu compris, a depuis
  tourné sous simavr — la **pile** y a été mesurée (pic 120 o, PRD §15). Le
  **blocage réel** reste inconnu : simavr n'a aucun esclave I2C sur le bus, donc
  la durée d'un transfert n'y est pas fidèle. Wokwi ou le module trancheront.

## Décision

Le rendu est **étalé : une bande par passage de la boucle principale**. Le
renderer n'est pas modifié — il est déjà pur, ce qui est précisément ce qui rend
l'étalement possible.

La séquence vit dans `include/flexseq/PagedScreen.h`, **templatisée sur le type
d'affichage** comme `PatternScreen` l'est sur le canvas : le firmware l'instancie
sur `gravity.display`, un test natif sur un faux affichage. Le `Display` est
passé à chaque appel plutôt que conservé, pour ne pas payer 2 octets de
référence. Deux appels seulement : `begin()` gèle et dessine la première bande,
`advance()` transfère la bande courante et dessine la suivante.

`src/main.cpp` et `src/wokwi_main.cpp` l'utilisent **tous deux** : le harnais de
validation visuelle doit exercer le chemin de rendu réel, sans quoi il ne valide
plus le firmware (PRD §14).

Le modèle est **gelé au début de l'image** et les 8 bandes sont dessinées depuis
cette copie : `PatternScreenModel` (8 octets) **plus une copie du `Pattern`**
(15 octets), soit **23 octets**. La copie du contenu est nécessaire parce que le
pattern est partagé et éditable pendant la lecture (PRD §6.3) : sans elle, deux
bandes de la même image pourraient montrer des contenus différents.

## Conséquences

- Le blocage au pire cas passe de l'image entière à **une bande**, soit de
  l'ordre de 3 ms au lieu de 25. Une image occupe 9 passages : un pour le gel et
  la première bande — `firstPage()` ne transfère rien — puis 8 transferts.
- Le bénéfice principal n'est pas l'affichage mais le **chronométrage des
  triggers** : pendant un blocage, les ticks s'accumulent et les onsets se
  tassent au drainage suivant.
- Rend praticable la destination **CV → RESET par channel** (PRD §10), dont la
  détection de front est échantillonnée dans la boucle principale.
- Coût **mesuré** (build `nanoatmega328`) : RAM **1466 → 1490 o** (+24 : les 23
  du gel plus le drapeau d'état), Flash **20100 → 20236 o** (+136). Une image
  s'étale désormais sur plusieurs passages : l'affichage retarde la réalité
  d'autant.
- Couvert par `test/test_paged_screen` (6 assertions) : une image tient
  exactement 8 bandes, l'écran reste occupé jusqu'à la dernière, `advance()` hors
  image ne transfère rien, et **une édition survenue pendant l'image ne la
  déchire pas**. Cette dernière assertion a été vérifiée par mutation — le gel
  retiré, elle rougit (500 pixels sur la première bande, 707 sur la suivante).
- **Ne dispense pas de mesurer.** La largeur minimale d'impulsion réellement
  captée reste à établir, rendu et chronométrage tournant ensemble.

## Alternatives écartées

- **Un second objet U8g2 en mode `_F_`** (tampon de 1024 octets, une seule
  passe) : +1024 octets de RAM, hors budget, et contraire au PRD §12.
- **Baisser la fréquence de rafraîchissement** : réduit la fréquence des
  blocages, pas leur durée. Le problème est la durée.
- **Échantillonner le CV dans une interruption dédiée** (timer ou fin de
  conversion) : retirerait à libGravity la propriété du convertisseur, dont
  `cv1.Process()` dépend via `analogRead`. Écarté pour l'instant, conservé comme
  repli si la mesure l'exige.

## Références

- PRD §10 (CV), §12 (UI / contrainte d'affichage), §14 (niveaux de validation).
- `include/flexseq/PagedScreen.h`, `src/main.cpp`, `src/wokwi_main.cpp`,
  `include/flexseq/PatternScreen.h`, `test/test_paged_screen/`.
- U8g2 : `clib/u8x8_d_ssd1306_128x64_noname.c`, `U8x8lib.cpp`.
- libGravity `9be88be1f4` : `libGravity.cpp` (`initDisplay`).
