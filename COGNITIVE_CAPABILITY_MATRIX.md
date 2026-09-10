# gnucashcog-v3 Cognitive Capability Matrix

This document is the **source of truth** for what is production-ready versus
scaffolding. Prefer this matrix over phase banners that say “COMPLETE”.

Legend:

| Status | Meaning |
|--------|---------|
| **production** | Wired into normal operation (or CLI), tested, documented honestly |
| **library** | Real API + unit tests; requires explicit init / feature flag |
| **simulated** | First-class in-process fallback (no OpenCog/ggml required) |
| **stub** | API shape only; not production |
| **deferred** | Out of v3.0 scope |

## Core engine

| Capability | Status | Notes |
|------------|--------|-------|
| Simulated AtomSpace (`std::map` handles) | **production / simulated** | Default when OpenCog absent |
| OpenCog AtomSpace backend | **library** (optional) | Compile-time `HAVE_OPENCOG_*` |
| Link outgoing participants | **library** | Stored for Inheritance/Evaluation/Hierarchy links |
| Book → AtomSpace account mapping | **library** | `gnc_book_to_atomspace()` |
| Transaction commit → PLN + ECAN | **library** | Active when AtomSpace initialized |
| Feature flag `GNC_COGNITIVE_ENABLED` | **production** | Auto book open/save/close hooks |
| Cognitive account type KVP | **library** | `cognitive-type` on accounts |
| AtomSpace snapshot sidecar | **library** | `*.cognitive.json` via fincosys schema v1 |
| PLN double-entry validation | **library / simulated** | Uses real split sums |
| ECAN attention economy | **library / simulated** | Wages/rent/decay; meta can apply funds/decay |
| MOSES / URE advanced paths | **library / experimental** | Present; not required for v3.0 “complete” |
| Meta-cognition `apply_config` | **library** | Applies STI/LTI funds + decay to live ECAN |
| Ontogenesis kernel | **stub** | Until OZC-272 |
| Tensor network | **library / simulated** | ggml optional acceleration |
| Neural-symbolic kernels | **library / simulated** | ggml optional |
| In-process cognitive API / GraphQL | **library** | **Not** a listen/accept HTTP server |
| Real HTTP + WebSocket server | **deferred** | v3.2+ |
| Unity3D / ROS embodiment | **stub / deferred** | Do not advertise as complete |
| Fincosys ecosystem sync v1 | **production** | CLI import/export + fixture tests |
| Python SWIG cognitive exports | **deferred** | Interim ctypes/CLI bridge available |
| GUI preferences / reports | **deferred** | v3.1 |

## CLI (`gnucash-cli`)

| Command | Status |
|---------|--------|
| `--import-fincosys-sync` / `--export-fincosys-sync` | **production** |
| `--cognitive-dump` | **production** |
| `--cognitive-capabilities` | **production** |
| `--cognitive-validate` | **production** |

## Environment

| Variable | Effect |
|----------|--------|
| `GNC_COGNITIVE_ENABLED=1` (or `true`/`yes`/`on`) | Register book open → init + map + load sidecar; save → write sidecar; close → shutdown |

Programmatic: `gnc_cognitive_set_enabled(TRUE)`.

## Acceptance vs slogans

Do **not** claim:

- Unity/ROS “operational”
- Network REST server without `HAVE_COGNITIVE_HTTP`
- True AGI / consciousness metrics
- OpenCog/ggml required for basic cognitive features

Do claim only what this matrix marks **production** or **library** with tests green.

## Related docs

- `docs/V3_0_ACCEPTANCE.md` — v3.0 must-have checklist vs issue #37
- `docs/FINCOSYS_ECOSYSTEM_SYNC.md` — ecosystem JSON contract
- `COGNITIVE_ACCOUNTING.md` — design notes (may still contain historical phase language)
- `MASTER_COORDINATION.md` — high-level roadmap (see honesty banner)
- `PHASE4_IMPLEMENTATION_SUMMARY.md` — in-process API only; network/Unity/ROS deferred
