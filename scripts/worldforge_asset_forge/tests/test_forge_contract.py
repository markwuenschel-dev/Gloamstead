import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock
import zipfile
import random
import re

HERE = Path(__file__).resolve()
ROOT = HERE.parents[3]
import sys
sys.path.insert(0, str(HERE.parents[1]))

import forge_contract as fc


class ContractTests(unittest.TestCase):
    def test_committed_contract_is_closed_and_hash_bound(self):
        verdict = fc.validate_repository_contract(ROOT)
        self.assertEqual([], verdict, verdict)

    def test_inventory_rejects_unknown_field_path_escape_and_cycle(self):
        inventory = fc.load_json(ROOT / fc.VERSION_ROOT / "inventory.json")
        inventory["unexpected"] = True
        self.assertIn("FAIL-CONTRACT-DRIFT", "\n".join(fc.validate_inventory(inventory)))
        inventory = fc.load_json(ROOT / fc.VERSION_ROOT / "inventory.json")
        inventory["deliverables"][0]["source_path"] = "../escape.fbx"
        self.assertIn("FAIL-CONTRACT-DRIFT", "\n".join(fc.validate_inventory(inventory)))
        inventory = fc.load_json(ROOT / fc.VERSION_ROOT / "inventory.json")
        inventory["deliverables"][0]["dependency_ids"] = [inventory["deliverables"][0]["id"]]
        self.assertIn("FAIL-BIOME-KIT-INCOMPLETE", "\n".join(fc.validate_inventory(inventory)))

    def test_inventory_is_dependency_closed_for_channels_and_ruin_interfaces(self):
        inventory = fc.load_json(ROOT / fc.VERSION_ROOT / "inventory.json")
        by_id = {item["id"]: item for item in inventory["deliverables"]}
        self.assertEqual(107, len(by_id))
        for surface in ("ashen-soil", "cracked-stone", "withered-loam-moss"):
            self.assertEqual(
                {f"texture-{surface}-base-color", f"texture-{surface}-normal", f"texture-{surface}-orm"},
                set(by_id[f"surface-{surface}"]["dependency_ids"]) - {"master-surface"})
        self.assertEqual(8, sum(1 for item in by_id.values() if item["id"].startswith("texture-decal-")))
        self.assertEqual(4, sum(1 for item in by_id.values() if item["id"].startswith("flipbook-vfx-")))
        roles = set()
        for item in (value for value in by_id.values() if value["family"] == "architecture"):
            spec = item["production_spec"]
            roles.add(spec["functional_role"])
            self.assertEqual(3, len(spec["dimensions_cm"]))
            self.assertTrue(spec["snap_interfaces"])
            self.assertTrue(spec["silhouette_requirement"])
            self.assertTrue(spec["placement_consumers"])
        self.assertEqual(12, len(roles))

    def test_fake_promoted_object_is_rejected_before_persistence(self):
        with self.assertRaises(fc.ForgeError) as caught:
            fc.validate_result_bindings({"state": "Promoted"}, {"request_id": "request"})
        self.assertEqual("FAIL-CONTRACT-DRIFT", caught.exception.code)

    def test_catalog_inspection_requires_exact_soft_reference_closure(self):
        request = {"desired_active_pointer": {"path": "/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog", "value": "sanctuary-biome-kit-1.0.0"},
                   "version_roots": {"unreal_root": "/Game/Gloamstead/Generated/Biomes/Sanctuary/1_0_0"},
                   "output_allowlist": {"unreal_paths": ["/Game/Gloamstead/Generated/Biomes/Sanctuary/1_0_0/mesh.mesh"]}}
        result = {"state": "Promoted", "promotion_receipt_sha256": "a" * 64}
        report = {"class_name": "GloamsteadGeneratedAssetCatalog", "bundle_id": "sanctuary-biome-kit-1.0.0",
                  "receipt_sha256": "a" * 64, "version_root": request["version_roots"]["unreal_root"],
                  "entry_count": 1, "soft_object_paths": [request["output_allowlist"]["unreal_paths"][0]]}
        fc.validate_catalog_inspection_report(report, request, result)
        report["soft_object_paths"] = []
        with self.assertRaises(fc.ForgeError) as caught: fc.validate_catalog_inspection_report(report, request, result)
        self.assertEqual("FAIL-POINTER-CLOSURE", caught.exception.code)

    def test_acceptance_cannot_be_weakened_by_automatic_run(self):
        profile = fc.load_json(ROOT / fc.VERSION_ROOT / "acceptance-profile.json")
        profile["automatic_mutation_allowed"] = True
        self.assertIn("FAIL-CONTRACT-DRIFT", "\n".join(fc.validate_acceptance(profile)))

    def test_runtime_identity_matches_cpp_canonical_wire(self):
        observed = {
            "engine_version": "5.8.0", "compatible_engine_version": "5.8.0",
            "engine_build_version": "++UE5+Release-5.8-CL-55116800",
            "engine_changelist": 55116800, "compatible_engine_changelist": 55116800,
            "gloamstead_commit": "a" * 40, "plugin_version": "0.2.0",
            "plugin_engine_version": "5.8.0", "plugin_descriptor_sha256": "b" * 64,
            "installed_plugin_tree_sha256": "c" * 64, "vendor_lock_sha256": "d" * 64,
            "declared_plugin_package_sha256": "e" * 64,
            "declared_plugin_build_identity": "wfplugin-" + "f" * 64,
            "enabled_plugins": [{"plugin_name": "WorldForge", "plugin_version": "0.2.0",
                "descriptor_sha256": "b" * 64, "installed_plugin_tree_sha256": "c" * 64,
                "build_identity": "wfplugin-" + "f" * 64,
                "script_packages": ["/Script/WorldForgeCore"]}],
            "engine_script_packages": ["/Script/Engine"],
            "gloamstead_script_packages": ["/Script/Gloamstead"]
        }
        canonical, authorities = fc.canonical_runtime_identity(observed)
        self.assertTrue(canonical.startswith("gloamstead.worldforge.runtime-identity@1\nengine_version="))
        self.assertIn("terminal_script_authority=/Script/WorldForgeCore|worldforge_plugin|WorldForge|", canonical)
        self.assertEqual(3, len(authorities))

    def test_checked_in_hostile_fixtures_are_enforced(self):
        fixtures = {p.name: fc.load_json(p) for p in (ROOT / "specs/worldforge_asset_forge/fixtures/hostile").glob("*.json")}
        expected = {name: fixture["expected_failure"] for name, fixture in fixtures.items()}
        self.assertEqual({"authority-inversion.json", "dependency-cycle.json", "path-traversal.json", "stale-plugin.json", "weaken-acceptance.json"}, set(expected))
        self.assertEqual("FAIL-SCOPE-CREEP", expected["authority-inversion.json"])
        self.assertEqual([], fc.validate_repository_contract(ROOT), "committed positive fixture must remain canonical")
        validators = {"inventory": ("inventory.json", fc.validate_inventory), "acceptance": ("acceptance-profile.json", fc.validate_acceptance), "intent": ("art-intent.json", fc.validate_intent), "lock": (None, fc.validate_lock)}
        for fixture in fixtures.values():
            root_key, path = fixture["target"].split(".", 1)
            filename, validator = validators[root_key]
            document = fc.load_json(ROOT / (fc.LOCK_PATH if filename is None else fc.VERSION_ROOT / filename))
            current = document
            parts = path.split(".")
            for part in parts[:-1]:
                match = re.fullmatch(r"([^[]+)\[(\d+)\]", part)
                current = current[match.group(1)][int(match.group(2))] if match else current[part]
            current[parts[-1]] = fixture["value"]
            self.assertIn(fixture["expected_failure"], "\n".join(validator(document)), fixture["target"])

    def test_hash_binding_fuzz_rejects_every_mutation(self):
        original = fc.load_json(ROOT / fc.VERSION_ROOT / "inventory.json")
        rng = random.Random(20260801)
        for _ in range(200):
            mutated = json.loads(json.dumps(original))
            member = rng.choice(mutated["deliverables"])
            member["art_purpose"] += chr(rng.randint(65, 90))
            self.assertIn("FAIL-EVIDENCE-INTEGRITY", "\n".join(fc.validate_inventory(mutated)))

    def test_workstation_probe_is_honestly_red(self):
        failures = fc.probe_workstation(ROOT)
        self.assertTrue(any(x.startswith("FAIL-AI-PIN-DRIFT") for x in failures))
        self.assertTrue(any("Substance" in x for x in failures))
        self.assertTrue(any("21.0.729" in x and "21.0.753" in x for x in failures))

    def test_qualified_label_without_independent_probe_evidence_is_red(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "root"
            shutil.copytree(ROOT / fc.VERSION_ROOT, root / fc.VERSION_ROOT)
            pins = Path(td) / "pins.json"
            pins.write_text(json.dumps({"qualified": True}), encoding="utf-8")
            requirements_path = root / fc.VERSION_ROOT / "toolchain-requirements.json"
            requirements = fc.load_json(requirements_path)
            requirements["status"] = "qualified"
            requirements["qualified_pins_sha256"] = fc.canonical_hash(fc.load_json(pins))
            requirements_path.write_text(json.dumps(requirements), encoding="utf-8")
            failures = fc.probe_workstation(root, pins)
            self.assertTrue(any(x.startswith("FAIL-UNVERIFIED-RUNTIME") for x in failures))
            self.assertTrue(any("probe_evidence" in x for x in failures))

    def test_real_json_schema_rejects_unknown_field_and_traversal(self):
        inventory = fc.load_json(ROOT / fc.VERSION_ROOT / "inventory.json")
        inventory["unknown"] = True
        inventory["deliverables"][0]["source_path"] = "../escape.fbx"
        with tempfile.TemporaryDirectory() as td:
            candidate = Path(td) / "bad.json"; candidate.write_text(json.dumps(inventory), encoding="utf-8")
            schema = ROOT / "specs/worldforge_asset_forge/schemas/inventory.schema.json"
            command = f"$j=Get-Content -Raw -LiteralPath '{candidate}'; if ($j | Test-Json -SchemaFile '{schema}' -ErrorAction SilentlyContinue) {{ exit 0 }} else {{ exit 3 }}"
            completed = subprocess.run(["pwsh", "-NoProfile", "-Command", command], capture_output=True, text=True)
            self.assertEqual(3, completed.returncode, completed.stdout + completed.stderr)


class VendorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package = Path(os.environ.get("WF_TEST_PACKAGE", r"D:\Unreal Projects\.artifacts\worldforge\0.2.0\WorldForge-0.2.0.zip"))
        cls.manifest = Path(os.environ.get("WF_TEST_MANIFEST", r"D:\Unreal Projects\.artifacts\worldforge\0.2.0\release-manifest.json"))

    def make_repo(self):
        td = tempfile.TemporaryDirectory()
        root = Path(td.name)
        shutil.copytree(ROOT / "specs/worldforge_asset_forge", root / "specs/worldforge_asset_forge")
        shutil.copy2(ROOT / ".gitattributes", root / ".gitattributes")
        (root / "Plugins" / "WorldForge").mkdir(parents=True)
        (root / "Plugins" / "WorldForge" / "old.txt").write_text("old", encoding="utf-8")
        subprocess.run(["git", "init"], cwd=root, check=True, capture_output=True)
        subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=root, check=True)
        subprocess.run(["git", "config", "user.name", "test"], cwd=root, check=True)
        subprocess.run(["git", "config", "core.autocrlf", "false"], cwd=root, check=True)
        excludes = root / ".fixture-global-ignore"; excludes.write_text("", encoding="utf-8")
        subprocess.run(["git", "config", "core.excludesFile", str(excludes)], cwd=root, check=True)
        subprocess.run(["git", "add", "."], cwd=root, check=True)
        subprocess.run(["git", "commit", "-m", "fixture"], cwd=root, check=True, capture_output=True)
        return td, root

    def test_verified_package_installs_and_hand_edit_is_rejected(self):
        td, root = self.make_repo()
        self.addCleanup(td.cleanup)
        fc.sync_plugin(root, self.package, self.manifest)
        self.assertEqual([], fc.verify_installed_plugin(root))
        with (root / "Plugins/WorldForge/WorldForge.uplugin").open("ab") as stream:
            stream.write(b" ")
        self.assertIn("FAIL-STALE-PLUGIN", fc.verify_installed_plugin(root)[0])

    def test_zip_traversal_and_unexpected_file_rejected(self):
        td, root = self.make_repo()
        self.addCleanup(td.cleanup)
        hostile = root / "hostile.zip"
        with zipfile.ZipFile(self.package) as source, zipfile.ZipFile(hostile, "w") as out:
            for item in source.infolist():
                out.writestr(item, source.read(item.filename))
            out.writestr("../escape", b"x")
        subprocess.run(["git", "add", "hostile.zip"], cwd=root, check=True)
        subprocess.run(["git", "commit", "-m", "hostile fixture"], cwd=root, check=True, capture_output=True)
        with self.assertRaises(fc.ForgeError) as caught:
            fc.sync_plugin(root, hostile, self.manifest)
        self.assertEqual("FAIL-EVIDENCE-INTEGRITY", caught.exception.code)

    def test_forbidden_zip_parts_are_case_insensitive(self):
        for name in ("binaries/x.dll", "InTeRmEdIaTe/x.obj", "WFRUNTIME/x.uasset", "RePoRtS/x.json"):
            with tempfile.TemporaryDirectory() as td:
                package = Path(td) / "hostile.zip"
                with zipfile.ZipFile(package, "w") as archive: archive.writestr(name, b"x")
                with zipfile.ZipFile(package) as archive:
                    with self.assertRaises(fc.ForgeError, msg=name):
                        fc._validate_archive_entry_names(archive.infolist())

    def test_vendor_swap_failure_never_leaves_ambiguous_plugin(self):
        class FailingOps(fc.SyncFileOps):
            def __init__(self, fail_at): self.count, self.fail_at = 0, fail_at
            def _before(self):
                self.count += 1
                if self.count == self.fail_at: raise OSError("injected")
            def replace(self, source, target): self._before(); return super().replace(source, target)
            def remove_tree(self, path): self._before(); return super().remove_tree(path)
        for fail_at in range(1, 6):
            with self.subTest(fail_at=fail_at):
                td, root = self.make_repo()
                try:
                    try: fc.sync_plugin(root, self.package, self.manifest, file_ops=FailingOps(fail_at), operation_id=f"failure-{fail_at}")
                    except fc.ForgeError as exc: self.assertIn(exc.code, {"FAIL-ROLLBACK", "FAIL-STALE-PLUGIN"})
                    old = (root / "Plugins/WorldForge/old.txt").is_file()
                    verified = fc.verify_installed_plugin(root) == []
                    self.assertNotEqual(old, verified, "target must be exactly old or exactly verified")
                finally: td.cleanup()

    def test_request_matches_frozen_worldforge_wire(self):
        td, root = self.make_repo()
        self.addCleanup(td.cleanup)
        fc.sync_plugin(root, self.package, self.manifest)
        h = "1" * 64
        pins = {"qualified": True, "comfy": {"repository_commit":"a"*40,"creation_workflow_sha256":h,"evaluation_workflow_sha256":h,"checkpoint_sha256":h,"vae_sha256":h,"lora_sha256":h,"control_model_sha256":h,"custom_node_lock_sha256":h,"adapter":"comfy-local-v1","seed":1138,"sampler":"dpmpp-2m","scheduler":"karras","hardware_class":"rtx-workstation"},
            "substance":{"graph_ref":"specs/worldforge_asset_forge/toolchains/sanctuary.sbs","graph_sha256":h,"sbscooker_version":"15.0","sbscooker_sha256":h,"sbsrender_version":"15.0","sbsrender_sha256":h},
            "houdini":{"hda_ref":"specs/worldforge_asset_forge/toolchains/sanctuary.hdalc","hda_sha256":h,"houdini_version":"21.0.753","houdini_executable_sha256":h,"houdini_engine_version":"21.0.753","houdini_engine_plugin_sha256":h},
            "vendor_pins":{"python":{"version":"3.13.0"}}, "validator_pins":{"gloamstead-qualification":{"version":"1.0.0"}}}
        pins_path = root / "qualified-pins.json"
        pins_path.write_text(json.dumps(pins), encoding="utf-8")
        with mock.patch.object(fc, "validate_qualified_pins", return_value=pins) as validator:
            request = fc.build_request(root, pins_path, "2026-08-02T00:00:00Z", "refs/heads/test", None)
        validator.assert_called_once_with(root, pins_path)
        request_path = root / "request.json"; request_path.write_text(json.dumps(request), encoding="utf-8")
        worldforge = Path(r"D:\Unreal Projects\.worktrees\worldforge-production-asset-forge")
        code = "import json,sys; from tools.asset_forge.contracts import BiomeKitRequest; BiomeKitRequest.from_dict(json.load(open(sys.argv[1],encoding='utf-8')))"
        completed = subprocess.run([sys.executable, "-c", code, str(request_path)], cwd=worldforge, capture_output=True, text=True)
        self.assertEqual(0, completed.returncode, completed.stderr)

    def test_generated_commit_requires_canonical_lfs_pointer_and_local_object(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            subprocess.run(["git", "init"], cwd=root, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=root, check=True)
            subprocess.run(["git", "config", "user.name", "test"], cwd=root, check=True)
            (root / "seed.txt").write_text("seed", encoding="utf-8")
            subprocess.run(["git", "add", "seed.txt"], cwd=root, check=True)
            subprocess.run(["git", "commit", "-m", "seed"], cwd=root, check=True, capture_output=True)
            payload = b"qualified texture bytes"
            digest = hashlib.sha256(payload).hexdigest()
            path = "SourceArt/WorldForge/Accepted/Sanctuary/1.0.0/surfaces/test.png"
            physical = root / path; physical.parent.mkdir(parents=True)
            physical.write_text(f"version https://git-lfs.github.com/spec/v1\noid sha256:{digest}\nsize {len(payload)}\n", encoding="utf-8", newline="\n")
            obj = root / ".git/lfs/objects" / digest[:2] / digest[2:4] / digest
            obj.parent.mkdir(parents=True); obj.write_bytes(payload)
            pointer_payload = b"active generated catalog bytes"
            pointer_digest = hashlib.sha256(pointer_payload).hexdigest()
            pointer_path = "Content/Gloamstead/Generated/DA_GeneratedAssetCatalog.uasset"
            pointer_file = root / pointer_path; pointer_file.parent.mkdir(parents=True)
            pointer_file.write_text(f"version https://git-lfs.github.com/spec/v1\noid sha256:{pointer_digest}\nsize {len(pointer_payload)}\n", encoding="utf-8", newline="\n")
            pointer_obj = root / ".git/lfs/objects" / pointer_digest[:2] / pointer_digest[2:4] / pointer_digest
            pointer_obj.parent.mkdir(parents=True); pointer_obj.write_bytes(pointer_payload)
            subprocess.run(["git", "add", path, pointer_path], cwd=root, check=True)
            subprocess.run(["git", "commit", "-m", "generated"], cwd=root, check=True, capture_output=True)
            commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, check=True, capture_output=True, text=True).stdout.strip()
            request = {"output_allowlist": {"source_paths": [path], "unreal_paths": []},
                       "desired_active_pointer": {"path": "/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog"}}
            result = {"generated_commit": commit, "evidence_refs": [], "artifacts": {path: {
                "sha256": digest, "size_bytes": len(payload), "lfs_oid": f"sha256:{digest}"}}}
            fc.verify_generated_commit_lfs_closure(root, request, result)
            result["artifacts"][path]["size_bytes"] += 1
            with self.assertRaises(fc.ForgeError): fc.verify_generated_commit_lfs_closure(root, request, result)


if __name__ == "__main__":
    unittest.main()
