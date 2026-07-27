# Contributing to Halcyon

Thank you for your interest in contributing to Halcyon.

Halcyon is an experimental, long-term engineering project that evolves the
Tilted Evolution / Skyrim Together Reborn codebase into a context-oriented,
progressively server-authoritative multiplayer platform.

The project values working code, honest documentation, reproducible research,
and incremental migration. Documentation is part of the architecture and MUST
not be treated as an afterthought.

---

## 1. Read This First

Before making a substantial change, read:

1. [`AGENTS.md`](AGENTS.md)
2. [`docs/HTDS/README.md`](docs/HTDS/README.md)
3. the relevant HTDS volume and subsystem documents;
4. the relevant Architecture Decision Records in [`docs/ADR`](docs/ADR);
5. the relevant Requests for Comments in [`docs/RFC`](docs/RFC);
6. any related research in [`docs/Research`](docs/Research).

The HTDS describes the intended architecture. Much of it is still conceptual and
does not represent code that already exists.

Contributors MUST distinguish between:

- current Tilted Evolution behavior;
- transitional Halcyon behavior;
- target Halcyon architecture;
- unvalidated research ideas.

Do not describe planned systems as implemented.

---

## 2. Documentation Hierarchy

Halcyon uses the following documentation hierarchy:

### HTDS — Technical Design Specification

The HTDS defines accepted intended architecture, terminology, subsystem
responsibilities, constraints, and long-term direction.

### ADR — Architecture Decision Record

An ADR records an accepted architectural decision and its consequences.

### RFC — Request for Comments

An RFC proposes a design that is still under discussion, research, or prototype
validation.

### Research

Research documents record source-code analysis, experiments, external-project
comparisons, measurements, uncertainties, and open questions.

### Source Code

Source code represents current implementation behavior. It may temporarily differ
from the target architecture while migration is in progress.

When implementation and documentation disagree, do not silently choose one.
Determine whether:

- the implementation is legacy or transitional;
- the HTDS is outdated;
- an RFC or ADR is required;
- both code and documentation need to change.

---

## 3. Contribution Principles

Contributions SHOULD follow these principles:

- preserve working behavior unless a replacement is ready;
- prefer incremental migration over broad rewrites;
- keep Linux and Proton compatibility in scope;
- move authority toward the server progressively;
- use Contexts as the primary state-isolation mechanism;
- keep gameplay policy out of the low-level core where practical;
- avoid adding mandatory Chromium or CEF dependencies to the Linux path;
- expose clear diagnostics for new systems;
- document limitations honestly;
- add tests or reproducible validation steps where practical.

Architectural purity alone is not enough reason to remove functioning systems.

---

## 4. AI-Assisted Contributions

AI-assisted contributions are explicitly allowed and welcome.

This includes tools such as:

- ChatGPT;
- Codex;
- GitHub Copilot;
- Claude Code;
- Aider;
- OpenHands;
- local language models;
- autonomous coding agents.

However, AI output is not considered self-validating.

### Human responsibility

Every submitted contribution MUST have a human contributor who takes
responsibility for:

- understanding the change;
- checking that the change matches the contributor's intent;
- validating relevant APIs and assumptions;
- compiling or testing the change when practical;
- reviewing security and persistence implications;
- confirming that documentation is accurate;
- confirming that implementation-status claims are truthful;
- ensuring license compliance.

The human contributor remains the author and reviewer of the submitted work even
when AI produced most of the initial draft.

### Acceptable AI usage

Good uses include:

- drafting documentation;
- explaining unfamiliar code;
- generating focused prototypes;
- suggesting refactors;
- generating tests;
- identifying likely failure paths;
- helping map existing architecture;
- preparing repetitive migration work;
- reviewing diffs.

### Unacceptable AI usage

Do not submit:

- code copied blindly without understanding it;
- fabricated APIs, classes, functions, files, or protocol messages;
- invented test results or benchmark numbers;
- fake implementation-status claims;
- citations that were not verified;
- large rewrites that were not reviewed;
- security-sensitive code without human analysis;
- generated code that was never compiled when compilation is reasonably possible.

If AI was materially involved, contributors MAY mention it in the pull request.
Disclosure is encouraged for large generated changes, but human validation matters
more than the specific tool used.

---

## 5. Choosing the Correct Contribution Path

### Small implementation fix

Examples:

- crash fix;
- logging improvement;
- typo;
- narrow Proton compatibility fix;
- isolated UI correction.

Usually requires:

- focused code change;
- test or reproduction notes;
- no RFC unless architecture changes.

### Architectural change

Examples:

- new authority model;
- Context behavior;
- protocol extension;
- persistence schema;
- plugin API;
- quest synchronization policy.

Usually requires:

1. research or source mapping;
2. RFC;
3. prototype;
4. ADR after acceptance;
5. HTDS update;
6. implementation.

### Experimental prototype

Prototypes MUST be clearly marked.

They SHOULD:

- live behind a feature flag, experimental branch, or isolated module;
- avoid pretending to be production-ready;
- include success and failure criteria;
- document what assumption they test;
- record results in Research or the relevant RFC.

---

## 6. Pull Request Guidelines

Pull requests SHOULD be focused and reviewable.

A good pull request includes:

- a clear problem statement;
- a concise description of the solution;
- current behavior and intended behavior;
- compatibility impact;
- Linux and Proton impact where relevant;
- validation performed;
- known limitations;
- links to relevant HTDS, ADR, RFC, issues, or research;
- screenshots or logs when UI/runtime behavior changes.

Avoid:

- unrelated formatting changes;
- opportunistic rewrites;
- mixing documentation, refactoring, and a feature without explanation;
- changing public behavior without documenting it;
- claiming a subsystem is complete because interfaces were added.

---

## 7. Commit Guidance

Use descriptive commit messages.

Preferred examples:

```text
fix(sync): restore absolute health after respawn
docs(htds): define Context lifecycle
feat(server): add experimental Context identifiers
refactor(replication): isolate recipient filtering
test(protocol): cover stale Context revisions
```

A commit should represent one understandable step when practical.

Do not rewrite authorship or remove upstream attribution.

---

## 8. Coding Expectations

Follow the coding conventions already used by the surrounding code unless an
accepted project-wide style change says otherwise.

General expectations:

- preserve complete code paths unless removal is intentional;
- avoid hidden global state;
- prefer explicit ownership and lifecycle;
- validate network input;
- reject stale revisions;
- keep persistence operations idempotent where retries are possible;
- do not trust client-reported rewards or permanent outcomes;
- make failure paths observable;
- use strong types for identifiers when practical;
- avoid premature abstractions that are not needed by a prototype.

Run formatting tools required by the repository on modified code.

---

## 9. Linux and Proton Requirements

Changes to the client, launcher, input, graphics, process startup, or UI MUST
consider Proton behavior.

Do not assume:

- Windows paths;
- case-insensitive filesystems;
- native Windows process behavior;
- CEF or Chromium stability;
- a visible console;
- identical raw-input behavior;
- a fixed Skyrim image base.

When relevant, include:

- tested Proton version;
- Skyrim runtime version;
- launch method;
- log paths;
- reproduction steps.

---

## 10. Testing and Validation

The expected validation level depends on the contribution.

Possible validation includes:

- compilation;
- unit tests;
- protocol serialization tests;
- two-client gameplay tests;
- server restart and reconnect tests;
- persistence recovery tests;
- Proton launch tests;
- static analysis;
- log inspection;
- targeted source-code reasoning when runtime testing is unavailable.

If a change was not tested, say so clearly.

Never invent a successful test result.

---

## 11. Documentation Status Language

Use these labels consistently:

- **Conceptual** — described but not implemented;
- **Research Draft** — investigation or early proposal;
- **Draft Skeleton** — architectural outline expected to change;
- **Prototype** — experimental implementation;
- **Partially Implemented** — meaningful pieces exist, but major requirements remain;
- **Implemented** — substantially represented in production code;
- **Validated** — tested against documented acceptance criteria;
- **Legacy** — inherited behavior awaiting migration;
- **Transitional** — temporary bridge toward target architecture;
- **Deprecated** — still present but discouraged;
- **Superseded** — replaced by a newer decision or design.

---

## 12. Security and Abuse Prevention

New persistent or public-server systems MUST consider abuse.

Examples:

- reward duplication;
- replayed requests;
- stale revisions;
- unauthorized Context membership;
- spoofed Entity IDs;
- client-declared quest completion;
- plugin permission escalation;
- malformed packet handling;
- resource exhaustion;
- cross-Context state leaks.

Halcyon does not aim to be a kernel anti-cheat product, but durable server state
must not depend on blind client trust.

---

## 13. License and External Code

Before copying or linking external code:

- verify the exact license of the relevant component;
- preserve required notices;
- confirm compatibility with Halcyon's license;
- identify whether GPL or AGPL obligations apply;
- document the source and modifications.

Do not assume that every directory in an external repository uses the same license.

Ideas and architecture may be studied without copying implementation, but copied
code must be handled correctly.

---

## 14. Review Checklist

Before submitting, confirm:

- [ ] I understand the change.
- [ ] The change is focused.
- [ ] Existing behavior is preserved or the replacement is documented.
- [ ] Relevant HTDS documents were reviewed.
- [ ] An RFC or ADR was added when necessary.
- [ ] Planned architecture is not described as implemented.
- [ ] Linux and Proton impact was considered.
- [ ] Network input and persistence implications were considered.
- [ ] Tests or validation steps are documented.
- [ ] AI-generated content received human review.
- [ ] No test result, benchmark, API, or citation was fabricated.
- [ ] External code and licenses were checked.

---

## 15. Getting Help

For uncertain architectural work:

1. open an issue or discussion;
2. create a Research document if evidence is incomplete;
3. create a Draft RFC for a proposed system;
4. request review before implementing a broad rewrite.

A small, honest prototype is usually more valuable than a large unvalidated design.
