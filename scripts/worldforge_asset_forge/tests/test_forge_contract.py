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
        inventory = fc.load_json(ROOT / fc.VERSION_ROOT / "inventory.json")
        kind_tokens = {"StaticMesh":"static_mesh", "Material":"material", "MaterialInstance":"material_instance",
            "NiagaraSystem":"niagara_system", "PlacementRulesDataAsset":"placement_rules_data_asset",
            "PrimaryDataAsset":"primary_data_asset", "DataAsset":"data_asset", "Texture2D":"texture_2d",
            "SubUVAnimation":"subuv_animation"}
        class_paths = {"static_mesh": "/Script/Engine.StaticMesh", "material": "/Script/Engine.Material",
            "material_instance": "/Script/Engine.MaterialInstanceConstant", "niagara_system": "/Script/Niagara.NiagaraSystem",
            "placement_rules_data_asset": "/Script/Engine.PrimaryDataAsset", "primary_data_asset": "/Script/Engine.PrimaryDataAsset",
            "data_asset": "/Script/Engine.DataAsset", "texture_2d": "/Script/Engine.Texture2D",
            "subuv_animation": "/Script/Engine.SubUVAnimation"}
        deliverables = {item["id"]: {"kind": kind_tokens[item["asset_class"]], "semantic_role": item["semantic_role"],
            "restoration_state": item["restoration_state"], "unreal_object_path": item["unreal_object_path"],
            "dependency_ids": item["dependency_ids"]} for item in inventory["deliverables"]}
        request = {"desired_active_pointer": {"path": "/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog",
                       "value": inventory["pointer_value"]}, "version_roots": {"unreal_root": inventory["unreal_root"]},
                   "deliverables": deliverables}
        receipt = "a" * 64
        result = {"state": "Promoted", "promotion_receipt_sha256": receipt,
                  "artifacts": {item["unreal_object_path"]: {"sha256": hashlib.sha256(item_id.encode()).hexdigest(),
                      "owner": "Gloamstead", "license_id": "Proprietary-Gloamstead"}
                      for item_id, item in deliverables.items()}}
        entries = []
        for item_id, item in deliverables.items():
            artifact = result["artifacts"][item["unreal_object_path"]]
            dependencies = sorted(deliverables[dependency]["unreal_object_path"] for dependency in item["dependency_ids"])
            entries.append({"deliverable_id": item_id, "semantic_role": item["semantic_role"],
                "restoration_state": item["restoration_state"], "asset_path": item["unreal_object_path"],
                "expected_class_path": class_paths[item["kind"]], "object_sha256": artifact["sha256"],
                "receipt_sha256": receipt, "direct_package_dependencies": sorted(path.split(".", 1)[0] for path in dependencies),
                "declared_direct_package_dependencies": sorted(path.split(".", 1)[0] for path in dependencies),
                "dependencies": dependencies, "ownership_id": artifact["owner"], "license_id": artifact["license_id"]})
        report = {"schema_version": "gloamstead.generated-catalog-inspection.v1",
                  "class_name": "GloamsteadGeneratedAssetCatalog", "bundle_id": inventory["pointer_value"],
                  "receipt_sha256": receipt, "version_root": inventory["unreal_root"],
                  "entry_count": 107, "entries": entries, "terminal_platform_package_roots": [],
                  "terminal_platform_packages": [], "terminal_script_packages": [], "external_packages": []}
        fc.validate_catalog_inspection_report(report, request, result)
        mutations = [
            lambda value: value["entries"][0].__setitem__("semantic_role", "invented-role"),
            lambda value: value["entries"][0]["dependencies"].append("/Engine/EngineMeshes/Cube.Cube"),
            lambda value: value["entries"][0]["direct_package_dependencies"].append("/Engine/EngineMeshes/Cube"),
            lambda value: value["entries"][0].__setitem__("expected_class_path", "/Script/Engine.Texture2D"),
            lambda value: value["entries"][0].__setitem__("restoration_state", "invented"),
            lambda value: value.__setitem__("entry_count", 106),
            lambda value: value["entries"][0].__setitem__("deliverable_id", value["entries"][1]["deliverable_id"]),
        ]
        for mutate in mutations:
            hostile = json.loads(json.dumps(report))
            mutate(hostile)
            with self.assertRaises(fc.ForgeError):
                fc.validate_catalog_inspection_report(hostile, request, result)
        explicitly_terminal = json.loads(json.dumps(report))
        explicitly_terminal["terminal_platform_package_roots"] = ["/Engine"]
        explicitly_terminal["entries"][0]["direct_package_dependencies"].append("/Engine/EngineMeshes/Cube")
        explicitly_terminal["entries"][0]["direct_package_dependencies"].sort()
        explicitly_terminal["entries"][0]["declared_direct_package_dependencies"].append("/Engine/EngineMeshes/Cube")
        explicitly_terminal["entries"][0]["declared_direct_package_dependencies"].sort()
        fc.validate_catalog_inspection_report(explicitly_terminal, request, result)

    def test_catalog_reload_script_queries_asset_registry_instead_of_echoing_declarations(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); git_dir = root / "git"; git_dir.mkdir()
            editor = root / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
            editor.parent.mkdir(parents=True); editor.write_bytes(b"fixture")
            request = {"target": {"project": "Gloamstead5_8.uproject"},
                "desired_active_pointer": {"path": "/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog"},
                "deliverables": {"mesh": {"unreal_object_path": "/Game/Test/SM_Test.SM_Test"}}}
            def editor_probe(command, **kwargs):
                script_arg = next(value for value in command if str(value).startswith("-ExecutePythonScript="))
                script = Path(str(script_arg).split("=", 1)[1]).read_text(encoding="utf-8")
                self.assertIn("registry.get_dependencies", script)
                self.assertIn('"declared_direct_package_dependencies"', script)
                compile(script, "catalog-inspection.py", "exec")
                return subprocess.CompletedProcess(command, 9, "", "injected stop after script QA")
            with mock.patch.object(fc, "load_json", return_value={"ue": {"root": str(root)}}), \
                    mock.patch.object(fc, "_worktree_git_dir", return_value=git_dir), \
                    mock.patch.object(subprocess, "run", side_effect=editor_probe):
                with self.assertRaises(fc.ForgeError) as caught:
                    fc.inspect_active_catalog_with_unreal(root, request, {},)
            self.assertEqual("FAIL-UNVERIFIED-RUNTIME", caught.exception.code)

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
        old_host = root / fc.HOST_MANIFEST_PATH
        old_host.parent.mkdir(parents=True)
        old_host.write_text('{"old":true}\n', encoding="utf-8", newline="\n")
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

    def test_vendor_precommit_failure_restores_canonical_state_and_preserves_backups(self):
        class FailingOps(fc.SyncFileOps):
            def __init__(self, fail_at): self.count, self.fail_at = 0, fail_at
            def _before(self):
                self.count += 1
                if self.count == self.fail_at: raise OSError("injected")
            def replace(self, source, target): self._before(); return super().replace(source, target)
            def remove_tree(self, path): self._before(); return super().remove_tree(path)
            def remove_file(self, path): self._before(); return super().remove_file(path)
        for fail_at in range(1, 5):
            with self.subTest(fail_at=fail_at):
                td, root = self.make_repo()
                try:
                    try:
                        fc.sync_plugin(root, self.package, self.manifest, file_ops=FailingOps(fail_at),
                                       operation_id=f"precommit-{fail_at}")
                    except fc.ForgeError as exc:
                        self.assertIn(exc.code, {"FAIL-ROLLBACK", "FAIL-STALE-PLUGIN"})
                    old = (root / "Plugins/WorldForge/old.txt").is_file()
                    verified = fc.verify_installed_plugin(root) == []
                    self.assertNotEqual(old, verified, "target must be exactly old or exactly verified")
                finally: td.cleanup()

    def test_committed_vendor_cleanup_failure_preserves_new_state_and_replays_both_orders(self):
        class CleanupFailure(fc.SyncFileOps):
            def __init__(self, fail_kind): self.fail_kind = fail_kind
            def remove_tree(self, path):
                if self.fail_kind == "plugin" and ".WorldForge.backup-" in path.name: raise OSError("injected")
                return super().remove_tree(path)
            def remove_file(self, path):
                if self.fail_kind == "host" and ".VerifiedReleaseManifest.backup-" in path.name: raise OSError("injected")
                return super().remove_file(path)
        for cleanup_order in (("plugin", "host"), ("host", "plugin")):
            for fail_kind in cleanup_order:
                with self.subTest(cleanup_order=cleanup_order, fail_kind=fail_kind):
                    td, root = self.make_repo()
                    try:
                        operation_id = f"cleanup-{cleanup_order[0]}-{fail_kind}"
                        with self.assertRaises(fc.RecoveryRequiredError) as caught:
                            fc.sync_plugin(root, self.package, self.manifest,
                                file_ops=CleanupFailure(fail_kind), operation_id=operation_id,
                                cleanup_order=cleanup_order)
                        self.assertEqual("cleanup_pending", caught.exception.phase)
                        self.assertEqual([], fc.verify_installed_plugin(root))
                        self.assertEqual(fc.sha256_file(root / fc.HOST_MANIFEST_PATH),
                                         fc.load_json(root / fc.LOCK_PATH)["release_manifest_sha256"])
                        for backup_path in caught.exception.backup_paths:
                            self.assertTrue(Path(backup_path).exists())
                        fc.sync_plugin(root, self.package, self.manifest, operation_id=operation_id,
                                       cleanup_order=cleanup_order)
                        self.assertEqual([], fc.verify_installed_plugin(root))
                        journal = Path(caught.exception.journal_path)
                        self.assertEqual("complete", fc._journal_entries(journal)[-1]["step"])
                        fc.sync_plugin(root, self.package, self.manifest, operation_id=operation_id,
                                       cleanup_order=cleanup_order)
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

    def test_operator_rejects_clean_alternate_worldforge_checkout_before_request_build(self):
        with tempfile.TemporaryDirectory() as td:
            checkout = Path(td)
            module = checkout / "tools/asset_forge/__main__.py"
            module.parent.mkdir(parents=True); module.write_text("raise SystemExit(99)\n", encoding="utf-8")
            subprocess.run(["git", "init"], cwd=checkout, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=checkout, check=True)
            subprocess.run(["git", "config", "user.name", "test"], cwd=checkout, check=True)
            subprocess.run(["git", "remote", "add", "origin",
                            fc.load_json(ROOT / fc.LOCK_PATH)["source_repository"]], cwd=checkout, check=True)
            subprocess.run(["git", "add", "."], cwd=checkout, check=True)
            subprocess.run(["git", "commit", "-m", "hostile alternate"], cwd=checkout, check=True, capture_output=True)
            with mock.patch.object(fc, "_git_clean", return_value=True), \
                    mock.patch.object(fc, "build_request") as request_builder:
                with self.assertRaises(fc.ForgeError) as caught:
                    fc.run_operator(ROOT, checkout / "pins.json", "2026-08-02T00:00:00Z",
                                    "refs/heads/test", checkout, Path(sys.executable))
            self.assertEqual("FAIL-STALE-PLUGIN", caught.exception.code)
            request_builder.assert_not_called()

    def test_worldforge_release_history_accepts_source_record_tooling_chain_and_rejects_reverse(self):
        with tempfile.TemporaryDirectory() as td:
            checkout = Path(td)
            subprocess.run(["git", "init"], cwd=checkout, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=checkout, check=True)
            subprocess.run(["git", "config", "user.name", "test"], cwd=checkout, check=True)
            repository = "https://example.invalid/WorldForge.git"
            subprocess.run(["git", "remote", "add", "origin", repository], cwd=checkout, check=True)
            module = checkout / "tools/asset_forge/__main__.py"
            schema = checkout / "tools/asset_forge/schemas/biome-kit-request.schema.json"
            module.parent.mkdir(parents=True); schema.parent.mkdir(parents=True)
            module.write_text("raise SystemExit(0)\n", encoding="utf-8")
            schema.write_text('{"schema_version":"fixture.v1"}\n', encoding="utf-8", newline="\n")
            subprocess.run(["git", "add", "."], cwd=checkout, check=True)
            subprocess.run(["git", "commit", "-m", "package source"], cwd=checkout, check=True,
                           capture_output=True)
            source_commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=checkout, check=True,
                capture_output=True, text=True).stdout.strip()
            manifest_path = checkout / "releases/worldforge-plugin/0.2.0/release-manifest.json"
            manifest_path.parent.mkdir(parents=True)
            manifest = {"source_commit": source_commit, "source_tree_sha256": "a" * 64}
            manifest_path.write_bytes(fc.canonical_json_bytes(manifest))
            subprocess.run(["git", "add", "."], cwd=checkout, check=True)
            subprocess.run(["git", "commit", "-m", "release record"], cwd=checkout, check=True,
                           capture_output=True)
            record_commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=checkout, check=True,
                capture_output=True, text=True).stdout.strip()
            (checkout / "tools/asset_forge/tooling.txt").write_text("qualified cli\n", encoding="utf-8")
            subprocess.run(["git", "add", "."], cwd=checkout, check=True)
            subprocess.run(["git", "commit", "-m", "tooling"], cwd=checkout, check=True,
                           capture_output=True)
            tooling_commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=checkout, check=True,
                capture_output=True, text=True).stdout.strip()
            lock = {"source_repository": repository, "source_commit": source_commit,
                    "release_manifest_commit": record_commit,
                    "release_manifest_sha256": fc.sha256_file(manifest_path),
                    "source_tree_sha256": "a" * 64,
                    "schemas": [{"path": "tools/asset_forge/schemas/biome-kit-request.schema.json",
                                 "sha256": fc.sha256_file(schema)}]}
            identity = fc.validate_worldforge_checkout(checkout, lock, tooling_commit)
            self.assertEqual(tooling_commit, identity["tooling_commit"])
            reverse = dict(lock, source_commit=tooling_commit, release_manifest_commit=source_commit)
            with self.assertRaises(fc.ForgeError) as caught:
                fc.validate_worldforge_checkout(checkout, reverse, tooling_commit)
            self.assertEqual("FAIL-STALE-PLUGIN", caught.exception.code)

    def test_operator_rejects_hostile_executor_before_reconcile(self):
        with tempfile.TemporaryDirectory() as td:
            checkout = Path(td) / "approved-checkout"; checkout.mkdir()
            fake_executor = Path(td) / "hostile-python.exe"; fake_executor.write_bytes(b"not approved")
            approved = Path(sys.executable).resolve()
            python_pin = {"version": f"{sys.version_info.major}.{sys.version_info.minor}",
                          "executable": str(approved), "executable_sha256": fc.sha256_file(approved)}
            lock = fc.load_json(ROOT / fc.LOCK_PATH)
            cli_pin = {"repository": lock["source_repository"],
                "package_source_commit": lock["source_commit"],
                "release_record_commit": "b" * 40,
                "tooling_commit": "a" * 40,
                "release_manifest_sha256": lock["release_manifest_sha256"],
                "schemas_sha256": fc.canonical_hash(lock["schemas"])}
            request = {"creation_stacks": {"surfaces": {
                "vendor_pins": {"python": python_pin, "worldforge_cli": cli_pin}}}}
            with mock.patch.object(fc, "_git_clean", return_value=True), \
                    mock.patch.object(fc, "validate_worldforge_checkout", return_value={
                        "package_source_commit": cli_pin["package_source_commit"],
                        "release_record_commit": cli_pin["release_record_commit"],
                        "tooling_commit": cli_pin["tooling_commit"]}), \
                    mock.patch.object(fc, "build_request", return_value=request), \
                    mock.patch.object(subprocess, "run", wraps=subprocess.run) as runner:
                with self.assertRaises(fc.ForgeError) as caught:
                    fc.run_operator(ROOT, Path(td) / "pins.json", "2026-08-02T00:00:00Z",
                                    "refs/heads/test", checkout, fake_executor)
            self.assertEqual("FAIL-EVIDENCE-INTEGRITY", caught.exception.code)
            self.assertFalse(any("tools.asset_forge" in str(call) and "reconcile" in str(call)
                                 for call in runner.call_args_list))

    def test_operator_wires_explicit_roots_config_and_canonical_git_run_inputs(self):
        with tempfile.TemporaryDirectory() as td:
            checkout = Path(td) / "worldforge"
            checkout.mkdir()
            gloam_git = Path(td) / "gloam-git"
            worldforge_git = Path(td) / "worldforge-git"
            gloam_git.mkdir()
            worldforge_git.mkdir()
            head = "c" * 40
            request = {
                "request_sha256": "a" * 64,
                "target": {"base_commit": head},
                "creation_stacks": {"surfaces": {"vendor_pins": {"worldforge": "pinned"}}},
                "qualification_stack": {"validator_pins": {"qualification": "pinned"}},
            }
            result = {
                "state": "Promoted",
                "verified_target": {"commit": head},
                "generated_commit": head,
                "result_sha256": "d" * 64,
            }
            calls = []

            def fake_run(command, **kwargs):
                calls.append((list(command), kwargs))
                if command[:3] == ["git", "merge-base", "--is-ancestor"]:
                    return subprocess.CompletedProcess(command, 0)
                if command[:3] == ["git", "rev-parse", "HEAD"]:
                    return subprocess.CompletedProcess(command, 0, stdout=head + "\n")
                if "tools.asset_forge" in command and "reconcile" in command:
                    return subprocess.CompletedProcess(command, 0, stdout=json.dumps(result))
                raise AssertionError(command)

            def git_dir_for(path):
                return worldforge_git if Path(path).resolve() == checkout.resolve() else gloam_git

            with mock.patch.object(fc, "_git_clean", return_value=True), \
                    mock.patch.object(fc, "validate_worldforge_checkout", return_value={}), \
                    mock.patch.object(fc, "validate_worldforge_executor", return_value=Path(sys.executable)), \
                    mock.patch.object(fc, "build_request", return_value=request), \
                    mock.patch.object(fc, "independently_verify_result", return_value=result), \
                    mock.patch.object(fc, "_worktree_git_dir", side_effect=git_dir_for), \
                    mock.patch.object(fc.subprocess, "run", side_effect=fake_run):
                observed = fc.run_operator(ROOT, Path(td) / "pins.json", "2026-08-02T00:00:00Z",
                                           "refs/heads/test", checkout, Path(sys.executable))

            self.assertEqual(result, observed)
            reconcile = next(command for command, _ in calls if "tools.asset_forge" in command and "reconcile" in command)
            for flag in ("--host-root", "--target-root", "--config", "--request", "--json"):
                self.assertIn(flag, reconcile)
            config_arg = reconcile[reconcile.index("--config") + 1]
            request_arg = reconcile[reconcile.index("--request") + 1]
            self.assertTrue(config_arg.startswith(".git/worldforge-operator-runs/"))
            self.assertTrue(request_arg.startswith(".git/worldforge-operator-runs/"))
            target_run = gloam_git / "worldforge-operator-runs" / request["request_sha256"]
            host_run = worldforge_git / "worldforge-operator-runs" / request["request_sha256"]
            self.assertEqual(fc.canonical_json_bytes(request), (target_run / "request.json").read_bytes())
            config = fc.load_json(host_run / "config.json")
            self.assertEqual(config["config_sha256"], fc.canonical_hash(config, "config_sha256"))
            self.assertEqual(config["trust"]["request_sha256"], request["request_sha256"])
            self.assertEqual(config["trust"]["target_sha256"], fc.canonical_hash(request["target"]))
            self.assertEqual(
                request_arg,
                fc.PurePosixPath(".git", *target_run.joinpath("request.json").relative_to(gloam_git).parts).as_posix(),
            )
            self.assertEqual(
                config_arg,
                fc.PurePosixPath(".git", *host_run.joinpath("config.json").relative_to(worldforge_git).parts).as_posix(),
            )

    def test_operator_preserves_typed_worldforge_production_red(self):
        with tempfile.TemporaryDirectory() as td:
            checkout = Path(td) / "worldforge"
            checkout.mkdir()
            gloam_git = Path(td) / "gloam-git"
            worldforge_git = Path(td) / "worldforge-git"
            gloam_git.mkdir()
            worldforge_git.mkdir()
            request = {
                "request_sha256": "b" * 64,
                "target": {"base_commit": "e" * 40},
                "creation_stacks": {"surfaces": {}},
                "qualification_stack": {},
            }
            failure = {"failure": {"code": "FAIL-CAPABILITY-UNAVAILABLE", "stage": "capability",
                                    "retry_disposition": "never", "message": "licensed adapters unavailable"}}

            def fake_run(command, **kwargs):
                if command[:3] == ["git", "merge-base", "--is-ancestor"]:
                    return subprocess.CompletedProcess(command, 0)
                if "tools.asset_forge" in command and "reconcile" in command:
                    return subprocess.CompletedProcess(command, 2, stdout=json.dumps(failure))
                raise AssertionError(command)

            def git_dir_for(path):
                return worldforge_git if Path(path).resolve() == checkout.resolve() else gloam_git

            with mock.patch.object(fc, "_git_clean", return_value=True), \
                    mock.patch.object(fc, "validate_worldforge_checkout", return_value={}), \
                    mock.patch.object(fc, "validate_worldforge_executor", return_value=Path(sys.executable)), \
                    mock.patch.object(fc, "build_request", return_value=request), \
                    mock.patch.object(fc, "_worktree_git_dir", side_effect=git_dir_for), \
                    mock.patch.object(fc.subprocess, "run", side_effect=fake_run):
                with self.assertRaises(fc.ForgeError) as caught:
                    fc.run_operator(ROOT, Path(td) / "pins.json", "2026-08-02T00:00:00Z",
                                    "refs/heads/test", checkout, Path(sys.executable))
            self.assertEqual("FAIL-CAPABILITY-UNAVAILABLE", caught.exception.code)
            self.assertEqual("capability", caught.exception.stage)
            self.assertEqual("never", caught.exception.retry)

    def test_independent_verify_wires_explicit_roots_config_and_git_run_refs(self):
        with tempfile.TemporaryDirectory() as td:
            checkout = Path(td) / "worldforge"
            checkout.mkdir()
            gloam_git = Path(td) / "gloam-git"
            worldforge_git = Path(td) / "worldforge-git"
            gloam_git.mkdir()
            worldforge_git.mkdir()
            run_root = gloam_git / "worldforge-operator-runs" / ("a" * 64)
            run_root.mkdir(parents=True)
            config_path = worldforge_git / "worldforge-operator-runs" / ("a" * 64) / "config.json"
            config_path.parent.mkdir(parents=True)
            config_path.write_bytes(b"{}")
            head = "h" * 40
            request = {"request_sha256": "a" * 64}
            result = {
                "result_id": "result-id", "result_sha256": "d" * 64,
                "request_id": "request-id", "request_sha256": "a" * 64,
                "kit": {"kit_id": "kit", "version": "1.0.0", "pointer_value": "bundle"},
                "plan_sha256": "p" * 64, "bundle_sha256": "b" * 64,
                "qualification_sha256": "q" * 64, "promotion_id": "promotion-id",
                "promotion_receipt_sha256": "r" * 64, "promotion_target_sha256": "t" * 64,
                "evidence_sha256": "e" * 64, "verified_target_sha256": "v" * 64,
                "reverified_hashes": {"evidence_ref": "evidence.json"},
                "verified_target": {"commit": head},
            }
            snapshot = {"commit": head, "bundle_id": "bundle"}
            calls = []

            def fake_run(command, **kwargs):
                calls.append((list(command), kwargs))
                if command[:3] == ["git", "rev-parse", "HEAD"]:
                    return subprocess.CompletedProcess(command, 0, stdout=head + "\n")
                if "tools.asset_forge" in command and "verify" in command:
                    return subprocess.CompletedProcess(
                        command, 0,
                        stdout=json.dumps({"verified": True, "mismatches": [], "failure": None}),
                    )
                raise AssertionError(command)

            def git_dir_for(path):
                return worldforge_git if Path(path).resolve() == checkout.resolve() else gloam_git

            with mock.patch.object(fc, "validate_worldforge_checkout", return_value={}), \
                    mock.patch.object(fc, "validate_worldforge_executor", return_value=Path(sys.executable)), \
                    mock.patch.object(fc, "_strict_worldforge_contract"), \
                    mock.patch.object(fc, "validate_result_bindings"), \
                    mock.patch.object(fc, "inspect_active_catalog_with_unreal", return_value={"bundle_id": "bundle"}), \
                    mock.patch.object(fc, "observe_target_snapshot", return_value=snapshot), \
                    mock.patch.object(fc, "verify_generated_commit_lfs_closure"), \
                    mock.patch.object(fc, "_git_clean", return_value=True), \
                    mock.patch.object(fc, "_worktree_git_dir", side_effect=git_dir_for), \
                    mock.patch.object(fc.subprocess, "run", side_effect=fake_run):
                self.assertEqual(
                    result,
                    fc.independently_verify_result(
                        ROOT, checkout, Path(sys.executable), request, result,
                        run_root=run_root, config_path=config_path,
                    ),
                )

            verify = next(command for command, _ in calls if "tools.asset_forge" in command and "verify" in command)
            for flag in ("--host-root", "--target-root", "--config", "--result", "--target", "--json"):
                self.assertIn(flag, verify)
            self.assertEqual(str(checkout), verify[verify.index("--host-root") + 1])
            self.assertEqual(str(ROOT.resolve()), verify[verify.index("--target-root") + 1])
            self.assertTrue(verify[verify.index("--config") + 1].startswith(".git/worldforge-operator-runs/"))
            self.assertTrue(verify[verify.index("--result") + 1].startswith(".git/worldforge-operator-runs/"))
            self.assertTrue(verify[verify.index("--target") + 1].startswith(".git/worldforge-operator-runs/"))
            self.assertTrue((run_root / "result-ref.json").is_file())
            self.assertTrue((run_root / "target.json").is_file())

    def test_generated_commit_requires_canonical_lfs_pointer_and_local_object(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            subprocess.run(["git", "init"], cwd=root, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=root, check=True)
            subprocess.run(["git", "config", "user.name", "test"], cwd=root, check=True)
            (root / "seed.txt").write_text("seed", encoding="utf-8")
            subprocess.run(["git", "add", "seed.txt"], cwd=root, check=True)
            subprocess.run(["git", "commit", "-m", "seed"], cwd=root, check=True, capture_output=True)
            base_commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, check=True,
                capture_output=True, text=True).stdout.strip()
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
            metadata_path = (fc.VERSION_ROOT / "active-kit-pointer.json").as_posix()
            metadata = root / metadata_path; metadata.parent.mkdir(parents=True)
            metadata.write_bytes(fc.canonical_json_bytes({"schema_version": "gloamstead.sanctuary.active-pointer.v1",
                "catalog_object_path": "/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog",
                "active_bundle_id": "sanctuary-biome-kit-1.0.0", "expected_next_bundle_id": None}))
            json_artifact_path = "SourceArt/WorldForge/Accepted/Sanctuary/1.0.0/placement/rules.json"
            json_artifact_bytes = b'{"density":7,"schema_version":"gloamstead.placement.v1"}'
            json_artifact = root / json_artifact_path; json_artifact.parent.mkdir(parents=True)
            json_artifact.write_bytes(json_artifact_bytes)
            request_sha256 = "b" * 64
            document_fields = [("plan.json", "plan_sha256"), ("bundle.json", "bundle_sha256"),
                               ("qualification.json", "qualification_sha256"),
                               ("promotion-receipt.json", "promotion_receipt_sha256")]
            evidence_refs, document_hashes = [], {}
            for filename, hash_field in document_fields:
                repo_path = f"specs/worldforge_asset_forge/sanctuary-biome-kit-1.0.0/runs/{request_sha256}/{filename}"
                document = {"schema_version": "test.v1", hash_field: "0" * 64}
                document[hash_field] = fc.canonical_hash(document, hash_field)
                evidence_file = root / repo_path; evidence_file.parent.mkdir(parents=True, exist_ok=True)
                evidence_file.write_bytes(fc.canonical_json_bytes(document))
                evidence_refs.append(repo_path); document_hashes[hash_field] = document[hash_field]
            subprocess.run(["git", "add", path, json_artifact_path, pointer_path, metadata_path, *evidence_refs], cwd=root, check=True)
            subprocess.run(["git", "commit", "-m", "generated"], cwd=root, check=True, capture_output=True)
            commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, check=True, capture_output=True, text=True).stdout.strip()
            request = {"request_sha256": request_sha256, "target": {"base_commit": base_commit},
                       "output_allowlist": {"source_paths": [path, json_artifact_path], "unreal_paths": []},
                       "desired_active_pointer": {"path": "/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog",
                                                  "value": "sanctuary-biome-kit-1.0.0"}}
            result = {"generated_commit": commit, "verified_target": {"commit": commit},
                "pointer_after": request["desired_active_pointer"],
                "evidence_refs": evidence_refs, **document_hashes, "artifacts": {
                    path: {"sha256": digest, "size_bytes": len(payload), "lfs_oid": f"sha256:{digest}"},
                    json_artifact_path: {"sha256": hashlib.sha256(json_artifact_bytes).hexdigest(),
                                         "size_bytes": len(json_artifact_bytes), "lfs_oid": None}}}
            fc.verify_generated_commit_lfs_closure(root, request, result)
            result["artifacts"][path]["size_bytes"] += 1
            with self.assertRaises(fc.ForgeError): fc.verify_generated_commit_lfs_closure(root, request, result)
            result["artifacts"][path]["size_bytes"] -= 1
            json_artifact.write_bytes(b'{"dirty":"working-tree bytes must not be trusted"}')
            with mock.patch.object(fc, "_git_clean", return_value=True):
                fc.verify_generated_commit_lfs_closure(root, request, result)
                hostile = json.loads(json.dumps(result))
                hostile_bytes = json_artifact.read_bytes()
                hostile["artifacts"][json_artifact_path]["sha256"] = hashlib.sha256(hostile_bytes).hexdigest()
                hostile["artifacts"][json_artifact_path]["size_bytes"] = len(hostile_bytes)
                with self.assertRaises(fc.ForgeError):
                    fc.verify_generated_commit_lfs_closure(root, request, hostile)
            subprocess.run(["git", "restore", json_artifact_path], cwd=root, check=True)
            subprocess.run(["git", "rm", metadata_path], cwd=root, check=True, capture_output=True)
            hostile_tree = subprocess.run(["git", "write-tree"], cwd=root, check=True,
                capture_output=True, text=True).stdout.strip()
            hostile_commit = subprocess.run(["git", "commit-tree", hostile_tree, "-p", commit,
                "-m", "hostile missing pointer metadata"], cwd=root, check=True,
                capture_output=True, text=True).stdout.strip()
            subprocess.run(["git", "update-ref", "HEAD", hostile_commit], cwd=root, check=True)
            missing_pointer = json.loads(json.dumps(result))
            missing_pointer["generated_commit"] = hostile_commit
            missing_pointer["verified_target"]["commit"] = hostile_commit
            missing_pointer_request = json.loads(json.dumps(request))
            missing_pointer_request["target"]["base_commit"] = commit
            with self.assertRaises(fc.ForgeError) as caught:
                fc.verify_generated_commit_lfs_closure(root, missing_pointer_request, missing_pointer)
            self.assertEqual("FAIL-PARTIAL-PROMOTION", caught.exception.code)


if __name__ == "__main__":
    unittest.main()
