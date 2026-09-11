# Iteration 2 — Distributed Agentic Cognitive Grammar Network

## Master coordination status (honest)

*Status: **scaffolding complete; v3.0 product cut in progress***  
*Source of truth: [COGNITIVE_CAPABILITY_MATRIX.md](COGNITIVE_CAPABILITY_MATRIX.md)*

Phases 1–6 delivered **library APIs, tests, and demos**. That is not the same as
a full product UX (GUI reports, real HTTP server, Unity/ROS). Claims below use
the matrix legend: **production / library / simulated / stub / deferred**.

## Phase status

### Phase 1: Cognitive primitives & hypergraph encoding
- **Scaffolding:** complete (API + tests)
- **Production readiness:** **library**
- **Files:** `gnc-cognitive-primitives.*`, ko6ml translation

### Phase 2: ECAN attention & resource kernel
- **Scaffolding:** complete (API + tests)
- **Production readiness:** **library / simulated**
- **Files:** integrated in `gnc-cognitive-accounting.*`
- **Live path:** feature-flagged book hooks + transaction commit

### Phase 3: Neural-symbolic ggml kernels
- **Scaffolding:** complete (API + tests)
- **Production readiness:** **library / fallback** (ggml optional)
- **Files:** `gnc-tensor-network.*`, `gnc-neural-symbolic-kernels.*`

### Phase 4: Distributed mesh API & embodiment
- **Scaffolding:** in-process API + GraphQL helpers
- **Production readiness:** **partial**
  - In-process handlers: **library**
  - Real HTTP/WebSocket server: **deferred (v3.2+)**
  - Unity3D / ROS: **stub / deferred**
- **Files:** `gnc-cognitive-api.*`, `gnc-cognitive-graphql.*`
- **Do not claim:** network REST server or Unity/ROS “operational”

### Phase 5: Meta-cognition & evolution
- **Scaffolding:** complete (API + apply/rollback path)
- **Production readiness:** **library** (ontogenesis **stub** until OZC-272)
- **Files:** `gnc-meta-cognitive.*`, `gnc-ontogenesis-bridge.*`

### Phase 6: Testing & unification
- **Scaffolding:** comprehensive cognitive gtests + unification module
- **Production readiness:** **in progress** toward v3.0 acceptance
- **Files:** `gnc-cognitive-unification.*`, engine cognitive tests, CI job
  `cognitive-engine`

## v3.0 must-haves (issue #37)

| Must-have | Status |
|-----------|--------|
| Clean CI engine + cognitive tests without OpenCog/ggml | CI job `cognitive-engine` |
| Feature-flagged auto-init + txn hooks | `GNC_COGNITIVE_ENABLED` / commit hook |
| Snapshot + KVP cognitive metadata | sidecar `*.cognitive.json` + account KVP |
| Fincosys schema v1 CLI + fixtures | CLI + `test-fincosys-bridge` + data fixtures |
| PLN/ECAN deterministic on fixtures | library tests |
| Python access | interim CLI/ctypes bridge (SWIG deferred) |
| Docs match reality | capability matrix + this honesty pass |
| No classic bookkeeping regressions | cognitive features additive |

## Metrics honesty

Unverified slogans (sub-100ms, 1000 agents, 99.9% uptime, “100% complete”) are
**not** acceptance criteria. Publish measured numbers only after benchmarks.

## Related docs

- [MASTER_COORDINATION.md](MASTER_COORDINATION.md)
- [COGNITIVE_CAPABILITY_MATRIX.md](COGNITIVE_CAPABILITY_MATRIX.md)
- [docs/FINCOSYS_ECOSYSTEM_SYNC.md](docs/FINCOSYS_ECOSYSTEM_SYNC.md)
- [PHASE4_IMPLEMENTATION_SUMMARY.md](PHASE4_IMPLEMENTATION_SUMMARY.md)
