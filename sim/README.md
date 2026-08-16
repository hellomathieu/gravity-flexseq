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

- [x] `Pattern` — modèle + tests miroir (P0).
- [x] `PatternStore` — modèle + tests miroir (P1).
- [x] Simulateur UI — squelette EDIT PATTERN, backend TS (P2).
- [ ] Transport / masterPhase / sorties trigger.
- [ ] Backend avr8js (branché sur le même seam `SimBackend`).

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
