# Risques ouverts et sujets de vigilance

**Dernière revue : 2026-08-21.** Cinq lignes ouvertes, onze closes ou acceptées.

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

## Ce qui reste ouvert

Cinq lignes, et chacune dit **ce qu'elle attend et de qui**. Une ligne qui
n'attend rien de personne n'a plus sa place ici : elle est dans le tableau
suivant.

| # | Sujet | Gravité | Ce qui reste, et par qui |
|---|---|---|---|
| 1 | **Rien n'a jamais tourné sur le module.** Tout est simulation et tests natifs | **dominante** | le premier flash. **Différé par décision du propriétaire (2026-08-21)** : le module est disponible, l'attente est volontaire. `env:bringup` rend ce flash diagnosticable ligne par ligne, pas moins risqué |
| 2 | **Le montage physique de l'OLED** conditionne que l'image tournée tombe à l'endroit | faible (était moyenne) | rien à faire — se lève avec la ligne 1. **Corroboré** : la config Rev 2+ du firmware d'origine résout à `U8G2_R2`, comme FlexSeq (la logique de `rotateScreen` est inversée : `false` → R2). C'est une option de menu persistée en EEPROM, donc un module dont la rotation avait été inversée montrerait FlexSeq à l'envers — sans gravité, réversible |
| 6 | **5,6 % de CPU** payés par l'ISR de l'ADC même sans channel routé | faible | **différé au §10.2 par décision (2026-08-21)**. Isolé, le conditionnement serait inapplicable : aucun channel ne route de CV, donc conditionner reviendrait à couper le CV |
| 11b | **Aucun contrôle n'est relié dans `main.cpp`** : seul `clock.AttachIntHandler()` est appelé — ni EXT, ni source, ni tempo, ni start/stop ; boutons et encodeur `Process()`-és sans callback | moyenne | **c'est le travail suivant** : relier l'UI (§12) et le transport (§8). Ce n'est pas un défaut mais l'avancement. Conséquence : un flash aujourd'hui validerait la chaîne matérielle, pas les fonctionnalités |
| 12 | **L'impulsion de trigger fait 8,8 ms** et non les 5 ms configurés : l'extinction est en fin de `loop()`, donc 5 ms arrondis au passage suivant | faible | **rien aujourd'hui** — 1,8 % d'un step à 120 BPM en `/1`. À revoir quand SUBDIV rapide existera : à `x4` (125 ms/step) c'est 7 % du step, et 12 % au pire passage. Le seuil est écrit, il n'y a plus à le redécouvrir |

## Ce qui est clos ou accepté

Ces lignes **n'attendent plus rien**. Elles restent écrites pour que personne ne
les rouvre sans savoir ce qui a déjà été établi ou chiffré.

| # | Sujet | État | Par quoi |
|---|---|---|---|
| 0 | Révision du module non confirmée | **clos 2026-08-21** | le propriétaire a confirmé un **bouton SHIFT** sur son panneau, donc **Rev 2+** : le brochage de libGravity est le bon. La Rev 1 définit `SHIFT_BTN_PIN 100`, c'est-à-dire aucun bouton SHIFT |
| 1b | Aucun binaire n'exerçait le chemin `pattern → onset → impulsion` | **mesuré 2026-08-21** | `tools/run-trigger-probe.sh` injecte le contenu en RAM simulée et observe les 7 broches du binaire de production : 6/6 sorties, 11/11 écarts, gigue 1,00 ms au pire (0,2 % d'un step). Clos **côté simulation** ; le matériel reste la ligne 1 |
| 3 | « Le tas est corrompu pendant un run de simavr » | **résolu 2026-08-21** | c'était une lecture hors bornes d'un octet dans le journal UART de simavr. Les quatre harnais désarment `AVR_UART_FLAG_STDIO` : 2 SIGSEGV sur 5 → 0 sur 5, 3 rapports ASan sur 3 → 0 sur 3 |
| 4 | RAM et Flash croissent à chaque fonctionnalité | **accepté et dimensionné 2026-08-21** | ce n'est pas un risque mais une contrainte permanente sous garde active — **et le reste à construire tient**, voir le chiffrage ci-dessous |
| 5 | La marge de pile se réduit à mesure que la mesure se complète | **accepté et dimensionné 2026-08-21** | 159 o de pic pour 361 o de marge, six familles d'ISR démontrées parcourues. Un seul trou nommé, et son obligation est écrite ci-dessous |
| 7 | Pic de 15,3 ms sur le rafraîchissement complet, 1 image sur 16,0 | **propriété de conception** | ADR 0001. Les deux façons de s'en débarrasser sont chiffrées ci-dessous ; ne pas rouvrir sans un meilleur chiffre |
| 8 | État mixte pendant un transfert, ~4,6 ms | **impossible à corriger, arithmétiquement** | le seul remède est un tampon complet de 1024 o, contre 264 o disponibles. Voir ci-dessous |
| 9 | Wokwi non vérifié : `"rotate"` et le routage des fils | **accepté 2026-08-21** (décision) | hors chemin critique depuis que `run-screen-dump.sh` valide le rendu. `env:wokwi` reste utile : il sert de cible à cette sonde, sous simavr, sans rapport avec Wokwi |
| 10 | `decode-velvetscreen.py` exige le clone `GravityFW` voisin | **acceptable** | outil à usage unique ; `--src`, `$GRAVITY_FW_INO`, et une erreur explicite avec l'URL du clone |
| 11 | `CLAUDE.md` ne survit à aucun `git clone` | **clos par décision** (2026-08-19) | décision explicite du propriétaire, non-versionnement délibéré.|
| 13 | `PULSE` de l'expandeur MIDI reste muet | **observation, pas défaut** | `main.cpp` ne pilote pas `gravity.pulse` : l'expandeur n'est pas encore dans le chemin (PRD §16) |

### Le budget mémoire, chiffré une fois — lignes 4 et 5

Ces deux lignes disaient « surveillé » sans jamais dire **combien il reste et pour
quoi faire**. C'est chiffré depuis le 2026-08-21, et le chiffrage vit au **PRD §15**
— sa source normative — et non ici. Ce qu'il faut retenir pour clore ces lignes :

- **264 o de RAM disponibles** pour de nouvelles données statiques (520 o libres
  moins la réserve de pile de 256 o), contre **~52 o estimés** pour tout ce qui
  reste à construire — UI, transport, persistance, destinations CV, RECORDING.
  Marge d'environ **5×**. La persistance ne coûte presque rien parce que la banque
  est **déjà** en RAM : l'écriture EEPROM la lit sur place, sans copie.
- **6244 o de Flash** avant que le garde-fou refuse à 90 %. L'UI complète est le
  seul poste vraiment coûteux à venir, de l'ordre de 2 à 4 ko.
- **Le déclencheur est explicite**, il n'y a plus à en juger au cas par cas :
  échec au-delà de +16 o de RAM ou +512 o de Flash non acquittés, plafonds à 256 o
  libres ou 90 % de Flash. `--accept` ne se fait jamais sans regarder le
  diagnostic par symboles.
- **L'unique trou de la mesure de pile, et son obligation.** La sonde mesure ce
  que le firmware **exécute pendant le run**. L'écriture EEPROM n'y est pas parce
  qu'elle n'existe pas — mais elle n'y sera pas non plus **automatiquement** le
  jour où elle existera : il faudra que le run la **provoque**. C'est la seule
  chose à ne pas oublier au §11.

### Les deux propriétés du rendu étalé, et le prix de s'en débarrasser — lignes 7 et 8

**Ligne 7, le pic de 15,3 ms.** Deux façons de le supprimer, toutes deux évaluées :

- *rastériser le titre sur deux passages*, en accumulant dans le tampon de 128 o
  que U8g2 possède déjà — ne rien effacer ni envoyer entre les deux. Coût : 0 o de
  RAM, mais une machine à états de dessin partiel qui **casse l'invariant du cycle
  indivisible** sur lequel ADR 0001 repose, et une bande qui montre son ancien
  contenu un passage de plus. Refusé : c'est beaucoup de fragilité pour un pic qui
  concerne une image sur seize et tient déjà dans son budget ;
- *raccourcir le titre ou réduire la police*. Refusé par le propriétaire, qui veut
  des titres explicites.

Le pic reste donc, et c'est un choix : un filet volontaire contre un défaut de
notre propre logique de bande sale, qui se répare seul en quelques images.

**Ligne 8, l'état mixte de ~4,6 ms.** Le seul remède est un double tampon, donc
**1024 o** — contre 264 o disponibles. Ce n'est pas un arbitrage, c'est une
impossibilité arithmétique. La ligne est close pour cette raison, et non parce
qu'on aurait décidé de vivre avec.

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
