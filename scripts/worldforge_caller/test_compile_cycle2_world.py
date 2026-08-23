#!/usr/bin/env python3
"""Focused contract tests for the Cycle II -> WorldForge caller bridge."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MODULE = Path(__file__).with_name("compile_cycle2_world.py")
INTENT = REPO_ROOT / "specs/world/cycle-2-corruption-neglect.world.json"
SCHEMA = REPO_ROOT / "specs/world/gloamstead_world_spec.schema.json"
COMPILER = Path(r"D:/Unreal Projects/.worktrees/worldforge-gloamstead-cycle2-factory/tools/pipeline/compile_authored_world.py")


def run_bridge(intent: Path, schema: Path, compiler: Path | None, output: Path) -> subprocess.CompletedProcess[str]:
    command = [sys.executable, str(MODULE), "--intent", str(intent), "--schema", str(schema),
               "--output-root", str(output)]
    if compiler is not None:
        command.extend(["--worldforge-compiler", str(compiler)])
    return subprocess.run(command, cwd=REPO_ROOT, capture_output=True, text=True, check=False)


class Cycle2WorldForgeBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not COMPILER.is_file():
            raise unittest.SkipTest("real external WorldForge compiler worktree is unavailable")

    def _inputs(self, root: Path) -> tuple[Path, Path, bytes, bytes]:
        intent, schema = root / "intent.json", root / "schema.json"
        before_intent, before_schema = INTENT.read_bytes(), SCHEMA.read_bytes()
        intent.write_bytes(before_intent)
        schema.write_bytes(before_schema)
        return intent, schema, before_intent, before_schema

    def test_valid_projection_is_deterministic_and_honest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            intent, schema, before_intent, before_schema = self._inputs(root)
            first, second = root / "first", root / "second"
            first_run = run_bridge(intent, schema, COMPILER, first)
            self.assertEqual(0, first_run.returncode, first_run.stderr)
            second_run = run_bridge(intent, schema, COMPILER, second)
            self.assertEqual(0, second_run.returncode, second_run.stderr)
            self.assertEqual(before_intent, intent.read_bytes())
            self.assertEqual(before_schema, schema.read_bytes())
            expected = {"manifest.json", "terrain-slice.json", "poi-descriptors.json",
                        "placement-variants.json", "material-variants.json", "survey-requests.json",
                        "intent-provenance.json", "bridge-receipt.json"}
            self.assertEqual(expected, {path.name for path in first.iterdir()})
            self.assertEqual({path.name: path.read_bytes() for path in first.iterdir()},
                             {path.name: path.read_bytes() for path in second.iterdir()})
            provenance = json.loads((first / "intent-provenance.json").read_text(encoding="utf-8"))
            receipt = json.loads((first / "bridge-receipt.json").read_text(encoding="utf-8"))
            self.assertEqual("not_materialized", provenance["execution_status"])
            self.assertEqual("not_observed", provenance["observation_status"])
            self.assertIn("GardenRot", json.dumps(provenance["semantic_intent_retained_by_gloamstead"]))
            self.assertEqual(hashlib.sha256(before_intent).hexdigest(), receipt["source"]["intent_sha256"])
            self.assertEqual(hashlib.sha256(before_schema).hexdigest(), receipt["source"]["schema_sha256"])
            self.assertEqual("Cycle2_Garden", receipt["poi_id"])
            self.assertEqual("Cycle2_Garden.Anchor", receipt["poi_anchor_id"])
            self.assertEqual(["untouched", "restored"],
                             [item["label"] for item in receipt["worldforge_state_scenarios"]])
            self.assertIn("world_spec_path", receipt)
            self.assertIn("schema_path", receipt)
            self.assertIn("worldforge_source_clean", receipt)
            self.assertIn("manifest_source_tree_dirty", receipt["compiler"])
            self.assertEqual("not_materialized", receipt["execution_status"])
            self.assertEqual("not_observed", receipt["observation_status"])
            self.assertEqual("none", receipt["materialization_claim"])
            self.assertNotIn("materialized", receipt["scope"].replace("not materialized", ""))
            self.assertEqual(hashlib.sha256((first / "manifest.json").read_bytes()).hexdigest(),
                             receipt["outputs"]["output_manifest_sha256"])
            terrain = json.loads((first / "terrain-slice.json").read_text(encoding="utf-8"))
            self.assertNotIn("GardenRot", json.dumps(terrain))

    def _assert_preinvoke_failure(self, mutate, needle: str):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            intent, schema, _, _ = self._inputs(root)
            value = json.loads(intent.read_text(encoding="utf-8"))
            mutate(value)
            intent.write_text(json.dumps(value), encoding="utf-8")
            fake = root / "fake_compiler.py"
            marker = root / "invoked"
            fake.write_text("from pathlib import Path\nPath(r'{}').write_text('called')\n".format(marker), encoding="utf-8")
            result = run_bridge(intent, schema, fake, root / "out")
            self.assertEqual(2, result.returncode, result.stderr)
            self.assertIn(needle, result.stderr)
            self.assertFalse(marker.exists(), "invalid intent invoked external compiler")
            self.assertFalse((root / "out").exists())

    def test_warning_evidence_and_unknown_fields_fail_before_invocation(self):
        self._assert_preinvoke_failure(lambda value: value["subjects"][0].__setitem__("warningId", "Wrong"),
                                       "$.subjects[0].warningId")
        self._assert_preinvoke_failure(lambda value: value["evidence"]["supportBindings"][0].__setitem__("surface", "audio"),
                                       "$.evidence.supportBindings[0].surface")
        self._assert_preinvoke_failure(lambda value: value.__setitem__("unknown", True), "$: unknown field")

    def test_json_types_and_schema_drift_fail_before_invocation(self):
        self._assert_preinvoke_failure(lambda value: value.__setitem__("specVersion", True),
                                       "$.specVersion: expected integer")
        self._assert_preinvoke_failure(lambda value: value["generationInput"].__setitem__("seed", 42.0),
                                       "$.generationInput.seed: expected integer")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            intent, schema, _, _ = self._inputs(root)
            schema.write_text("{}", encoding="utf-8")
            fake = root / "fake_compiler.py"
            marker = root / "invoked"
            fake.write_text("from pathlib import Path\nPath(r'{}').write_text('called')\n".format(marker), encoding="utf-8")
            result = run_bridge(intent, schema, fake, root / "out")
            self.assertEqual(2, result.returncode, result.stderr)
            self.assertIn("semantic schema SHA-256", result.stderr)
            self.assertFalse(marker.exists())
            self.assertFalse((root / "out").exists())

    def test_output_publish_is_atomic_and_destination_is_not_reused(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            intent, schema, _, _ = self._inputs(root)
            partial = root / "partial_compiler.py"
            partial.write_text(
                "import sys\n"
                "from pathlib import Path\n"
                "out = Path(sys.argv[sys.argv.index('--output-root') + 1])\n"
                "(out / 'manifest.json').write_text('partial', encoding='utf-8')\n"
                "raise SystemExit(7)\n", encoding="utf-8")
            staged_failure = root / "staged-failure"
            result = run_bridge(intent, schema, partial, staged_failure)
            self.assertEqual(2, result.returncode, result.stderr)
            self.assertFalse(staged_failure.exists(), "failed compile leaked a partial final output")

            first = root / "first"
            first_run = run_bridge(intent, schema, COMPILER, first)
            self.assertEqual(0, first_run.returncode, first_run.stderr)
            before = {path.name: path.read_bytes() for path in first.iterdir()}
            marker = root / "reused"
            reuse = root / "reuse_compiler.py"
            reuse.write_text("from pathlib import Path\nPath(r'{}').write_text('called')\n".format(marker), encoding="utf-8")
            second_run = run_bridge(intent, schema, reuse, first)
            self.assertEqual(2, second_run.returncode, second_run.stderr)
            self.assertIn("output root already exists", second_run.stderr)
            self.assertFalse(marker.exists(), "existing destination was handed to the compiler")
            self.assertEqual(before, {path.name: path.read_bytes() for path in first.iterdir()})

    def test_duplicate_json_and_path_traversal_fail_before_invocation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fake = root / "fake_compiler.py"
            marker = root / "invoked"
            fake.write_text("from pathlib import Path\nPath(r'{}').write_text('called')\n".format(marker), encoding="utf-8")
            intent, schema, _, _ = self._inputs(root)
            duplicate = intent.read_text(encoding="utf-8").replace('"specVersion": 1,', '"specVersion": 1, "specVersion": 1,', 1)
            intent.write_text(duplicate, encoding="utf-8")
            result = run_bridge(intent, schema, fake, root / "out")
            self.assertEqual(2, result.returncode)
            self.assertIn("duplicate JSON object key", result.stderr)
            self.assertFalse((root / "out").exists())
            self.assertFalse(marker.exists())

            intent, schema, _, _ = self._inputs(root)
            output = REPO_ROOT / "Content" / ".cycle2-bridge-test-illegal"
            result = run_bridge(intent, schema, fake, output)
            self.assertEqual(2, result.returncode)
            self.assertIn("inside Content", result.stderr)
            self.assertFalse(output.exists())
            self.assertFalse(marker.exists())

    def test_missing_compiler_fails_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            intent, schema, _, _ = self._inputs(root)
            result = run_bridge(intent, schema, root / "missing.py", root / "out")
            self.assertEqual(2, result.returncode)
            self.assertIn("compiler does not exist", result.stderr)
            self.assertFalse((root / "out").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
