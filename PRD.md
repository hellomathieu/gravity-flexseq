<!--
SOURCE NORMATIVE DU PRD — ELLE VIT DANS CE DEPOT DEPUIS LE 2026-08-30.

Decision du proprietaire, 2026-08-30 : le PRD vit dans le depot. Ce fichier
remplace la page Notion comme source normative des exigences et des decisions
produit.

Importe depuis Notion le 2026-08-30T09:29:24Z, page
https://app.notion.com/p/3bed2c2576ce80459448cc525a929d9d
La page Notion devient une source HISTORIQUE. Ne plus l'editer, et ne plus s'y
referer pour trancher : en cas de divergence, ce fichier fait foi.

Routage : .claude/rules/knowledge-persistence.md
Hierarchie des sources : CLAUDE.md, section "Source hierarchy"

DECISION EN ATTENTE, inscrite ici pour ne pas etre perdue : ce fichier est en
francais, et la regle 3 de .claude/rules/github-conventions.md demande l'anglais
pour toute documentation poussee sur GitHub. Cette regle exemptait le PRD parce
qu'il n'etait pas pousse. La premise a disparu. Traduire, ou inscrire une
exception explicite : le proprietaire tranche.
-->

**Firmware alternatif du module Eurorack Sitka Instruments Gravity — Trigger Sequencer dont les patterns ont une capacité de 36 steps, la longueur réglable à l'interface allant de 1 à 24 jusqu'au lot F.**
> **Statut :** version normative — intègre les décisions validées par le POC TypeScript et l'alignement firmware C++ (tests natifs verts, vérification simavr, RAM mesurée sur ATmega328P).
> **Base logicielle :** `libGravity` (Adam Wonak), résolu depuis le **fork du projet** `github.com/hellomathieu/libGravity`, figé au commit `4c5b4d0b4f38a9e04055ad48f1f7e2d90541c93c` — `platformio.ini` fait foi, et la charte de ce que le fork a le droit de réparer est l'**ADR 0008**. L'ancien pin `9be88be1f4` est obsolète.
> **Hardware :** Gravity strictement inchangé.
---
## 1. Vision
Gravity FlexSeq est un firmware alternatif du Gravity, développé **sans modification du hardware** et basé techniquement sur `libGravity`. Il fait évoluer le **Trigger Sequencer** d'origine (16 steps fixes) vers un séquenceur dont les patterns ont une **capacité de 36 steps**, la longueur réglable à l'interface allant de **1 à ****`MAX_LENGTH`** — 24 aujourd'hui, 36 au lot F —, avec une conception temporelle plus générale, tout en conservant les fonctionnalités historiques du module.
Le projet vise aussi un **cycle de développement rapide** : une fonctionnalité doit pouvoir être développée, testée et visualisée **avant** tout flash sur le module physique.
⚠️ **RÈGLE NORMATIVE, redonnée par le propriétaire le 2026-08-23 après un essai sur le module.** Toutes les **features** et toutes les **pages** du firmware d'origine sont **conservées**. Seul le **mode SEQ** évolue. Le reste ne change que par effet de cette évolution. Cette règle a priorité sur toute conception d'écran : si une page de FlexSeq porte moins que la page correspondante de l'original, c'est FlexSeq qui a tort.
**Trois omissions constatées, toutes de la même famille.** Les trois modes de channel, absents du domaine (§4.2, 2026-08-22) ; les champs de l'onglet BPM, un sur quatre (§12.1, 2026-08-23) ; l'indicateur Play/Stop de la barre d'onglets (§12.1, 2026-08-23). Chaque fois, la page avait été reconstruite depuis une **conception** et non depuis le **code de dessin de l'original**. La conception n'était pas fausse, elle était incomplète, et rien ne la comparait à la source.
**Conséquence opérationnelle :** un **audit de conformité** écran par écran et champ par champ passe **avant** le reste du travail d'interface. **Fait le 2026-08-23 : ****`docs/original-conformity.md`**. Chaque ligne y porte la ligne du firmware d'origine, pour que la lecture soit vérifiable. Il a produit **six décisions ouvertes** (§16) et sept constats que rien n'avait relevés.
⚠️ **DEUXIÈME RÈGLE, même jour et même propriétaire : le DESIGN des pages originales est conservé**, avec **les glyphes et les polices déjà présentes**. Si une police est trop lourde pour la Flash, **on la redessine nous-mêmes** pour économiser. On ne change pas le design pour contourner un coût.
Cette règle **tranche une question qui était ouverte** : la police unique de FlexSeq (`setFont(u8g2_font_5x7_tr)`, appelé une fois et jamais changé) n'est pas une simplification acceptable. Les dix glyphes maison, conçus et non implémentés, sont donc du **travail** et non plus un arbitrage : \~500 o estimés contre 2646 pour `logisoso26`.
⚠️ **TROISIÈME RÈGLE, même jour : les GESTES de l'original sont conservés**, et l'addition de FlexSeq avec eux. Voir §12.1.
---
## 2. Contraintes hardware
- MCU : **ATmega328P**, 16 MHz.
- RAM : **2048 B** (ressource critique).
- Flash applicative : **30720 B**.
- Hardware Gravity, MIDI Expander, Expansion Header : **inchangés**.
- Aucun remplacement du MCU.
- Toolchain : PlatformIO / Arduino AVR ; `libGravity` résolu depuis le fork du projet, figé au commit `4c5b4d0b4f38…` (ADR 0008). L'ancien pin `9be88be1f4` est obsolète.
>
> **Chemin de flash — vérifié sur les sources le 2026-08-21, puis MESURÉ sur le module le même jour. Il n'existe aucun chemin de briquage par cette voie.** Le schéma KiCad de `GravityHW` place un **Arduino Nano v2.x** sur la carte, et le téléversement passe par le **bootloader USB** du Nano. Il **n'écrit aucun fusible** et ne passe pas par l'ISP — or les fusibles sont le seul moyen de rendre un AVR injoignable. Le firmware d'origine est lui-même un projet Arduino (`.ino`) téléversé de la même façon : flasher FlexSeq est l'opération que le fabricant pratique. La lecture réussie de la flash confirme l'argument au lieu de seulement le raisonner.
>
> ⚠️ **La vitesse : 115200, et non les 57600 du manifeste — corrigé le 2026-08-21 sur mesure.** Le manifeste `nanoatmega328` de PlatformIO donne `protocol: arduino`, 57600 bauds : c'est vrai du manifeste et **faux de ce module**, qui porte un bootloader **optiboot**. La première tentative de sauvegarde est morte sur `stk500_getsync(): not in sync: resp=0x00`, dix essais. À 115200 la carte répond immédiatement : `Device signature = 0x1e950f`. `platformio.ini` surcharge donc `upload_speed = 115200` sur les **trois** environnements téléversables (`nanoatmega328`, `bringup`, `eepromdump`) et sur eux seuls — `simavr` et `wokwi` ne quittent jamais la simulation. Les deux manifestes de carte ne diffèrent **que** par cette vitesse : `maximum_size` vaut 30720 des deux côtés, donc aucun chiffre du §15 n'est touché.
>
> **Le port se nomme ****`/dev/cu.usbserial-*`**** sur macOS** et n'existe que module branché. Le chercher par `ls /dev/cu.*` : un motif de glob qui ne correspond à rien fait avorter la ligne entière sous zsh, avant même l'exécution.
>
> **Directions de broches identiques à l'origine, les dix.** La configuration active de `GravityFW/src/Gravity/Gravity.ino` est **Rev 2+** (le bloc Rev 1 est en commentaire) et correspond broche pour broche à `peripherials.h` de libGravity : encodeur 17/4, switch 14, PLAY 5, SHIFT 12, EXT 2, CV1 A7, CV2 A6, sorties 7/8/10/6/9/11, PULSE 3. Le firmware d'origine ne met en `OUTPUT` que les six sorties et l'horloge ; FlexSeq n'en met pas d'autres, et **ne contient aucun ****`pinMode`**** en propre** — tout passe par libGravity (`DigitalOutput` → OUTPUT, `AnalogInput` → INPUT, `Button` → INPUT_PULLUP). Il n'y a donc pas de conflit de direction possible, seul mécanisme qui pourrait abîmer du silicium.
>
> ⚠️ **Précondition : le module doit être Rev 2+.** La **Rev 1** a un autre brochage (CV sur A2/A1, horloge sur 13, sorties dans un autre ordre) et libGravity ne la vise pas. Elle se reconnaît **à l'œil** : Rev 1 définit `SHIFT_BTN_PIN 100`, c'est-à-dire qu'elle **n'a pas de bouton SHIFT**. Un panneau portant un bouton SHIFT est Rev 2+.
>
> ✅ **Précondition SATISFAITE, confirmée par le propriétaire le 2026-08-21** : son panneau porte un bouton SHIFT, donc le module est **Rev 2+** et le brochage de libGravity est le bon. Cette vérification n'a plus à être refaite.
>
> **État utilisateur : tout en EEPROM, et FlexSeq n'y écrit rien.** Le firmware d'origine y range bpm, mode d'horloge maître, channels, les **16 séquences A1–B8**, la calibration CV et la préférence de rotation. FlexSeq n'a **pas encore** de persistance (§11) donc n'écrit aucun octet d'EEPROM, et un téléversement par bootloader n'y touche pas. Sauvegarder quand même avant le premier flash : « devrait survivre » et « a survécu » ne sont pas la même affirmation.
>
> ✅ **Sauvegarde de la flash : FAITE et VÉRIFIÉE le 2026-08-21.** 32768 octets, table de vecteurs d'interruption authentique en tête, 14,8 % de `0xFF`. Conservée **hors du dépôt** : c'est le firmware GPLv3 du fabricant plus les réglages du propriétaire, les versionner les publierait. La restauration porte sur la **région applicative** ; la région du bootloader ne peut pas être réécrite par le bootloader lui-même, et n'est jamais menacée puisque aucune de nos opérations ne l'atteint. **Non testée** : vérifier qu'on sait restaurer exigerait d'écraser le firmware d'origine.
>
> **Le bootloader est mesuré, plus déduit (2026-08-22).** Lecture de la sauvegarde : les 512 derniers octets (`0x7E00`–`0x7FFF`) portent **506 octets écrits**, et les zones `0x7800`–`0x7DFF` sont **entièrement vides**. C'est donc **optiboot, 512 octets, à ****`0x7E00`** — cohérent avec les 115200 bauds. L'application d'origine s'arrête à `0x6BFF` (27648 o).
>
> ⚠️ **La restauration doit être ROGNÉE à ****`0x0000`****–****`0x6BFF`****.** Le bootloader est intact et c'est lui qui écrit : lui envoyer ses propres pages ferait échouer la vérification. Le HEX de sauvegarde couvrant `0x0000`–`0x7FFF`, il ne peut pas être restauré tel quel.
>
> ⚠️ **Levier Flash possible, NON vÉRIFIÉ.** Si le bootloader n'occupe que 512 octets, l'espace applicatif vaut **32256** octets et non les **30720** que déclare le manifeste PlatformIO (32768 − 2048), soit **1536 octets de plus** — ce qui compterait vu la tension du budget au §12.1. Mais ce qui est mesuré est le *contenu*, pas l'espace réservé : celui-là dépend du fusible `BOOTSZ`, et le bootloader ne sait pas lire les fusibles (ils sortent à `0x0`). Lire `BOOTSZ` exigerait un programmateur ISP. **Les 30720 restent donc l'hypothèse sûre et ne sont pas touchés.**
>
> ⚠️ **Le bootloader ne sait PAS lire l'EEPROM, et il ne le dit pas.** optiboot compile ce support hors du binaire ; `avrdude -U eeprom:r:` rend alors du **contenu de flash** sans signaler d'erreur. Le dump obtenu paraissait crédible — 1024 octets, 1,2 % de `0xFF` — et s'est révélé **identique octet pour octet aux 1024 premiers octets de la flash (1024/1024)**. Le critère d'une sauvegarde n'est donc ni sa taille ni son entropie, mais que les octets soient ceux qu'on a demandés.
>
> **`env:eepromdump`**** — le firmware lit son EEPROM lui-même.** `src/eeprom_dump_main.cpp` lit les 1024 octets et les émet en **Intel HEX** sur le port série à **9600 bauds** (vitesse choisie pour que `cat` fonctionne sans configurer le port sous macOS). Aucune dépendance : ni libGravity, ni `NeoHWSerial`, qui remplacerait l'ISR du port série. Lecture seule, aucune écriture d'EEPROM. L'algorithme de somme de contrôle est validé contre la sortie d'avrdude elle-même : **1025/1025 enregistrements identiques**. Le dump est émis **trois fois**. Coût mesuré : **RAM 216 o, Flash 1844 o**.
>
> ✅ **Sauvegarde EEPROM : FAITE et VÉRIFIÉE le 2026-08-21**, par `tools/run-eeprom-dump.sh` — qui téléverse, capture, vérifie, **puis** écrit, dans cet ordre. **Trois critères bloquants, et un critère non évaluable compte comme un échec** plutöt que de passer en silence : sommes de contrôle toutes valides et 1024 octets couverts · capture **différente** des 1024 premiers octets de la flash · deux dumps consécutifs identiques. Rien n'est écrit si l'un échoue, et une sauvegarde existante n'est jamais écrasée sans `FORCE=1`. Mesuré : 3 dumps, 64 enregistrements, **0 somme invalide**, **40/1024** identiques à la flash, 68,1 % de cellules jamais écrites. Rejoué depuis zéro, le script reproduit la sauvegarde **octet pour octet**. Chemins rouges exercés : fenêtre trop courte, référence de flash absente, sauvegarde déjà présente.
>
> **Le contenu est identifié, pas seulement plausible.** `saveState()` du firmware d'origine (`GravityFW/src/Gravity/Gravity.ino`) écrit le **bpm à l'adresse 0** : on y lit **120**. Suivent `bpmModulationChannel`, `bpmModulationRange`, `masterClockMode`, `channels`, puis les 16 séquences `seqA1`…`seqB8`. C'est par cette disposition que la **calibration CV réelle** sera récupérable pour le §10.
>
> ⚠️ **Trois pièges de l'outillage, tous découverts en s'en servant, tous la même faute : l'outil dépendait d'un état qu'il ne contrôlait pas.** (1) Le dump ne sortait qu'une fois, donc la capture lisait le retard du tampon du port et commençait au milieu d'un enregistrement — d'où les trois exemplaires, dont le critère de stabilité a besoin de toute façon. (2) `stty` **ne survit pas** à la fermeture du descripteur qu'il configure : première capture propre, suivantes brouillées à une vitesse que personne n'avait posée — le port est désormais configuré par `termios` explicitement. (3) Un téléversement **ne peut pas se synchroniser tant que la carte émet** : avrdude lit les octets périmés comme réponse et annonce `not in sync`. Vider le tampon **ne suffit pas**, les octets arrivant *entre* le vidage et la synchronisation ; le script **attend le silence** (2 s d'inactivité, plafond 45 s). À retenir au-delà de cet outil : ouvrir un `/dev/cu.*` **réinitialise la carte**, donc toute mesure sur le port série commence par un redémarrage.
>
> **Voies ISP écartées, avec leur raison.** Un USBasp (5 à 10 €) ou un Arduino en « Arduino as ISP » liraient l'EEPROM, mais le bus ISP occupe D11/D12/D13 — or le brochage Rev 2+ met une **sortie de trigger sur D11** et le **bouton SHIFT sur D12**. Le bus de programmation porterait donc un driver de sortie et une résistance de tirage : cela marche souvent, échoue parfois, et se diagnostique mal. La voie firmware n'a pas ce problème.
>
> ⚠️ **Conséquence pour le §10 : la calibration CV réelle est hors d'atteinte pour l'instant.** libGravity n'a **aucune** notion d'EEPROM — vérifié sur la source du commit épinglé. `CvSampler` est donc configuré depuis les défauts de libGravity, `CALIBRATED_LOW = -566` et `CALIBRATED_HIGH = 512`, et non depuis la calibration de ce module, qui n'existe que dans l'EEPROM et dans une disposition connue du seul firmware d'origine. Suivi : `docs/open-risks.md` ligne 17.
---
## 3. Base logicielle & architecture
Le firmware est développé **à partir de ****`libGravity`**, pas en repartant du firmware Sitka original (qui reste une référence de comportement).
```javascript
libGravity (hardware + clock)
   ↓
Hardware Integration / Adapters
   ↓
FlexSeq Domain
   ├── Pattern            (contenu : 36 steps + 1 ratchet par step)
   ├── PatternBank        (16 patterns partagés)
   ├── SequencerEngine    (masterPhase, état par channel)
   ├── Transport          (clock/MIDI → moteur)
   ├── TriggerSequencer   (onset + step actif → trigger)
   ├── Musical Grid        (SUBDIV ; séparation de mesure graphique)
   ├── CV Mapping / Reset
   ├── UI Logic
   └── Persistence API
```
Le domaine est **testable sans hardware** (tests natifs + simulateur). La conversion horloge → progression logique appartient à **Transport**, pas au Sequencer Engine.
---
## 4. Fonctionnalités conservées
6 Multi-Mode Channels · Clock Mode · Random Skip Mode · 2 CV Inputs · External Clock · BPM interne · MIDI via MIDI Expander · Expansion/Connectivity · Settings · Calibration CV. Le MIDI Expander et l'Expansion Header ne sont pas modifiés.
### 4.2 Les modes de channel — OMISSION CORRIGÉE le 2026-08-22, PORTÉE À QUATRE le 2026-08-23
⚠️ **Cette section décrit trois modes. Il y en a quatre depuis le 2026-08-23 : ****`GATE`**** est reprise de ****`1.2-dev`****, et ****`SWING`**** devient un paramètre de ****`SEQ`**** et non un mode. Voir §5.0.**
⚠️ **FlexSeq n'implémentait aucun mode.** Constaté par le propriétaire **sur le module** : l'écran n'affichait que `SEQ`. Ce n'était pas un écart de mise en page — les trois modes n'existaient pas du tout dans le domaine, `UiController` supposant en silence que les six channels sont des séquenceurs. Le §4 les listait pourtant. **Omission, pas décision.**
Contrainte posée par le propriétaire : **garder toutes les fonctionnalités d'origine, ne faire évoluer que ****`SEQ`****.**
**Faits lus dans le firmware d'origine** (`GravityFW/src/Gravity/`, source #4) :
- `byte mode; // 0 CLK, 1 RND, 2 SEQ`, et le **défaut d'usine est ****`CLOCK`** pour les six channels, `subDiv` à l'unité ;
- **`OFFSET`** : déclenche quand `channelPulseCount == offset` — un décalage de phase **en pulses** dans le cycle du channel, affiché `offset/pulsesParStep` ;
- **`SKIP CHANCE`** : `random(10)+1 > randAmount`. ⚠ **Deux bornes distinctes, relevées le 2026-08-23** : l'interface d'origine écrête la valeur **stockée** à **9** (`Interactions.ino:157-161`), donc 0 % à 90 % ; la **génération** écrête `random + randMod` à **10** (`Gravity.ino:479-483`), et seule la modulation CV atteint donc 100 %. FlexSeq laissait régler 10 directement : **le propriétaire a tranché pour le plafond à 9** (§16), la valeur effective gardant 10 par le CV ;
- **`SEQ`**** joue des 1/16 en dur**, et l'écran SEQ d'origine n'expose **aucun** SUBDIV. L'exposer est une divergence FlexSeq assumée (§6.1).
**Décidé par le propriétaire le 2026-08-22** : les trois modes reçoivent **champ ET comportement** dès maintenant, pas un champ inerte. `CLOCK` = un trigger par step à la position `offset` · `RANDOM` = un trigger à la position 0, sauté avec la probabilité `skipChance/10` · `SEQ` = le comportement actuel. Le générateur pseudo-aléatoire prend une **graine fixe** : l'original ne sème pas, donc son comportement est reproductible d'un démarrage à l'autre.
État : **implémenté le 2026-08-23**, en C++ et en TypeScript (lot 9). Ce qui manque est l'accès : aucun champ `MODE` dans l'interface avant le lot 11, donc un module flashé aujourd'hui joue **six horloges et aucun pattern**. Le domaine est juste, l'interface n'y donne pas encore accès.
Trois faits établis à l'implémentation, et qui font partie du comportement du module :
- **Seul ****`SEQ`**** lit la banque.** `CLOCK` et `RANDOM` ignorent le contenu du pattern **et ses ratchets** : dans l'original le chemin du séquenceur est une branche séparée de celle de l'horloge. Un ratchet posé sur un channel en `CLOCK` est donc inerte, et le redevient actif en repassant en `SEQ`.
- **À ****`offset = 0`****, le déclenchement EST la frontière de step.** La position 0 et la frontière sont le même instant : les traiter séparément émettrait deux déclenchements par step. La frontière l'émet dans ce cas, le franchissement de l'offset dans tous les autres — ce qui laisse aussi un `advance()` groupé compter chaque franchissement qu'il enjambe.
- **L'offset est écrêté à ****`ticksPerStep - 1`**, y compris quand la cadence change, comme le fait l'original. Un offset **exactement égal** à la durée du step ne serait jamais atteint : le channel serait muet.
Le tirage de `RANDOM` est consommé **une fois par onset**, dans `TriggerSequencer::update()` appelé juste après `advance()`. Le générateur est un xorshift 16 bits à graine fixe, et un **vecteur témoin de cinq valeurs** est asserté des deux côtés pour que C++ et TypeScript ne divergent pas en silence.
Coût AVR mesuré : RAM +32 o, Flash +310 o. Découpage de la suite dans `WORKPLAN.md` (lots 10 à 14).
### 4.1 Préférences persistées de l'original — à reprendre (constaté le 2026-08-22)
L'écran de réglages du firmware d'origine (`Interactions.ino`, `displayScreen == 2`) expose **trois** préférences, toutes sauvegardées en EEPROM par `saveState()`. FlexSeq doit les reprendre plutôt que de les figer, sans quoi un module dont le propriétaire en a changé une se comporte différemment sous FlexSeq :
- **`rotateScreen`** — logique inversée : `false` donne `U8G2_R2`, `true` donne `U8G2_R0`. FlexSeq code `U8G2_R2` **en dur**. Compatible tant que la préférence est à `false` ; un module basculé afficherait FlexSeq tête en bas. **À exposer dans l'UI (§12).**
- **`reverseEnc`** — sens de rotation de l'encodeur. **Ce n'est pas un défaut de libGravity** : la direction dépend du câblage des deux broches, et les deux bibliothèques exposent chacune le réglage (`Encoder::SetReverseDirection(bool)` côté libGravity, `false` par défaut et jamais appelé). Leurs défauts sont **opposés** : constaté sur le module, FlexSeq décrémente quand on tourne à droite là où l'original incrémente. FlexSeq doit donc appeler `SetReverseDirection(true)` **pour se comporter comme l'original par défaut**, et exposer le réglage. Suivi : `docs/open-risks.md` ligne 21.
- **`calibrateCVs()`** — le « Calibration CV » ci-dessus. Nécessaire bien que le convertisseur soit numérique : l'étage analogique d'entrée (diviseur, décalage, tolérances des résistances) fait que 0 V au jack ne tombe pas au milieu de l'échelle. Mesuré sur le module le 2026-08-22 : FlexSeq lit **-27 / -28** au repos, rien branché, soit \~0,27 V d'erreur systématique et cohérente entre les deux voies. L'original stocke un `uint16` par voie ; libGravity modélise autrement (`low` / `high` / `offset`), donc les deux formats ne se transposent pas directement.
**Disposition EEPROM de l'original, établie le 2026-08-22** par lecture de `saveState()` et vérifiée sur un dump réel : `bpm` (1 o, adresse 0) · `bpmModulationChannel` (1) · `bpmModulationRange` (1) · `masterClockMode` (1) · `channels[6]` (struct de 9 octets, soit 54, adresse 4) · `seqA1`…`seqB8` (16 × `bool[16]` = 256, adresse 58) · `CV1Calibration` et `CV2Calibration` (`uint16` chacun, adresses 314 et 316) · `rotateScreen` (318) · `extClockPPQN` (319) · `reverseEnc` (320). L'adresse **1023 porte ****`memCode`**, que `loadState()` compare à `'D'` avant d'accepter le contenu : c'est la garde de version de l'original, et elle sert de **preuve de décodage** — une disposition mal reconstruite ne la fait pas tomber juste. Cette disposition rend les 16 patterns de l'original **importables** si le §11 le décide ; ce serait une décision produit, elle n'est pas prise.
---
## 5. Trigger Sequencer — évolution principale
### 5.0 REVUE DE LA VERSION DE RÉFÉRENCE — DÉCIDÉ le 2026-08-23
> **Cette section supersède cinq décisions antérieures.** Elles restent écrites là où elles étaient, marquées, pour que le raisonnement reste lisible.
**La référence comportementale est ****`main`**** @ ****`40d4aac`** (2026-03-10, « clean up and going public »), le firmware public. La branche **`1.2-dev`**** @ ****`f7b2150acf`** (2025-03-11) est **plus ancienne et jamais fusionnée** : elle sert de **catalogue de features candidates**, sans valeur normative. Inventaire vérifié ligne par ligne dans `docs/original-1.2-dev-features.md`.
**1. Patterns : modèle template / instance.** Les 16 patterns sont des **templates stockés en EEPROM**. Un channel en `SEQ` charge un template, et travaille ensuite sur une **copie locale en RAM**. Éditer cette copie n'affecte ni le template ni les autres channels. Recharger le template écrase la copie.
- **A1–A8 sont figés** : contenu d'usine, édition refusée par la règle `index < 8`, sans octet supplémentaire.
- **B1–B8 sont libres** : vides au départ, éditables.
- **Le contenu d'usine est celui de l'original, et il est en place depuis le 2026-08-24.** Les huit motifs viennent de `Gravity.ino:83-90` : contenu sur les steps 0 à 15, silence au-delà. Ils vivent dans une table PROGMEM de huit masques de 16 bits, semés quand la persistance ne trouve pas d'image valide — premier démarrage ou format inconnu. Le lot B sèmera les templates EEPROM depuis la même table. **Sans ce contenu, la règle ****`index < 8`**** gelait huit emplacements vides** : elle retirait la moitié de la banque et ne donnait rien. Coût mesuré : RAM +0 o — la table est en PROGMEM, la banque reste en `.bss` — et Flash +78 o. Vérifié par trois dérivations indépendantes : listes de steps littérales dans les assertions, masques calculés à la main, puis décodage de la table comparé au `.ino`. 8/8 identiques, B1–B8 confirmés vides. Les masques sont posés par setLowStepMask(), une écriture en bloc des 16 premiers steps : 16 appels à writeStep par pattern coûtaient 104 octets de Flash de plus. Mesuré le 2026-08-24. Le miroir TypeScript porte la même surface, pour que les deux langages gardent le même contrat.
- **Conséquence mémoire, chiffres du 2026-08-26.** La banque résidente de 16 patterns disparaît de la RAM. Elle pèse **368 octets** (16 × 23) et six instances en pèsent **138** (6 × 23), donc l'arithmétique donne **230 octets de différence théorique**. ⚠️ **230 est le net théorique de la coexistence, et non le gain mesuré du lot de retrait.** Mesuré le 2026-08-30 : B4b.7 rend **370 o** contre le firmware qui y entrait, à 1708 o. Les 138 o d'instances étaient déjà payés depuis B4b.3, et le champ pointeur `bank_` ajoute 2 o. ⚠️ **Le chiffre de 230 n'est pas une mesure de RAM libérée.** Le critère reste la **mesure AVR**, et elle n'est pas encore possible : le codec de la version 3 était déclaré sans être appelé jusqu'au 2026-08-28, donc le linker l'éliminait entièrement du binaire — dérive RAM et Flash mesurée à **0 / 0** le 2026-08-26. **Le chiffre réel a été relevé à l'activation, le 2026-08-28 : Flash +2 octets.** Le linker a supprimé l'image de la version 2 en échange — `avr-nm` trouve `PersistentImageV3` et plus aucun symbole `PersistentImage::`. Le firmware ne porte donc qu'**une** implémentation d'image, ce qui explique un coût net presque nul. Côté EEPROM les chiffres sont exacts et calculables : **384 octets de modèles** et **138 octets d'instances**, soit 522 octets pour les patterns dans une image de 588. Voir §11.1. ⚠️ **Les valeurs « \~144 octets rendus » et « 256 octets consommés » écrites ici jusqu'au 2026-08-26 dataient de la version à 32 pas** et d'un modèle sans longueur stockée.
**2. Pattern : 36 steps, un nibble par ratchet.** 5 octets de steps, 18 octets de ratchets, `sizeof(Pattern) == 23`.
⚠️ **Distinction à ne pas confondre, décidée le 2026-08-23, chiffres remis à jour le 2026-08-26.** La longueur n'est **pas** dans la structure `Pattern` en RAM — le channel porte déjà son `effectiveLength`. Elle est dans l'**enregistrement EEPROM du modèle**, qui fait donc **24 octets** : 23 de contenu plus 1 de longueur. `sizeof(Pattern)` reste à **23**.
⚠️ **Les valeurs 21 et 20 écrites ici jusqu'au 2026-08-26 dataient de la version à 32 pas.** La distinction, elle, ne change pas : la longueur est un fait de **stockage**, pas de contenu. C'est aussi pourquoi le **record d'instance** ne la porte pas — 23 octets, le contenu seul — puisque le channel qui joue cette instance porte déjà sa longueur effective.
Le nibble a été préféré à un champ de 3 bits **après** que le modèle template a sorti la banque de la RAM : il ne reste que 6 instances résidentes au lieu de 16 patterns, donc les 4 octets de plus par pattern coûtent 24 octets de RAM sur 321 disponibles, là où le champ compact aurait coûté 50 à 150 octets de Flash sur 534. **La Flash est la contrainte, pas la RAM.** ADR 0007.
**3. LENGTH : par channel, et le modèle la stocke.** Charger un modèle donne au channel **la longueur enregistrée dans le modèle**. Elle est ensuite éditable dans le channel, et propre à lui.
La **déduction** — dernier pas non vide plus un — ne sert plus qu'**une fois** : quand un slot vide reçoit un contenu pour la première fois.
**Pourquoi le modèle la stocke à présent.** Il ne la stockait pas, pour économiser de la RAM, du temps où la banque était résidente. Les modèles vivant en EEPROM, une longueur coûte **0 octet de RAM** et 16 octets d'EEPROM. Et sans elle, la feature d'enregistrement du §5.0 point 10 **perdrait la longueur à chaque aller-retour** : un channel de longueur 20 dont les pas actifs s'arrêtent au 5e reviendrait à 5.
Trois états de pas, et un seul bit stocké :
<table header-row="true">
<tr>
<td>Rendu</td>
<td>État</td>
<td>Compte dans LENGTH</td>
<td>Stockage</td>
</tr>
<tr>
<td>carré plein</td>
<td>actif</td>
<td>oui</td>
<td>bit à 1</td>
</tr>
<tr>
<td>carré creux</td>
<td>inactif, un **soupir**</td>
<td>oui</td>
<td>bit à 0</td>
</tr>
<tr>
<td>`  • `</td>
<td>absent du pattern</td>
<td>**non**</td>
<td>rien : c'est `index >= length`</td>
</tr>
</table>
Un `•` est toujours une **queue**, jamais un trou au milieu.
**4. Modes de channel : quatre, pas trois.** `CLOCK`, `RAND`, `SEQ`, et **`GATE`** reprise de `1.2-dev`. GATE maintient la sortie haute sur un **pourcentage du pas** au lieu d'émettre une impulsion fixe : c'est le seul mode dont la sortie n'est pas un trigger de largeur constante.
**SWING n'est pas un mode.** C'est un **paramètre de ****`SEQ`** : retard des pas d'index **impair**, de **0 à 49 %** de la durée du pas, **plafonné à la résolution réelle** du pas courant — comme `ratchetFitsStep()` refuse déjà les ratchets injouables. ⚠️ **Divergence délibérée** : le mode SWING de `1.2-dev` disparaît, son effet reste, appliqué au pattern et à la cadence du channel plutôt qu'à des doubles-croches fixes.
**Écarté : le 7e channel** de `1.2-dev`, qui transforme la sortie d'horloge en channel. FlexSeq réserve cette sortie à l'expandeur.
**5. Gestes.** `SHIFT` + `PLAY` **mute le channel** (feature de `1.2-dev`). **RECORDING passe sur ****`SHIFT`**** + appui court**, qui était libre. Ce choix écarte `PLAY` appui long, dont le handler part **au relâchement** : un `PLAY` maintenu 750 ms aurait armé RECORDING au lieu de démarrer le transport.
**6. Barre d'onglets : neuf onglets et un indicateur.** `horloge · 1 2 3 4 5 6 · PATTERNS · CONF`, puis l'**indicateur de transport ferré à droite**. Emplacements de 12,9 px au lieu de 16.
- l'indicateur est **fixe** : ▶ en marche, ■ à l'arrêt. Pas de clignotement, pour ne pas donner à l'écran principal un élément variant dans le temps — c'est ce qui lui permet de ne presque jamais redessiner ;
- ⚠️ le déclencheur de redessin doit surveiller l'**état de marche**, pas seulement le compteur de révision : un **MIDI Start** ne passe par aucun geste, et l'indicateur mentirait ;
- le glyphe de **CONF devient une roue crantée 7×7**. Le carré plein actuel se lit comme un indicateur stop, ce que le propriétaire a vérifié sur le module ; le laisser à deux emplacements d'un vrai indicateur réintroduirait l'ambiguïté, côte à côte ;
- le glyphe de **PATTERNS est une mini-grille de steps**, deux lignes de trois points en 7×5 px.
**7. Tempo aligné sur l'original, impulsion conservée.** `MIN_TEMPO` passe de 30 à **20**, `MAX_TEMPO` de 300 à **200**. L'impulsion **reste à 5 ms**.
L'original a 20–200 **et** 12 ms. On prend sa plage, pas sa largeur, et le calcul le justifie : à 200 BPM en `x24`, un pas dure 12,5 ms, donc une impulsion de 12 ms en occuperait **96 %** — un signal carré, pas un trigger. Avec un triolet à cette cadence, les sous-déclenchements sont à 2,8 ms les uns des autres et 12 ms les recouvrirait tous. L'original peut se le permettre : il tourne à 24 PPQN et n'a pas de ratchets.
Gain annexe, depuis que l'accélération de l'encodeur a disparu : **180 crans** pour traverser la plage au lieu de 270.
**8. Grille de 36 steps à l'écran.** 3 lignes de 12 pleines, donc 36 emplacements et aucun mort. Le pas de colonne et **la taille des glyphes ne changent pas** — la lisibilité passe avant. Le vertical se gagne en remontant la ligne de titre et en réduisant l'espace entre le glyphe et son chiffre de ratchet, et **le pied disparaît de l'écran EDIT**, comme dans l'original.
**9. Onglet PATTERNS.** L'édition des templates vit dans son propre onglet, avec audition par le **CHAN 1**. Elle **réutilise ****`LEVEL_EDIT`** — aucun quatrième niveau d'interface — avec une autre source de pattern et un autre titre.
**10. La copie va dans les deux sens.** Depuis l'onglet PATTERNS, un slot **occupé** se **charge** dans un channel, et un slot **vide** reçoit le pattern d'un channel. C'est le même écran et la même sélection, donc **aucun geste nouveau** — le budget des neuf gestes est plein.
- un slot **occupé** peut être écrasé, après une confirmation à l'écran (**décidé le 2026-08-26**). « Vide » se calcule : les 36 cases inactives ;
- **A1–A8 refusent** l'écriture, comme partout ailleurs ;
- l'écriture fait 24 octets, soit environ 82 ms étalées par l'ordonnanceur de persistance.
### 5.1 Banque de patterns partagée
> **Décision validée.** Il existe **une banque unique de 16 patterns partagés** (A1–A8, B1–B8). Chaque channel possède un **sélecteur** `selectedPattern` (0–15). ⚠️ **SUPERSÉDÉ le 2026-08-23, voir §5.0.** Le modèle devient **template / instance** : les 16 patterns sont des **templates stockés en EEPROM**, et chaque channel travaille sur une **copie locale en RAM**. Éditer le pattern d'un channel n'affecte donc **aucun** autre channel.
Ceci reprend le modèle du firmware Sitka original (16 tableaux partagés `seqA1..seqB8` + `channel.seqPattern`). Le modèle « 16 patterns privés par channel » est **abandonné** (divergeait de la référence et coûtait \~560 B de RAM).
**Contenu d'un Pattern :** **36** steps binaires + **un code de ratchet par step, un nibble chacun** (décidé le 2026-08-23, `sizeof(Pattern) == 23` octets depuis le 2026-08-26, ADR 0007). **Aucune longueur** dans le Pattern, et **aucune séparation de mesure** (aide de lecture par channel).
```javascript
CHANNEL_COUNT = 6
PATTERN_COUNT = 16     (banque partagée)
STEP_COUNT    = 32     (grille)
```
### 5.2 LENGTH — par channel
> **Décision validée.** La **LENGTH est un état d'exécution par channel**, pas une propriété du Pattern. Deux channels jouant le même pattern peuvent avoir des longueurs différentes (polyrythmie à partir d'un contenu commun).
- `1 ≤ effectiveLength ≤ MAX_LENGTH` par channel — **`MAX_LENGTH`**** vaut 24 jusqu'au lot F, puis 36**, la capacité du `Pattern`. ⚠️ La capacité de stockage et le plafond d'interface sont **deux quantités distinctes** : 36 est ce qu'un pattern peut contenir, 24 est ce que l'utilisateur peut régler aujourd'hui. Longueur **déduite du template au chargement** (dernier pas non vide) — ⚠️ **et cette déduction ne sert plus qu'à un slot VIDE qui reçoit un contenu pour la première fois.** Partout ailleurs la longueur vient de l'enregistrement du modèle, qui la STOCKE : 24 octets, 23 de contenu plus 1 de longueur. Voir le §5.0 point 3, qui fait foi. Elle est ensuite éditable dans le channel.
- Une modulation CV peut produire une `effectiveLength` temporaire **dans les deux sens** — raccourcir **ou** allonger — clampée à `[1, MAX_LENGTH]`, soit `[1, 24]` aujourd'hui et `[1, 36]` au lot F (**décidé 2026-08-20, borne liée au plafond d'interface le 2026-08-28**). La valeur réglée à l'écran et persistée est la **base** ; le CV ne l'écrase jamais. Voir §10.
- Réduire la longueur **ne détruit pas** le contenu du pattern.
- Changer la longueur **ne réinitialise pas** `masterPhase` et **ne fait pas sauter** le playhead (voir §7).
### 5.3 Sélection de pattern
Chaque channel sélectionne l'un des 16 patterns. Le CV peut cibler la sélection de pattern selon le mapping retenu.
### 5.4 Reset
Le **Reset global** relève de l'entrée d'horloge externe (voir Transport §8), **pas** d'une destination CV. Le CV peut en revanche cibler un **Reset par channel** (**décidé 2026-08-20**) : voir §10.
### 5.5 RECORDING — à concevoir
> **Fonction prévue, non prioritaire — inscrite le 2026-08-20.** Le firmware Sitka d'origine possède un mode RECORDING avec enregistrement au rythme, que FlexSeq reprendra. La conception détaillée (contrôles, écran) reste à faire ; les points ci-dessous sont **déjà tranchés** et n'ont pas à être re-débattus.
**Référence, extraite de ****`GravityFW`** (`src/Gravity/Interactions.ino`, `Gravity.ino`) : l'entrée en mode **force la lecture** (`isPlaying = true` — on n'enregistre pas à l'arrêt) ; SHIFT y joue un **double rôle**, déclenchement live immédiat de la sortie du channel affiché *et* écriture dans le pattern ; la quantification décale le coup sur le step suivant via `recordToNextStep`.
**Acquis de conception :**
- **Grille de quantification : le step du channel enregistré**, et non une grille globale de 1/16 comme dans l'original — un step FlexSeq est une unité de temps dont la durée est le `ticksPerStep` du channel. Seuil aux **2/3 de ****`stepTicks`**, valeur que le moteur met déjà en cache à chaque frontière. Se généralise à un step étiré par un TRIOLET.
- **Partage 2/3 – 1/3 conservé.** L'original, à sa résolution de `PPQN 24`, place le seuil à 4 impulsions sur 6. Il tolère largement le **retard** et n'anticipe que sur le dernier tiers, ce qui correspond à la façon dont on joue réellement : à 120 BPM sur un step de 125 ms, \~83 ms de retard toléré contre \~62 pour un partage symétrique.
- **On n'écrit que des 1.** Pas d'enregistrement de ratchet — écarté explicitement : impraticable à BPM rapide.
- **Effacement à l'identique de l'original** : appui long qui vide le pattern ; pas d'effacement en direct.
- **Déclenchement live à l'APPUI**, pas au relâchement. Contrainte : les callbacks de `Button` de libGravity se déclenchent au **relâchement** (nécessaire pour distinguer court et long) — il faut donc lire l'**état** du bouton, pas son callback. L'original contourne de même, par `digitalRead` direct.
- **Persistance différée** (§11).
**Reste à concevoir : l'affectation des contrôles physiques.** Inventaire au commit épinglé : `shift_button` et `play_button` (appui court + appui long à 750 ms), `encoder` (rotation, appui, rotation-pendant-appui — **pas** d'appui long : c'est le commit amont `5c0c34f` qui l'ajoute, et nous ne l'avons pas). À noter aussi l'anomalie auditée de `Button`, qui perd un relâchement survenant dans la fenêtre d'anti-rebond.
---
## 6. Musical Grid — statut
### 6.1 SUBDIV — validé (convention libGravity officielle, 96 PPQN)
SUBDIV **divise ou multiplie le BPM global** — c'est le **seul** paramètre qui détermine la vitesse d'un step. Elle ne consulte jamais la séparation de mesure (purement graphique, §6.2). Référence rythmique du pas, **par channel**. Alignement sur libGravity (`firmware/Gravity/channel.h` — `CLOCK_MOD` / `CLOCK_MOD_PULSES`) : l'unité `/1` est la **noire** (96 ticks). Affichage façon Gravity original : `/N` = **division** (plus lent), `xN` = **multiplication** (plus rapide).
Mapping (validé, testé, reproduit exactement `CLOCK_MOD_PULSES`) :
`ticksPerStep = subdiv > 0 ? 96 × subdiv : 96 / |subdiv|`
- `/1` → 96 ticks (noire — **défaut par channel** depuis 2026-08-17 ; aligne le channel Sitka d'origine dont le subDiv défaut = unité) · `/2` → 192 · … · `/128` → 12288.
- `x2` → 48 · `x4` → 24 (1/16, pas historique du séquenceur — plus le défaut) · `x8` → 12 · … · `x24` → 4.
Chaque channel a sa propre SUBDIV. La liste exposée suit celle de libGravity (25 valeurs : `x24 … x2`, `/1`, `/2 … /128`) et **remplace** la liste Sitka historique (20 valeurs).
⚠️ **L'ORDRE de cette liste est normatif, précisé le 2026-08-22** — parce que le §11.1 persiste l'**index** et non la valeur. L'ordre retenu est celui écrit ci-dessus, **du plus rapide au plus lent**, donc `x24` à l'index 0 et `/1` à l'**index 8**. C'est l'ordre du modèle de référence TypeScript (`sim/src/domain/subdiv.ts`), et le C++ y a été aligné (`include/flexseq/Subdiv.h`, `src/domain/Subdiv.cpp`). Ce n'est **pas** l'ordre de la table brute de libGravity (`CLOCK_MOD`), qui est l'inverse — du plus lent au plus rapide, unité à l'index 16. L'**ensemble** des 25 valeurs est identique dans les deux cas ; seul l'ordre diffère, et c'est lui qui compte dès qu'un index est écrit en EEPROM. La table est en `PROGMEM` sur AVR : **0 o de RAM**.
### 6.1.1 Moment d'application d'un changement de SUBDIV — DÉCIDÉ le 2026-08-23
> **Décision validée.** Un changement de SUBDIV prend effet **au prochain temps** quand le transport joue. À l'arrêt, ou si la phase maître est déjà sur un temps, il prend effet **immédiatement**. La valeur **choisie** est visible tout de suite : l'écran et l'EEPROM portent le choix sans attendre.
**Le défaut constaté sur le module.** Deux channels réglés sur `/1` ne tombaient plus ensemble après que l'un soit passé par d'autres cadences puis revenu à `/1`. Mesuré sur le modèle de référence, après retour à `/1` : **0 tick** par une division, **48 ticks** par `x2` ou `x4`, **64 ticks** par `x3`. À 120 BPM, 48 ticks font 250 ms et 64 ticks font 333 ms. Le décalage est audible et permanent.
**La cause.** Une division dure un multiple de 96 ticks, donc ses onsets restent sur la grille de la noire. Une multiplication dure `96 / n` ticks et place ses onsets entre les temps. FlexSeq appliquait la cadence immédiatement, en milieu de step, conservait l'accumulateur, et ne le repliait que s'il dépassait la nouvelle durée. La phase n'était jamais re-dérivée de `masterPhase`, donc une phase hors grille survivait indéfiniment.
**Ce que fait le firmware d'origine**, trouvé à deux endroits qui se répondent : `Interactions.ino:141` et `:275` n'appliquent la cadence tout de suite que si le transport est arrêté (`if (!isPlaying) { calculateCycles(); }`), et `Gravity.ino:454` la pose sur le temps, sous `if (pulseCount == 0)`, avec le commentaire de l'auteur — *switching modes on the beat and resetting channel clock*. L'original définit `PPQN 24` (`Gravity.ino:13`), donc `pulseCount` compte 0 à **23** par noire et ce test **est** le temps. FlexSeq tourne à 96 PPQN, donc son temps à lui est `masterPhase % 96 == 0`.
**Ce que FlexSeq fait de plus, volontairement.** L'alignement est **exact** : au moment d'appliquer, la phase est re-dérivée du temps. L'original, lui, remet son compteur au pulse *suivant* le temps et garde 1 tick de décalage résiduel, soit 5,2 ms à 120 BPM. La re-dérivation rend aussi le correctif robuste au drainage : `advance()` peut recevoir plusieurs ticks d'un coup quand la boucle a été bloquée, donc un temps peut être franchi sans être observé exactement.
**La conséquence est assumée** : la cadence prend effet jusqu'à un temps plus tard, soit 500 ms à 120 BPM. C'est le comportement de l'original, et le §1 le conserve.
**L'alignement est sur le temps, pas sur l'origine globale.** Deux channels réglés sur la même division à des temps différents peuvent encore différer d'un temps entier. L'original ne réaligne pas une division non plus, donc c'est fidèle — et bien moins cher : aligner sur l'origine demande `masterPhase % stepTicks`, un modulo 32 bits.
**Portée.** La même règle couvre `setTicksPerStep()`. Un reset global applique une cadence en attente au lieu de la perdre. **Coût : 12 o de RAM** (2 par channel) et **278 o de Flash**.
⚠️ **Cinq autres chemins ont été mesurés avant d'être assertés, et aucun ne décale un channel** : une édition de LENGTH, la sélection d'un autre pattern, un changement de mode, l'édition d'un ratchet sur le step courant, et un aller-retour de `setTicksPerStep`. **Le triolet non plus**, contre une première lecture qui le soupçonnait : un step de triolet dure exactement deux fois `ticksPerStep` sur les 25 cadences, donc il reste sur la sous-grille du channel, qui divise 96.
Mécanisme et alternatives écartées : **ADR 0004**. 14 assertions par langage, 10 mutants.
### 6.2 Séparation de mesure — GRAPHIQUE uniquement
> **Révision complète (2026-08-17). Remplace intégralement METER / MEASURES**, qui sont **supprimés** du domaine (signature rythmique, numérateur/dénominateur, `ticksPerMeasure`, MEASURES dérivé, temps composés : tout le module est retiré).
>
> **Principe : UN STEP EST UNE UNITÉ DE TEMPS**, quelle que soit la « valeur de note » qu'on lui prête. Le BPM et la **SUBDIV** du channel font seuls le travail temporel. La séparation de mesure n'est donc qu'une **aide de lecture** : elle n'a **aucun** effet sur la durée des steps ni sur la subdivision.
>
> **Paramètre :** une barre tous les **N steps**, `N ∈ {aucune, 2, 3, 4, 6}` — **par channel**, défaut `4`. Seules ces valeurs sont autorisées car elles divisent une ligne de 12 sans reste : une barre ne tombe donc jamais à cheval sur un retour à la ligne. Libellés affichés à la musicale (`2/4 · 3/4 · 4/4 · 6/4`).
>
> **Rendu :** fine verticale dans la gouttière entre deux colonnes, jamais en bord de ligne, **sans ajouter de position**.
### 6.3 RATCHETS — remplace les groupes ternaires
> **Révision complète (2026-08-17), implémentée, testée et mesurée.** Le modèle « groupe ternaire = 3 steps consécutifs » (départ ≤ 21, sans chevauchement) est **abandonné** : il faisait occuper **3 positions** de grille pour **1 unité de temps**, rendant la grille temporellement irrégulière.
>
> **Un ratchet est une propriété d'UN step** (contenu du pattern, donc partagé). Le step garde sa position et son unité de temps ; le ratchet dit **combien de déclenchements** il émet :
>
> - `2 · 3 · 4 · 6` — N déclenchements régulièrement espacés **dans la durée du step** (durée **inchangée** : le step « joue plus vite »). Affichés par un **chiffre sous le step**.
> - `TRIOLET` (▲) — **3 déclenchements sur DEUX unités** (« un triolet de noires vaut une blanche ») : le step **dure le double** et **décale la suite du pattern**. C'est le seul code qui étire le temps. Affiché par un **triangle plein** à la place du disque.
⚠️ **LE MODÈLE EST RECONFIRMÉ LE 2026-08-23, sur ses propres exemples.** Le propriétaire a redonné la règle musicale — un triolet joue 3 notes dans le temps normalement pris par 2 — puis l'a illustrée : un pattern de 6 pas dont le 4e porte un triolet **vaut 7 pas** ; ramené à 4 pas, il **vaut 5 pas**. C'est exactement ce que le code calcule (`ratchetSpan(TRIOLET) = 2`, `ratchetTriggers(TRIOLET) = 3`, durée du cycle = somme des `span`). Le modèle ci-dessus n'est donc **pas** révisé, et **la dérive d'un temps par cycle est confirmée comme acceptée**.
**Un triolet sur un pas INACTIF garde ses deux unités et n'émet rien** : c'est un **silence en triolet**. Un ratchet `2/3/4/6` sur un pas inactif n'a lui aucun effet, sa durée valant une unité. Dans les deux cas le code est **conservé dans le pattern** et redevient actif avec le pas.
### 6.3.1 Placement des sous-déclenchements — DÉCIDÉ le 2026-08-23
Un ratchet N découpe **un** step en N tranches ; le triolet découpe **deux** steps en 3. À 96 PPQN, un step vaut `ticksPerStep` (§6.1) : **4 ticks à ****`x24`**, 96 à `/1`, 12288 à `/128`. Les tranches ne tombent donc pas toujours sur un tick entier.
**Trois décisions, prises sur des chiffres.**
1. **La position d'un sous-déclenchement se calcule ****`(stepTicks x k) / triggers`**, et non `slotTicks x k` avec un `slotTicks` déjà tronqué. La première forme garde l'erreur **sous un tick** quel que soit le rang ; la seconde la **multiplie** par le rang. Exemple mesuré : R6 à `x6`, step de 16 ticks, la forme tronquée place les six notes à 0-2-4-6-8-10 et **laisse les ticks 11 à 15 vides** ; la forme correcte donne 0-2-5-8-10-13.
2. **Une tranche doit valoir au moins DEUX ticks.** C'est le plancher retenu par le propriétaire. À 120 BPM, deux ticks valent 10,4 ms pour une impulsion de 5 ms, donc deux notes nettement distinctes jusqu'à 240 BPM. Le plancher est exprimé **en ticks et non en millisecondes** : le tick suit le tempo, l'impulsion non, et un ratchet ne doit pas disparaître parce qu'on accélère.
3. **Un ratchet impossible à la cadence du channel ne se sélectionne pas** : il est retiré de la liste de choix. Le signaler à l'écran a été **écarté** comme trop coûteux.
**Six combinaisons sont refusées par le plancher**, toutes aux trois cadences les plus rapides : `x24` avec R3, R4 et R6 · `x16` avec R4 et R6 · `x12` avec R6. À partir de `x6` tout passe, la plus petite tranche y valant 2,67 ticks. **Le triolet passe partout**, sa tranche la plus courte étant de 2,67 ticks à `x24`.
**Une seule combinaison est impossible par arithmétique et non par plancher : R6 à ****`x24`**. Quatre ticks ne peuvent pas porter six instants distincts, et aucune formule ne crée des ticks.
⚠️ **LE RATCHET DORMANT — conséquence de la banque partagée, tranchée le 2026-08-23.** Le ratchet est du **contenu** (partagé), la cadence est un **état d'exécution par channel** : le même pattern vu depuis un channel en `/1` et un channel en `x24` n'a donc pas la même légalité. Refuser à la saisie ne rend pas l'état inatteignable. Quand la cadence d'un channel rend impossible un ratchet déjà posé :
- le pattern le **garde** ;
- l'écran **ne l'affiche pas** sur ce channel ;
- le moteur émet **un** déclenchement ;
- revenir à une cadence plus lente le **rend visible et jouable** à nouveau.
C'est la même règle que le ratchet d'un pas inactif : **dormant, jamais perdu**.
> ⚠️ **Allongement du pattern — COMPORTEMENT SOUHAITÉ, pas un effet de bord.** Un `▲` **étend volontairement la durée totale du pattern** d'une unité : la grille de 24 positions n'est donc plus strictement régulière dans le temps, et un channel portant un `▲` prend du retard sur les autres à chaque tour. C'est précisément l'outil qui permet de « ralentir artificiellement le rythme ». **Ne pas « corriger » ce décalage** : décision explicite du propriétaire du PRD (2026-08-19), confirmée après que la conséquence lui a été signalée. Seul un reset global réaligne les channels.
> **Aucune contrainte** de position ni de chevauchement : n'importe quel step peut porter un ratchet.
> **Ratchet 5 — ÉCARTÉ (décidé 2026-08-20).** Fait mesuré : à 96 PPQN (= 2⁵ × 3) il n'existe aucun facteur 5 ; un cinquième n'est exact que sur **2 des 25** valeurs de SUBDIV (`/5`, `/10`), partout ailleurs il dériverait. Les ratchets 2/3/4/6 sont exacts sur 21 à 25 des 25 valeurs. Le propriétaire du PRD a tranché sur cette base : **le ratchet 5 n'est pas exposé**, et l'implémentation actuelle est donc conforme.
> **Repli documenté :** si un sous-slot ne tombe pas sur un tick entier (SUBDIV très rapides : `x24`, `x16`, `x12`, `x6`, `x3`), le ratchet est **ignoré** pour cette combinaison — jamais de tick fractionnaire, jamais de dérive.
> **Moteur :** le `SequencerEngine` reçoit la banque partagée (`setPatternBank`) et met en cache, à chaque frontière de step, la durée (`stepTicks`) et l'espacement des sous-déclenchements (`slotTicks`) — **aucune division dans le chemin critique** (leçon d'un précédent essai : une division 16 bits par tick avait décalé le signal simavr de 40 → 43 ms). `onsetCount(ch)` expose le nombre de déclenchements du dernier `advance()`. `masterPhase` reste inchangée.
> **Édition en cours de lecture — CONSERVÉE (confirmé 2026-08-20).** `refreshTiming()` relit le ratchet du step courant après modification du contenu, sinon l'édition ne prendrait effet qu'au passage suivant. La possibilité d'éditer pendant la lecture a été remise en question puis **maintenue** : le firmware Sitka d'origine l'implémente de **deux** façons — bascule du step sous le curseur, et mode RECORDING (§5.5) — la retirer serait donc une régression par rapport à la référence. `refreshTiming()` sert par ailleurs à appliquer une édition faite à l'arrêt avant de relancer.
> **Vérifié en lecture réelle** (simulateur, 240 BPM, SUBDIV `/1`) : steps normaux **250 ms** · step **▲ triolet : 511 ms** = exactement 2 unités.
---
## 7. Modèle temporel — `masterPhase`
> **Décisions validées** (étaient « à valider ») :
- **Unité :** compteur monotone de **ticks à 96 PPQN** (résolution interne libGravity ; SUBDIV historiques exacts). Le 1/16 n'est **pas** figé : chaque channel divise par son `ticksPerStep`.
- **Représentation :** **`uint32`** (déborde après \~221 j à 140 BPM ⇒ aucune normalisation nécessaire ; limite documentée).
- **Indépendance :** `masterPhase` est indépendant de LENGTH, du pattern et de la phase locale. `stop()` conserve la phase ; le reset global la remet à 0 et réaligne tous les channels.
- **Projection → ****`effectiveStep`**** (phase locale lissée) :** chaque channel maintient une position locale (`localStep` + accumulateur de ticks). Un changement de LENGTH **ne fait pas sauter** le playhead : `localStep` est conservé, replié (`% nouvelleLongueur`) uniquement s'il sort de la nouvelle longueur. C'est le rôle de la `phase locale`/`phaseOffset`. Conséquence assumée : `effectiveStep` dépend de l'historique des changements de LENGTH ; un reset global réaligne à 0.
- ⚠️ **UNE POSITION PAR CHANNEL : c'est une divergence assumée, inscrite le 2026-08-25.** Le firmware d'origine n'a **qu'un seul** pas courant, `currentStep`, global et partagé par les six channels, qui boucle à 15 (`Gravity.ino:99` et `446-450`). Ses six channels en `SEQ` sont donc **verrouillés ensemble** et ne peuvent pas dériver. FlexSeq donne à chaque channel son `localStep`, ce que la LENGTH et la SUBDIV par channel **exigent** : deux channels de longueurs différentes ne peuvent pas partager une position. La conséquence musicale est réelle et voulue — des motifs de longueurs différentes se déphasent et se recroisent, ce que l'original ne sait pas faire. Le **reset global** reste le point où tout se réaligne.
**API moteur (testée) :** `start()`, `stop()`, `reset()`, `advance(ticks=1)`, `setPatternBank(bank)`, `refreshTiming()` ; par channel `selectedPattern`, `effectiveLength`, `subdiv`/`ticksPerStep`, `barLength`, `effectiveStep`, `hasStepped`, **`onsetCount`** (déclenchements du dernier `advance()`, ratchets inclus), `currentStepTicks`, `currentStepTriggers`.
---
## 8. Transport
La conversion *événement horloge → progression* appartient à **Transport**. Source unifiée : le callback **96 PPQN de sortie** de libGravity (interne et externe y surfacent) → 1 tick.
<table header-row="true">
<tr>
<td>Événement</td>
<td>Transport</td>
<td>Effet moteur</td>
</tr>
<tr>
<td>MIDI Start</td>
<td>`start()`</td>
<td>reset global puis run</td>
</tr>
<tr>
<td>MIDI Continue</td>
<td>`resume()`</td>
<td>run sans reset</td>
</tr>
<tr>
<td>MIDI Stop</td>
<td>`stop()`</td>
<td>stop sans reset (phase conservée)</td>
</tr>
<tr>
<td>External Reset</td>
<td>`reset()`</td>
<td>reset global (phase → 0)</td>
</tr>
<tr>
<td>MIDI / Ext Clock</td>
<td>`tick(n)`</td>
<td>`advance(n)`</td>
</tr>
</table>
**Implémentation :** le callback ISR accumule un compteur `volatile` de ticks, drainé **atomiquement** dans la boucle principale → `Transport.tick(n)` (moteur muté uniquement en contexte main-loop).
> **Différé :** les hooks distincts MIDI Start/Continue/Stop et External Reset ne sont pas exposés séparément par libGravity au commit figé (uClock gère start/stop en interne). `resume/stop/reset` sont implémentés et testés mais pas encore déclenchés (seuls `start()` au boot et `tick` sont câblés).
### 8.1 Câblage — VALIDÉ le 2026-08-22
Quatre câblages, dont un jamais posé :
<table header-row="true">
<tr>
<td>Ce qu'on câble</td>
<td>Comment</td>
</tr>
<tr>
<td>Sortie 96 PPQN vers le moteur</td>
<td>`AttachIntHandler` — **déjà fait**</td>
</tr>
<tr>
<td>Entrée d'horloge externe</td>
<td>`AttachExtHandler(onExtClock)`, et `onExtClock` appelle `clock.Tick()`</td>
</tr>
<tr>
<td>Marche / arrêt</td>
<td>PLAY appui court (§12.1)</td>
</tr>
<tr>
<td>Tempo et source</td>
<td>champs de l'onglet `◔` (§12.1)</td>
</tr>
</table>
⚠️ **`AttachExtHandler`**** attache l'interruption sur ****`EXT_PIN`****, mais le rappel est le NÔTRE.** libGravity n'appelle pas `uClock.clockMe()` lui-même — lu dans `clock.h` le 2026-08-22. Sans ce câblage, **une horloge externe ne produit rien**. C'est la pièce que `main.cpp` n'a jamais posée, et elle n'est visible ni dans les tests natifs ni en simulation.
**PLAY repart de zéro — décidé par le propriétaire le 2026-08-22.** Un appui sur PLAY après un arrêt appelle `Transport::start()`, donc **reset global puis marche**, et non `resume()`. Deux raisons : c'est ce qu'on attend d'un séquenceur à motifs courts, et cela **donne un geste au réalignement des channels**, qui autrement n'en aurait aucun — un step en TRIOLET étire le temps, donc les channels dérivent et le §6.3 dit que seul un reset global les réaligne. `resume()` et `stop()` ne sont pas jetés : ils restent le point d'entrée de MIDI Continue quand les hooks seront exposés.
**Source d'horloge : DEUX CHAMPS, ****`MODE`**** et ****`PPQN`**** — révisé le 2026-08-23.** La rédaction précédente exposait les six valeurs de libGravity dans un seul champ `SRC`. Le propriétaire a tranché pour la séparation de l'original : `MODE` porte **INT / EXT / MIDI**, et `PPQN` n'apparaît **qu'en EXT**.
**`PPQN`**** expose les QUATRE cadences de libGravity** — 24, 4, 2 et 1 — là où l'original n'en offrait que deux. C'est une **addition assumée** : elle n'enlève rien, l'utilisateur de l'original retrouve `24` et `4` là où il les attend, et les deux autres sont des capacités que la dépendance sait déjà fournir.
**`MODE`**** et ****`PPQN`**** sont DEUX VUES D'UN SEUL OCTET**, celui de la source. La grille de libGravity est exactement leur produit :
- `SOURCE_INTERNAL` → `MODE = INT` ;
- `SOURCE_EXTERNAL_PPQN_24` → `MODE = EXT`, `PPQN = 24` ;
- `SOURCE_EXTERNAL_PPQN_4` → `MODE = EXT`, `PPQN = 4` ;
- `SOURCE_EXTERNAL_PPQN_2` → `MODE = EXT`, `PPQN = 2` ;
- `SOURCE_EXTERNAL_PPQN_1` → `MODE = EXT`, `PPQN = 1` ;
- `SOURCE_EXTERNAL_MIDI` → `MODE = MIDI`.
Deux conséquences, et ce sont des conséquences et non des choix. **Aucun octet EEPROM n'est ajouté pour ****`PPQN`**, l'octet de source suffit. Et **aucun état incohérent n'est représentable** : stocker les deux séparément permettrait d'enregistrer `MODE = INT` avec `PPQN = 4`, qui ne veut rien dire, alors qu'une vue dérivée ne peut pas mentir.
⚠️ **`PPQN`**** DÉSIGNE L'ENTRÉE, JAMAIS LE MOTEUR.** Le moteur tourne à **96 PPQN dans tous les modes**, y compris INT : `clock.h:59` appelle `setOutputPPQN(PPQN_96)` **une seule fois** et `SetSource()` n'y touche jamais. `SetSource()` n'appelle que `setInputPPQN`, et seulement pour les modes externes. Le champ dit donc combien d'impulsions par noire le **signal reçu** transporte, et uClock les multiplie jusqu'à 96 :
- `PPQN = 24` → une impulsion reçue vaut **4** ticks internes ;
- `PPQN = 4` → **24** ticks ;
- `PPQN = 2` → **48** ticks ;
- `PPQN = 1` → **96** ticks.
En **INT** il n'y a aucun signal à interpréter : le champ n'a pas de sujet, et ce n'est donc pas « `PPQN = 96` ». En **MIDI**, `clock.h:114` force l'entrée à 24 — la norme de l'horloge MIDI — donc le champ n'a pas de sujet non plus. C'est exactement pourquoi l'original n'affiche `PPQN` qu'en EXT et un seul champ en MIDI.
⚠️ **Ne jamais passer ****`SOURCE_LAST`**** à ****`SetSource()`****.** C'est la sentinelle de fin d'énumération, et le `switch` de libGravity ne la traite pas — anomalie auditée, §18. Le champ doit donc s'arrêter sur les six valeurs valides sans jamais boucler par elle. À absorber dans `InputAdapter` (ADR 0002).
**Tempo borné 30–300.** L'API accepte 1 à 400. Sous 30 le séquenceur n'est plus jouable ; au-delà de 300 les SUBDIV rapides tombent sous la milliseconde par step. L'original stockait le bpm sur un octet, donc ces bornes restent compatibles avec son format (§4.1).
---
## 9. Génération des triggers
> **Décision validée.** Un channel émet un **trigger** quand il franchit l'onset d'un **step actif** de son pattern sélectionné : `triggered(ch) = onset(ch) ∧ pattern[selectedPattern(ch)].step(effectiveStep(ch))`. Le firmware traduit en impulsion sur la sortie (`DigitalOutput.Trigger()`, 5 ms par défaut).
Chaîne vérifiée en **simavr** : `clock → Transport → SequencerEngine → TriggerSequencer → DigitalOutput → GPIO` (VCD CH1, période conforme au pattern de test).
---
## 10. CV
> **Arbitré et validé le 2026-08-20.** La contradiction relevée le 2026-08-17 est **résolue** : LENGTH, RESET et STEP sont assumées comme des **extensions FlexSeq**. La conception Phase 2 reste la référence pour la *mécanique* — recentrage, offset additif puis clamp, exclusion mutuelle CV1/CV2, modulation BPM globale — mais **pas** pour la liste des destinations.
### 10.1 Mécanique — reprise de Phase 2
Les 2 entrées restent bipolaires ±5 V. `AnalogInput::Read()` renvoie une valeur **déjà recentrée et calibrée** dans ±512 (`Voltage() = read / 512 × 5`) : le domaine ne voit **jamais** le brut du convertisseur. Le neutre est calibré (`SetCalibrationLow/High`, `SetOffset` — le « Calibration CV » du §4) ; l'ancienne mention « 512 = neutre » portait sur le domaine **brut**, où le zéro mesuré vaut 538.
⚠️ **L'EXCLUSION MUTUELLE EST LEVÉE — décidé par le propriétaire le 2026-08-22.** Un channel peut router **CV1 ET CV2 simultanément**, chacun vers sa propre destination. Deux modulations qui visent la **même** destination **s'additionnent puis s'écrêtent** — généralisation directe du « offset additif puis clamp » ci-dessous.
**C'est une ADDITION FlexSeq, pas une restitution, et le propriétaire l'a tranché en le sachant.** Vérification faite dans `Interactions.ino` : l'original **impose** l'exclusion mutuelle. Son champ `MOD` ne cycle que sur trois états — `OFF`, `CV1`, `CV2` — son compteur `channelCV` ne prend que 0, 1 ou 2, et la branche par défaut remet les **deux** cibles à zéro. Les deux champs `CV1Target`/`CV2Target` de sa structure sont un détail d'implémentation, pas deux routages offerts à l'utilisateur. Coût assumé : +12 o d'EEPROM, +24 o de RAM.
⚠️ **Il n'existe AUCUN dosage de modulation par channel dans l'original, contrairement à ce que sa structure suggère.** `CV1Range` et `CV2Range` n'apparaissent qu'à la déclaration : jamais lus, jamais écrits, jamais affichés. L'amplitude y est codée en dur (`map(randMod, 0, 1023, -5, +5)`). Le format §11.1 n'a donc **pas** besoin d'octet de plage, et le §10.4 ci-dessous — zones uniformes sur la pleine échelle — ne perd rien. Le dosage existe bien dans l'original, mais **au niveau global du tempo** (`bpmModulationRange`, 1 à 5, affiché ×10), et il est conservé tel quel.
— *rédaction précédente, remplacée le 2026-08-22 :* Chaque channel route **au plus une** source parmi `aucune / CV1 / CV2` — exclusion mutuelle, comme en Phase 2. La modulation est un **offset additif puis clamp** appliqué à une **base** réglée à l'écran et persistée : le CV ne l'écrase jamais.
La **modulation BPM globale** de Phase 2 est conservée telle quelle : globale, pas par channel. Elle se compose avec la SUBDIV de channel, il n'y a donc **aucune priorité à arbitrer** — la mention « et les priorités » de l'ancienne rédaction était un faux problème.
### 10.2 Destinations — par channel
Destinations : `aucune / PATTERN / LENGTH / RESET / STEP`. Le choix est **explicite**. Phase 2 le déduisait du **mode** du channel, ce qui ne suffit plus depuis que LENGTH est un état d'exécution par channel (§5.2) : un channel en mode séquenceur possède désormais **deux** paramètres modulables au lieu d'un.
- **PATTERN** — `selectedPattern = clamp(base + f(cv), 0, 15)`, à la frontière de step.
- **LENGTH** — `effectiveLength = clamp(base + f(cv), 1, MAX_LENGTH)`, à la frontière de step. `MAX_LENGTH` vaut **24 jusqu'au lot F**, puis 36.
- **STEP** — **décalage de lecture** : `stepLu = (localStep + f(cv)) % effectiveLength`. L'horloge continue de piloter `localStep` ; **rien n'est muté**, donc ramener le CV à zéro remet la lecture exactement où elle serait. Le §9 reste intact.
- **RESET** — par channel : `localStep = 0` et accumulateur de ticks à 0. Application **immédiate** (§10.3).
**La position absolue est écartée** pour STEP (`localStep = quantize(cv)`) : le channel cesserait d'avancer seul, le CV deviendrait son horloge, et le §9 (`triggered = onset ∧ step actif`) s'effondrerait faute d'onset. Ce serait un **nouveau mode de channel**, pas une destination CV.
**Le routage survit au changement de mode** : conservé, ignoré tant que le mode ne s'y prête pas, réappliqué au retour. Phase 2 l'effaçait parce qu'il n'y était qu'**implicite** ; un réglage posé explicitement ne disparaît pas en silence.
### 10.3 Moment d'application
**À la frontière de step, sauf RESET.** Rien ne change en milieu de step, sinon le step courant changerait de contenu — ratchet compris — en cours de route. Ce verrou borne aussi la dépendance à l'historique assumée au §7 : **au plus un repliement** de playhead par step, au lieu de plusieurs centaines par seconde sous CV continu.
La frontière de **step** plutôt que la fin de **pattern** est un choix délibéré : « la fin du pattern » n'est pas un moment partagé — chaque channel a sa LENGTH, sa SUBDIV, et un ratchet TRIOLET **étire son temps** (§6.3), donc les fins de boucle dérivent indépendamment. Attendre la fin du pattern signifierait attendre un délai imprévisible, jusqu'à `MAX_LENGTH` steps — 24 aujourd'hui, 36 au lot F.
**RESET est l'exception : immédiat.** Un reset qui attend n'est pas un reset. Il remet aussi en phase l'horloge de step du channel — c'est la synchronisation dure attendue. En revanche le step 0 est **armé** et sort au **prochain onset**, jamais dans l'instant : tirer sur le champ mitraillerait la sortie sur une entrée nerveuse, et violerait le §9.
### 10.4 Quantification (PATTERN, LENGTH, STEP)
Zones uniformes sur la plage, avec une **hystérésis** d'environ un quart de zone : il faut dépasser franchement une frontière pour changer, et revenir d'autant pour repasser — l'idiome des quantizers Eurorack. Pour PATTERN, ±5 V couvre un décalage de −15 à +15, soit 31 zones sur 1024 pas ≈ **33 pas (\~0,32 V) par zone**, très au-dessus du bruit du convertisseur ; combiné au verrou par step, le tremblement ne peut produire plus d'un changement par step.
Conséquence du modèle « base + offset » : à la base A1 (0), la moitié négative du CV n'a **aucun** effet, le clamp l'écrasant. On place donc la base **au milieu de la banque** pour disposer des deux sens.
### 10.5 RESET — détection de front
`AnalogInput::IsRisingEdge()` **ne doit pas être utilisée**. `old_read_` y est déclaré `uint16_t` alors que `read_` est `int16_t` : une valeur précédente négative devient un grand positif, donc « elle était haute » est vrai **chaque fois qu'elle était en réalité négative**, et un passage négatif → positif ne produit **aucun** front. C'est le cas le plus naturel sur une entrée bipolaire. Anomalie auditée, reproduite par `test_analog_input`, et **non corrigée en amont** — vérifié le 2026-08-20 : `analog_input.h` est identique 3 commits après le commit épinglé.
FlexSeq implémente donc sa propre détection :
- **seuil de Schmitt** : armement au-dessus de **+1 V**, réarmement en dessous de **+0,5 V**, soit **+102** et **+51** en unités `Read()`. +1 V est le point de conception usuel des entrées de trigger Eurorack : au-dessus du bruit, sous les 5 à 10 V qu'envoient les modules. L'écart de 0,5 V **est** l'hystérésis — aucun anti-rebond nécessaire. Le `GATE_THRESHOLD = 0` de libGravity est inutilisable tel quel : il place le seuil au milieu de l'échelle, là où un signal qui flâne fait claquer la détection ;
- **bit d'état précédent initialisé explicitement** à « bas », ce qui écarte le faux front au démarrage (`read_` et `old_read_` ne sont pas initialisés dans libGravity) ;
- **front uniquement, jamais niveau** : une gate maintenue haute produit **un** reset, pas un flux ;
- **verrou** : l'événement est retenu dès qu'il est vu, puis consommé par le moteur. C'est ce verrou qui fait survivre une impulsion courte à un passage de boucle long — l'impulsion doit être **vue**, pas vue au bon moment.
#### VÉRIFIÉ SUR LE MODULE — 2026-08-22
`CvGate` est validé sur du matériel, dans les **deux** régimes, via `env:bringup` :
- **Front.** Horloge externe dans CV1 : un front par impulsion, et le rythme **suit le tempo** à travers un changement de 4:1 (30, 60, 120 BPM). Aucun artefact interne ne peut se mettre à l'échelle du tempo d'une source externe — c'est ce qui rend la vérification concluante malgré un comptage à l'œil.
- **Niveau.** Une sortie du Gravity rebouclée dans son propre CV1 : l'entrée monte à 512 et redescend à 153, une fois toutes les 6,4 s, des dizaines de fois — et produit **zéro** front. Front et non niveau, démontré avec un signal dont nous produisons nous-mêmes la forme.
- **Démarrage à froid.** Le compteur lit 0 et y reste : **aucun faux front à l'initialisation**. L'initialisation explicite du bit d'état précédent, posée contre l'anomalie de libGravity, tient sur le matériel.
**Le bruit au repos est mesuré : −26 ± 3**, identique sur les deux voies, rien branché. Le seuil d'armement à +102 garde donc **128 points de marge**, soit plus de quarante fois l'amplitude du bruit. Le choix de +1 V n'est plus un principe de conception, il est chiffré.
⚠️ **Contrainte mesurée sur le seuil de réarmement, à connaître au §10.2.** Le +0,5 V suppose que la source **redescend sous +0,5 V**. Une source qui repose plus haut verrouille la porte définitivement : un reset, jamais plus. Ce n'est pas théorique — **une sortie du Gravity, éteinte, présente environ +1,5 V à une entrée CV** (+153 en unités `Read()`, contre −27 câble débranché). Vue du jack, la sortie est donc **haute ou flottante**, et non haute ou basse, ce qui évoque une LED en série bloquant le courant inverse. Pamela's redescend bien à 0 et fonctionne. **Aucune décision n'est prise** : remonter le seuil serait un arbitrage, à poser sur des mesures de plusieurs sources, pas sur ce seul cas. Suivi : `docs/open-risks.md`.
### 10.6 Coût et dépendances
**Coût :** 2 octets d'état, une comparaison par channel routé et par passage de boucle, et **aucune lecture de convertisseur supplémentaire** — `Gravity::Process()` appelle déjà `cv1.Process()` et `cv2.Process()` à chaque passage. Ni division, ni interruption nouvelle.
**Propriété utile :** plusieurs channels peuvent router la **même** source vers RESET — une impulsion les resette tous, ce qui donne un reset quasi global sans mécanisme supplémentaire. RESET compose aussi proprement avec STEP, qui n'est qu'un décalage à la lecture.
**TRANCHÉ ET IMPLÉMENTÉ le 2026-08-20 : le CV est échantillonné SOUS INTERRUPTION, et la garantie est de 1 ms.**
La mesure a conduit la décision, en deux temps. Le CV n'était lu qu'une fois par passage de boucle, et le pire passage dépasse 15 ms rendu OLED actif (§14) : une impulsion plus courte pouvait passer inaperçue, alors qu'un trigger Eurorack fait usuellement 1 à 10 ms. Première voie essayée, réduire le coût du dessin (§12) : gain réel sur la médiane, mais le **pire** cas ne bouge quasiment pas. Le propriétaire du PRD a donc tranché : **on garantit 1 ms**, le convertisseur passe sous interruption.
**Ce que ça implique.** FlexSeq prend la **propriété de l'ADC**. `Gravity::Process()` appelle `cv1.Process()`/`cv2.Process()`, donc un `analogRead` bloquant, incompatible avec des conversions pilotées par ISR : `main.cpp` n'appelle plus cette fonction mais ses morceaux (boutons, encodeur). Les sorties étaient déjà pilotées explicitement — FlexSeq ne dépend donc plus du tout de `Gravity::Process()`, ni de son index de boucle non initialisé (§18). La calibration reste lue sur les objets `AnalogInput` de libGravity, qui la détiennent.
**Cadence.** Prescaler 128 → 13 × 128 cycles = **104 µs** par conversion, les deux voies en alternance : une voie toutes les **\~208 µs**, donc 4 à 5 échantillons dans une impulsion de 1 ms. Les conversions sont **relancées depuis l'ISR** et non laissées en roue libre : changer `ADMUX` en roue libre entre en course avec le démarrage de la conversion suivante, et l'échantillon pourrait être attribué à la mauvaise voie. Aucun temporisateur n'est mobilisé — uClock possède Timer1.
**Coût.** ISR de \~57 cycles toutes les 104 µs, soit **5,6 % de CPU** sur matériel (fraction *mesurée*, §14 ; 4,9 % publié avant le 2026-08-21, cadence d'ISR mal mesurée). RAM **+24 o**, Flash **+354 o**.
**Vérifié :** `tools/run-cv-capture-probe.sh` — impulsions de 1 ms injectées sous charge réelle, rendu OLED actif : **27/27 vues, 0 ratée**. La détection de front elle-même (`CvGate`) est un composant pur, testé nativement (13 assertions).
---
## 11. Persistence
> **Impact du nouveau modèle (à concevoir).**
- **Contenu :** 16 patterns partagés (**15 octets** chacun : 3 pour les steps + 12 pour les ratchets en quartets) sauvegardés **une seule fois** (**240 B**), et non 6×16.
- **Par channel :** `selectedPattern` + **base de LENGTH** + `subdiv` + `barLength` + le **routage CV** (source et destination, §10). La longueur est sauvegardée **par channel**, plus « avec le pattern » ; et c'est la **base** qui est persistée, jamais la valeur modulée par le CV.
- Sans allocation dynamique, dans le budget RAM/Flash. Format EEPROM **figé le 2026-08-22, implémenté le 2026-08-23** — voir 11.1.
### 11.1 Format — FIGÉ le 2026-08-22
⚠️ **Écraser l'EEPROM d'origine serait IRRÉVERSIBLE avec nos outils.** Le bootloader optiboot ne sait ni lire ni écrire l'EEPROM (§2). Si FlexSeq écrivait par-dessus les données du firmware d'origine, on ne pourrait pas les remettre sans un programmateur ISP ou un firmware de restauration à écrire. Or le §17 et le §19 posent la **restauration du firmware d'origine** comme contrainte de projet : restaurer le binaire, on sait faire, la sauvegarde flash existe ; restaurer ses réglages, non.
**D'où la décision, qui ne coûte rien : FlexSeq écrit à partir de l'adresse 384.** Le firmware d'origine occupe **0 à 320** plus l'octet **1023** (disposition établie au §4.1). Il reste 702 octets libres ; FlexSeq en demande 304. Conséquence : on peut reflasher le firmware d'origine et **retrouver ses patterns, sa calibration et ses réglages intacts**, sans matériel supplémentaire.
**LE FORMAT EST EN VERSION 2 — conçu le 2026-08-22, implémenté le 2026-08-23 (lot 10).** L'arrivée des trois modes (§4.2) et du second routage CV (§10.1) fait passer l'enregistrement par channel de **6 à 9 octets** : les 4 existants (pattern, LENGTH, index SUBDIV, séparation) plus **mode**, **offset**, **skip chance**, **cible CV1**, **cible CV2**. Aucun octet de plage — voir §10.1. L'image passe de **286 à 304 octets**, l'octet de version de **1 à 2**. À 384 + 304 = 688, on reste loin de l'octet 1023 de l'original.
⚠️ **LE FORMAT PASSE EN VERSION 3 — décidé le 2026-08-23.** L'audit de conformité a rendu deux décisions qui touchent la zone globale (§16) : la séparation `MODE` + `PPQN` revient, et `RANGE` revient. `MODE` et `PPQN` sont **deux vues de l'octet de source** et ne coûtent donc rien (§8.1) ; seuls `MOD` et `RANGE` s'ajoutent. La zone globale passe de **3 à 5 octets** — tempo (2), source (1), `MOD` (1), `RANGE` (1). **L'octet de version passe de 2 à 3.**
⚠️ **Le total de 306 octets annoncé ici est SUPERSÉDÉ.** Il datait d'avant la fondation 36 pas et d'avant le modèle template / instance. La zone globale à 5 octets, elle, est conservée telle quelle. **La taille de l'image v3 est fixée plus bas, à 588 octets**, et c'est ce tableau qui fait foi.
**Les deux décisions partagent le même changement de version**, et c'est pourquoi elles sont prises ensemble : un seul retour aux défauts au lieu de deux.
**Ce que le retour aux défauts emporte** : l'état FlexSeq écrit sur le module depuis le premier flash de production. Ce qu'il n'emporte pas : les réglages du firmware d'origine, sous l'adresse 320, qu'aucune écriture de FlexSeq n'atteint.
**C'était à faire AVANT le premier flash de production, et c'est fait.** Un changement de format fait repartir des défauts et perd les patterns créés **dans FlexSeq** ; ceux du firmware d'origine, sous l'adresse 384, ne risquent rien. Le firmware de production n'ayant jamais tourné sur le module, aucune image v1 n'existe nulle part : le passage de 1 à 2 n'a rien perdu.
**Trois faits établis à l'implémentation.**
- **Les deux octets de cible CV sont réservés, pas vivants.** Ils rendent 0, et une valeur stockée est ignorée. C'est précisément la raison de les réserver maintenant : le routage CV (§10.2) les remplira **sans changer le format**, donc sans faire repartir des défauts.
- **Un octet hors plage est refusé, jamais appliqué**, et le reste de l'enregistrement se charge quand même. Un mode à 3, une chance de saut à 99 : la valeur précédente reste, l'enregistrement voisin est lu normalement.
- **`resetToDefaults()`**** remet les trois nouveaux champs à leur défaut.** Sans cela, un octet de version refusé laisserait le mode précédent en place.
**L'offset tient sur UN octet, et la limite est conservée telle quelle.** Décision du propriétaire, 2026-08-23. Le firmware d'origine déclare `uint8_t offset` (`Gravity.ino:69`), donc le domaine stocke aussi un `uint8_t` : le plafond de 255 est le type. Conséquence assumée, **pas un défaut à corriger** : au-delà de SUBDIV `/2` un step dure plus de 256 ticks, et l'offset n'atteint plus la fin du step — exactement comme dans l'original.
**Coût AVR du lot 10, mesuré :** RAM **−6 o** (l'offset passe à un octet, six channels), Flash **+236 o**. Empreinte 1725 / 28774 o, pile 207 o, 410 o sous le garde-fou. 32 mutants, 32 tués.
**304 octets à partir de 384 — version 2, en service depuis le 2026-08-23 :**
<table header-row="true">
<tr>
<td>Zone</td>
<td>Taille</td>
<td>Contenu</td>
</tr>
<tr>
<td>En-tête</td>
<td>1 o</td>
<td>octet de version, vérifié avant tout chargement</td>
</tr>
<tr>
<td>Patterns</td>
<td>240 o</td>
<td>16 × 15 o, la banque partagée</td>
</tr>
<tr>
<td>Par channel</td>
<td>54 o</td>
<td>6 × 9 o : pattern choisi, LENGTH de base, **index** de SUBDIV, séparation, mode, offset, chance de saut, cible CV1, cible CV2</td>
</tr>
<tr>
<td>Global</td>
<td>3 o</td>
<td>tempo sur 2 o, source d'horloge sur 1 o</td>
</tr>
<tr>
<td>Préférences</td>
<td>6 o</td>
<td>rotation d'écran, sens de l'encodeur, décalage de calibration par voie</td>
</tr>
</table>
**Trois choix expliqués.** L'**octet de version** reprend le principe du `memCode` de l'original : si l'octet ne correspond pas, on ignore le contenu et on repart des défauts — sans lui, un changement de format lirait d'anciens octets comme s'ils étaient valides. **SUBDIV en index et non en valeur** : la liste compte 25 valeurs, un index tient sur un octet quand la valeur brute en demanderait deux, soit 6 de plus. **Calibration en décalage seul** : libGravity modélise `low`/`high`/`offset`, soit 6 octets par voie, mais le défaut mesuré sur le module est un décalage (−26 sur les deux voies, échelle correcte), donc 2 octets par voie suffisent — à confirmer au §10 quand la calibration sera implémentée.
**Délai de calme : 3 secondes** après la dernière modification, décidé par le propriétaire le 2026-08-22. Assez long pour ne pas écrire pendant qu'on tourne l'encodeur, assez court pour qu'une coupure juste après une édition ne perde rien.
- **Écriture différée (décidé 2026-08-20).** On conserve la sémantique de l'original : `EEPROM.put` d'Arduino **compare avant d'écrire** (`update()` octet par octet), donc seuls les octets réellement modifiés sont écrits et **l'usure n'est pas le problème** — un octet par nouveau step activé, sur 100 000 cycles par cellule. Le problème est le **temps** : une écriture EEPROM sur AVR prend \~3,4 ms **pendant lesquelles la boucle attend**, plus la relecture de comparaison à chaque appel. On écrit donc **après un délai de calme** suivant la dernière modification, **jamais dans la foulée d'un événement musical**. L'original appelait `saveState()` à chaque frappe d'enregistrement — et réécrivait tout l'état, plus de 300 octets relus à chaque fois — ce qui plaçait ce blocage au pire endroit.
**⚠️ FORMAT EN SERVICE — version 3, arrêtée le 2026-08-26, ACTIVÉE LE 2026-08-28 par le commit ****`815546b`****.** Le tableau plus haut décrit la version 2, qui ne tourne plus. `main.cpp` construit un `PersistentImageV3` et appelle `bootstrap()` ; l'octet de version effectivement écrit est **3**, à l'adresse 384, constaté sur AVR simulé et non déduit.
**Il n'y a pas de migration depuis la version 2.** Une image v2 valide est **refusée** et les défauts sont pris, ce qui fait repartir l'état FlexSeq de zéro une fois. Les réglages du firmware d'origine, sous l'adresse 320, ne sont jamais touchés.
**L'image fait 588 octets physiques, mais le balayage périodique ne porte que sur 204 octets logiques** — en-tête, six instances, channels, global et préférences. Les **384 octets de modèles** restent HORS du balayage et sont lus ou écrits sur geste (ADR 0006). `PersistentImageV3::addressAt()` traduit un index logique en adresse et c'est le **seul** endroit où cette correspondance vit.
**⚠️ La version occupe le DERNIER index logique, 203, donc la dernière écriture d'un balayage.** Elle valide l'image entière et pas seulement son propre octet : une coupure n'importe où laisse une version qui n'est pas 3, et le démarrage suivant recommence. La version 2 l'écrivait en **premier**, et ce contrat-là ne change pas pour la classe v2, que les tests exercent toujours.
<table header-row="true">
<tr>
<td>Zone</td>
<td>Taille</td>
<td>Décalage</td>
<td>Adresse</td>
</tr>
<tr>
<td>En-tête — l'octet de version</td>
<td>1 o</td>
<td>0</td>
<td>384</td>
</tr>
<tr>
<td>**Modèles** — 16 × **24 o** : 5 de steps, 18 de ratchets, **1 de longueur**</td>
<td>**384 o**</td>
<td>1</td>
<td>385 – 768</td>
</tr>
<tr>
<td>**Instances** — 6 × **23 o**, le pattern que chaque channel joue, **sans longueur**</td>
<td>**138 o**</td>
<td>385</td>
<td>769 – 906</td>
</tr>
<tr>
<td>Par channel — 6 × 9 o, inchangé</td>
<td>54 o</td>
<td>523</td>
<td>907 – 960</td>
</tr>
<tr>
<td>**Global** — tempo (2), source (1), **`MOD`**** (1)**, **`RANGE`**** (1)**</td>
<td>**5 o**</td>
<td>577</td>
<td>961 – 965</td>
</tr>
<tr>
<td>Préférences — inchangé</td>
<td>6 o</td>
<td>582</td>
<td>966 – 971</td>
</tr>
<tr>
<td>**Total, depuis l'adresse 384**</td>
<td>**588 o**</td>
<td></td>
<td>**384 – 971**</td>
</tr>
</table>
**Il reste 51 octets libres, de 972 à 1022.** L'adresse **1023** porte le `memCode` du firmware d'origine et FlexSeq ne l'écrit jamais.
**`MOD`**** et ****`RANGE`**** sont RÉSERVÉS, et c'est une décision de format — pas une conséquence arithmétique.** Décidée par le propriétaire le 2026-08-26. Les deux octets existent dans la version 3 dès maintenant, **inertes** : le firmware ne les lit pas et ne les écrit pas tant que l'onglet BPM (§16) n'existe pas. Le motif est explicite : la taille d'une image fait partie du contrat du format, donc réserver la place maintenant évite de refaire la disposition plus tard, et surtout **évite un second retour aux défauts**. C'est le même choix que les deux octets de cible CV du record de channel, réservés depuis la version 2 et toujours inertes.
**Les instances sont persistées** (décidé le 2026-08-26). Une coupure ne perd donc pas le travail de l'utilisateur. Voir l'ADR 0006 et ses amendements.
**Le contenu d'un pattern occupe 23 octets** — 5 de steps, 18 de ratchets. Les **bits 36 à 39** du cinquième octet n'appartiennent à aucun pas : ils sont **canoniquement à zéro**. Le codec les force à zéro à l'écriture et les masque à la lecture. Voir l'ADR 0007 et son amendement.
**La longueur d'un modèle vaut de 1 à 36**, la borne haute étant la capacité du `Pattern` et **non** le plafond d'interface `MAX_LENGTH`, qui vaut 24 jusqu'au lot F. À l'émission, une longueur hors plage est **écrêtée** ; au chargement, elle est **refusée** sans que le contenu déjà lu soit perdu.
**Exigence sur un format antérieur.** Le comportement du firmware devant une image d'une version différente doit être **explicite, déterministe et testé**. Il n'est pas laissé au hasard d'un chargement partiel.
---
## 12. UI
### 12.1 Modèle d'interaction — VALIDÉ le 2026-08-22
**Principe retenu : hybride.** On garde la structure de l'original, et on ne redéfinit l'édition que là où le nouveau modèle l'exige (24 steps, ratchets par step, LENGTH par channel). Motif : le §4 demande de conserver les fonctionnalités historiques, et l'original a déjà résolu le problème de trois contrôles pour beaucoup de fonctions. Contrainte posée par le propriétaire : **effort cognitif minimal** — créer ou éditer un motif doit demander le moins de gestes possible.
**La barre d'onglets EST la navigation**, comme dans l'original :
```javascript
────────────────────────────
◔  [1] 2 3 4 5 6            ■
```
`◔` = onglet horloge et paramètres globaux · `1` à `6` = les six channels, l'actif en inversé · `⚙` = **page de configuration globale** · `▶`/`■` = **indicateur Play/Stop, HORS navigation**.
⚠️ **NAVIGATION ARRÊTÉE LE 2026-08-23, sur le module.** La barre porte **huit onglets navigables** — horloge, les six channels, puis la **configuration globale** — et, **tout à droite et hors de la navigation**, l'**indicateur Play/Stop**. L'encodeur ne s'arrête jamais sur l'indicateur.
**Ce qui manquait, et ce que le code fait aujourd'hui.** FlexSeq dessine huit onglets navigables dont le huitième est un `drawBox(cx - 2, cy - 2, 5, 5)`, soit un **carré plein de 5x5**. Le propriétaire l'a lu comme un indicateur Play/Stop sur le module : c'est exactement à quoi ressemble un indicateur d'arrêt, et le nom dans le code (`drawSettingsGlyph`) portait une intention que les pixels ne portaient pas. Il manque donc **deux** choses : la roue crantée, et l'indicateur.
**L'original, pour référence, lu dans son code de dessin.** Sept onglets — glyphe `w` puis les chiffres `1` à `6` — et un glyphe de statut **séparé** à `x = 121` : `t` à l'arrêt, `r` en lecture, dessiné **seulement quand l'horloge est interne** (`masterClockMode == 0`).
**L'ONGLET DE L'HORLOGE — disposition arrêtée le 2026-08-23.** Le nombre de champs **change avec le mode**, comme dans l'original, dont le `lastMenuItem` vaut 1, 2 ou 3 selon l'état :
- **`INT`** : paramètre principal = le **tempo** ; champs = `MOD`, puis `RANGE` **si ****`MOD`**** n'est pas à ****`OFF`** ;
- **`EXT`** : paramètre principal = `EXT` écrit en gros à la place du nombre ; champ = `PPQN` ;
- **`MIDI`** : paramètre principal = `MIDI` ; **aucun champ**.
La raison pour laquelle `PPQN` disparaît en INT et en MIDI n'est pas cosmétique : dans ces deux modes le champ **n'a pas de sujet**, l'entrée étant absente ou imposée à 24 par la norme MIDI. Voir §8.1.
**LES GESTES DE L'ENCODEUR — conservés tels quels, 2026-08-23.** Relevés sur la carte d'origine fournie par le propriétaire :
- **tourner** : parcourir le menu courant ou les valeurs du paramètre sélectionné ;
- **appuyer** : entrer dans l'onglet, ou en édition de la valeur du paramètre sélectionné ;
- **appui long** (\~1 s) : revenir en arrière ;
- **SHIFT + tourner** : changer vite le paramètre sélectionné — **ou le paramètre PRINCIPAL de l'onglet quand on est sur la barre d'onglets**, **ou le RATCHET du pas quand on est dans EDIT PATTERN sur un pas sélectionné ET actif**.
⚠️ **LES RATCHETS PASSENT SUR UN GESTE DE L'ORIGINAL — décidé le 2026-08-23.** Le geste inventé pour eux, `appui + tourner`, est **abandonné**. Les ratchets et le triolet se règlent désormais par `SHIFT` + tourner, dans EDIT, sur un pas **actif**.
**Ce n'est pas une économie de geste, c'est une lecture de la carte.** Elle dit que `SHIFT` + tourner change « le paramètre sélectionné ». Dans EDIT, le paramètre sélectionné **est** le pas sous le curseur, et le ratchet est sa valeur. FlexSeq n'a donc plus besoin d'un geste neuf, et la règle du §1 n'a plus d'exception à porter : **l'évolution du mode SEQ se fait avec les gestes de l'original**.
**Deux conséquences à trancher à l'audit, et non à découvrir ensuite.**
1. `SHIFT` + tourner dans EDIT **change de channel** aujourd'hui — une addition FlexSeq absente de la carte. La combinaison étant prise, ce geste disparaît ou se déplace.
2. **La condition « actif » n'existe pas dans le code.** `adjustRatchet()` ne lit pas l'état du pas, et `toggleStep()` n'efface pas le ratchet quand il désactive un pas. Un pas inactif porte donc un ratchet en silence, et le §11.1 le persiste. Ce que devient ce ratchet — effacé à la désactivation, ou gardé et rendu avec le pas — est une **décision produit** à poser.
⚠️ **LE CHANGEMENT DE CHANNEL DANS EDIT DISPARAÎT — décidé le 2026-08-23.** `SHIFT` + tourner y servait à changer de channel, une addition FlexSeq absente de la carte d'origine et dont la combinaison est désormais prise par les ratchets. Rien dans le firmware d'origine ne change de channel depuis l'éditeur de pattern, donc ce retrait **restitue** la navigation d'origine. Prix assumé : depuis EDIT, changer de channel demande appui long, appui long, tourner, appuyer, appuyer.
**LE RACCOURCI DE L'ORIGINAL VERS LES RÉGLAGES EST CONSERVÉ — décidé le 2026-08-23.** `Interactions.ino:84` ouvre les SETTINGS par **SHIFT + un appui de plus de 2 s sur l'encodeur**. Ce geste reste, **en plus** de l'onglet à roue crantée : deux chemins vers la même page, dont un venu de l'original.
**RETOUR À LA PAGE PRÉCÉDENTE — VALIDÉ le 2026-08-23.** L'appui long garde son sens de « remonter d'un niveau », et la règle ci-dessous ne concerne que le raccourci :
- le **raccourci** `SHIFT` + 2 s mémorise l'onglet et le niveau quittés — **un octet** ;
- l'appui long sur la page des réglages **y retourne**, au lieu de tomber sur la barre d'onglets ;
- arriver aux réglages **en tournant** la barre ne mémorise rien : l'appui long remonte normalement ;
- la règle existante reste **première** : l'appui long ferme d'abord un champ ouvert ;
- quitter les réglages **en tournant** oublie le point de retour.
**Un seul point de retour est mémorisé**, celui du dernier raccourci. Ce n'est pas un historique de navigation : l'historique général a été **écarté**, parce qu'il coûte de la RAM par niveau et rend le geste « revenir » imprévisible — il cesserait d'être « remonter d'un niveau », ce que la carte des gestes annonce.
⚠️ **Un geste manque, et il explique un constat du module.** `SHIFT + tourner` **sur la barre d'onglets ne fait rien** : `handleTabBar()` ne traite que `EVENT_ROTATE` et `EVENT_PRESS`. Le propriétaire a signalé le 2026-08-23 que SHIFT semblait ne pas réagir ; sur la barre, il ne réagit effectivement pas. Ligne 31 de `docs/open-risks.md`.
**Deux écarts mineurs à vérifier à l'audit.** L'appui long de FlexSeq dure **750 ms** quand la carte dit \~1 s — à mesurer dans l'original, pas à déduire de la carte. Et deux gestes de FlexSeq ne figurent pas sur la carte : `SHIFT + tourner` dans EDIT change de **channel**, `SHIFT + appui long` dans EDIT **efface le pattern**. Si ce sont des additions, elles doivent être assumées ici.
**Les glyphes de navigation sont ceux de l'ORIGINAL.** Décision du propriétaire, 2026-08-23 : on reprend les formes de l'original pour l'horloge et pour Play/Stop, et on **crée** un glyphe pour la configuration globale, une **roue crantée**, l'original n'ayant pas d'onglet pour cette page. Les formes d'origine sont récupérables : `tools/decode-velvetscreen.py` décode la police `velvetscreen` du firmware d'origine glyphe par glyphe. **Provenance à conserver** — la police est GPLv3, comme FlexSeq lui-même, donc la réutilisation est compatible et seule l'attribution est due.
⚠️ **QUATRE ÉCRANS À PARTIR DU 2026-08-22, ET NON TROIS.** Le propriétaire **renverse** la phrase ci-dessous « il n'existe pas d'écran CONFIG PATTERN séparé » : il en veut un. Le compte devient **principal · EDIT PATTERN · CONFIG PATTERN · réglages**.
**La page d'un channel reprend la forme du legacy** — un gros paramètre à gauche avec son étiquette dessous, **trois** lignes à droite — et ce gros paramètre **change de nature selon le mode**, comme dans l'original :
\| \| CLOCK \| RANDOM \| SEQ \|
\|---\|---\|---\|---\|
\| gros + étiquette \| SUBDIV · `SUBDIVISION` \| skip · `SKIP CHANCE` \| `A1` · `PATTERN` \|
\| 1 \| `MODE:` \| `MODE:` \| `MODE:` \|
\| 2 \| `OFFSET:` \| `SUBDIV:` \| `EDIT` \|
\| 3 \| `MOD:` \| `MOD:` \| `CONFIG` \|
**`CONFIG`**** ouvre la quatrième page**, qui porte `LENGTH`, `SUBDIV` et `MOD`. Conséquence heureuse : la page SEQ n'expose plus SUBDIV, ce qui la **rapproche** de l'original, qui n'en avait pas non plus.
**LE RENDU D'UN PAS — validé le 2026-08-23.** Cinq cas, dans cet ordre de priorité :
- **actif + triolet** : triangle **plein** ;
- **actif, autre ratchet** : disque plein **et le chiffre** sous le pas ;
- **inactif + triolet** : triangle **vide** — le seul glyphe à créer ;
- **inactif, tout autre cas** : anneau ;
- **le chiffre n'est jamais dessiné sur un pas inactif**, ni quand la cadence du channel rend le ratchet impossible (§6.3.1).
**L'asymétrie est voulue** : un triolet agit sur le temps même éteint, donc le cacher tromperait ; un ratchet `2/3/4/6` éteint n'agit sur rien, donc l'afficher encombrerait. L'écran montre alors exactement ce qui a un effet.
**Coût à prévoir** : `PatternScreenModel` ne porte pas la cadence du channel aujourd'hui. Il lui faut **un champ de plus** pour appliquer la dernière règle.
**La séparation de mesure descend dans EDIT PATTERN** : c'est une aide de lecture de la grille, une addition FlexSeq que l'original n'a pas.
**Le glyphe de droite dans la barre est ****`▶`**, comme l'original, et non le `■` écrit plus bas.
**Le gros nom de pattern est dessiné par nos propres glyphes** — dix suffisent, `A`, `B`, `1` à `8` — comme les chiffres de ratchet le sont déjà. \~500 o estimés contre 2646 pour `logisoso26`, et aucune donnée GPLv3.
État : **conçu, non implémenté**, sauf le format v2, qui l'est depuis le 2026-08-23 (§11.1, lot 10). Les mises en page de CLOCK et RANDOM et la navigation **dans** CONFIG sont **assemblées par Claude à partir de l'original** et attendent une validation d'ensemble. Découpage dans `WORKPLAN.md` (lots 11 à 14).
— *rédaction précédente, partiellement remplacée :*
**Trois écrans**, exactement ceux de l'original :
1. **Écran principal** — la barre d'onglets et les paramètres de l'onglet actif. Il n'existe **pas** d'écran « CONFIG PATTERN » séparé : les réglages d'un channel sont le contenu de son onglet.
2. **EDIT PATTERN** — la grille de 24 steps, atteinte depuis l'entrée `EDIT PATTERN` de l'onglet d'un channel.
3. **Réglages** — atteints par l'onglet `■`. Différés : rotation d'écran, sens de l'encodeur, calibration CV (§4.1).
**Deux niveaux dans l'écran principal**, comme l'original : on est soit **sur** la barre d'onglets, soit **dans** un onglet.
**Les huit gestes :**
<table header-row="true">
<tr>
<td>Geste</td>
<td>Sur la barre</td>
<td>Dans un onglet</td>
<td>Dans EDIT PATTERN</td>
</tr>
<tr>
<td>Tourner</td>
<td>change d'onglet</td>
<td>change de champ</td>
<td>déplace le curseur</td>
</tr>
<tr>
<td>Appui court encodeur</td>
<td>entre dans l'onglet</td>
<td>ouvre le champ, ou entre dans EDIT</td>
<td>active / désactive le step</td>
</tr>
<tr>
<td>**Appui long encodeur**</td>
<td>—</td>
<td>revient à la barre</td>
<td>revient à l'onglet</td>
</tr>
<tr>
<td>Tourner en appuyant</td>
<td>—</td>
<td>—</td>
<td>règle le ratchet du step sous le curseur</td>
</tr>
<tr>
<td>SHIFT tenu + tourner</td>
<td>—</td>
<td>change la valeur du champ</td>
<td>change de channel</td>
</tr>
<tr>
<td>SHIFT appui long</td>
<td>—</td>
<td>—</td>
<td>vide le pattern (identique à l'original, §5.5)</td>
</tr>
<tr>
<td>PLAY appui court</td>
<td>marche / arrêt</td>
<td>marche / arrêt</td>
<td>marche / arrêt</td>
</tr>
</table>
`SHIFT` appui court **reste libre** : aucun emploi ne lui est donné plutôt que de l'occuper sans raison.
**L'appui long remonte d'UN niveau, partout — validé le 2026-08-22.** La table ci-dessus ne disait pas ce que font les gestes quand un champ est **ouvert**, et c'est le cœur de la machine d'états. La règle tranchée par le propriétaire est unique et vaut aux trois niveaux :
- champ ouvert → l'appui long **referme le champ** et laisse dans l'onglet ;
- champ fermé, dans un onglet → l'appui long revient à la **barre d'onglets** ;
- dans EDIT PATTERN → l'appui long revient à **l'onglet**.
L'appui court fait l'inverse : il descend d'un niveau, ou bascule le step dans EDIT. Une seule règle à apprendre au lieu de trois — c'est la contrainte d'**effort cognitif minimal**. `SHIFT` tenu + tourner reste un **raccourci** : il change la valeur **sans ouvrir** le champ.
**Les positions bouclent, les valeurs s'arrêtent aux bornes — validé le 2026-08-22.** Boucler un tempo de 300 à 30 serait un accident musical ; refuser d'avancer au dernier onglet serait un cul-de-sac. Donc : onglet, curseur de champ, curseur de step et changement de channel **bouclent** ; tempo, source, pattern, LENGTH, SUBDIV, séparation et ratchet **s'écrêtent**.
⚠️ **L'écrêtage n'est pas redondant avec les contrôles du moteur, et une mutation l'a prouvé.** libGravity **accélère** une rotation rapide (×3 sous 16 ms, ×2 sous 32 ms), donc un cran vaut parfois 3. Sans écrêtage, +3 depuis LENGTH 23 est **refusé** par `SequencerEngine::setEffectiveLength` et la valeur ne bouge pas ; avec écrêtage elle atterrit sur 24. Deux mutations avaient survécu à la première passe exactement pour cette raison.
**L'onglet ****`■`**** est inerte tant que son contenu est différé.** Un appui sur un onglet sans champ ne fait **rien**, plutôt que d'entrer dans un niveau vide qui aurait l'air cassé.
**`selectedChannel`**** est dérivé de l'onglet courant, jamais stocké.** La barre d'onglets et le changement de channel dans EDIT ne peuvent donc pas se contredire, et c'est un octet de moins.
**État mesuré (2026-08-22).** `UiController` existe en C++ et en TypeScript, 35 assertions de chaque côté, **22 mutations sur 22 tuées** de chaque côté. Coût AVR mesuré avec un point d'appel temporaire dans `main.cpp` : **RAM +26 o, Flash +1224 o** — à comparer aux 15 o estimés par l'ADR 0002 et aux 2 à 4 ko estimés au §15 pour l'UI complète. Rien ne l'appelle encore, donc le build livré reste à 1528 / 21404.
⚠️ **`SHIFT`**** tenu se lit en ÉTAT, pas en rappel.** Les callbacks de `Button` se déclenchent au relâchement — c'est la condition pour distinguer appui court et long. Un modificateur tenu exige donc `On()`, qui lit la broche. Vérifié sur le module.
⚠️ **L'appui long de l'encodeur n'existe pas dans l'API de libGravity** au commit épinglé : `Encoder` expose l'appui court, la rotation et la rotation-pendant-appui, et son `Button` interne est privé. Il faut donc **notre propre ****`Button`** sur `ENCODER_SW_PIN`. Les deux ne se gênent pas : `Encoder` ne déclenche que sur `CHANGE_RELEASED`, le nôtre que sur `CHANGE_RELEASED_LONG`. Coexistence **raisonnée sur le code, pas mesurée** — test natif et vérification sur le module exigés ; repli, lire la broche directement. Voir ADR 0002.
**Contenu de l'onglet d'un channel :** pattern sélectionné, **LENGTH**, **SUBDIV** (affichée façon Gravity original : `/N` division, `xN` multiplication), **SÉPARATION de mesure** (graphique : aucune/2/3/4/6), et l'entrée `EDIT PATTERN`. Plus tard la source et la destination CV (§10.2). **Aucun réglage global dans un onglet de channel**, pour qu'aucune confusion ne soit possible — décision du propriétaire. Pas de champ SPEED, pas de METER ni de MEASURES (supprimés — voir §6.2).
**Contenu de l'onglet ****`◔`**** :** tempo, borné **30–300**, et source d'horloge. Voir §8.
**MISE EN PAGE TRANCHÉE le 2026-08-22 — CINQ champs tiennent sans défilement.** La question restée ouverte au découpage est résolue par **deux colonnes** sous la grande police : `LEN` et `SUB` sur une rangée, `SEP` et `EDIT PATTERN` sur la suivante. La grande police est **elle-même le champ 0**, donc les 30 px qu'elle occupe sont sélectionnables et non décoratifs — nom du pattern sur un onglet de channel, **tempo** sur l'onglet `◔`. Aucun défilement, donc aucun geste supplémentaire à apprendre.
Géométrie : barre d'onglets en **bas**, huit cases de 16 px, l'onglet actif en inversé · filet à y 52 · rangées de champs à y 32 et y 40 · grande police centrée, ligne de base y 28. Curseur = cadre ; champ ouvert = **inversé**. Sur la barre d'onglets, aucun curseur de champ n'est dessiné.
**Libellés des six sources d'horloge, à confirmer** : `INT`, `EXT24`, `EXT4`, `EXT2`, `EXT1`, `MIDI`. Ils correspondent à l'énumération de libGravity (`SOURCE_INTERNAL`, `SOURCE_EXTERNAL_PPQN_24/4/2/1`, `SOURCE_EXTERNAL_MIDI`) et `SOURCE_LAST` n'est jamais atteignable (§8.1). Choix de nommage fait à l'implémentation, pas encore validé par le propriétaire.
**Les polices restent hors du renderer** : le modèle porte deux poignées de police, donc le composant est pur et un test lui passe des sentinelles au lieu de tables U8g2. La largeur de la grande chaîne est portée comme celle du titre d'EDIT, `getStrWidth()` coûtant \~1 ms par appel.
⚠️ **Un ****`switch`**** coûte de la RAM sur avr-gcc, et c'est mesuré une seconde fois.** Le compilateur émet une table `CSWTCH` en `.data` — donc en RAM — pour un `switch` sur de petites valeurs. `PatternScreen.h` documentait déjà le piège pour ses chiffres de ratchet ; il a coûté ici **10 o** pour les libellés de source, récupérés en repassant à des comparaisons `if` contre 12 o de Flash. Les deux tables de choix d'`UiController` ont été converties pour la même raison.
⚠️ **LA GRANDE POLICE EST RETIRÉE — décision du propriétaire le 2026-08-22, qui REMPLACE celle prise quelques heures plus tôt et rapportée ci-dessous.** Le premier arbitrage s'appuyait sur une mesure **incomplète** : elle couvrait l'écran principal et la police, mais **ni la persistance ni le câblage de l'UI**. Ensemble, le tout mesure **28242 o, 91,9 %**, au-dessus du garde-fou. Le nom du pattern et le tempo s'affichent donc dans la même police 5×7 que le reste, et le renderer **ne change plus jamais de police** : les deux poignées quittent le modèle.
**Mesuré, tout câblé : Flash 25510 / 30720 (83,0 %)**, soit **2138 o** de marge, et **RAM 1639** avec 409 o libres contre une réserve de 256. Mieux que les 25596 o prédits par l'arithmétique, parce que retirer la police retire aussi le code de décodage de glyphes qu'elle mobilisait dans U8g2.
**Ce qui est perdu, sans l'enjoliver** : le nom du pattern ne se lit plus d'un coup d'œil à distance, et c'était précisément l'objet de la décision sur `logisoso26`. Les replis restent chiffrés ci-dessous si le budget se desserrait, et la ligne 24 de `docs/open-risks.md` rendrait à elle seule 1536 o si elle était vérifiée.
**La mise en page se resserre** plutôt que de laisser un trou là où étaient les glyphes de 26 px : l'en-tête passe à 10 px et les deux rangées de champs remontent à y 14 et y 22. Ce qui reste dessous n'est pas du vide mais **la place des champs de source et destination CV du §10.2** — un test assertionne qu'il y tient exactement deux rangées de plus, donc ces champs n'exigeront pas de refonte.
**Décision REMPLACÉE, conservée parce que ses chiffres restent utiles :** Mesuré sur le build de production avec les **deux** écrans câblés puis reverté : **RAM 1607 / 2048 (78,5 %)**, **Flash 26060 / 30720 (84,8 %)**, dont **2646 o** pour la police elle-même. Il reste **1588 o** avant le refus du garde-fou à 90 %, et les lots 2 (adaptateurs), 5 (transport) et 6 (persistance) restent à construire — estimés 1200 à 1500 o. Ça passe, **sans marge**.
✅ **ÉTAT FINAL, mesuré le 2026-08-22 sur le firmware complet** — deux écrans, huit gestes, transport, persistance, tout câblé : **RAM 1699 / 2048 (83,0 %)**, **Flash 28050 / 30720 (91,3 %)**, pile **206 o** (80 % de la réserve de 256).
⚠️ **CES CHIFFRES SONT PÉRIMÉS. Mesuré le 2026-08-25 : RAM 1429 / 2048 (69,8 %), Flash 27170 / 30720 (88,4 %), pile toujours 206 o, marge 413 o, et 2014 o sous le garde-fou de 95 %.** Trois changements du même jour, et le premier pèse plus que tout ce qui précède. **Le transport de l'écran** : le SSD1306 est en écriture seule, mais Arduino Wire apportait un pilote bidirectionnel, sous interruption, capable de mode esclave. Un transport TWI scruté, sens unique, a rendu **1678 o de Flash et 216 o de RAM**, et la boucle est devenue **plus rapide** — p90 8,80 puis 6,50 ms, une bande d'écran 4,88 puis 3,36 ms. Il a fallu une ligne **additive** dans le fork, plus l'interrupteur `U8X8_NO_HW_I2C` d'u8g2 : sans lui l'objet global Wire restait, son constructeur étant dans `.init_array`, que l'éditeur de liens ne retire jamais. **Les tampons série**, 64 → 16 octets chacun : **96 o de RAM**, aucun coût en Flash. Et `adjustFieldValue`, qui a cessé de répéter son garde et ses conversions : **94 o**. ⚠️ Ces 94 o appartiennent à cette campagne du 2026-08-25 et sont **distincts** des 126 o du lot S, mesurés le 2026-08-30.
**Le plafond Flash passe de 90 à 95 %**, décidé par le propriétaire sur ce chiffre. Ce n'est pas un plafond repoussé pour se donner de l'air : la **vraie** limite est 30720 o, et 95 % avertit encore 1134 o avant elle. Ce qu'on accepte est une réserve plus petite, jamais un risque de brique — l'éditeur de liens refuserait bien avant. Le garde-fou de **dérive** reste à +512 o : cela ne devient pas un droit de grossir en silence.
**Deux campagnes d'économies ont précédé la décision, pas suivi**, et rendu **1378 o** : `-mcall-prologues` (534 o, coût mesuré sur les quatre sondes — pile 161 → 175 o, p90 8,52 → 8,96 ms) · le jeu de glyphes réduit `u8g2_font_5x7_tr` au lieu de `_tf` (808 o, et le panneau prouve le rendu identique : **863 pixels d'encre avant comme après**) · un formateur de nombres partagé (36 o).
**Mesuré et écarté, pour que personne ne le retente** : le constructeur du moteur ne vaut que **88 o** sur les 1646 o de code de constructeurs globaux, et l'ELF le confirme le 2026-08-30 — il n'a **aucun symbole**, LTO l'inline dans `main`.
**⚠️ Une phrase de ce paragraphe est CORRIGÉE le 2026-08-30, parce qu'une mesure la réfute.** Elle disait : « **tous** les leviers d'inlining rendent zéro ou pire sous `-Os` avec LTO ». Ce qui est établi, et rien de plus :
- les **réglages globaux d'inlining du compilateur** essayés pendant la campagne Flash du 2026-08-25 n'ont produit aucune économie exploitable sous `-Os` avec LTO — **fait mesuré** ;
- un **désinlining ciblé** de trois helpers de `UiController.cpp` produit une économie de **126 o** — **fait mesuré**, 2026-08-30, ADR 0010 ;
- les deux expériences portent sur des **mécanismes différents** — **fait technique**. Un réglage global agit sur tout le binaire ; `noinline` sur une fonction nommée matérialise **plus** de fonctions et réduit pourtant la taille, parce que la duplication du code inliné coûtait plus cher que les appels ;
- la portée exacte que la formulation d'origine voulait donner à « tous » — **inconnue**, et ce texte ne la reconstruit pas.

**Réglage global du compilateur et désinlining ciblé d'une fonction sont deux stratégies distinctes, et un résultat sur l'une ne dit rien de l'autre.**
**Une économie annoncée doit reposer sur une taille mesurée.** Avant d'annoncer ou de conserver une estimation d'économie, identifier le symbole dans l'ELF et mesurer sa taille avec `avr-nm`. Si le compilateur inline le code, le symbole n'existe pas et aucune économie ne doit lui être attribuée. Une taille de symbole ne constitue pas une économie à elle seule : le gain doit être démontré par une comparaison de builds. Une estimation réfutée reste réfutée dans le document de décision.
**Également réfuté par le lot S** : une table `PROGMEM` des bornes à la place du `switch` de `adjustFieldValue`. Les bornes ne coûtent rien — chacune est un immédiat dans un clamp — et les sept cas ne sont pas uniformes, donc une table ne peut pas les porter.
**Deux gros consommateurs sont mesurés et NON qualifiés** : `main` (**5976 o**) et `PagedScreen::renderFrom` (**2490 o**), lus à l'`avr-nm` le 2026-08-30, soit 8466 o ensemble et 31 % de la Flash occupée. Ce sont des **tailles, pas des économies** : aucune analyse de leur contenu n'existe et aucun gain ne leur est attribué. Suivi en ligne 61 de `docs/open-risks.md`.
⚠️ **La liste qui suit est celle du 2026-08-25 et elle est PÉRIMÉE sur un point** : les 574 o de `Wire` sont partis avec tout le chemin de son transport le 2026-08-25. Ce qui reste est soit la dépendance épinglée — ISR d'uClock 974 o, Wire 574 o, u8g2 \~1500 o, et **586 o d'allocateur** qu'uClock impose en allouant quatre octets à l'init — soit les deux renderers dont l'interface a besoin.
⚠️ **Conséquence assumée par le propriétaire** : la Flash se mesure à **chaque** lot restant, pas seulement à la fin. Le repli est une ligne de code et la décision se reprend alors sur le chiffre réel : `logisoso22_tr` rend 420 o (même famille, 22 px au lieu de 26), `profont22_tr` rend 984 o (autre famille, l'allure change). Le garde-fou à 90 % n'est **pas** levé : le lever ne créerait pas de place, il retirerait un avertissement. ⚠️ **Ce seuil de 90 % est HISTORIQUE** : le propriétaire l'a porté à **95 %** le 2026-08-22, sur le chiffre réel du firmware complet. Levier non vérifié : `docs/open-risks.md` ligne 24.
**Contrôle de rendu de bout en bout** : `run-screen-dump.sh` couvre cet écran et déduit lequel des deux il vérifie depuis l'environnement, les critères n'ayant rien en commun. Lu dans la mémoire du panneau : **8/8** cases d'onglet à leur place après `U8G2_R2`, barre d'onglets en panneau y 0..7, grande police en panneau y 33..62, filet en y 11. Vérifié **rouge** en supprimant la grande police : 379 px → 0.
**Pied de page d'EDIT PATTERN.** La grille laisse la **bande 7** libre : le pixel dessiné le plus bas de la seconde rangée est son chiffre de ratchet à **y 47**, et la bande 7 couvre **y 56 à 63**. Une ligne dont la ligne de base est à 63 y tient avec 8 pixels de dégagement. Elle portera le channel et le tempo. **L'en-tête ne change donc pas** : il reste `EDIT PATTERN A1`, 15 caractères, et la décision du 2026-08-20 de le garder explicite n'est pas touchée. Le saut de bande devient **bilatéral** — même argument géométrique qu'en haut, voir ADR 0001 amendé.
**IMPLÉMENTÉ ET MESURÉ le 2026-08-22, et le saut économise DEUX bandes, pas une.** Le critère est « la bande est entièrement sous le pixel le plus bas que la grille puisse poser », et **deux** des huit bandes y répondent : celle du pied de page (y 56–63) et celle qui sépare la grille du pied (y 48–55), qui ne porte rien et n'en portera jamais. Les deux dépendent de la même chaîne de pied, donc les deux sautent ensemble. Une image courante envoie **5 bandes au lieu de 7** : image **44,2 → 32,1 ms**, p90 corrigé **8,46 ms** contre un budget de 12 ms, médiane 6,82 ms. Coût **RAM +11 o, Flash +184 o**, dérive acquittée ; pic de pile remesuré à 160 o.
**Le contrôle de rotation épingle désormais les DEUX bouts.** `run-screen-dump.sh` vérifiait « le titre atterrit en bas du panneau et le haut est vide » ; or le haut du panneau est précisément où `U8G2_R2` place le pied. Le critère est donc maintenant : titre en bas, **pied en haut**, et la bande intermédiaire vide. Une rotation inversée ne peut plus passer. Mesuré : 106 pixels en panneau y 0..6, et 0 dans l'intervalle. Vérifié **rouge** en retirant le pied du firmware de démonstration.
Le pied est **aligné à gauche** à x = 4, comme le filet d'en-tête : centrer aurait exigé un `getStrWidth()` de plus, \~1 ms par appel sur ce MCU. Son contenu est **fourni par l'appelant** — le renderer reste pur et ne connaît ni channel ni tempo.
**Grande police pour le nom du pattern — décidé le 2026-08-22, coût MESURÉ.** L'original affiche `A1` en gros. FlexSeq fait de même avec `u8g2_font_logisoso26_tr`, jamais avec la police de l'original (données GPLv3, écartées). Mesuré par build de production : **+2688 octets de Flash**, dont 2646 pour le tableau de la police et 42 pour le code que U8g2 mobilise ; **0 octet de RAM**. Flash portée à 24092 / 30720 (78,4 %), soit **3556 octets avant le refus du garde-fou à 90 %** — contre une UI complète estimée 2 à 4 ko au §15. La borne haute dépasserait le seuil d'environ 450 octets, ce qui exigerait un acquittement explicite. **Choix délibérément réversible** : `profont22_tr` coûte 1704 o et `logisoso22_tr` 2268 o, si le budget se tend. Les variantes chiffres seuls (387 à 548 o) ne suffisent pas : dix glyphes sont nécessaires, `A`, `B` et `1` à `8`.
**EDIT PATTERN** — 24 positions en 2 lignes de 12. **Révision complète (2026-08-17)** : la représentation suit désormais le POC Wokwi et les glyphes du firmware d'origine.
⚠️ **La grille reste à 24 positions jusqu'au lot F, alors que le ****`Pattern`**** en porte 36.** Le §7 point 8 décrit la grille cible, 3 lignes de 12. En attendant, `screen::GRID_STEPS` et `UiController::STEP_COUNT` valent **24** et ne lisent délibérément pas `Pattern::DEFAULT_TOTAL_STEPS` : les steps au-delà de 23 sont stockables et persistés, mais aucun geste ne les atteint.
>
> **Géométrie de référence = le POC Wokwi** `flexseq-oled-playground/sketch.ino` (SSD1306 128×64, rotation R2) : 24 steps en **2 lignes de 12**, pas horizontal de **10 px**, grille **centrée**, glyphes **5×5** identiques au firmware d'origine, cadre de sélection **9×9**.
>
> ⚙️ **Portée du sketch — précision (2026-08-19).** Le sketch fait foi pour la **géométrie et les glyphes uniquement**. Son objet d'affichage (`U8G2_..._2_HW_I2C`, buffer 256 B) n'était qu'un **test visuel expérimental** et n'est **pas** normatif.
>
> **Contrainte d'implémentation (décision validée) :** le firmware **réutilise l'objet de libGravity** — `gravity.display`, de type `U8G2_SSD1306_128X64_NONAME_1_HW_I2C` (**mode ****`_1_`**, buffer de **128 B déjà payé** dans l'empreinte mesurée). **Ne jamais instancier un second objet U8G2** : ce serait +256 B de RAM pour rien. On garde ce que libGravity permet ; le mode `_1_` implique simplement davantage d'itérations `firstPage()/nextPage()`. **Coût RAM de l'objet d'affichage : 0 B supplémentaire** — à ne pas lire comme « le rendu ne coûte rien » : l'étalement en 8 bandes, ci-dessous, coûte 24 o de gel.
>
> **Écartement par bande (2026-08-20).** Le renderer ne dessine plus que les éléments qui tombent dans la bande en cours : il reçoit celle-ci en paramètre (`Band{y0, y1}`, par défaut tout l'écran). Sans cela les 24 steps, leurs chiffres et le titre étaient recalculés **huit fois** — U8g2 découpe ce qu'on lui envoie, mais l'appel a lieu quand même.
>
> ⚠️ **La bande reçue est en coordonnées d'AFFICHAGE, le renderer travaille en coordonnées LOGIQUES.** `U8G2_R2` fait tourner de 180° *avant* le découpage : `PagedScreen` applique donc l'inverse. Omettre cette inversion donnait à chaque bande la **moitié inverse** de ce qu'elle affiche, et l'écran restait quasi blanc — défaut qui a vécu un commit, trouvé en lisant la mémoire du panneau (§14), invisible aux tests natifs qui fournissaient la bande déjà en coordonnées logiques.
>
> **Gain mesuré, rendu correct des deux côtés** (§14) : passage médian **8,52 → 6,48 ms**, image entière **74,0 → 59,1 ms** — soit −24 % et −20 %, et non le facteur 2 annoncé d'abord. Le **pire** passage ne bouge quasiment pas (16,16 → 15,32 ms) : il était porté par le **titre**, que l'écartement ne rend pas moins cher — il ne l'évite pas. Coût : **+522 o de Flash, 0 o de RAM**. Propriété vérifiée par test : la réunion des 8 bandes rend **exactement** l'image complète, pixel pour pixel.
>
> **L'EN-TÊTE RESTE EXPLICITE — décidé 2026-08-20.** `EDIT PATTERN A1` est conservé en entier. Sa rastérisation coûtait 8,8 ms par image (§14), et deux voies ont été **écartées** par le propriétaire du PRD : raccourcir l'en-tête (≤ 9 caractères auraient suffi à tenir le budget, mais au prix de la clarté) et mettre la bande rendue en **cache RAM** (128 o, refusé : la RAM doit rester disponible pour des fonctionnalités).
>
> **Saut de la bande inchangée (2026-08-20).** La solution retenue ne coûte **aucune RAM** : le titre ne changeant qu'au changement de pattern sélectionné, sa bande n'est ni effacée, ni dessinée, ni envoyée quand il est identique. Le SSD1306 étant un écran à mémoire, une bande non envoyée continue d'afficher ce qu'elle affichait. La condition est **géométrique** — une bande entièrement au-dessus du filet d'en-tête ne peut contenir que le titre — donc solidaire de la mise en page. Deux points de conception détaillés dans l'**ADR 0001** : le cycle effacer‑dessiner‑envoyer est **indivisible**, et une image sur seize est rendue intégralement en filet contre un défaut de notre propre logique. Gain : passage courant **15,4 → 8,44 ms**, image **56 → 44,2 ms**, pour **+9 o de RAM et +160 o de Flash**. La ligne de base du titre passe de 8 à **7** pour qu'il tienne dans une seule bande, et sa largeur n'est plus mesurée qu'une fois par image.
>
> **Rendu étalé — ADR 0001 (2026-08-20).** Ces 8 itérations ne s'enchaînent plus dans un seul appel, et la boucle de pages est désormais **manuelle** (`setBufferCurrTileRow` + `clearBuffer` + dessin + `sendBuffer`) puisque `firstPage()/nextPage()` ne permet pas de sauter une bande : **une bande par passage** de la boucle principale, le modèle étant **gelé** au début de l'image (`PatternScreenModel` 8 o + copie du `Pattern` 15 o = **23 o**, plus un drapeau d'état, soit **24 o** mesurés au build ; nécessaire puisque le contenu est partagé et éditable en lecture — §6.3). Motif : le bus I2C tourne à **400 kHz** (déclaré par le descripteur U8g2 du SSD1306, appliqué à chaque transfert ; ni libGravity ni FlexSeq ne le fixent), et une image pleine représente \~25 ms de bus — pendant lesquelles la boucle est bloquée, donc les ticks s'accumulent et les onsets se tassent. Voir `docs/decisions/0001-boucle-principale-non-bloquante.md`.
>
> **Espacement vertical de la grille — TRANCHÉ (2026-08-20) :** `cy = 20 / 38`, et non le `22 / 35` du POC Wokwi. L'écart (18 px au lieu de 13) est imposé par le chiffre de ratchet logé sous le step ; revenir au POC exigerait des chiffres de 4 px de haut.
>
> **Légende (validée 2026-08-17) :**
> - `○` anneau 5×5 — step **non sélectionné**
> - `●` disque plein 5×5 — step **sélectionné** (actif)
> - `▲` triangle plein 5×5 — step actif en **TRIOLET** (3 déclenchements sur 2 unités : on « ralentit »)
> - `.` 1 pixel — position **au-delà de LENGTH**
> - **chiffre sous le step** — **ratchet** `2/3/4/6` (N déclenchements dans la durée du step)
> - **barre verticale** dans la gouttière — **séparation de mesure** (graphique seule)
> - **cadre 9×9** — step en cours d'**édition**
> - **pixel central inversé** — **step joué** (blanc sur un step actif, noir sur un step vide)
>
> Aucun numéro de step, **aucune position ajoutée**. Le rendu précédent « légende de note + marques » (glyphes ronde→quadruple-croche en en-tête) est **abandonné** : la valeur de note n'est plus une notion du modèle — un step est une unité de temps, la SUBDIV fait le tempo.
### 12.9 Enregistrer un pattern — flux `SAVE`, décidé le 2026-08-26
L'instance qu'un channel édite vit en RAM et elle est persistée. `SAVE` sert à **publier** cette instance vers un modèle, pour que les autres channels puissent la charger.
1. l'instance éditée est une **copie locale** du modèle chargé ;
2. elle peut être enregistrée dans **n'importe quel slot ****`B1`**** à ****`B8`** ;
3. un slot `B` **occupé** est remplacé **après une confirmation à l'écran** ;
4. `A1` à `A8` refusent toujours l'écriture ;
5. `SAVE` n'apparaît que lorsque l'instance a été **modifiée** ;
6. après un enregistrement réussi, le channel **adopte le slot de destination** comme nouveau modèle de référence, et le drapeau de modification retombe ;
7. le choix de la destination ne détruit **jamais** l'instance éditée avant l'enregistrement.
La conception détaillée — bouton dans l'en-tête, sélecteur de destination, modale de confirmation — appartient au lot E.
---
## 13. Architecture logicielle & workflow
Trois rôles séparés :
- **TypeScript** — modèle de référence, tests, scénarios, **Gravity Simulator** visuel (feedback rapide).
- **C++ AVR** — firmware réel, intégration libGravity, PlatformIO, cible ATmega328P.
- **simavr / avr8js** — exécution du firmware AVR compilé pour validation avant hardware.
Le modèle TypeScript et le C++ **ne doivent pas devenir deux spécifications indépendantes** : parité comportementale maintenue (Pattern, PatternBank, SequencerEngine).
```javascript
Itération rapide : TypeScript → tests → Gravity Simulator → feedback
Validation firmware : C++ → PlatformIO → .elf → simavr/avr8js
Validation physique : C++ → flash → Gravity (au minimum)
```
Le simulateur reproduit l'écran OLED (police `velvetscreen` réelle décodée) pour juger le rendu final sans hardware.
---
## 14. Tests & vérification
Tout code de domaine est testé unitairement.
- **C++ natif** (`pio test -e native`, sans hardware) : Pattern, PatternBank, SequencerEngine, Transport, TriggerSequencer, Subdiv, PatternScreen. **Doit être vert.**
- **Caractérisation libGravity** (`pio test -e native_libgravity`) : décrit le comportement **réel** de la dépendance figée, anomalies incluses — donc **partiellement rouge par construction** (§18). Le critère est la **conformité à l'audit**, pas l'absence d'échec.
- **TypeScript** (vitest) : mêmes contrats en miroir + simulateur.
- **simavr** : firmware AVR réel → VCD → assertions de signal (ex. CH1). **Ne peut pas montrer l'écran** (il vérifie des signaux GPIO, pas un rendu I2C).
- **Wokwi** (extension VS Code : `wokwi.toml` + `diagram.json` à la racine) : **validation VISUELLE du rendu OLED**. Wokwi exécute le **firmware réel** compilé par PlatformIO (`pio run -e wokwi` → `src/wokwi_main.cpp`) : même domaine, même renderer, même objet `gravity.display`. **Aucune copie du code**, donc aucune dérive possible. `env:wokwi` = firmware réel + un contenu de démonstration préchargé (`main.cpp` en reste vierge), sur le même principe que `simavr_main.cpp` pour les signaux.
	**Rotation gérée côté SCHÉMA** : `diagram.json` applique `"rotate": 180` à l'écran, ce qui modélise le montage physique (OLED tête en bas) — le firmware garde donc son `U8G2_R2` **sans aucune divergence**. L'OLED est placé au-dessus de la carte : tourné, ses broches passent en bas et font face au Nano, donc les fils ne traversent pas l'écran.
	⚠️ **État de vérification, précisé le 2026-08-20.** Trois choses distinctes, qu'il ne faut pas confondre :
	· **Côté firmware : VÉRIFIÉ.** `tools/run-screen-dump.sh` lit la mémoire du panneau SSD1306 dans simavr et **assertionne** que l'image y est tournée de 180° — titre en bas, haut du panneau vide — et que les 24 steps sont à leur position attendue après `U8G2_R2`. Ce n'est plus un raisonnement mais un test : changer la constante de rotation le ferait rougir.
	· **Côté Wokwi : toujours NON vérifié.** La prise en charge de `"rotate"` par la pièce `board-ssd1306` et le routage effectif des fils n'ont pas été constatés (extension VS Code non exécutable côté assistant). Ce point n'est en revanche **plus sur le chemin critique** : la validation visuelle du rendu ne dépend plus de Wokwi. Si l'écran y reste inversé, le repli est `setDisplayRotation(U8G2_R0)` dans `wokwi_main.cpp` **uniquement** — jamais dans `main.cpp`.
	· **Côté matériel : non vérifiable en simulation.** Que l'image tournée tombe à l'endroit dépend du montage physique de l'OLED. Le PRD affirme qu'il est monté tête en bas ; aucun simulateur ne peut le confirmer, seul le module le dira (voir `env:bringup`).
- **avr8js** : prévu pour exécuter le firmware dans l'UI du simulateur.
- **Sonde de capture CV** (`tools/run-cv-capture-probe.sh`) : injecte des impulsions CV d'une largeur donnée, esclave SSD1306 attaché, et vérifie que le firmware les voit toutes. Le firmware n'est **pas** instrumenté : le harnais lit et efface le verrou dans la RAM simulée, jouant le consommateur que sera la boucle. Résultat 2026-08-20 : **toutes les impulsions de 1 ms vues, 0 ratée** (27/27 au réglage courant). La période d'injection doit dépasser la fenêtre de grâce du harnais, sans quoi une impulsion encore en attente de son consommateur est déclarée perdue à l'injection suivante — ce qui s'était lu 42 % de capture pour une latence de 116 ms. La latence de consommation est mesurée et rapportée **séparément** de la perte : le verrou garantit que l'impulsion est **vue**, pas qu'elle est consommée vite.
	**Deux réserves de fidélité, toutes deux diagnostiquées sur des résultats faux.** Il faut renseigner `vcc`/`avcc`/`aref` à 5000 mV dans la structure de firmware — elles viennent normalement de la section `.mmcu`, absente d'un `.hex` ; sans elles simavr garde une référence de \~3,3 V, le repos se lit 813 au lieu de 537, donc au-dessus du seuil d'armement, et la porte s'arme une fois sans jamais se réarmer. Et l'ADC de simavr est \~8× trop rapide (fin de conversion planifiée après `prescale` cycles au lieu de 13 × `prescale`) : la simulation valide donc la **logique** — front, verrou, réarmement, sous charge — et c'est l'**arithmétique** qui porte la garantie jusqu'au matériel. Le verdict vérifie les deux.
- **Sonde de blocage** (`tools/run-blocking-probe.sh`, `tools/simavr-ssd1306/`) : mesure la durée réelle d'un passage de boucle avec un **esclave SSD1306 réel** sur le bus I2C, corrige l'artefact de l'ADC par une mesure à deux régimes, et imprime le coût **par position dans l'image** — c'est ce dernier qui dit quelle bande paie. Ne modifie pas le firmware. Le verdict porte sur le passage courant (p90 corrigé) contre `PASS_BUDGET_MS` (défaut 12 ms) : **vert, à 8,44 ms**.
- **Fonction musicale — les six sorties** (`tools/run-trigger-probe.sh`, 2026-08-21) : la seule vérification qui réponde à « le firmware émet-il le motif écrit ? ». **Aucun binaire n'exerçait ce chemin** — `main.cpp` émet les triggers mais sa banque est vide (`patterns{}`) et aucune UI n'y écrit, `wokwi_main.cpp` porte du contenu sans `TriggerSequencer`. Le firmware n'est pas instrumenté : le contenu est écrit dans la RAM simulée à l'adresse que `avr-nm` donne pour `patternBank`, comme la sonde CV le fait du verrou. **La sonde ne suppose pas la phase du playhead** — une première version qui la supposait a déclaré « hors grille » un firmware juste, `transport.start()` ayant lieu dans `setup()` ; ce qui se vérifie sans la phase est la **suite des écarts entre impulsions, à une rotation cyclique près**, qui est aussi la seule affirmation ayant un sens musical. **Révisée le 2026-08-23 (lot 10) : DEUX COURSES sur le même firmware, une par mode de channel.** Le mode et le contenu du pattern arrivent désormais par une **image EEPROM préchargée** dans la machine simulée, fabriquée par `tools/eeprom-image.cpp` qui lie le code du domaine : le harnais ne porte aucune copie du format et n'écrit plus dans `patternBank`. La course **CLOCK** vérifie un train régulier, un écart par step, le motif restant dans la banque **sans être joué** ; la course **SEQ** vérifie que la suite des écarts est une rotation cyclique de celle du motif.
Mesuré le 2026-08-23, 20 s par course : **6/6 sorties** dans les deux, **38/38 écarts** en CLOCK et **11/11** en SEQ, **499,96 à 499,97 ms** par step contre 500,00 attendus, fronts coïncidents à moins de 200 µs. La gigue **change d'une course à l'autre**, 1,01 à 1,29 ms sur quatre courses : c'est une borne, pas une valeur, et le budget est de 2 %.
**L'asymétrie est la preuve.** `MUTATE=7` ajoute un step à l'image et pas à l'attente : SEQ rougit (6/14), CLOCK reste vert (38/38). Aucune course seule ne pouvait établir que CLOCK ignore la banque ; deux courses sur un firmware le font. `DROP=3` rougit les deux, `JITTER_BUDGET_PCT=0.1` rougit la gigue. `MUTATE` ne mord que **sous** la LENGTH jouée, 16 par défaut.
Deux constats rapportés et non assertés. L'impulsion a été mesurée à **8,8 ms** le 2026-08-21 contre les 5 ms de `DEFAULT_TRIGGER_DURATION_MS`, ce qui fut lu comme 5 ms arrondis au passage suivant — l'extinction étant en fin de `loop()`. **Remesurée depuis : 4,70 ms, puis 4,78 ms en CLOCK et 5,01 ms en SEQ.** Elle passe donc **sous** la valeur configurée et **change avec le mode** : l'explication ne tient plus, le chiffre reste. Suivi à la ligne 12 de `docs/open-risks.md`. Et **`PULSE`**** reste muet**, `main.cpp` ne pilotant pas `gravity.pulse`.
- **Vidage de la mémoire du panneau** (`tools/run-screen-dump.sh`) : lit l'image que le SSD1306 affiche réellement et assertionne la géométrie après `U8G2_R2` (24/24 steps à leur place) ainsi que la rotation. `WATCH=` échantillonne le panneau en continu et vérifie qu'**aucune bande, une fois qu'elle a porté de l'encre, ne se retrouve vide** — la seule façon dont le saut de bande (§12) pourrait scintiller. **20 000 relevés, un toutes les 0,5 ms : aucune.** Les minima bas mais non nuls sont des états **en cours de transfert**, une bande partant en six transactions ; inhérent à toute mise à jour partielle.
- **Sonde de pile** (`tools/run-stack-probe.sh`) : mesure à l'exécution la pile réellement consommée du firmware **de production**, que le linker ne peut pas voir. Un harnais C peint la RAM avant le premier cycle et injecte du trafic d'interruption ; le firmware n'est pas instrumenté. Résultat et méthode au §15.
> ⚠️ **Lacune de vérification — relevée le 2026-08-20.** Le rendu et le chronométrage n'ont **jamais tourné ensemble** : `src/simavr_main.cpp` ne contient aucun rendu, donc les durées de step validées en simavr (§6.3 : 250 ms / 511 ms) l'ont été **sans affichage**. Or une image pleine représente \~25 ms de bus I2C. **À mesurer :** le blocage réel de la boucle principale, et la largeur minimale d'impulsion CV effectivement captée (§10.6, ADR 0001). Cette mesure ne dépend d'aucune décision produit.
>
> **Lacune LEVÉE le 2026-08-20.** Le firmware complet — rendu compris — tourne sous simavr, ce qui n'avait jamais eu lieu ; la **pile** y est mesurée (§15) et le **blocage réel** aussi, un vrai esclave SSD1306 sur le bus.
>
> **Comment.** `tools/run-blocking-probe.sh` et `tools/simavr-ssd1306/` câblent l'esclave `ssd1306_virt` de simavr sur le TWI, dans les deux sens, et chronomètrent les transferts. Le binaire générique `run_avr` n'attache aucune pièce — un transfert y avorte sur NACK dès l'octet d'adresse, et toute durée mesurée là serait bien trop courte ; c'est ce qui avait fait écrire ici, à tort, que seuls Wokwi ou le module pourraient trancher. Le firmware n'est pas instrumenté : pendant le rendu, chaque passage envoie exactement une bande (ADR 0001), donc le trafic I2C borne directement la réponse. Les bandes sont groupées **par le protocole et non par un seuil de temps** — l'octet de contrôle U8g2 vaut `0x40` pour les données et `0x00` pour les commandes — et le contrôle des 128 octets par bande valide le découpage.
>
> **Résultats (estimation matérielle, à jour au 2026-08-21).** Transfert d'une bande **4,62 ms** · passage de boucle pendant le rendu **6,13 ms** en médiane et **8,44 ms** en régime courant (p90) · image courante **44,2 ms**, un passage par bande. Une bande part en **6 transactions Wire** de \~21 octets.
>
> Le pic de **15,31 ms** ne subsiste que sur le **rafraîchissement complet périodique** — une image de **59,5 ms** : le verdict porte donc sur le passage courant et rapporte ce pic séparément, avec sa fréquence.
>
> **Ce pic est désormais mesuré et non étiqueté, sa fréquence aussi.** Il ne concerne qu'une image sur seize et les images sont espacées de \~470 ms : à l'ancienne durée par défaut de 8 s, **aucune** image complète ne tombait dans le régime sans ADC, et le maximum d'une image **courante** héritait de la légende « rafraîchissement complet périodique ». Les deux populations sont séparées par le protocole — une image complète envoie **8** bandes, une image courante moins — le harnais dit explicitement quand il n'a rien observé, et `DURATION` vaut **32 s** pour qu'il en observe. Le ratio sort à **1 image sur 16,0**, lu pour la première fois sur une mesure et non sur `FULL_REFRESH_EVERY`.
>
> **La durée d'une image se mesure dans un seul régime.** Mélangée, la distribution est bimodale — \~41,7 ms sans ADC contre \~55,7 ms avec — et les deux populations sont à peu près à égalité : la médiane basculait d'un mode à l'autre selon la durée du run (41,8 ms à 8 s, 55,7 à 32 s). Le « 41,8 ms par image » publié le 2026-08-20 était le côté d'une pièce lancée en l'air.
>
> **Les images sont délimitées par l'adressage de page du SSD1306** (`0xB0 | page`, cherché parmi les octets d'une transaction de commande, U8g2 en groupant plusieurs). C'est ce qui a retiré le dernier seuil temporel de cette mesure — devenu nécessaire dès que le saut de bande a rendu le nombre de bandes par image variable, ce qui avait fait annoncer des images de 504 ms.
>
> **L'artefact de l'ADC est CORRIGÉ**, par une mesure à **deux régimes en une seule exécution** : simavr déclenche son ISR d'ADC \~4× trop souvent, donc à la moitié du run le harnais efface le bit `ADIE` d'`ADCSRA` — et comme c'est l'ISR qui relance les conversions, les couper les arrête toutes. La correction est un rapport de **fractions de CPU** et non une soustraction de maxima (leurs valeurs extrêmes viennent d'événements différents) : **22 % volés en simulation, 5,6 % sur matériel**, la cadence simulée étant *mesurée* au compteur de conversions du firmware.
>
> **La cadence se mesure sur une fenêtre intérieure, et il faut qu'il en soit ainsi.** Elle était obtenue en divisant **toute** la première moitié du run par le nombre de conversions achevées — or celles-ci ne démarrent qu'à `cv::start()`, après les constructeurs globaux, `init()` d'Arduino et l'initialisation de l'afficheur. Compter cet intervalle mort comme du temps de conversion faisait **dépendre la cadence de la durée de la mesure** : 31,5 µs à 4 s, 28,5 à 8 s, 27,2 à 16 s. Mesurée entre le quart et la moitié du run, elle vaut **26,0 µs aux trois durées**. C'est de là que vient la correction de 4,9 % à 5,6 %.
>
> ⚠️ **Chiffres publiés antérieurement : INVALIDES.** « 47 ms par image, 7,74 ms au pire, dessin à 1,24 ms » avaient été relevés sur le build dont la conversion de bande était inversée (§12) : l'écran ne dessinait presque rien, et ces chiffres mesuraient surtout l'absence de dessin.
>
> ⚠️ **Une cause a été affirmée ici, puis démentie par la mesure.** Il avait été écrit que le pic venait du passage dessinant une **ligne de 12 steps**, déduit du fait qu'un passage sur sept était long et qu'une image compte sept intervalles. C'était une **inférence géométrique, fausse**. En mesurant le coût par POSITION dans l'image, la bande coûteuse est celle des lignes logiques 0-7 : le **titre**. Attribué par expérience — avec `title = nullptr` cette bande tombe de 15,30 à 5,49 ms — puis décomposé : `getStrWidth` \~1 ms, `drawStr` **\~8,8 ms**, soit **\~0,59 ms par caractère** avec `u8g2_font_5x7_tf` sur ce MCU. Leçon de méthode : une distribution bimodale dit *combien* de passages sont lents, jamais *lesquels*.
>
> **Deux estimations corrigées.** « \~25 ms de bus par image » et « \~3 ms par bande » étaient **basses** : le calcul ne comptait que les bits, pas le découpage — les intervalles entre morceaux coûtent \~107 µs chacun, et le bus fait bien ses 400 kHz (22,5 µs par octet, plus 4,55 µs d'ISR TWI). Le vrai total est **36,8 ms de bus** par image.
>
> **Ce que ça change pour le CV : voir §10.6.**
>
> **Trois outils écartés, vérifiés :** `-fstack-usage` rend des fichiers vides (`-flto` déplace la génération de code au link) ; l'`avr-gdb` de la toolchain PlatformIO (2019) est lié à Python 2.7 et ne démarre plus sur macOS courant ; la trace `sram16` de simavr n'émet que des horodatages, **sans valeur** (`$var wire 0`), seule `portpin` étant exploitable. Piège associé : `run_avr` **n'honore pas un chemin de VCD absolu**. Depuis que les harnais sont écrits en C et lisent directement la mémoire simulée, ces limites ne coûtent plus rien.
>
> ✅ **« Le tas est corrompu » était une lecture hors bornes d'un octet dans simavr — résolu le 2026-08-21.** AddressSanitizer l'a nommée 3 fois sur 3 : simavr arme `AVR_UART_FLAG_STDIO` par défaut, accumule les octets écrits dans `UDR` dans un tampon de 256 o, puis le passe à `vfprintf` en `%s` **sans le terminer** quand il est exactement plein. Notre firmware alimente ce chemin — le MIDI sort par le port série. La faute tombait dans `libsystem_malloc` parce que la lecture traversait les métadonnées de l'allocateur, non parce qu'une de nos allocations était fautive.
>
> Les quatre harnais désarment ce drapeau juste après `avr_load_firmware()` (`tools/simavr-ssd1306/simavr_uart_quiet.h`). simavr expose lui-même l'interrupteur, donc la dépendance épinglée n'est pas touchée, et un harnais de mesure n'a aucun besoin d'un journal de console — aucune donnée mesurée n'y passe. Avant/après sur `blocking_probe` : **2 SIGSEGV sur 5 → 0 sur 5**, **3 rapports ASan sur 3 → 0 sur 3**, et **256 octets d'UART déversés dans le rapport → 0**. Ces 256 octets étaient le tampon vidé dans `stdout` au milieu du rapport : c'est pour eux que les scripts relisaient leur log en `errors='replace'`.
>
> Deux habitudes restent, pour leur mérite propre et non comme contournement : les tableaux de statistiques sont **statiques** (rien ne serait gagné à les allouer), et le script rapporte une sortie anormale plutôt que de jeter un rapport complet. Corollaire toujours valable : `stdout` redirigé étant tamponné par blocs, une sortie non tamponnée est indispensable, sans quoi le rapport disparaît au plantage et l'on cherche le défaut là où il n'est pas.
Niveaux : Domaine → Virtuel/Simulateur → Firmware AVR (simavr = signaux, Wokwi = écran) → Hardware réel.
---
## 15. Empreinte mémoire (mesurée)
Build `nanoatmega328`, `libGravity` figé au commit `4c5b4d0b4f38…` du fork du projet.
**⚠️ CHIFFRES COURANTS, mesurés le 2026-08-30 après le lot S.** Tout ce qui suit est antérieur et conservé comme historique.
**RAM 1338 / 2048 (65,3 %)**, 710 o libres · **Flash 27320 / 30720 (88,9 %)**, **1864 o** avant le garde-fou de 95 % qui est à 29184 o · **pic de pile 205 o**, marge 505 o, couvert 1,2× par la réserve de 256 · tests C++ 458, adaptateur 5, image EEPROM 14, TypeScript 451, typage propre, caractérisation libGravity conforme · **mutation 230/230** · sondes : gestes 103, dérive 222/222, frontière EEPROM 588/588, rendu identique à 863 pixels d'encre · 7 environnements AVR compilent.
**Le lot S rend 126 o de Flash et coûte 0 o de RAM**, mesuré le 2026-08-30. Un **désinlining ciblé** : `clampIndex`, `wrapIndex` et `oneStep` portent `noinline`, et `clampRange` reste inline. Le pic de pile ne bouge pas. La décision, les variantes essayées et les contre-épreuves vivent dans l'**ADR 0010** ; ce paragraphe ne les recopie pas.
Les chiffres du 2026-08-30 qu'ils remplacent, après le lot B4b.7 : RAM 1338 / 2048 (65,3 %), Flash 27446 / 30720 (89,3 %).
Les chiffres du 2026-08-28 qu'ils remplacent : RAM 1699 / 2048 (83,0 %), 349 o libres, Flash 27164 / 30720 (88,4 %), marge 144 o, tests C++ 422 et TypeScript 415.
**⚠️ Le pic transitoire du lot B4b est TERMINÉ depuis le 2026-08-30.** La banque résidente de 368 o et les six instances de 138 o ont coexisté de B4b.3 à B4b.7, parce que rien n'est retiré avant que les tests aient prouvé qu'il peut l'être. **B4b.7 rend 370 o mesurés, et non 230** : 368 o pour la banque et 2 o pour le champ pointeur `bank_`, qui ne part qu'avec l'API elle-même. Le relevé de dérive a été acquitté le 2026-08-30, commit `4e2a24d`, et le garde de `run-build-memory.sh` est vert.
**⚠️ ****`PatternBank`**** reste dans le dépôt.** Le lot retire la dépendance du moteur à la banque, pas la banque du projet : `PersistentImage` v2, `loadFactoryPatterns`, le générateur d'image, `gestureRecipes` et `PATTERN_COUNT` s'en servent encore.
Chiffres **remesurés le 2026-08-22**, firmware **complet** : deux écrans, les huit gestes, le transport, la persistance, tout câblé.
Chiffres **remesurés le 2026-08-23**, après le lot 9 (les trois modes de channel) : **RAM 1731 / 2048 (84,5 %)**, 317 o libres · **Flash 28538 / 30720 (92,9 %)** · **pic de pile 207 o**, couvert 1,2× par la réserve de 256 · dérive +0/+0 · **269 assertions C++**, 226 TypeScript.
Chiffres **remesurés le 2026-08-23**, après les lots 20 et 21 (couverture des cadences, puis placement des sous-déclenchements) : **RAM 1713 / 2048 (83,6 %)**, 335 o libres · **Flash 28916 / 30720 (94,1 %)** · **pic de pile 210 o**, couvert 1,2× par la réserve de 256 · dérive +0/+0 · **297 assertions C++**, 254 TypeScript, **score de mutation 54/54**. Il reste **79 o** de RAM au-dessus de la réserve et **268 o** de Flash sous le garde-fou.
Le lot 21 a coûté **Flash +142 o** et **RAM −12 o** : la RAM baisse parce que le champ `slotTicks` disparaît du moteur, deux octets par channel.
Les chiffres du lot 10 qu'ils remplacent : RAM 1725 (84,2 %), Flash 28774 (93,7 %), pile 207 o, 278 assertions C++ et 235 TypeScript. Le lot a coûté **RAM −6 o / Flash +236 o** : la RAM baisse parce que l'offset passe à un octet, six channels. Il reste **67 o** de RAM au-dessus de la réserve et **410 o** de Flash sous le garde-fou.
Les chiffres du lot 9 qu'ils remplacent : RAM 1731 (84,5 %), Flash 28538 (92,9 %), 269 assertions C++ et 226 TypeScript, 61 o de RAM au-dessus de la réserve et 646 o de Flash sous le garde-fou. Ce garde-fou est à **95 %** depuis le 2026-08-22, non plus à 90 % : partout où la suite de cette section écrit 90 %, lire 95 %.
Les chiffres du 2026-08-22 qu'ils remplacent : RAM 1699 (83,0 %), Flash 28228 (91,9 %), pile 206 o, 245 assertions C++ et 202 TypeScript.
⚠️ **Tout ce qui suit dans cette section date du 2026-08-21 et décrit un firmware où l'UI n'était pas câblée.** Les estimations y sont désormais remplaçables par des mesures : l'UI reliée a coûté **RAM +26 o / Flash +1224 o** (estimée \~16 o), la persistance **+10 o / +1044 o** (estimée \~8 o), le transport **+10 o / +1126 o** (estimé \~0 o, l'objet `clock` étant déjà alloué — mais `Clock::SetSource` ne l'était pas). La marge « d'environ 5× » annoncée ne s'est pas vérifiée : il reste **93 o** de RAM au-dessus de la réserve, pas 264.
>
> **LE BUDGET EST DIMENSIONNÉ, PAS SEULEMENT SURVEILLÉ (2026-08-21).** Il était dit « sous garde » sans jamais dire combien il restait ni pour quoi faire, ce qui laissait la question ouverte à chaque fonctionnalité.
>
> **RAM statique : 1528 o sur 2048, donc 520 o libres.** La réserve de pile étant de 256 o (pic mesuré **159 o**, couvert 1,6×), il reste **264 o pour de nouvelles données statiques**. Où sont passés les 1528, par ordre de taille : `gravity` **300 o** (l'objet libGravity), `patternBank` **240 o** (16 × 15), `NeoSerial` **159 o**, le tampon de page U8g2 **128 o**, les quatre tampons TWI **128 o**, `engine` **110 o**, `uClock` **67 o**, la séquence d'init u8x8 **53 o**, `uiScreen` **34 o**.
>
> **Ce qu'il reste à construire y tient, avec une marge d'environ 5×** — estimation, base indiquée, à ne pas confondre avec les mesures ci-dessus :
>
> \| À construire \| RAM estimée \| Base de l'estimation \|
> \|---\|---\|---\|
> \| UI reliée (§12) \| \~16 o \| curseur, mode d'édition, index de menu, accumulateur d'encodeur \|
> \| Transport, EXT et MIDI (§8) \| \~0 o \| `uClock` et l'objet `clock` sont **déjà** alloués \|
> \| Persistance (§11) \| \~8 o \| la banque est **déjà** en RAM : l'écriture EEPROM la lit sur place, sans copie \|
> \| Destinations CV (§10.2) \| \~24 o \| 6 channels × 2 entrées × 1 octet de cible, plus l'état de quantification \|
> \| RECORDING (§5.5) \| \~4 o \| un drapeau et un step en attente \|
> \| **Total** \| **\~52 o** \| contre **264 o** disponibles \|
>
> **Flash : 21404 o sur 30720.** Le garde-fou refuse à 90 %, soit 27648 o : il reste **6244 o** avant refus et 9316 o avant la limite dure. Points de comparaison mesurés dans ce dépôt — l'échantillonnage CV a coûté **+354 o**, l'écartement par bande **+522 o**, `PagedScreen` **+160 o**. Une UI complète avec ses menus est le seul poste vraiment coûteux à venir, de l'ordre de **2 à 4 ko** : cela passe, et chaque étape est rapportée par le garde-fou.
>
> **Le déclencheur est explicite**, il n'y a plus à en juger au cas par cas : échec au-delà de **+16 o de RAM** ou **+512 o de Flash** non acquittés, plafonds à **256 o libres** ou **90 % de Flash**. `--accept` ne se fait jamais sans regarder le diagnostic par symboles.
>
> ⚠️ **L'unique trou de la mesure de pile, et son obligation.** La sonde mesure ce que le firmware **exécute pendant le run**. L'écriture EEPROM de la persistance n'y est pas parce qu'elle n'existe pas — mais elle n'y sera pas non plus **automatiquement** le jour où elle existera : il faudra que le run la **provoque**. C'est la seule chose à ne pas oublier en implantant le §11. Les anciennes valeurs « Pattern 4 B / 384 B / 578 B libres » sont **caduques**.
> ⚠️ **Toujours la mesure AVR, jamais la mesure native.** `sizeof(SequencerEngine)` vaut 120 B compilé sur hôte x86 mais **110 B sur AVR** (relevé à `avr-nm` sur le `.elf`) : c'est cette dernière qui fait foi (règle `CLAUDE.md`).
> Le saut de Flash depuis 16316 B vient d'abord du **rendu OLED** (primitives U8g2 + police `u8g2_font_5x7_tf`), puis de l'échantillonnage CV sous interruption (§10.6) et du saut de bande (§12).
> ⚠️ **Un garde-fou de dérive existe depuis le 2026-08-20** : chaque build est comparé à `tools/memory-baseline`, versionné, et une croissance au-delà de 16 o de RAM ou 512 o de Flash **échoue**. L'accepter est un acte délibéré (`--accept`). Un plafond ne se déclenchant qu'à 90 % de Flash, il laissait passer une fonctionnalité de 3 ko sans un mot.
<table header-row="true">
<tr>
<td></td>
<td>Ancien modèle (6×16)</td>
<td>Modèle actuel (banque 16)</td>
</tr>
<tr>
<td>`sizeof(Pattern)`</td>
<td>7 B</td>
<td>**15 B** (3 steps + 12 ratchets)</td>
</tr>
<tr>
<td>Stockage patterns</td>
<td>672 B</td>
<td>**240 B**</td>
</tr>
<tr>
<td>`SequencerEngine`</td>
<td>—</td>
<td>**110 B** (dont le cache de timing par channel)</td>
</tr>
<tr>
<td>**RAM firmware**</td>
<td>1758 B (85.8 %)</td>
<td>**1528 B (74.6 %)**</td>
</tr>
<tr>
<td>**RAM libre**</td>
<td>\~290 B</td>
<td>**520 B**</td>
</tr>
<tr>
<td>Flash</td>
<td>15800 B</td>
<td>**21404 B (69.7 %)**</td>
</tr>
</table>
La contrainte RAM passe de **critique à confortable**.
### Pile — mesurée, et non plus estimée (2026-08-20)
> **Pic mesuré : 159 octets**, sur 520 libres — **361 o de marge**. Relevé à l'exécution sur le firmware **de production**, sans une ligne d'instrumentation (`tools/run-stack-probe.sh`).
>
> ⚠️ **À remesurer après chaque changement structurant** : le pic a bougé à chacun d'eux (120 → 154 → 159 o).
>
> **Méthode :** un harnais C écrit un motif dans la RAM libre de la machine simulée **avant le premier cycle**, laisse tourner le firmware en injectant du trafic d'interruption, puis relit la frontière du motif intact. Le balayage part du **haut** : `__heap_start` valant `_end`, une allocation salirait le bas et ferait conclure à tort que la pile est descendue jusque-là.
>
> **Les deux angles morts sont FERMÉS (2026-08-20), et ils valaient 43 o.** La version précédente peignait depuis le firmware au début de `setup()` et publiait le résultat en largeur d'impulsion, faute de pouvoir lire la mémoire du simulateur : elle annonçait 120 o, ignorait la pile d'avant `setup()` (constructeurs globaux, `init()` d'Arduino : **+24 o**) et n'exerçait aucune ISR d'entrée (**+19 o**). Elle exigeait une sonde dans `main.cpp` et un environnement dédié ; les deux sont supprimés.
>
> **Couverture vérifiée, pas supposée.** Le verdict exige que **les six familles d'ISR aient été parcourues** — PCINT1/PCINT2 de l'encodeur (les seules broches sous PCINT dans libGravity ; les boutons sont scrutés), uClock, millis, MIDI en réception, et l'ADC. Un vecteur muet fait échouer la mesure, puisque c'est précisément ainsi qu'elle était incomplète en silence.
>
> ⚠️ **Restent hors mesure :** les chemins que le firmware n'emprunte pas encore — l'écriture EEPROM de la persistance (§11) au premier chef, qui n'existe pas.
>
> **Conséquence — seuil de réserve ramené à 256 o (décidé 2026-08-20).** Le seuil de `tools/run-build-memory.sh` valait 512 o, posé à l'estime avant toute mesure : il annonçait 46 o de marge alors que la consommation réelle était bien en dessous du seuil lui-même. À 256 o, le budget réellement disponible est de **264 o** et la réserve couvre le pic **1,6×** — la marge s'est réduite au fil des mesures complètes, sans devenir étroite. Ne pas le relever sans une **nouvelle mesure**.
>
> **Levier tenu en réserve :** `NeoHWSerial` protège ses tailles de tampons par `#if !defined(...)`, donc `-DSERIAL_TX_BUFFER_SIZE=16 -DSERIAL_RX_BUFFER_SIZE=32` récupère **80 o** par simple option de compilation, sans toucher à la dépendance. Les 160 o de tampons de `Wire` (dont deux en réception, inutiles pour un écran qu'on ne lit jamais) et les 77 o de tables U8g2 déclarées sans `PROGMEM` exigeraient au contraire de patcher des dépendances — hors décision séparée.
---
## 16. Décisions — validées vs ouvertes
⚠️ **La revue de la version de référence du 2026-08-23 (§5.0) supersède cinq entrées de cette liste** : la banque partagée résidente, les 24 steps, la LENGTH comme propriété du seul channel, les trois modes, et `SHIFT` + `PLAY` réservé à RECORDING. Les décisions en vigueur sont celles du §5.0.
**Ajoutées le 2026-08-23 :** `main` @ `40d4aac` est la référence, `1.2-dev` un catalogue · patterns **template en EEPROM, instance par channel** · **36 steps**, **un nibble par ratchet** (ADR 0007) · LENGTH **déduite au chargement** puis propre au channel — ⚠️ formulation périmée, le modèle la STOCKE (§5.0 point 3) · **A1–A8 figés** · quatre modes avec **GATE** · **SWING paramètre de SEQ**, 0–49 %, plafonné · **mute** sur `SHIFT` + `PLAY` · **RECORDING** sur `SHIFT` + appui court · barre à **9 onglets** + indicateur de transport **fixe** · roue crantée pour CONF, mini-grille pour PATTERNS · tempo **20–200**, impulsion **5 ms** · grille **3 lignes de 12** sans pied · onglet PATTERNS réutilisant `LEVEL_EDIT` · **7e channel écarté**
**Validées :** banque de 16 patterns partagés · LENGTH par channel · `masterPhase` (96 PPQN, `uint32`, phase locale lissée) · **SUBDIV → ticksPerStep par channel** (convention libGravity, `/N`·`xN`, défaut `/1`) · **un changement de SUBDIV prend effet au prochain temps** (§6.1.1, décidé le 2026-08-23, ADR 0004) · Transport (mapping clock 96 PPQN → moteur) · génération de triggers (vérifiée simavr) · **un step = une unité de temps** · **séparation de mesure purement graphique** (aucune/2/3/4/6, par channel) · **RATCHETS par step** (2/3/4/6 + TRIOLET ▲ qui étire sur 2 unités) · géométrie et légende EDIT PATTERN (POC Wokwi) · **mapping CV complet** (§10 : destinations PATTERN / LENGTH / RESET / STEP par channel, application à la frontière de step sauf RESET, seuils de Schmitt +1 V / +0,5 V, routage survivant au changement de mode) · **ratchet 5 écarté** · **édition en cours de lecture conservée** · **espacement EDIT ****`20 / 38`** · **rendu OLED étalé sur ses 8 bandes** (ADR 0001) · **seuil de réserve RAM ramené de 512 à 256 o** sur la foi d'une mesure de pile (§15) · **CV échantillonné sous interruption, garantie 1 ms** (§10.6) · **en-tête conservé explicite** et **saut de la bande inchangée** (§12) · **garde-fou de dérive mémoire** avec relevé versionné (§15) · **format de persistance EEPROM v2** (§11.1, implémenté le 2026-08-23 : 304 o, 9 o par channel, deux octets de cible CV réservés pour le §10.2) · **offset sur UN octet**, fidèle au `uint8_t offset` de l'original, la limite étant conservée telle quelle.
**Les six décisions de l'audit de conformité, tranchées le 2026-08-23 :**
1. **La séparation ****`MODE`**** + ****`PPQN`**** revient**, comme dans l'original : `MODE` porte INT / EXT / MIDI, et `PPQN` n'apparaît qu'en EXT. La fusion en un champ `SRC` de six valeurs est abandonnée. **`PPQN`**** expose les QUATRE cadences de libGravity** (24, 4, 2, 1) et non les deux de l'original — addition assumée, décidée le 2026-08-23, qui n'enlève rien. Les deux champs sont **deux vues d'un seul octet de source**, donc `PPQN` ne coûte aucun octet EEPROM et aucun état incohérent n'est représentable. Détail au §8.1.
2. **La bascule d'un pas reste sur l'appui d'encodeur**, et **RECORDING prendra ****`SHIFT`**** + ****`PLAY`**** ensemble** — un **neuvième geste**, que l'original n'a pas. Ce n'est pas un rappel de fonction mais un état : `input::shiftHeld()` existe déjà, donc PLAY demande si SHIFT est enfoncé. Le transport garde PLAY seul.
3. **Le seuil d'appui long reste à 750 ms**, celui de libGravity, et non les 300 ms de l'original. Divergence assumée : 750 ms est **mesuré** sur le module (dix appuis délibérés, dix appuis courts, aucun faux long), 300 ms ne l'est pas, et la constante n'étant pas réglable dans la dépendance épinglée il faudrait chronométrer l'appui dans l'adaptateur.
4. **L'accélération de l'encodeur disparaît partout, y compris sur le tempo.** L'original n'accélère pas, et FlexSeq n'aura donc plus une règle et une exception mais une règle. **Conséquence assumée** : la plage de tempo compte 270 valeurs, donc la traverser demande 270 crans.
5. **Le filtre d'inversion reste à 12 ms**, et non les 200 ms de l'original. Divergence assumée, et c'est la seule qui repose sur une mesure des deux côtés : le rebond le plus rapide fait 2 ms, une inversion délibérée prend 509 à 1003 ms, et 200 ms avalerait une correction vive.
6. **La chance de saut plafonne à 9**, soit 90 %, comme l'interface d'origine. La valeur **effective** atteint toujours 10 par la modulation CV, comme dans l'original. Un channel muet se fait en changeant de mode, pas en réglant 100 %.
7. **`RANGE`**** revient maintenant, et le format passe en version 3.**
⚠️ **CE QUE LE PASSAGE EN V3 COÛTE, en octets et en données.** La zone globale de la version 2 tient 3 octets — le tempo sur deux, la source sur un. La restitution de l'onglet BPM demande `MOD` (la voie qui module le tempo) et `RANGE`. `MODE` et `PPQN` ne coûtent rien : ce sont deux vues de l'octet de source (§8.1). La zone passe donc à **5 octets** — tempo (2), source (1), `MOD` (1), `RANGE` (1). C'est de l'arithmétique, pas un choix.
⚠️ **Les deux octets sont DÉJÀ dans la version 3, réservés et inertes.** Le §11.1 fait foi pour la disposition et pour la taille de l'image. Le total de 306 octets écrit ici auparavant est **superséded** : il précédait la fondation 36 pas et le modèle template / instance. L'image v3 fait **588 octets**.
Un changement de version fait **repartir des défauts** : l'état FlexSeq écrit sur le module depuis le premier flash est perdu. Les réglages du firmware d'origine, sous l'adresse 320, ne sont jamais touchés. Le propriétaire a choisi de le faire **maintenant**, au moment de la série où le module contient le moins de travail.
**Abandonnés :** METER / MEASURES (signature rythmique et valeur dérivée) · groupes ternaires « 3 steps consécutifs » · glyphes de valeur de note (ronde→quadruple-croche) · écran CONFIG avec MEASURES · **CV → position absolue de STEP** (serait un nouveau mode de channel, pas une destination) · **CV → Reset global** (le Reset global relève de l'entrée d'horloge externe) · **usage de ****`AnalogInput::IsRisingEdge()`** (anomalie auditée, §10.5) · **raccourcissement de l'en-tête EDIT PATTERN** (la clarté primait) · **cache RAM de la bande du titre** (128 o : la RAM reste pour des fonctionnalités) · **appel à ****`Gravity::Process()`** depuis la boucle principale (incompatible avec l'ADC sous interruption ; ses morceaux sont appelés séparément, §10.6).
**Ouvertes / différées :** hooks d'événements transport MIDI/Ext distincts · backend avr8js dans le simulateur · **la fusion de la source d'horloge et du PPQN dans un seul champ ****`SRC`** : l'original sépare `MODE` (INT/EXT/MIDI) et `PPQN`, FlexSeq les a fondus en cinq valeurs (`INT`, `EXT24`, `EXT4`, `EXT2`, `EXT1`) — divergence **jamais décidée**, apparue à l'implémentation du transport, à trancher à l'audit de conformité · **une seule police pour tout l'écran** : `setFont(u8g2_font_5x7_tr)` est appelé une fois dans `main.cpp` et jamais changé, donc aucun paramètre ne se lit comme le principal ; les dix glyphes maison qui devaient remplacer `logisoso26` sont conçus, non implémentés · **`RANGE`**** n'a pas d'octet dans le format v2** : le §10.1 le conserve et l'original le persiste, donc son retour demanderait une image de 305 octets et un octet de version à 3 · **les valeurs ****`EXT2`**** et ****`EXT1`** : l'original n'offre que `24` et `4` en PPQN, et libGravity sait faire les quatre. Lu au pied de la lettre, « comme sur l'original » les supprime — seul reste ouvert de l'audit du 2026-08-23 · **mode RECORDING** (§5.5 — acquis de conception posés, affectation des contrôles physiques à définir ; non prioritaire) · **destinations CV** (§10.2) : la mécanique d'échantillonnage et la détection de front existent et sont vérifiées (§10.5, §10.6) ; le routage par channel, la quantification et l'application à la frontière de step restent à implémenter · **échantillonnage conditionné au routage** : les 5,6 % de CPU de l'ISR sont payés même quand aucun channel ne route de CV — **différé au §10.2 par décision du propriétaire (2026-08-21)**, avec sa raison : isolé le conditionnement serait inapplicable, aucun channel ne routant de CV aujourd'hui, donc conditionner reviendrait à couper le CV · **contrôles non reliés dans ****`main.cpp`**** — constaté le 2026-08-21** : le binaire de production n'appelle que `clock.AttachIntHandler()`. Ni `AttachExtHandler`, ni `SetSource`, ni `SetTempo`, ni `Start`/`Stop` ; et les boutons et l'encodeur ne sont que `Process()`-és, aucun callback n'étant relié à une action. Le firmware joue donc l'horloge **interne à 120 BPM fixe**, transport démarré d'office, six channels sur le pattern 0, plus l'écran — et rien n'est pilotable. Ce n'est pas un défaut mais l'état d'avancement : à relier avec l'UI (§12) et le transport (§8). Conséquence directe sur le premier flash : il validerait la **chaîne matérielle**, pas les fonctionnalités, qui ne sont pas encore là.
- **premier flash physique** — **différé par décision du propriétaire (2026-08-21) : le module est disponible, l'attente est volontaire et ne tient plus à une précondition manquante** : toutes les préconditions du §17 sont remplies (la référence « §12 » portait à faux : §12 est l'UI) et `env:bringup` rend ce flash diagnosticable ; deux gestes le précèdent, établis au §2 — vérifier que le module est **Rev 2+** (bouton SHIFT présent) et **sauvegarder Flash + EEPROM** ; ce qui reste ne se lève qu'en flashant (montage de l'OLED, conversion CV réelle, câblage des sorties, horloge externe).
---
## 17. Contraintes & règles de développement
- Ne pas modifier le hardware ni `libGravity` (sans décision séparée et versionnée).
- Surveiller RAM/Flash à chaque étape structurante.
- Éviter les allocations dynamiques dans le domaine embarqué.
- Tester unitairement tout nouveau comportement.
- Préserver la possibilité de restaurer le firmware original.
- Ne pas faire dépendre le firmware du Mac ni de Node.js ; TypeScript2Cxx et avr8js ne sont pas des dépendances runtime du module.
- **Cycle PRD :** conception → prototype/tests → décision → relecture PRD → mise à jour normative → implémentation.
---
## 18. Dépendance libGravity — anomalies auditées
`libGravity.cpp` (`Gravity::Process`) contient une boucle à index **non initialisé** : `for (int i; i < OUTPUT_COUNT; i++)` (comportement indéfini). libGravity étant figée et non modifiable, FlexSeq **contourne** en pilotant l'auto-extinction des sorties explicitement dans sa boucle principale. À remonter en amont si une évolution de la dépendance est envisagée.
> **Suite de caractérisation — restaurée le 2026-08-20.** Les anomalies auditées ne sont plus décrites seulement par de la prose : l'environnement PlatformIO `native_libgravity` les **reproduit**, avec **7 assertions rouges par construction** sur 68, réparties sur `AnalogInput`, `Button`, `Encoder` et `DigitalOutput`. `test_gravity` porte en outre, depuis le 2026-08-20, la **caractérisation de ****`Gravity::Process()`** : elle lit exactement deux entrées analogiques — CV1 et CV2, une fois chacune — et scrute les deux boutons plus le bouton de l'encodeur. FlexSeq n'appelant plus cette fonction (§10.6), le risque n'est pas de rater une évolution amont — la dépendance est épinglée par décision — mais d'oublier de la re-auditer au prochain changement d'épingle. Un test jumeau vérifie que les morceaux appelés à la place couvrent les mêmes entrées avec **zéro** lecture analogique. Le script `tools/run-libgravity-tests.sh` vérifie que l'ensemble des échecs est **exactement** celui audité, et échoue sur toute dérive dans les deux sens — une anomalie qui disparaît comme un échec inattendu. Liste normative des anomalies : `CLAUDE.md` ; détail par test : `test/README`. Ces six tests avaient cessé de compiler pendant quatre mois sans émettre de signal, un `test_filter` les ayant retirés de la collecte dans le même commit qui supprimait leurs chemins d'inclusion. **Vérifié le 2026-08-20 :** aucune de ces anomalies n'est corrigée en amont, 3 commits après le commit épinglé.
>
> **AUDIT D'ATTEIGNABILITÉ — 2026-08-21.** Reproduire une anomalie n'est pas la même chose que savoir si elle est **sur notre chemin d'exécution**. Vérifié par lecture du code appelé, en distinguant code actif et commentaire :
>
> \| Anomalie \| Sur le chemin du binaire de production ? \|
> \|---\|---\|
> \| `AnalogInput::IsRisingEdge()` \| **non** — jamais appelée, `CvGate` la remplace (§10.5) \|
> \| `Gravity::Process()` (index non initialisé) \| **non** dans `main.cpp` ; **oui** dans `wokwi_main.cpp`, seul appel actif restant \|
> \| `Clock::SetSource()` (`SOURCE_LAST`) \| **non** — aucune source n'est choisie ; se manifeste comme avertissement du compilateur \|
> \| `Button` (relâchement perdu au débounce) \| **latente** — `Process()` est appelé, mais aucun callback n'est relié \|
> \| `Encoder` (faux premier mouvement) \| **latente** — idem \|
> \| `DigitalOutput::Init()` (n'éteint pas) \| **latente** — voir ci-dessous \|
>
> **Les trois « latentes » deviennent actives exactement au moment où l'UI et le transport seront reliés** (§12, §8) : c'est à la couche d'adaptation de les absorber, et c'est le piège à ne pas redécouvrir à ce moment-là.
>
> **`DigitalOutput::Init()`**** est inoffensive au démarrage à froid, pour une raison qu'il faut nommer** : un reset AVR laisse `PORT` à 0, donc `pinMode(pin, OUTPUT)` tire la broche au bas, et `gravity` est un objet **global** en `.bss`, donc `on_` part à faux. **Aucune sortie ne peut donc être bloquée haute au premier flash.** Elle devient réelle sur un **soft reset** — le firmware d'origine expose un `reboot()` — ou sur un second `Init()` : la broche resterait HAUTE alors que le firmware la croit éteinte, et rien ne l'éteindrait jamais. À traiter si un redémarrage logiciel est un jour ajouté.
>
> Suivi des actions correspondantes : `docs/open-risks.md`, lignes 14 à 16.
---
## 19. Critères de réussite
Le firmware final doit : fonctionner sur le Gravity hardware **inchangé** ; conserver les fonctionnalités historiques retenues ; offrir des patterns 1–24 steps avec le modèle temporel `masterPhase` ; rester dans le budget RAM/Flash ; permettre la restauration du firmware original.
<page url="https://app.notion.com/p/3c7d2c2576ce813fa814eb6259949d5f">Conception — fractionnement des salves SHIFT</page>
