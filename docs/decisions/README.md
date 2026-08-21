# Architecture Decision Records (ADR)

**Normative source for Gravity FlexSeq's architecture decisions.**

An ADR records **one** architecture decision that is **durable and
significant**: a structural choice that would have to be justified again if
anyone wanted to undo it.

## What an ADR is not

- **Not a session log**, and not an implementation history.
- **Not a product decision** — those live in the **Notion PRD**, which is their
  normative source. An ADR may *reference* it, never copy it.
- **Not a hypothesis**, and not an unconfirmed proposal.
- **Not a routine implementation detail** — the code is the source of truth for
  those.

When in doubt: do not create an ADR.

## Statuses

| Status | Meaning |
|---|---|
| `proposed` | written, not yet decided |
| `accepted` | decision in force |
| `superseded` | replaced by a later ADR (reference mandatory) |
| `rejected` | considered then set aside; kept so it is not re-debated |

## Superseding

A replaced decision **keeps its ADR**. It is neither deleted nor rewritten: its
status becomes `superseded` and it references the new ADR. The new ADR references
the one it replaces in return. The reasoning history stays readable that way.

## Naming

```
docs/decisions/NNNN-title-in-kebab-case.md
```

`NNNN` = four-digit sequential number, never reused (not even after a
`rejected`).

## Skeleton

```markdown
# NNNN — Decision title

- **Status:** proposed | accepted | superseded | rejected
- **Date:** YYYY-MM-DD
- **Supersedes:** (ADR or —)
- **Superseded by:** (ADR or —)

## Context
The problem, and the constraints that bear on it (hardware, pinned dependency,
memory budget…). Verified facts only.

## Decision
What is decided, in the active voice.

## Consequences
What this makes possible, what it closes off, and the cost accepted.

## Alternatives set aside
Only when their rejection illuminates the decision.

## References
PRD (section), code, measurements, issues/PRs.
```

## Reminder — one normative source

Every piece of durable knowledge has **one** normative source; others may
reference it without becoming competing copies. The full routing lives in
`.claude/rules/knowledge-persistence.md`.
