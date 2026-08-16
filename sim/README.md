# gravity-flexseq-sim

POC TypeScript **isolé du firmware** : modèle comportemental de référence,
tests miroir, et (à venir) simulateur visuel + backend avr8js.

> Ce workspace n'est **pas** compilé par PlatformIO (`build_src_filter` ne
> couvre que `src/`). Il ne crée aucune dépendance runtime du firmware
> envers Node.js / TypeScript.

## Rôle

- Modèle fonctionnel de référence (miroir comportemental du domaine C++).
- Tests rapides exécutables sans hardware.
- Base du futur Gravity Simulator (UI) avec deux backends : modèle TS et
  firmware AVR réel exécuté par avr8js.

## Contrat de parité

La cible de parité TS ↔ C++ est **comportementale**, pas mémoire. Les
contraintes de packing (`sizeof(Pattern) == 7`, `PatternStore == 672 B`)
sont vérifiées **côté C++/AVR uniquement**. Chaque `it(...)` de
`test/Pattern.test.ts` correspond à un `RUN_TEST(...)` de
`../test/test_pattern/test_pattern.cpp`.

## Commandes

```bash
cd sim
npm install
npm test          # vitest run (une passe)
npm run test:watch
npm run typecheck # tsc --noEmit
```

## État

- [x] `Pattern` — contenu partagé (24 steps + triolets, sans length) (P0, revu P4).
- [x] `PatternBank` — **banque de 16 patterns partagés** (remplace l'ancien 6×16) (P4).
- [x] `SequencerEngine.selectedPattern` + LENGTH **par channel** ; édition de pattern partagée (P4).
- [x] Simulateur UI — squelette EDIT PATTERN, backend TS (P2).
- [x] `SequencerEngine` — `masterPhase` (ticks 96 PPQN), effectiveStep par channel (P3.0).
- [x] Transport dans le simulateur — Play/Stop/Reset + playhead sur l'OLED (P3.1).
- [x] Pattern actif **par channel** + bande multi-channels (6 playheads simultanés) (P3.1).
- [x] `SequencerEngine.hasStepped` + `TriggerSequencer` (parité C++) ; **flash des triggers** par channel dans le sim.
- [x] **SUBDIV → ticksPerStep** par channel (convention libGravity 96 PPQN) + sélecteur SUBDIV (parité C++).
- [ ] METER / MEASURES.
- [ ] Grille musicale METER/SUBDIV/MEASURES ; sorties trigger.
- [ ] Couche Transport réelle (clock/MIDI) ; port C++ du moteur.
- [ ] Backend avr8js (branché sur le même seam `SimBackend`).

> Le playhead sur l'écran EDIT est une aide du simulateur ; le PRD ne l'exige
> pas comme fonctionnalité firmware. L'horloge du simulateur convertit le temps
> réel en ticks 96 PPQN selon le BPM (rôle tenu plus tard par la couche Transport).

### Lancer le simulateur

```bash
cd sim
npm run dev   # http://localhost:5173 (Vite, hot-reload)
```

Architecture : `src/web/main.ts` (UI) → `src/sim/backend.ts` (`SimBackend`,
seam pour brancher avr8js plus tard) → `src/domain/*` → `src/sim/PatternView.ts`
(rendu pur, testé).

Côté C++, le loop de test sans hardware est `pio test -e native` (env ajouté
en P1). Parité mesurée : TS 35/35 ; C++ natif 42/42 (`test_pattern` 25,
`test_pattern_store` 13, `test_pattern_store_virtual` 4).
