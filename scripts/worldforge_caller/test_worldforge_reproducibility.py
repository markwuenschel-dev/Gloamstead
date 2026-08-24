"""Enforce the two properties Gloamstead actually needs from the WorldForge bridge.

The bridge had no enforcement around it: the compiler is a separate product, so nothing in this repo
proved either of the properties the game depends on. These tests do, whenever the external compiler is
reachable, and skip cleanly when it is not - so a machine without WorldForge checked out still gets a
green suite instead of a false failure.

1. REPRODUCIBLE. Two independent compiles of the same authored intent, into different output roots, must
   produce byte-identical artifacts. Determinism is what makes a planned world reviewable and diffable;
   without it every re-run is a new world and no receipt means anything.

2. THE SEMANTIC AUTHORITY BOUNDARY HOLDS. Gloamstead owns intent; WorldForge owns execution. The
   compiler must never receive - and must never echo back - Gloamstead's semantic vocabulary: warnings,
   ritual meaning, evidence. Opaque identifiers are fine; interpretation is not. The semantic material
   belongs in the caller-owned provenance sidecar, and only there.
"""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
BRIDGE = REPO_ROOT / "scripts" / "worldforge_caller" / "compile_cycle2_world.py"

WORLDFORGE_ARTIFACTS = (
    "manifest.json",
    "terrain-slice.json",
    "poi-descriptors.json",
    "placement-variants.json",
    "material-variants.json",
    "survey-requests.json",
)
CALLER_ARTIFACTS = ("intent-provenance.json", "materialization-request.json", "bridge-receipt.json")

# Vocabulary that is Gloamstead's to interpret and WorldForge's to never see. `Cycle2_Garden` is
# deliberately NOT here: it is the world id, an opaque handle, and carrying it is not interpreting it.
SEMANTIC_VOCABULARY = ("GardenRot", "warning", "ritual", "evidence", "veilheart", "lantern")


def _discover_pipeline() -> Path | None:
    """Locate the external WorldForge tools/pipeline directory, or None to skip.

    Discovery only - never invents a path. WORLDFORGE_COMPILER wins; otherwise look in the conventional
    sibling checkout and any sibling worktree.
    """
    env = os.environ.get("WORLDFORGE_COMPILER")
    if env and Path(env).is_file():
        return Path(env).parent

    siblings = [REPO_ROOT.parent / "WorldForge" / "tools" / "pipeline"]
    worktrees = REPO_ROOT.parent / ".worktrees"
    if worktrees.is_dir():
        siblings.extend(sorted(worktrees.glob("*/tools/pipeline")))

    for candidate in siblings:
        if (candidate / "compile_authored_world.py").is_file():
            return candidate
    return None


PIPELINE = _discover_pipeline()
requires_worldforge = pytest.mark.skipif(
    PIPELINE is None,
    reason="external WorldForge compiler not reachable; set WORLDFORGE_COMPILER to enforce these properties",
)


def _compile(output_root: Path) -> None:
    result = subprocess.run(
        [
            sys.executable,
            str(BRIDGE),
            "--worldforge-compiler", str(PIPELINE / "compile_authored_world.py"),
            "--worldforge-materialization-preparer", str(PIPELINE / "prepare_authored_world_materialization.py"),
            "--output-root", str(output_root),
        ],
        cwd=str(REPO_ROOT), capture_output=True, text=True,
    )
    assert result.returncode == 0, "bridge failed: {}".format(result.stderr.strip()[:600])


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


@requires_worldforge
def test_compilation_is_byte_reproducible(tmp_path: Path) -> None:
    first, second = tmp_path / "run-a", tmp_path / "run-b"
    _compile(first)
    _compile(second)

    drifted = []
    for name in WORLDFORGE_ARTIFACTS + CALLER_ARTIFACTS:
        a, b = first / name, second / name
        assert a.is_file(), "run A produced no {}".format(name)
        assert b.is_file(), "run B produced no {}".format(name)
        if _sha256(a) != _sha256(b):
            drifted.append(name)

    assert not drifted, "these artifacts are not reproducible across runs: {}".format(", ".join(drifted))


@requires_worldforge
def test_worldforge_never_receives_gloamstead_semantics(tmp_path: Path) -> None:
    root = tmp_path / "boundary"
    _compile(root)

    leaks = []
    for name in WORLDFORGE_ARTIFACTS:
        body = (root / name).read_text(encoding="utf-8").lower()
        for term in SEMANTIC_VOCABULARY:
            if term.lower() in body:
                leaks.append("{} leaked '{}'".format(name, term))

    assert not leaks, (
        "WorldForge artifacts must carry no Gloamstead semantic vocabulary - it interprets nothing and "
        "owns execution only: {}".format("; ".join(leaks))
    )

    # ...and the meaning must actually survive somewhere: the caller-owned sidecar.
    sidecar = (root / "intent-provenance.json").read_text(encoding="utf-8")
    assert "GardenRot" in sidecar, "the caller-owned provenance sidecar lost the authored warning identity"


@requires_worldforge
def test_compiler_claims_no_materialization(tmp_path: Path) -> None:
    # The compiler plans; it does not start Unreal, load a map, or observe anything. A manifest that
    # claimed otherwise would be evidence of work that never happened.
    root = tmp_path / "status"
    _compile(root)
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["execution_status"] == "not_materialized", (
        "the compiler must not claim materialization it did not perform; got {!r}".format(
            manifest["execution_status"])
    )
    assert manifest["generated_at_utc"] == "1970-01-01T00:00:00Z", (
        "the pinned epoch is what makes the manifest reproducible; a real timestamp breaks determinism"
    )
