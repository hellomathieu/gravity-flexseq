# Décisions d'architecture (ADR)

**Source normative des décisions d'architecture** de Gravity FlexSeq.

Un ADR consigne **une** décision d'architecture **durable et significative** :
un choix structurant qu'il faudrait justifier à nouveau si on voulait le
défaire.

## Ce qu'un ADR n'est pas

- **Pas un journal de session** ni un historique d'implémentation.
- **Pas une décision produit** — celles-ci vivent dans le **PRD Notion**, qui en
  est la source normative. Un ADR peut le *référencer*, jamais le recopier.
- **Pas une hypothèse** ni une proposition non confirmée.
- **Pas un détail d'implémentation courant** — le code en est la source de
  vérité.

En cas de doute : ne pas créer d'ADR.

## Statuts

| Statut | Sens |
|---|---|
| `proposed` | rédigé, pas encore tranché |
| `accepted` | décision en vigueur |
| `superseded` | remplacé par un ADR plus récent (référence obligatoire) |
| `rejected` | envisagé puis écarté ; conservé pour ne pas re-débattre |

## Remplacement

Une décision remplacée **conserve son ADR**. On ne le supprime pas et on ne le
réécrit pas : on passe son statut à `superseded` et on référence le nouvel ADR.
Le nouvel ADR référence en retour celui qu'il remplace. L'historique des
raisonnements reste ainsi lisible.

## Nommage

```
docs/decisions/NNNN-titre-en-kebab-case.md
```

`NNNN` = numéro à 4 chiffres, séquentiel, jamais réutilisé (même après un
`rejected`).

## Squelette

```markdown
# NNNN — Titre de la décision

- **Statut :** proposed | accepted | superseded | rejected
- **Date :** AAAA-MM-JJ
- **Remplace :** (ADR ou —)
- **Remplacé par :** (ADR ou —)

## Contexte
Le problème, et les contraintes qui s'imposent (hardware, dépendance figée,
budget mémoire…). Faits vérifiés uniquement.

## Décision
Ce qui est décidé, à la voix active.

## Conséquences
Ce que cela rend possible, ce que cela ferme, et le coût accepté.

## Alternatives écartées
Uniquement si leur rejet éclaire la décision.

## Références
PRD (section), code, mesures, issues/PR.
```

## Rappel — une seule source normative

Chaque connaissance durable a **une** source normative ; les autres peuvent la
référencer sans en devenir des copies concurrentes. Le routage complet est dans
`.claude/rules/knowledge-persistence.md`.
