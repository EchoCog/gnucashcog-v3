# gnucashcog-v3.0 acceptance checklist

Derived from issue #37. Prefer
[COGNITIVE_CAPABILITY_MATRIX.md](../COGNITIVE_CAPABILITY_MATRIX.md) over phase
“COMPLETE” banners.

## Must-have (v3.0)

| # | Criterion | How verified |
|---|-----------|--------------|
| 1 | Clean CI build of engine + cognitive unit tests without OpenCog/ggml | `.github/workflows/ci-tests.yml` job `cognitive-engine` |
| 2 | Feature-flagged auto-init on book open; hooks on ordinary transactions | `GNC_COGNITIVE_ENABLED` / `gnc_cognitive_set_enabled`; `HOOK_BOOK_*`; `xaccTransCommitEdit` → `gnc_cognitive_on_transaction_committed`; `test-cognitive-lifecycle` |
| 3 | Cognitive metadata + AtomSpace snapshot persist across save/load | Account KVP cognitive-type; `gnc_cognitive_save_snapshot` / `load_snapshot`; sidecar `*.cognitive.json` |
| 4 | Fincosys schema v1 import/export CLI + fixture tests | `gnucash-cli --import-fincosys-sync` / `--export-fincosys-sync`; `test-fincosys-bridge`; `data/fincosys_sync/`; optional workflow dry-run |
| 5 | PLN double-entry + ECAN attention deterministic on fixtures | `test-cognitive-accounting`, lifecycle commit attention test |
| 6 | Python access to core ops (or interim bridge) | `bindings/python/example_scripts/cognitive_ctypes_bridge.py` (CLI/ctypes); SWIG deferred |
| 7 | Docs match implementation | Capability matrix; honest Phase 4/iteration banners; no Unity/ROS/WebSocket-complete claims |
| 8 | No regressions in classic bookkeeping | Cognitive path additive/feature-flagged; full `ninja check` on product builds |

## Explicit non-goals for v3.0

- Production Unity3D or ROS integrations
- Real listen/accept HTTP(+WS) server (v3.2+)
- Requiring OpenCog/ggml for basic cognitive features
- Unverified AGI / uptime / agent-count slogans

## Operator smoke (local)

```bash
cmake -G Ninja -B build \
  -DWITH_GNUCASH=OFF -DWITH_SQL=OFF -DWITH_AQBANKING=OFF \
  -DWITH_OFX=OFF -DWITH_PYTHON=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target gnc-engine \
  test-cognitive-accounting test-cognitive-lifecycle \
  test-fincosys-bridge test-tensor-network test-cognitive-api
cd build && ctest --output-on-failure -R \
  'test-cognitive|test-fincosys|test-tensor|test-meta|test-ko6ml|test-neural|test-phase6|test-ontogenesis'
```

Install `libjson-glib-dev` if you need `test-cognitive-api` (optional JSON-GLib).
