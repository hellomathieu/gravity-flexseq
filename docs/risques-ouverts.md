# Risques ouverts et sujets de vigilance

**Dernière revue : 2026-08-20.**

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
| 1 | **Rien n'a jamais tourné sur le module.** Tout est simulation et tests natifs, et le risque grandit à chaque couche | **dominante** | ouvert | PRD §16 (décision ouverte) | le premier flash. `env:bringup` le rend diagnosticable ligne par ligne, pas moins risqué |
| 2 | **Le montage physique de l'OLED** conditionne que l'image tournée tombe à l'endroit — aucun simulateur ne peut le dire | moyenne | ouvert | PRD §14 | le premier flash. Repli : `U8G2_R0` dans `wokwi_main.cpp` **uniquement** |
| 3 | **Le tas est corrompu pendant un run de simavr** (`EXC_BAD_ACCESS` dans `libsystem_malloc`) | moyenne | **contourné**, pas résolu | `CLAUDE.md`, PRD §14 | un correctif amont. En attendant : aucun harnais ne touche l'allocateur après `avr_init`, et la sortie reste non tamponnée |
| 4 | **RAM et Flash croissent** à chaque fonctionnalité. 1528 / 21404 o aujourd'hui | moyenne | **sous garde** | PRD §15 | rien : c'est structurel. Le garde-fou de dérive est le seul rempart — ne jamais faire `--accept` sans regarder le diagnostic par symboles |
| 5 | **La marge de pile se réduite** à mesure que la mesure se complète : 438 → 361 o | moyenne | surveillé | PRD §15 | remesurer après chaque changement structurant. L'écriture EEPROM de la persistance n'est **pas** dans la mesure |
| 6 | **4,9 % de CPU payés inconditionnellement** par l'ISR de l'ADC, même sans channel routé | faible | ouvert | PRD §16 | l'échantillonnage conditionné au routage, à faire **avec** le §10.2 — isolé, c'est de la complexité prématurée |
| 7 | **Pic de 14,5 ms** sur le rafraîchissement complet périodique, une image sur seize | faible | **délibéré** | ADR 0001 | rien : c'est un filet volontaire contre un défaut de notre propre logique de bande sale |
| 8 | **État mixte pendant un transfert** : ~4,6 ms où une bande montre un mélange ancien/nouveau | faible | inhérent | ADR 0001 | rien. Propriété de toute mise à jour partielle, antérieure au saut de bande |
| 9 | **Wokwi non vérifié** : `"rotate"` de `board-ssd1306` et le routage des fils | faible | ouvert | PRD §14 | plus sur le chemin critique — la validation visuelle passe par `run-screen-dump.sh` |
| 10 | **`decode-velvetscreen.py` exige le clone `GravityFW`** voisin | faible | acceptable | l'en-tête du script | rien : outil à usage unique, erreur explicite et URL donnée si le clone manque |
| 11 | **`CLAUDE.md` ne survit à aucun `git clone`** — tout l'outillage y est documenté | faible | **assumé** | `.claude/rules/knowledge-persistence.md` | décision explicite du propriétaire (2026-08-19). Ne pas la rouvrir sans lui |

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

**Un outil ne doit pas supposer ce qu'il mesure.** La sonde de blocage groupait
les bandes par huit et a annoncé des images de 504 ms le jour où il n'y en a eu
que sept. Les regroupements se font désormais par le **protocole** — octet de
contrôle U8g2, adressage de page SSD1306 — et non par des seuils.

**Une sortie perdue envoie chercher le défaut ailleurs.** `stdout` redirigé est
tamponné par blocs : un rapport disparaissait au plantage, ce qui m'a fait
chercher un défaut de chargement là où le problème était dans l'allocateur.
