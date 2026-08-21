# Risques ouverts et sujets de vigilance

**Dernière revue : 2026-08-21.**

## Ce que ce document est, et n'est pas

C'est un **index de suivi**, pas une source de vérité. Chaque ligne renvoie à
l'endroit où le fait est établi — PRD Notion, ADR, `CLAUDE.md` ou le code — et
n'en recopie que le strict nécessaire pour être compréhensible seul. La règle du
projet est stricte sur ce point : un fait vit dans **une seule** source, ailleurs
on le référence (`.claude/rules/knowledge-persistence.md`).

Il ne contient donc **aucune décision**. Une décision va au PRD si elle est
produit, en ADR si elle est d'architecture.

À relire à chaque *knowledge checkpoint* : une ligne se ferme, se reformule, ou
disparaît. Un risque qui reste écrit sans bouger pendant plusieurs revues est
soit clos sans qu'on l'ait noté, soit accepté sans qu'on l'ait dit.

## Risques

| # | Sujet | Gravité | État | Le fait vit | Ce qui le clôt |
|---|---|---|---|---|---|
| 0 | ~~Révision du module non confirmée~~ | moyenne | **clos 2026-08-21** | PRD §2 | clos : le propriétaire a confirmé un **bouton SHIFT** sur son panneau, donc **Rev 2+** — le brochage de libGravity est le bon. (La Rev 1 définit `SHIFT_BTN_PIN 100`, c'est-à-dire aucun bouton SHIFT.) |
| 1b | **Aucun binaire n'exerçait le chemin `pattern → onset → impulsion`** : `main.cpp` émet les triggers mais sa banque est vide et aucune UI n'y écrit ; `wokwi_main.cpp` a du contenu mais pas de `TriggerSequencer` | **était dominante avec la ligne 1** | **mesuré 2026-08-21** | `CLAUDE.md` (fonction musicale) | clos côté simulation : `tools/run-trigger-probe.sh` injecte le contenu en RAM simulée et observe les 7 broches du binaire de production. 6/6 sorties, 11/11 écarts, gigue 1,00 ms max (0,2 % d'un step) |
| 1 | **Rien n'a jamais tourné sur le module.** Tout est simulation et tests natifs, et le risque grandit à chaque couche | **dominante** | ouvert, **flash différé** par décision du propriétaire (2026-08-21) — le module est disponible, l'attente est volontaire | PRD §16 | le premier flash. `env:bringup` le rend diagnosticable ligne par ligne, pas moins risqué |
| 2 | **Le montage physique de l'OLED** conditionne que l'image tournée tombe à l'endroit — aucun simulateur ne peut le dire | **faible** depuis le 2026-08-21 (était moyenne) | ouvert, suspendu au même flash que la ligne 1 | PRD §14, PRD §2 | le premier flash. **Corroboré entre-temps** : la config Rev 2+ du firmware d'origine résout à `U8G2_R2`, exactement ce que fait FlexSeq (attention, la logique du flag `rotateScreen` est inversée : `false` → R2). C'est aussi une option de menu persistée en EEPROM, donc un module dont l'utilisateur avait inversé la rotation montrerait FlexSeq à l'envers — sans gravité et réversible. Repli : `U8G2_R0` dans `wokwi_main.cpp` **uniquement** |
| 3 | ~~Le tas est corrompu pendant un run de simavr~~ — **c'était une lecture hors bornes d'un octet dans le journal UART de simavr**, tombant dans l'allocateur parce qu'elle traversait ses métadonnées | moyenne | **résolu 2026-08-21** | `CLAUDE.md` (section sonde de blocage), `tools/simavr-ssd1306/simavr_uart_quiet.h` | clos : les quatre harnais désarment `AVR_UART_FLAG_STDIO`. 2 SIGSEGV sur 5 → 0 sur 5, 3 rapports ASan sur 3 → 0 sur 3 |
| 4 | **RAM et Flash croissent** à chaque fonctionnalité. 1528 / 21404 o aujourd'hui | moyenne | **sous garde** | PRD §15 | rien : c'est structurel. Le garde-fou de dérive est le seul rempart — ne jamais faire `--accept` sans regarder le diagnostic par symboles |
| 5 | **La marge de pile se réduit** à mesure que la mesure se complète : 438 → 361 o | moyenne | surveillé | PRD §15 | remesurer après chaque changement structurant. L'écriture EEPROM de la persistance n'est **pas** dans la mesure |
| 6 | **5,6 % de CPU payés inconditionnellement** par l'ISR de l'ADC, même sans channel routé (4,9 % publié avant le 2026-08-21 : cadence d'ISR mal mesurée) | faible | ouvert, **différé au §10.2** par décision du propriétaire (2026-08-21) | PRD §16 | l'échantillonnage conditionné au routage, à faire **avec** le §10.2. Isolé il serait inapplicable : aucun channel ne route de CV aujourd'hui, donc conditionner reviendrait à couper le CV |
| 7 | **Pic de 15,3 ms** sur le rafraîchissement complet périodique — ratio désormais **mesuré** : 1 image sur 16,0 | faible | **délibéré** | ADR 0001 | rien : c'est un filet volontaire contre un défaut de notre propre logique de bande sale |
| 8 | **État mixte pendant un transfert** : ~4,6 ms où une bande montre un mélange ancien/nouveau | faible | inhérent | ADR 0001 | rien. Propriété de toute mise à jour partielle, antérieure au saut de bande |
| 9 | **Wokwi non vérifié** : `"rotate"` de `board-ssd1306` et le routage des fils, dans `diagram.json` / `wokwi.toml` | faible | **accepté 2026-08-21** (décision du propriétaire) | PRD §14 | rien, et c'est assumé : hors chemin critique depuis que `run-screen-dump.sh` valide le rendu. `env:wokwi` reste utile — il sert de cible à cette sonde, sous simavr, sans rapport avec Wokwi |
| 10 | **`decode-velvetscreen.py` exige le clone `GravityFW`** voisin | faible | acceptable | l'en-tête du script | rien : outil à usage unique, erreur explicite et URL donnée si le clone manque |
| 11 | **`CLAUDE.md` ne survit à aucun `git clone`** — tout l'outillage y est documenté | faible | **assumé** | `.claude/rules/knowledge-persistence.md` | décision explicite du propriétaire (2026-08-19). Ne pas la rouvrir sans lui |

| 11b | **Aucun contrôle n'est relié dans `main.cpp`** : seul `clock.AttachIntHandler()` est appelé — ni EXT, ni source, ni tempo, ni start/stop ; boutons et encodeur sont `Process()`-és sans callback relié | moyenne | **constaté 2026-08-21**, état d'avancement et non défaut | PRD §16 | relier l'UI (§12) et le transport (§8). Conséquence : un flash validerait la **chaîne matérielle**, pas les fonctionnalités |
| 12 | **L'impulsion de trigger fait 8,8 ms et non les 5 ms configurés** : l'extinction est en fin de `loop()`, donc la durée est 5 ms arrondis au passage suivant | faible | **mesuré, à surveiller** | `CLAUDE.md` (fonction musicale) | rien aujourd'hui — 1,8 % d'un step à 120 BPM en `/1`. Devient un sujet à SUBDIV rapide : à `x4` (125 ms/step) l'impulsion vaut 7 % du step, et au pire passage (15,3 ms) 12 % |
| 13 | **`PULSE` de l'expandeur MIDI reste muet** : `main.cpp` ne pilote pas `gravity.pulse` | faible | observation | le code | une décision produit : l'expandeur n'est pas encore dans le chemin (PRD §16) |

## Règles de méthode nées de ces sujets

Elles ont chacune coûté une erreur réelle, et elles valent au-delà du sujet qui
les a produites.

**Mesurer où, pas seulement combien.** Une distribution bimodale dit *combien* de
passages sont lents, jamais *lesquels*. J'ai déduit de « un passage sur sept est
long » qu'il s'agissait de la ligne de 12 steps ; c'était le titre. La mesure par
position l'a établi en une exécution (PRD §14).

**Un test vert ne prouve rien tant qu'il n'a pas été rouge.** Chaque assertion
importante de ce dépôt a été vérifiée par mutation — retirer le gel du modèle,
inverser la conversion de bande, abaisser la réserve. Deux d'entre elles
passaient pour la mauvaise raison avant cette vérification.

**Un outil ne doit pas supposer ce qu'il mesure.** Quatre occurrences, toutes sur
la même sonde. Elle groupait les bandes par huit et a annoncé des images de
504 ms le jour où il n'y en a eu que sept ; les regroupements se font désormais
par le **protocole** — octet de contrôle U8g2, adressage de page SSD1306 — et non
par des seuils. Le 2026-08-21, trois de plus, découvertes en vérifiant un seul
chiffre qui ne se reproduisait pas :

- elle divisait **toute** la première moitié du run par le nombre de conversions
  pour en tirer une cadence d'ISR, alors que celles-ci ne démarrent qu'après
  `setup()` : la cadence dépendait de la durée de la mesure (31,5 µs à 4 s,
  27,2 à 16 s). Mesurée sur une fenêtre intérieure : 26,0 µs aux trois durées ;
- elle **étiquetait** son maximum « rafraîchissement complet périodique » sans
  qu'aucune image complète ne soit tombée dans le régime mesuré ;
- sa durée d'image mélangeait les deux régimes, distribution bimodale à peu près
  à égalité : la médiane basculait d'un mode à l'autre selon la durée du run.

Trois symptômes d'une même cause : **une grandeur qui bouge quand la durée de la
mesure bouge n'est pas une grandeur.** C'est le test le moins coûteux à faire
passer à un outil, et il n'avait jamais été fait.

Une cinquième le même jour, sur une autre sonde : `trigger_probe` **supposait la
phase du playhead** — que la première impulsion observée serait le premier step
actif du pattern. Comme `transport.start()` a lieu dans `setup()`, le moteur
tourne déjà quand le contenu arrive, et la sonde a déclaré « hors grille » un
firmware parfaitement juste. Ce qui se vérifie sans connaître la phase est la
**suite des écarts**, à une rotation près — et c'est aussi la seule affirmation
qui ait un sens musical.

**Une sortie perdue envoie chercher le défaut ailleurs.** `stdout` redirigé est
tamponné par blocs : un rapport disparaissait au plantage, ce qui m'a fait
chercher un défaut de chargement là où le problème était dans l'allocateur.
