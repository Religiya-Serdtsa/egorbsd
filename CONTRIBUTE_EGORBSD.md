# CONTRIBUTE.md

This repository is a FreeBSD-derived system where deterministic resource control and structural clarity outrank "generality at any cost".

If you want to contribute, read this entire document first. If you disagree with the constraints, do not open a PR.

---

## 1) Project Scope (Supported Targets)

We care about the following environments:

- Modern laptops/desktops/workstations
- Low-cost SBCs (e.g., Raspberry Pi class devices)
- Mini PCs

Primary ISA targets:

- x86_64
- arm64 (AArch64)
- riscv64

Other architectures may be considered only if they do **not** degrade structure, maintainability, or determinism.

---

## 2) Non-Goals

We will reject changes that:

- Add abstraction layers "for future portability" without a measurable need.
- Introduce indirection or generalized frameworks that make control flow harder to reason about.
- Expand supported hardware at the cost of turning the codebase into a maze of conditionals.
- Increase baseline memory footprint without an explicit trade-off and proof.

"Portability" is not rejected by default. **Portability that dirties structure is rejected.**

---

## 3) Contribution Gates (Choose One)

To reduce noise, contributors must pass one gate before or with their first substantial PR.

### Gate A: The Letov Test (Cultural Alignment)
Listen to Grazhdanskaya Oborona (Гражданская Оборона) — 1st album — once.
Submit a short note (a few paragraphs) explaining what you took from it.

This is not about "taste". It is about whether you understand that this project is not optimized for mainstream comfort.

### Gate B: The Logic Test (Engineering Alignment)
Write a short technical note describing your coding philosophy with respect to:

- Determinism and predictability
- Memory lifecycle governance
- Performance over abstraction (when it matters)
- How you prevent structural decay in a long-lived system

If your philosophy is "make it generic first, optimize later", you're in the wrong project.

Where to submit:
- Open a GitHub Issue titled: `Gate A` or `Gate B`
- Or include it as part of your first PR description (preferred for small PRs)

---

## 4) Mandatory UML for Any Non-Trivial Change

Any PR that changes control flow, ownership/lifetime boundaries, scheduling/memory behavior, or subsystem interfaces must include UML.

Required:
- At least one diagram: Class / Component / Sequence / State
- Must reflect **the actual change**, not a decorative diagram
- Must include new/modified interfaces, main call paths, and ownership/lifetime boundaries

Accepted formats:
- PlantUML (`.puml`)
- Mermaid (`.mmd` / markdown Mermaid blocks)
- SVG/PNG exported from the above

Where to place diagrams:
- `docs/uml/<PR_NUMBER>/...`
- Or embed in the PR description with source kept under `docs/uml/`

PRs without the required UML will be closed without review.

---

## 5) Proof Requirements (Benchmarks, Memory, and Regressions)

This project uses a "burden of proof" standard.

If your change claims or implies improvements, you must provide evidence.

### Minimum evidence for performance/memory-sensitive changes
- Before/after numbers
- Test environment (CPU, RAM, storage, board/laptop model)
- Build config (compiler, flags, kernel config summary)
- Reproduction steps

### Regression policy
- If baseline memory footprint increases, you must either:
  - justify it as unavoidable for a clearly defined capability, and
  - demonstrate a compensating reduction elsewhere, or
  - show that the increase is within an explicitly stated threshold for that subsystem.

If you cannot prove it, do not ship it.

---

## 6) Coding Constraints (What We Enforce)

### Clarity and determinism
- Prefer explicit control flow over "clever" meta-programming.
- Prefer predictable memory lifetimes over ad-hoc allocation patterns.
- Avoid "framework-like" generic layers unless they shrink complexity.

### Architecture boundaries
- Keep platform-specific code localized.
- Avoid scattering `#ifdef` spaghetti across shared logic.
- Prefer well-defined per-arch shims that preserve a clean core.

### Safety and correctness
- No "it probably works" merges.
- For risky changes: add a minimal test plan and failure mode notes.

---

## 7) PR Types and How We Review

### Fast path (usually accepted quickly)
- Bugfixes with tight scope
- Determinism improvements with proof
- Structural cleanups that reduce complexity
- Portability support that does not pollute architecture

### Slow path (high scrutiny)
- Scheduler changes
- Memory allocator changes
- VM, locking, SMP, interrupt path modifications
- Large refactors

If you're touching a core path, expect deeper review and stricter proof requirements.

---

## 8) Communication Rules

- Keep discussion technical.
- Avoid ideology and social meta-arguments.
- If you want to propose a big direction change: open an Issue first with UML + rationale + proof plan.

We do not guarantee extended debates. Decisions may be terse.

---

## 9) What Gets Rejected Immediately

- "Generality first" changes that complicate the core.
- PRs missing required UML.
- PRs with claims but no measurements for measurable topics.
- Refactors that expand surface area without reducing complexity.

---

## 10) Practical Submission Checklist

Before opening a PR:

- [ ] I stayed within target scope (x86_64/arm64/riscv64 + modern laptop/Workstation/Desktop/miniPC focus).
- [ ] I did not add abstraction layers that increase indirection without clear necessity.
- [ ] I included required UML (if non-trivial).
- [ ] I included before/after numbers (if performance/memory relevant).
- [ ] I documented how to reproduce results.

---

## 11) License and Upstream Relationship

This project is derived from FreeBSD sources. Respect upstream licensing and attribution requirements.
If you import or sync upstream code, keep history and provenance clear.

---

## 12) Final Note

If you want to build something for everyone, there are many better places to do it.
If you want to build something that stays structurally clean while supporting the few architectures that matter in practice,
and you can prove your changes objectively, then contribute.

Proceed.
