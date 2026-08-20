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
  l'ordre de **25 ms** de temps de bus à 400 kHz. *Estimation du 2026-08-20,
  corrigée le même jour par la mesure : **36,8 ms** de bus, l'estimation ne
  comptant que les bits et non le surcoût logiciel par morceau.*
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
  **blocage réel a été mesuré le même jour** par un harnais dédié
  (`tools/simavr-ssd1306/`) qui câble l'esclave `ssd1306_virt` de simavr sur le
  TWI : chiffres dans les conséquences ci-dessous.

## Décision

Le rendu est **étalé : une bande par passage de la boucle principale**, et le
renderer **ne dessine que ce qui tombe dans la bande courante**.

Le renderer reste pur — c'est ce qui rend l'étalement possible — mais il reçoit
désormais la bande en paramètre (`Band{y0, y1}`, par défaut tout l'écran) et
écarte en amont les éléments hors bande. Sans cet écartement, les 24 steps, leurs
chiffres et le titre étaient recalculés **huit fois** : U8g2 découpe ce qu'on lui
envoie, mais l'appel a lieu quand même, et cela coûtait autant que le transfert
I2C lui-même (chiffres ci-dessous). La bande est **passée** au renderer plutôt que
demandée au canvas, pour qu'il reste sans dépendance et testable sur n'importe
quelle bande ; `PagedScreen` l'obtient de l'affichage
(`getBufferCurrTileRow()`, `getBufferTileHeight()`).

⚠️ **La bande obtenue est en coordonnées d'AFFICHAGE, le renderer travaille en
coordonnées LOGIQUES.** libGravity construit son objet en `U8G2_R2` : U8g2 fait
tourner de 180° *avant* de découper (`u8g2_draw_l90_r2` transforme, puis le
découpage se fait contre `pixel_curr_row`). `PagedScreen::bandOf()` applique donc
l'inverse — sous R2, `logique = 63 − affichage`, ce qui échange les bornes.
Omettre cette inversion donnait à chaque bande **la moitié inverse** de ce
qu'elle affiche : l'écran restait quasi blanc. Le défaut a vécu un commit, et
c'est la lecture de la mémoire du panneau (`tools/run-screen-dump.sh`) qui l'a
trouvé — aucun test natif ne pouvait le voir, tous fournissaient la bande déjà en
coordonnées logiques. `test_paged_screen` modélise désormais la rotation dans son
faux affichage, et `test_the_band_conversion_is_the_right_way_round` fixe le
sens.

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

- Le blocage au pire cas passe de l'image entière à **une bande**. Une image
  occupe 9 passages : un pour le gel et la première bande — `firstPage()` ne
  transfère rien — puis 8 transferts.
- **Mesuré le 2026-08-20** (`tools/run-blocking-probe.sh`, esclave SSD1306 réel
  dans simavr). ⚠️ **Chiffres révisés** : une première série avait été relevée sur
  le build où la conversion de bande était fausse, donc où l'écran **ne dessinait
  presque rien** — « 47 ms par image, 7,74 ms au pire, dessin à 1,24 ms » mesurait
  surtout l'absence de dessin. Comparaison à rendu correct des deux côtés, ADC
  hors interruption ou son artefact corrigé :

  | | rendu complet à chaque bande | écarté à la bande |
  |---|---|---|
  | passage médian | 8,52 ms | **6,48 ms** |
  | passage au pire | 16,16 ms | **15,32 ms** |
  | image entière | 74,0 ms | **59,1 ms** |
  | transfert d'une bande | 4,60 ms | 4,62 ms |

  Le gain est donc réel mais **plus modeste** qu'annoncé : −24 % sur le passage
  médian et −20 % sur l'image, au lieu du facteur 2 rapporté. Et le **pire** cas
  ne bouge quasiment pas.
- **Pourquoi le pire cas résiste : l'écartement a CONCENTRÉ le dessin.** La
  distribution est bimodale — six bandes sur huit ne portent aucun élément et
  coûtent le seul transfert (~5,5 ms), tandis que celles qui portent une ligne de
  12 steps coûtent ~15 ms. Le p90 vaut 15,3 ms, soit environ un passage sur sept.
  Réduire le total sans réduire le pic était le résultat attendu du procédé ; le
  pic reste donc la cible d'une optimisation ultérieure, distincte de cette
  décision.
- **Plancher atteint.** Un passage ne peut plus descendre sous les ~4,6 ms du
  transfert d'une bande, qui domine désormais. Aller plus bas demanderait
  d'envoyer moins qu'une bande par passage — la granularité de U8g2 est la ligne
  de tuiles — ou de pousser le bus au-delà de 400 kHz, hors spécification du
  SSD1306.
- Estimations initiales corrigées : « ~3 ms par bande » et « ~25 ms par image »
  étaient **basses**. Une bande part en **6 transactions Wire** de ~21 octets, et
  les intervalles entre morceaux coûtent ~107 µs chacun. Le débit du bus est bien
  celui annoncé (22,5 µs d'octet à 400 kHz, plus 4,55 µs d'ISR TWI) ; c'est le
  découpage qui manquait au calcul.
- Coût de l'écartement : **+522 o de Flash**, **0 o de RAM** (20236 → 20758 o,
  RAM inchangée à 1490 o).
- **Propriété vérifiée par test** : la réunion des 8 bandes rend **exactement**
  l'image complète, pixel pour pixel (`test_pattern_screen`). Un pixel de moins et
  un élément aurait disparu de l'écran. Le faux canvas y découpe comme le mode
  page — sans ce découpage le test serait faux, un élément à cheval sur deux
  bandes étant dessiné deux fois.
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
- **Conséquence pour le CV, mesurée.** Le CV n'étant échantillonné qu'une fois
  par passage (`gravity.Process()`), une impulsion plus courte que le pire
  passage peut passer inaperçue. Ce seuil est passé de **16,16 à 7,74 ms** :
  une gate de 10 ms est désormais vue à coup sûr, ce qui n'était pas le cas
  avant. Une impulsion de 1 à 5 ms reste en revanche exposée, et le repli du
  PRD §10.6 — échantillonnage du convertisseur sous interruption — garde son
  sens pour ce cas. Décision ouverte, au propriétaire du PRD.

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
