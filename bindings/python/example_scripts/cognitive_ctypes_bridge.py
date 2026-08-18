#!/usr/bin/env python3
"""Interim Python bridge for gnucashcog-v3 cognitive operations.

Full SWIG exports of the cognitive C API are not yet in gnucash_core.i.
This module provides:

1. Subprocess helpers that call ``gnucash-cli`` cognitive commands when the
   CLI is on PATH (preferred for operators).
2. Optional ctypes bindings against ``libgnc-engine`` when
   ``GNC_ENGINE_LIB`` is set (advanced / in-process).

Neither path replaces eventual SWIG; both are supported interim interfaces
for fincosys automation and tests.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Optional


class CognitiveCliError(RuntimeError):
    pass


def _find_cli() -> str:
    env = os.environ.get("GNUCASH_CLI")
    if env and Path(env).exists():
        return env
    found = shutil.which("gnucash-cli")
    if found:
        return found
    raise CognitiveCliError(
        "gnucash-cli not found; set GNUCASH_CLI or install gnucash-cli on PATH"
    )


def run_cli(*args: str, check: bool = True) -> subprocess.CompletedProcess:
    cli = _find_cli()
    cmd = [cli, *args]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if check and proc.returncode != 0:
        raise CognitiveCliError(
            f"command failed ({proc.returncode}): {' '.join(cmd)}\n"
            f"stdout={proc.stdout}\nstderr={proc.stderr}"
        )
    return proc


def capability_report_text() -> str:
    """Return human-readable capability matrix from gnucash-cli."""
    return run_cli("--cognitive-capabilities").stdout


def dump_state(output_file: Optional[str] = None) -> Dict[str, Any]:
    """Dump cognitive state JSON (optionally write to output_file)."""
    args = ["--cognitive-dump"]
    if output_file:
        args.extend(["--output-file", output_file])
        run_cli(*args)
        with open(output_file, "r", encoding="utf-8") as fh:
            return json.load(fh)
    proc = run_cli(*args)
    return json.loads(proc.stdout)


def validate_book(book_path: Optional[str] = None,
                  output_file: Optional[str] = None) -> Dict[str, Any]:
    """Run PLN validation summary; optional book path."""
    args = ["--cognitive-validate"]
    if book_path:
        args.append(book_path)
    if output_file:
        args.extend(["--output-file", output_file])
        run_cli(*args)
        with open(output_file, "r", encoding="utf-8") as fh:
            return json.load(fh)
    proc = run_cli(*args)
    return json.loads(proc.stdout)


def import_fincosys(sync_file: str, export_file: Optional[str] = None) -> str:
    """Import fincosys JSON; optional re-export path. Returns stdout."""
    args = ["--import-fincosys-sync", sync_file]
    if export_file:
        args.extend(["--export-fincosys-sync", export_file])
    return run_cli(*args).stdout


def try_ctypes_lib() -> Optional[Any]:
    """Best-effort load of libgnc-engine for in-process experiments.

    Returns a loaded CDLL or None. Symbol availability varies by build.
    Set GNC_ENGINE_LIB to the full path of libgnc-engine.so.
    """
    import ctypes

    path = os.environ.get("GNC_ENGINE_LIB")
    if not path or not Path(path).exists():
        return None
    lib = ctypes.CDLL(path)
    # Common symbols (presence indicates a cognitive-enabled engine build)
    for name in (
        "gnc_cognitive_accounting_init",
        "gnc_cognitive_accounting_shutdown",
        "gnc_cognitive_dump_state_json",
        "gnc_cognitive_capability_report",
        "gnc_atomspace_count_atoms",
    ):
        if not hasattr(lib, name):
            # CDLL always has attributes via __getattr__ failure — probe
            try:
                getattr(lib, name)
            except AttributeError:
                return None
    return lib


def main(argv: Optional[list] = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ("-h", "--help"):
        print(
            "Usage:\n"
            "  cognitive_ctypes_bridge.py capabilities\n"
            "  cognitive_ctypes_bridge.py dump [out.json]\n"
            "  cognitive_ctypes_bridge.py validate [book.gnucash] [out.json]\n"
            "  cognitive_ctypes_bridge.py import SYNC.json [EXPORT.json]\n"
        )
        return 0
    cmd = argv[0]
    try:
        if cmd == "capabilities":
            print(capability_report_text(), end="")
        elif cmd == "dump":
            out = argv[1] if len(argv) > 1 else None
            data = dump_state(out)
            if not out:
                print(json.dumps(data, indent=2))
        elif cmd == "validate":
            book = argv[1] if len(argv) > 1 else None
            out = argv[2] if len(argv) > 2 else None
            data = validate_book(book, out)
            if not out:
                print(json.dumps(data, indent=2))
        elif cmd == "import":
            if len(argv) < 2:
                raise CognitiveCliError("import requires SYNC.json")
            export = argv[2] if len(argv) > 2 else None
            print(import_fincosys(argv[1], export), end="")
        else:
            raise CognitiveCliError(f"unknown command: {cmd}")
    except CognitiveCliError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
