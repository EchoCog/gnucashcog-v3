# Phase 4 Implementation Summary

> **Honesty banner (v3.0):** Phase 4 delivered an **in-process library API** and
> GraphQL helpers with unit tests. It did **not** ship a listen/accept HTTP or
> WebSocket server, and Unity3D/ROS are **experimental stubs**. Production
> readiness is tracked in
> [COGNITIVE_CAPABILITY_MATRIX.md](COGNITIVE_CAPABILITY_MATRIX.md). Real network
> HTTP(+WS) is deferred to **v3.2+**.

## Status: scaffolding / library (not a network product surface)

**Phase 4 objective (historical):** expose the network via REST/WebSocket APIs;
bind to Unity3D, ROS, and web agents for embodied cognition.

**What actually shipped for v3.0:**

| Component | Reality |
|-----------|---------|
| In-process REST-shaped handlers | **library** — request/response structs + handler table; no socket listen |
| WebSocket API | **stub** — connection registry in memory; no real protocol |
| GraphQL helpers | **library** — resolvers/helpers; subscriptions need real WS |
| Unity3D / ROS / web-agent | **stub / deferred** — adapter shapes only |
| Tests | `test-cognitive-api.cpp` covers in-process handlers |
| Demo | `phase4-api-demo.cpp` exercises library API, not a live server |

## Core API infrastructure (honest)

### In-process API (shipped)
- Handler-table model for embedding and tests (`gnc-cognitive-api.c/.h`)
- Routes sketched as path strings (`/api/v1/cognitive/state`, attention,
  accounts, transactions) — invoked in-process, not over the network
- GraphQL interface helpers (`gnc-cognitive-graphql.c/.h`)

### Network / embodiment (deferred)
- **HTTP listen/accept server** — not implemented (`HAVE_COGNITIVE_HTTP` future)
- **WebSocket protocol** — not implemented
- **Unity3D / ROS** — stub functions return success/JSON placeholders; do not
  advertise as operational
- Unverified performance slogans (sub-50ms, 1000 agents) are **not** acceptance
  criteria for v3.0

## Files

```
libgnucash/engine/gnc-cognitive-api.{h,c}
libgnucash/engine/gnc-cognitive-graphql.{h,c}
libgnucash/engine/test/test-cognitive-api.cpp
phase4-api-demo.cpp
PHASE4_API_DOCUMENTATION.md   # design notes; treat network/Unity/ROS as target design
```

## How to exercise (library)

```bash
cmake -G Ninja -B build -DWITH_GNUCASH=OFF -DWITH_SQL=OFF \
  -DWITH_AQBANKING=OFF -DWITH_OFX=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test-cognitive-api
cd build && ctest -R test-cognitive-api --output-on-failure
```

## Relation to v3.0 “complete”

v3.0 product surfaces are CLI (`--cognitive-*`, fincosys sync), feature-flagged
book lifecycle, and simulated AtomSpace/PLN/ECAN — **not** Phase 4 network
embodiment. See issue #37 workstream F and the capability matrix.
