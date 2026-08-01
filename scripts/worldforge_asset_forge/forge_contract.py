"""Gloamstead-owned contract, vendor and operator boundary for WorldForge.

Only standard-library code is used so a clean workstation can verify provenance
before trusting a vendored plugin or a WorldForge execution checkout.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import tempfile
import uuid
import zipfile
import urllib.request
import urllib.parse

VERSION_ROOT = Path("specs/worldforge_asset_forge/sanctuary-biome-kit-1.0.0")
LOCK_PATH = Path("specs/worldforge_asset_forge/worldforge-plugin.lock.json")
SHA_RE = re.compile(r"^[0-9a-f]{64}$")
ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$")
REPO_PATH_RE = re.compile(r"^(?!\s*$)(?![A-Za-z]:)(?!.*(?:^|/)\.{1,2}(?:/|$))[^/\\\r\n]+(?:/[^/\\\r\n]+)*$")
UNREAL_RE = re.compile(r"^/Game/(?!.*//)(?!.*(?:^|/)\.{1,2}(?:/|$))[^./\\\r\n]+(?:/[^./\\\r\n]+)*(?:\.[^./\\\r\n]+)?$")
FORBIDDEN_PACKAGE_PARTS = {"Binaries", "Intermediate", "Saved", "reports", "WFRuntime"}
INTENT_SHA256 = "eda8daa675fbc0409646e804763e2cb5eee947d250a12ad9767f5d6f5af48992"
ACCEPTANCE_SHA256 = "218f7d733cad0b6b2f60b5de1af7763198245252d63d51e6efc76c4cdadb7096"
INVENTORY_SHA256 = "ebd02aed2d50bde4b29fbbc9d46d04257bb92f1e95b8ba303576ac74469ef88f"


class ForgeError(RuntimeError):
    def __init__(self, code: str, message: str, stage: str = "contract"):
        super().__init__(message)
        self.code, self.stage = code, stage

    def as_dict(self):
        return {"code": self.code, "stage": self.stage, "retry": "after_change", "message": str(self)}


def load_json(path: Path):
    with path.open("r", encoding="utf-8", newline="") as stream:
        return json.load(stream)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def canonical_json_bytes(value) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def canonical_hash(value, omitted: str | None = None) -> str:
    if omitted and isinstance(value, dict):
        value = {k: v for k, v in value.items() if k != omitted}
    return sha256_bytes(canonical_json_bytes(value))


def _exact_keys(value, expected, label):
    if not isinstance(value, dict) or set(value) != set(expected):
        return [f"FAIL-CONTRACT-DRIFT: {label} fields must be exactly {sorted(expected)}"]
    return []


def _safe_repo_path(value) -> bool:
    return isinstance(value, str) and REPO_PATH_RE.fullmatch(value) is not None


def _safe_unreal_path(value) -> bool:
    return isinstance(value, str) and UNREAL_RE.fullmatch(value) is not None


def validate_inventory(doc):
    errors = _exact_keys(doc, ["schema_version", "document_sha256", "kit_id", "version", "pointer_value", "source_root", "unreal_root", "promotion_policy", "deliverables", "output_allowlist"], "inventory")
    if errors:
        return errors
    if (doc["schema_version"], doc["kit_id"], doc["version"], doc["pointer_value"], doc["promotion_policy"]) != (
        "gloamstead.sanctuary.inventory.v1", "sanctuary-biome-kit", "1.0.0", "sanctuary-biome-kit-1.0.0", "automatic_on_all_green"):
        errors.append("FAIL-CONTRACT-DRIFT: immutable kit identity or promotion policy changed")
    if doc["document_sha256"] != INVENTORY_SHA256 or doc["document_sha256"] != canonical_hash(doc, "document_sha256"):
        errors.append("FAIL-EVIDENCE-INTEGRITY: inventory canonical hash drift")
    source_root = "SourceArt/WorldForge/Accepted/Sanctuary/1.0.0"
    unreal_root = "/Game/Gloamstead/Generated/Biomes/Sanctuary/1_0_0"
    if doc["source_root"] != source_root or doc["unreal_root"] != unreal_root:
        errors.append("FAIL-CONTRACT-DRIFT: immutable version roots changed")
    required_member = ["id", "family", "semantic_role", "art_purpose", "asset_class", "restoration_state", "source_path", "unreal_object_path", "dependency_ids", "acceptance_tags", "ownership", "asset_family_lane"]
    ids, sources, objects = set(), set(), set()
    by_id = {}
    for index, item in enumerate(doc.get("deliverables", [])):
        item_errors = _exact_keys(item, required_member, f"deliverable[{index}]")
        if item_errors:
            errors.extend(item_errors); continue
        if not ID_RE.fullmatch(str(item["id"])) or item["id"] in ids:
            errors.append(f"FAIL-BIOME-KIT-INCOMPLETE: duplicate/invalid id {item['id']}")
        ids.add(item["id"]); by_id[item["id"]] = item
        if not _safe_repo_path(item["source_path"]) or not item["source_path"].startswith(source_root + "/"):
            errors.append(f"FAIL-CONTRACT-DRIFT: unsafe source path for {item['id']}")
        if not _safe_unreal_path(item["unreal_object_path"]) or not item["unreal_object_path"].startswith(unreal_root + "/"):
            errors.append(f"FAIL-CONTRACT-DRIFT: unsafe Unreal path for {item['id']}")
        folded_source, folded_object = item["source_path"].casefold(), item["unreal_object_path"].casefold()
        if folded_source in sources or folded_object in objects:
            errors.append(f"FAIL-VERSION-COLLISION: duplicate output for {item['id']}")
        sources.add(folded_source); objects.add(folded_object)
        if not item["semantic_role"].strip() or not item["art_purpose"].strip() or not item["acceptance_tags"]:
            errors.append(f"FAIL-BIOME-KIT-INCOMPLETE: missing Gloamstead meaning for {item['id']}")
        if item["ownership"] != {"owner": "Gloamstead", "license": "Proprietary-Gloamstead"}:
            errors.append(f"FAIL-LICENSE-PROOF: ownership/license drift for {item['id']}")
        blob = canonical_json_bytes(item).decode("utf-8").lower()
        if any(token in blob for token in ("todo", "placeholder", "wildcard", "infer destination", "path repair")):
            errors.append(f"FAIL-FAKE-GREEN: provisional content in {item['id']}")
    for item in doc.get("deliverables", []):
        if not isinstance(item, dict) or "id" not in item: continue
        for dep in item.get("dependency_ids", []):
            if dep not in by_id:
                errors.append(f"FAIL-BIOME-KIT-INCOMPLETE: {item['id']} has missing dependency {dep}")
    visiting, visited = set(), set()
    def visit(node):
        if node in visiting:
            errors.append(f"FAIL-BIOME-KIT-INCOMPLETE: dependency cycle at {node}"); return
        if node in visited or node not in by_id: return
        visiting.add(node)
        for dep in by_id[node]["dependency_ids"]: visit(dep)
        visiting.remove(node); visited.add(node)
    for member in sorted(by_id): visit(member)
    counts = {}
    for item in by_id.values(): counts[item["family"]] = counts.get(item["family"], 0) + 1
    expected = {"hero-ritual": 16, "architecture": 12, "surfaces": 3, "foliage": 6, "decals": 8, "vfx": 4, "placement": 4, "materials": 4, "integration": 3}
    if counts != expected:
        errors.append(f"FAIL-BIOME-KIT-INCOMPLETE: exact family coverage differs: {counts}")
    allow = doc.get("output_allowlist", {})
    if set(allow) != {"source_paths", "unreal_paths"} or allow.get("source_paths") != [x["source_path"] for x in doc.get("deliverables", [])] or allow.get("unreal_paths") != [x["unreal_object_path"] for x in doc.get("deliverables", [])]:
        errors.append("FAIL-POINTER-CLOSURE: output allowlist is not exact inventory order")
    return errors


def validate_acceptance(doc):
    expected = ["schema_version", "document_sha256", "profile_id", "immutable", "automatic_mutation_allowed", "style", "visual", "textures", "meshes", "pcg", "performance", "runtime", "required_gates"]
    errors = _exact_keys(doc, expected, "acceptance profile")
    if errors: return errors
    if doc["schema_version"] != "gloamstead.sanctuary.acceptance-profile.v1" or doc["style"] != "Withered Gothic Stylization" or doc["immutable"] is not True or doc["automatic_mutation_allowed"] is not False:
        errors.append("FAIL-CONTRACT-DRIFT: automatic run attempted to weaken acceptance")
    if doc["document_sha256"] != ACCEPTANCE_SHA256 or doc["document_sha256"] != canonical_hash(doc, "document_sha256"):
        errors.append("FAIL-EVIDENCE-INTEGRITY: acceptance canonical hash drift")
    required = {"schema", "provenance", "license", "texture", "mesh", "dependency", "collision", "lod", "pcg", "visual-day", "visual-night", "performance", "cook", "package", "pie-state-matrix", "packaged-first-night"}
    if set(doc["required_gates"]) != required:
        errors.append("FAIL-CONTRACT-DRIFT: required acceptance gate set changed")
    return errors


def validate_intent(doc):
    expected = ["schema_version", "document_sha256", "kit_id", "version", "style", "authority", "subjects", "states", "art_direction", "intent_rules", "inventory_ref"]
    errors = _exact_keys(doc, expected, "art intent")
    if errors: return errors
    if doc["style"] != "Withered Gothic Stylization" or doc["authority"] != {"owner": "Gloamstead", "worldforge_may_invent_semantics": False, "worldforge_may_reject": True}:
        errors.append("FAIL-SCOPE-CREEP: semantic authority inversion")
    if doc["document_sha256"] != INTENT_SHA256 or doc["document_sha256"] != canonical_hash(doc, "document_sha256"):
        errors.append("FAIL-EVIDENCE-INTEGRITY: art-intent canonical hash drift")
    if doc["states"] != ["before", "restoration_in_progress", "restored", "corrupted"]:
        errors.append("FAIL-CONTRACT-DRIFT: restoration state vocabulary changed")
    return errors


def validate_lock(lock):
    expected = ["schema_version", "plugin_id", "plugin_version", "source_repository", "source_commit", "release_manifest_commit", "source_tree_sha256", "descriptor_sha256", "package_sha256", "release_manifest_sha256", "packaged_tree_sha256", "supported_engine", "build_identity", "capabilities", "schemas", "runtime_identity_contract"]
    errors = _exact_keys(lock, expected, "vendor lock")
    if errors: return errors
    if lock["schema_version"] != "gloamstead.worldforge.plugin-lock.v1" or lock["plugin_id"] != "WorldForge" or lock["plugin_version"] != "0.2.0" or lock["supported_engine"] != "5.8":
        errors.append("FAIL-STALE-PLUGIN: version or engine drift")
    if lock["capabilities"] != ["asset_set.reconcile@1", "scene_survey"]:
        errors.append("FAIL-CONTRACT-DRIFT: capability drift")
    for field in ["source_tree_sha256", "descriptor_sha256", "package_sha256", "release_manifest_sha256", "packaged_tree_sha256"]:
        if not SHA_RE.fullmatch(str(lock[field])): errors.append(f"FAIL-EVIDENCE-INTEGRITY: invalid {field}")
    runtime = lock["runtime_identity_contract"]
    if runtime.get("contract_id") != "gloamstead.worldforge.runtime-identity@1" or runtime.get("requires_complete_enabled_plugin_inventory") is not True or runtime.get("requires_derived_terminal_authorities") is not True:
        errors.append("FAIL-CONTRACT-DRIFT: runtime identity contract drift")
    return errors


def validate_repository_contract(root: Path):
    errors = []
    errors += validate_intent(load_json(root / VERSION_ROOT / "art-intent.json"))
    errors += validate_acceptance(load_json(root / VERSION_ROOT / "acceptance-profile.json"))
    errors += validate_inventory(load_json(root / VERSION_ROOT / "inventory.json"))
    errors += validate_lock(load_json(root / LOCK_PATH))
    pointer = load_json(root / VERSION_ROOT / "active-kit-pointer.json")
    errors += _exact_keys(pointer, ["schema_version", "catalog_object_path", "active_bundle_id", "expected_next_bundle_id"], "active pointer")
    if not errors and pointer != {"schema_version":"gloamstead.sanctuary.active-pointer.v1","catalog_object_path":"/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog","active_bundle_id":None,"expected_next_bundle_id":"sanctuary-biome-kit-1.0.0"}:
        errors.append("FAIL-POINTER-CLOSURE: committed active pointer contract drift")
    toolchain = load_json(root / VERSION_ROOT / "toolchain-requirements.json")
    errors += _exact_keys(toolchain, ["schema_version", "status", "qualified_pins_sha256", "ue", "comfyui", "substance", "houdini"], "toolchain requirements")
    if toolchain.get("status") not in {"unqualified", "qualified"} or toolchain.get("ue", {}).get("changelist") != 55116800 or (toolchain.get("status") == "unqualified" and toolchain.get("qualified_pins_sha256") is not None) or (toolchain.get("status") == "qualified" and not SHA_RE.fullmatch(str(toolchain.get("qualified_pins_sha256")))):
        errors.append("FAIL-UNVERIFIED-RUNTIME: toolchain declaration drift")
    for schema_path in sorted((root / "specs/worldforge_asset_forge/schemas").glob("*.schema.json")):
        schema = load_json(schema_path)
        if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema" or schema.get("additionalProperties") is not False:
            errors.append(f"FAIL-CONTRACT-DRIFT: schema is not closed Draft 2020-12: {schema_path.name}")
    required_lfs = {"*.uasset", "*.umap", "*.sbs", "*.sbsar", "*.hda", "*.hdalc", "*.hdanc", "*.png", "*.tif", "*.tiff", "*.exr", "*.fbx", "*.obj", "*.abc", "*.vdb", "*.usd", "*.usda", "*.usdc"}
    attrs = (root / ".gitattributes").read_text(encoding="utf-8").splitlines()
    covered = {line.split()[0] for line in attrs if "filter=lfs" in line}
    missing = required_lfs - covered
    if missing: errors.append(f"FAIL-CONTRACT-DRIFT: required LFS patterns missing: {sorted(missing)}")
    return errors


def _plugin_record(plugin):
    packages = sorted(plugin["script_packages"])
    lines = [f"plugin={plugin['plugin_name']}", f"plugin_version={plugin['plugin_version']}",
        f"plugin_descriptor_sha256={plugin['descriptor_sha256'].lower()}",
        f"plugin_installed_tree_sha256={plugin['installed_plugin_tree_sha256'].lower()}",
        f"plugin_build_identity={plugin['build_identity']}"]
    lines += [f"plugin_script_package={p}" for p in packages]
    lines += [f"plugin_end={plugin['plugin_name']}"]
    return "\n".join(lines) + "\n"


def canonical_runtime_identity(identity):
    plugins = sorted(identity["enabled_plugins"], key=lambda p: p["plugin_name"])
    inventory = "gloamstead.enabled-plugin-inventory@1\n" + "".join(_plugin_record(p) for p in plugins)
    inventory_sha = sha256_bytes(inventory.encode("utf-8"))
    engine_owner = sha256_bytes(("gloamstead.script-owner.engine@1\n"
        f"engine_version={identity['engine_version']}\ncompatible_engine_version={identity['compatible_engine_version']}\n"
        f"engine_build_version={identity['engine_build_version']}\nengine_changelist={identity['engine_changelist']}\n"
        f"compatible_engine_changelist={identity['compatible_engine_changelist']}\n").encode())
    project_owner = sha256_bytes(("gloamstead.script-owner.project@1\n"
        f"gloamstead_commit={identity['gloamstead_commit']}\nengine_build_version={identity['engine_build_version']}\n").encode())
    authorities = []
    for p in identity["engine_script_packages"]: authorities.append((p, "engine", "UnrealEngine", engine_owner))
    for p in identity["gloamstead_script_packages"]: authorities.append((p, "gloamstead_project", "Gloamstead", project_owner))
    found_wf = False
    for plugin in plugins:
        owner = "worldforge_plugin" if plugin["plugin_name"] == "WorldForge" else "external_plugin"
        found_wf |= owner == "worldforge_plugin"
        digest = sha256_bytes(_plugin_record(plugin).encode())
        for package in plugin["script_packages"]: authorities.append((package, owner, plugin["plugin_name"], digest))
    if not found_wf or len({a[0].casefold() for a in authorities}) != len(authorities):
        raise ForgeError("FAIL-UNVERIFIED-RUNTIME", "Incomplete or overlapping runtime plugin inventory", "runtime_identity")
    authorities.sort()
    fields = [
        "gloamstead.worldforge.runtime-identity@1",
        f"engine_version={identity['engine_version']}", f"compatible_engine_version={identity['compatible_engine_version']}",
        f"engine_build_version={identity['engine_build_version']}", f"engine_changelist={identity['engine_changelist']}",
        f"compatible_engine_changelist={identity['compatible_engine_changelist']}", f"gloamstead_commit={identity['gloamstead_commit']}",
        f"plugin_version={identity['plugin_version']}", f"plugin_engine_version={identity['plugin_engine_version']}",
        f"plugin_descriptor_sha256={identity['plugin_descriptor_sha256'].lower()}",
        f"installed_plugin_tree_sha256={identity['installed_plugin_tree_sha256'].lower()}",
        f"vendor_lock_sha256={identity['vendor_lock_sha256'].lower()}",
        f"declared_plugin_package_sha256={identity['declared_plugin_package_sha256'].lower()}",
        f"declared_plugin_build_identity={identity['declared_plugin_build_identity'].lower()}",
        f"enabled_plugin_inventory_sha256={inventory_sha}"]
    fields += [f"terminal_script_authority={p}|{o}|{i}|{h}" for p, o, i, h in authorities]
    return "\n".join(fields) + "\n", authorities


def _tree_digest(entries):
    text = "".join(f"{path}\0{digest}\0{size}\n" for path, digest, size in sorted(entries))
    return sha256_bytes(text.encode("utf-8"))


def _verify_release(package: Path, manifest_path: Path, lock):
    if sha256_file(package) != lock["package_sha256"] or sha256_file(manifest_path) != lock["release_manifest_sha256"]:
        raise ForgeError("FAIL-EVIDENCE-INTEGRITY", "Package or manifest digest does not match lock", "vendor")
    manifest = load_json(manifest_path)
    bindings = {"plugin_id": "plugin_id", "plugin_version": "plugin_version", "source_commit": "source_commit", "source_tree_sha256": "source_tree_sha256", "descriptor_sha256": "descriptor_sha256", "package_sha256": "package_sha256", "supported_engine": "supported_engine", "build_identity": "build_identity", "capabilities": "capabilities"}
    for lock_key, manifest_key in bindings.items():
        if lock[lock_key] != manifest[manifest_key]:
            raise ForgeError("FAIL-CONTRACT-DRIFT", f"Release manifest drift: {manifest_key}", "vendor")
    if [{"path": s["path"], "sha256": s["sha256"]} for s in manifest["asset_forge_schemas"]] != lock["schemas"]:
        raise ForgeError("FAIL-CONTRACT-DRIFT", "Schema hash set drift", "vendor")
    expected = {f["path"]: (f["packaged_sha256"], f["packaged_size"]) for f in manifest["files"]}
    with zipfile.ZipFile(package) as archive:
        names, folded = [], set()
        for info in archive.infolist():
            name = info.filename.replace("\\", "/")
            parts = PurePosixPath(name).parts
            mode = (info.external_attr >> 16) & 0o170000
            if info.is_dir() or (mode not in {0, stat.S_IFREG}) or name.startswith("/") or ".." in parts or "." in parts or any(part in FORBIDDEN_PACKAGE_PARTS for part in parts) or name.casefold() in folded:
                raise ForgeError("FAIL-EVIDENCE-INTEGRITY", f"Unsafe archive entry: {name}", "vendor")
            folded.add(name.casefold()); names.append(name)
        if set(names) != set(expected) or len(names) != len(expected):
            raise ForgeError("FAIL-EVIDENCE-INTEGRITY", "Archive file set differs from signed manifest", "vendor")
        entries = []
        for name in names:
            data = archive.read(name)
            digest, size = expected[name]
            if sha256_bytes(data) != digest or len(data) != size:
                raise ForgeError("FAIL-EVIDENCE-INTEGRITY", f"Archive payload drift: {name}", "vendor")
            entries.append((name, digest, size))
    if _tree_digest(entries) != lock["packaged_tree_sha256"]:
        raise ForgeError("FAIL-EVIDENCE-INTEGRITY", "Installed-tree digest contract drift", "vendor")
    return manifest


def _git_clean(root):
    result = subprocess.run(["git", "status", "--porcelain", "--untracked-files=all"], cwd=root, capture_output=True, text=True)
    return result.returncode == 0 and not result.stdout.strip()


def sync_plugin(root: Path, package: Path, manifest_path: Path):
    root, package, manifest_path = root.resolve(), package.resolve(), manifest_path.resolve()
    if not _git_clean(root):
        raise ForgeError("FAIL-TARGET-DIRTY", "Gloamstead worktree must be clean before plugin sync", "vendor")
    lock = load_json(root / LOCK_PATH)
    lock_errors = validate_lock(lock)
    if lock_errors: raise ForgeError("FAIL-CONTRACT-DRIFT", "; ".join(lock_errors), "vendor")
    _verify_release(package, manifest_path, lock)
    target = (root / "Plugins/WorldForge").resolve()
    if target.parent != (root / "Plugins").resolve():
        raise ForgeError("FAIL-SCOPE-CREEP", "Plugin target escaped exact root", "vendor")
    stage_root = Path(tempfile.mkdtemp(prefix="gloam-wf-sync-", dir=target.parent))
    staged = stage_root / "WorldForge"
    backup = target.parent / f".WorldForge.backup-{uuid.uuid4().hex}"
    try:
        staged.mkdir()
        with zipfile.ZipFile(package) as archive: archive.extractall(staged)
        entries = [(p.relative_to(staged).as_posix(), sha256_file(p), p.stat().st_size) for p in staged.rglob("*") if p.is_file()]
        if _tree_digest(entries) != lock["packaged_tree_sha256"]:
            raise ForgeError("FAIL-EVIDENCE-INTEGRITY", "Staged plugin differs from lock", "vendor")
        if target.exists(): os.replace(target, backup)
        try: os.replace(staged, target)
        except BaseException:
            if backup.exists(): os.replace(backup, target)
            raise
        errors = verify_installed_plugin(root)
        if errors:
            shutil.rmtree(target)
            if backup.exists(): os.replace(backup, target)
            raise ForgeError("FAIL-STALE-PLUGIN", "; ".join(errors), "vendor")
        if backup.exists(): shutil.rmtree(backup)
    finally:
        if stage_root.exists(): shutil.rmtree(stage_root)


def verify_installed_plugin(root: Path):
    lock = load_json(root / LOCK_PATH)
    target = root / "Plugins/WorldForge"
    if not target.is_dir(): return ["FAIL-STALE-PLUGIN: plugin is not installed"]
    entries = []
    for path in target.rglob("*"):
        if path.is_symlink() or (path.is_file() and any(part in FORBIDDEN_PACKAGE_PARTS for part in path.relative_to(target).parts)):
            return ["FAIL-STALE-PLUGIN: forbidden or symbolic installed entry"]
        if path.is_file(): entries.append((path.relative_to(target).as_posix(), sha256_file(path), path.stat().st_size))
    if _tree_digest(entries) != lock["packaged_tree_sha256"]:
        return ["FAIL-STALE-PLUGIN: installed plugin tree hash differs from lock"]
    descriptor = target / "WorldForge.uplugin"
    if sha256_file(descriptor) != lock["descriptor_sha256"]:
        return ["FAIL-STALE-PLUGIN: descriptor hash drift"]
    parsed = load_json(descriptor)
    if parsed.get("VersionName") != "0.2.0" or not str(parsed.get("EngineVersion", "")).startswith("5.8"):
        return ["FAIL-STALE-PLUGIN: plugin version/engine drift"]
    return []


def _observed_file(path_value, expected_sha, label, failures, allow_directory=False):
    if not isinstance(path_value, str):
        failures.append(f"FAIL-UNVERIFIED-RUNTIME: {label} path is absent")
        return None
    path = Path(path_value)
    if not path.is_absolute() or not (path.is_file() or (allow_directory and path.is_dir())):
        failures.append(f"FAIL-UNVERIFIED-RUNTIME: {label} is not an explicit existing absolute {'path' if allow_directory else 'file'}")
        return None
    observed_sha = sha256_file(path) if path.is_file() else _tree_digest((p.relative_to(path).as_posix(), sha256_file(p), p.stat().st_size) for p in path.rglob("*") if p.is_file())
    if not SHA_RE.fullmatch(str(expected_sha)) or observed_sha != expected_sha:
        failures.append(f"FAIL-AI-PIN-DRIFT: {label} hash drift")
        return None
    return path


def _version_probe(executable, expected, label, failures):
    if executable is None: return
    try:
        observed = subprocess.run([str(executable), "--version"], capture_output=True, text=True, timeout=15)
    except (OSError, subprocess.TimeoutExpired):
        failures.append(f"FAIL-UNVERIFIED-RUNTIME: {label} version probe failed"); return
    if observed.returncode or expected not in (observed.stdout + observed.stderr):
        failures.append(f"FAIL-UNVERIFIED-RUNTIME: {label} version drift")


def probe_workstation(root: Path, pins_path: Path | None = None):
    req = load_json(root / VERSION_ROOT / "toolchain-requirements.json")
    failures = []
    build = Path(req["ue"]["root"]) / "Engine/Build/Build.version"
    if not build.is_file(): failures.append("FAIL-UNVERIFIED-RUNTIME: UE 5.8 Build.version missing")
    else:
        data = load_json(build)
        if data.get("MajorVersion") != 5 or data.get("MinorVersion") != 8 or data.get("Changelist") != req["ue"]["changelist"]:
            failures.append("FAIL-UNVERIFIED-RUNTIME: UE build identity drift")
    if pins_path is None:
        failures += ["FAIL-AI-PIN-DRIFT: ComfyUI/model/workflow/custom-node/license stack is unqualified",
                     "FAIL-UNVERIFIED-RUNTIME: licensed Substance Automation Toolkit is unavailable",
                     "FAIL-UNVERIFIED-RUNTIME: Houdini 21.0.729 does not match Houdini Engine 21.0.753"]
        return failures
    try: pins = load_json(pins_path)
    except (OSError, json.JSONDecodeError):
        failures.append("FAIL-UNVERIFIED-RUNTIME: qualified pins document is unreadable")
        return failures
    if req.get("status") != "qualified" or req.get("qualified_pins_sha256") != canonical_hash(pins):
        failures.append("FAIL-CONTRACT-DRIFT: qualified pins are not bound by the committed toolchain approval")
        return failures
    evidence = pins.get("probe_evidence")
    if set(pins) != {"qualified", "comfy", "substance", "houdini", "vendor_pins", "validator_pins", "probe_evidence"} or pins.get("qualified") is not True or not isinstance(evidence, dict) or set(evidence) != {"comfyui", "substance", "houdini", "license_receipts"}:
        failures.append("FAIL-UNVERIFIED-RUNTIME: qualified pins require exact probe_evidence")
        return failures
    for key in ("comfy", "substance", "houdini", "vendor_pins", "validator_pins"):
        if not isinstance(pins.get(key), dict) or not pins[key]: failures.append(f"FAIL-UNVERIFIED-RUNTIME: missing qualified {key} pins")
    if failures: return failures
    comfy, comfy_ev = pins["comfy"], evidence["comfyui"]
    expected_comfy_ev = {"checkout", "creation_workflow", "evaluation_workflow", "checkpoint", "vae", "lora", "control_model", "custom_node_lock", "server_url", "hardware_report", "hardware_report_sha256"}
    if not isinstance(comfy_ev, dict) or set(comfy_ev) != expected_comfy_ev:
        failures.append("FAIL-UNVERIFIED-RUNTIME: incomplete ComfyUI probe evidence")
    else:
        checkout = Path(comfy_ev["checkout"])
        if not checkout.is_absolute() or not (checkout / ".git").exists() or not _git_clean(checkout): failures.append("FAIL-UNVERIFIED-RUNTIME: ComfyUI checkout is not explicit and clean")
        else:
            commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=checkout, capture_output=True, text=True)
            if commit.returncode or commit.stdout.strip() != comfy.get("repository_commit"): failures.append("FAIL-AI-PIN-DRIFT: ComfyUI commit drift")
        for evidence_key, pin_key in [("creation_workflow","creation_workflow_sha256"),("evaluation_workflow","evaluation_workflow_sha256"),("checkpoint","checkpoint_sha256"),("vae","vae_sha256"),("lora","lora_sha256"),("control_model","control_model_sha256"),("custom_node_lock","custom_node_lock_sha256"),("hardware_report","hardware_report_sha256")]:
            _observed_file(comfy_ev[evidence_key], comfy.get(pin_key) if pin_key in comfy else comfy_ev.get(pin_key), f"ComfyUI {evidence_key}", failures)
        parsed_server = urllib.parse.urlparse(comfy_ev["server_url"])
        if parsed_server.scheme != "http" or parsed_server.hostname not in {"127.0.0.1", "localhost", "::1"}:
            failures.append("FAIL-SCOPE-CREEP: ComfyUI API must be explicit local loopback HTTP")
        try:
            hardware = load_json(Path(comfy_ev["hardware_report"]))
            if hardware.get("hardware_class") != comfy.get("hardware_class"): failures.append("FAIL-AI-PIN-DRIFT: ComfyUI hardware class drift")
        except (OSError, json.JSONDecodeError): failures.append("FAIL-UNVERIFIED-RUNTIME: ComfyUI hardware report is unreadable")
        if parsed_server.scheme == "http" and parsed_server.hostname in {"127.0.0.1", "localhost", "::1"}:
            try:
                with urllib.request.urlopen(comfy_ev["server_url"].rstrip("/") + "/system_stats", timeout=3) as response:
                    if response.status != 200 or not response.read(): failures.append("FAIL-UNVERIFIED-RUNTIME: ComfyUI API system probe failed")
            except Exception:
                failures.append("FAIL-UNVERIFIED-RUNTIME: ComfyUI API is unavailable")
    substance, sub_ev = pins["substance"], evidence["substance"]
    if not isinstance(sub_ev, dict) or set(sub_ev) != {"sbscooker", "sbsrender", "graph"}:
        failures.append("FAIL-UNVERIFIED-RUNTIME: incomplete Substance probe evidence")
    else:
        cooker = _observed_file(sub_ev["sbscooker"], substance.get("sbscooker_sha256"), "sbscooker", failures)
        renderer = _observed_file(sub_ev["sbsrender"], substance.get("sbsrender_sha256"), "sbsrender", failures)
        _observed_file(sub_ev["graph"], substance.get("graph_sha256"), "governed Substance graph", failures)
        _version_probe(cooker, substance.get("sbscooker_version", ""), "sbscooker", failures)
        _version_probe(renderer, substance.get("sbsrender_version", ""), "sbsrender", failures)
    houdini, h_ev = pins["houdini"], evidence["houdini"]
    if not isinstance(h_ev, dict) or set(h_ev) != {"executable", "engine_plugin", "hda"}:
        failures.append("FAIL-UNVERIFIED-RUNTIME: incomplete Houdini probe evidence")
    else:
        houdini_exe = _observed_file(h_ev["executable"], houdini.get("houdini_executable_sha256"), "Houdini executable", failures)
        _observed_file(h_ev["engine_plugin"], houdini.get("houdini_engine_plugin_sha256"), "Houdini Engine plugin", failures, allow_directory=True)
        _observed_file(h_ev["hda"], houdini.get("hda_sha256"), "governed HDA", failures)
        _version_probe(houdini_exe, houdini.get("houdini_version", ""), "Houdini", failures)
    if houdini.get("houdini_version") != houdini.get("houdini_engine_version"):
        failures.append("FAIL-UNVERIFIED-RUNTIME: Houdini and Houdini Engine version drift")
    receipts = evidence["license_receipts"]
    if not isinstance(receipts, list) or len(receipts) != 3 or {r.get("subject") for r in receipts if isinstance(r, dict)} != {"comfy-model-stack", "substance-automation-toolkit", "houdini-engine"}:
        failures.append("FAIL-LICENSE-PROOF: exact production license receipts are missing")
    else:
        for receipt in receipts:
            if set(receipt) != {"subject", "path", "sha256", "production_use_allowed"}: failures.append("FAIL-LICENSE-PROOF: license receipt shape drift"); continue
            if receipt.get("production_use_allowed") is not True: failures.append(f"FAIL-LICENSE-PROOF: {receipt.get('subject')} is not production-cleared"); continue
            _observed_file(receipt.get("path"), receipt.get("sha256"), f"{receipt.get('subject')} license receipt", failures)
    return failures


def build_request(root: Path, pins_path: Path, deadline: str, generation_ref: str, expected_pointer: str | None):
    errors = validate_repository_contract(root) + verify_installed_plugin(root)
    if errors: raise ForgeError(errors[0].split(":", 1)[0], "; ".join(errors), "request")
    pins = load_json(pins_path)
    if pins.get("qualified") is not True:
        raise ForgeError("FAIL-UNVERIFIED-RUNTIME", "Toolchain pins are not independently qualified", "probe")
    inventory = load_json(root / VERSION_ROOT / "inventory.json")
    intent_doc = load_json(root / VERSION_ROOT / "art-intent.json")
    acceptance_doc = load_json(root / VERSION_ROOT / "acceptance-profile.json")
    intent_path, accept_path = VERSION_ROOT / "art-intent.json", VERSION_ROOT / "acceptance-profile.json"
    lock = load_json(root / LOCK_PATH)
    base = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, check=True, capture_output=True, text=True).stdout.strip()
    families = sorted({d["family"] for d in inventory["deliverables"]})
    stacks = {}
    for family in families:
        family_items = [d for d in inventory["deliverables"] if d["family"] == family]
        pipeline = "surface" if family == "surfaces" else "mesh_candidate" if family in {"hero-ritual", "architecture", "foliage"} else "decal" if family == "decals" else "vfx" if family == "vfx" else "deterministic"
        stages = {"create": {"operation": "ai_create", "depends_on": []}, "normalize": {"operation": "substance_normalize" if pipeline == "surface" else "houdini_normalize" if pipeline == "mesh_candidate" else "deterministic_normalize", "depends_on": ["create"]}, "assemble": {"operation": "unreal_assemble", "depends_on": ["normalize"]}}
        stacks[family] = {"pipeline_class": pipeline, "input_sha256": canonical_hash(family_items), "comfy": pins["comfy"], "substance": pins["substance"] if pipeline == "surface" else None, "houdini": pins["houdini"] if pipeline == "mesh_candidate" else None, "vendor_pins": pins["vendor_pins"], "stages": stages}
    kind_tokens = {"StaticMesh":"static_mesh", "Material":"material", "MaterialInstance":"material_instance", "NiagaraSystem":"niagara_system", "PlacementRulesDataAsset":"placement_rules_data_asset", "PrimaryDataAsset":"primary_data_asset", "DataAsset":"data_asset"}
    deliverables = {d["id"]: {"family": d["family"], "kind": kind_tokens[d["asset_class"]], "semantic_role": d["semantic_role"], "art_purpose": d["art_purpose"], "source_path": d["source_path"], "unreal_object_path": d["unreal_object_path"], "restoration_state": d["restoration_state"], "dependency_ids": d["dependency_ids"]} for d in inventory["deliverables"]}
    request = {"request_id": f"sanctuary-biome-kit-1.0.0-{base[:12]}", "contract_id": "gloamstead-sanctuary-v1", "request_sha256": "0"*64, "deadline": deadline, "idempotency_key": "0"*64,
        "target": {"repository": "Gloamstead", "base_commit": base, "ue_build": "5.8.0-55116800", "project": "Gloamstead5_8.uproject", "generation_ref": generation_ref, "clean_worktree_required": True,
            "plugin_pins": {"WorldForge": {"version": lock["plugin_version"], "upstream_commit": lock["source_commit"], "source_sha256": lock["source_tree_sha256"], "descriptor_sha256": lock["descriptor_sha256"], "package_sha256": lock["package_sha256"], "build_id": lock["build_identity"]}}},
        "kit": {"kit_id": inventory["kit_id"], "version": inventory["version"], "pointer_value": inventory["pointer_value"]},
        "intent_ref": intent_path.as_posix(), "intent_sha256": intent_doc["document_sha256"], "acceptance_ref": accept_path.as_posix(), "acceptance_sha256": acceptance_doc["document_sha256"], "deliverables": deliverables,
        "version_roots": {"source_root": inventory["source_root"], "unreal_root": inventory["unreal_root"]},
        "expected_prior_active_pointer": {"path": "/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog", "value": expected_pointer},
        "desired_active_pointer": {"path": "/Game/Gloamstead/Generated/DA_GeneratedAssetCatalog.DA_GeneratedAssetCatalog", "value": inventory["pointer_value"]},
        "output_allowlist": inventory["output_allowlist"], "creation_stacks": stacks,
        "qualification_stack": {"acceptance_profile_ref": accept_path.as_posix(), "acceptance_profile_sha256": acceptance_doc["document_sha256"], "validator_pins": pins["validator_pins"]},
        "ownership_license_policy": {"generated_owner": "Gloamstead", "third_party_allowed": False, "license_allowlist": []}, "promotion_policy": {"mode": "automatic_on_all_green"},
        "schema_version": "wf.asset_forge.biome_kit_request.v1", "contract_version": "wf.asset_forge.asset_set.v1"}
    target, qualification = request["target"], request["qualification_stack"]
    idempotency_identity = {"intent_sha256":request["intent_sha256"],"acceptance_sha256":request["acceptance_sha256"],"kit":request["kit"],"deliverables":request["deliverables"],"version_roots":request["version_roots"],"output_allowlist":request["output_allowlist"],"expected_prior_active_pointer":request["expected_prior_active_pointer"],"desired_active_pointer":request["desired_active_pointer"],"target_base_commit":target["base_commit"],"target_repository":target["repository"],"target_project":target["project"],"target_generation_ref":target["generation_ref"],"ue_build":target["ue_build"],"plugin_pins":target["plugin_pins"],"creation_stacks":request["creation_stacks"],"acceptance_profile_ref":qualification["acceptance_profile_ref"],"acceptance_profile_sha256":qualification["acceptance_profile_sha256"],"qualification_validator_pins":qualification["validator_pins"],"ownership_license_policy":request["ownership_license_policy"],"promotion_policy":request["promotion_policy"]}
    request["idempotency_key"] = canonical_hash(idempotency_identity)
    request["request_sha256"] = canonical_hash(request, "request_sha256")
    return request


def run_operator(root: Path, pins_path: Path, deadline: str, generation_ref: str,
                 worldforge_checkout: Path, worldforge_python: Path):
    if not _git_clean(root):
        raise ForgeError("FAIL-TARGET-DIRTY", "Gloamstead worktree must be clean", "preflight")
    ancestor = subprocess.run(["git", "merge-base", "--is-ancestor", "origin/main", "HEAD"], cwd=root)
    if ancestor.returncode:
        raise ForgeError("FAIL-CONTRACT-DRIFT", "Generation branch is not derived from origin/main", "preflight")
    checkout = worldforge_checkout.resolve()
    if not (checkout / "tools/asset_forge/__main__.py").is_file() or not _git_clean(checkout):
        raise ForgeError("FAIL-UNVERIFIED-RUNTIME", "Explicit WorldForge checkout is missing or dirty", "preflight")
    if not worldforge_python.is_file():
        raise ForgeError("FAIL-UNVERIFIED-RUNTIME", "Explicit WorldForge Python executable is missing", "preflight")
    failures = probe_workstation(root, pins_path)
    if failures:
        raise ForgeError(failures[0].split(":",1)[0], "; ".join(failures), "probe")
    pointer = load_json(root / VERSION_ROOT / "active-kit-pointer.json")
    request = build_request(root, pins_path, deadline, generation_ref, pointer["active_bundle_id"])
    transient = checkout / f".gloamstead-request-{uuid.uuid4().hex}.json"
    try:
        transient.write_bytes(json.dumps(request, ensure_ascii=False, sort_keys=True, indent=2).encode("utf-8") + b"\n")
        completed = subprocess.run([str(worldforge_python), "-m", "tools.asset_forge", "reconcile", "--request", transient.name, "--json"], cwd=checkout, capture_output=True, text=True)
        try: result = json.loads(completed.stdout)
        except json.JSONDecodeError as exc:
            raise ForgeError("FAIL-EVIDENCE-INTEGRITY", "WorldForge returned non-JSON output", "reconcile") from exc
    finally:
        transient.unlink(missing_ok=True)
    run_root = root / VERSION_ROOT / "runs" / request["request_sha256"]
    run_root.mkdir(parents=True, exist_ok=True)
    request_bytes = json.dumps(request, ensure_ascii=False, sort_keys=True, indent=2).encode("utf-8") + b"\n"
    request_evidence = run_root / "request.json"
    if request_evidence.exists() and request_evidence.read_bytes() != request_bytes:
        raise ForgeError("FAIL-EVIDENCE-INTEGRITY", "Request hash directory contains different bytes", "evidence")
    request_evidence.write_bytes(request_bytes)
    result_digest = canonical_hash(result, "result_sha256") if isinstance(result, dict) else sha256_bytes(canonical_json_bytes(result))
    result_evidence = run_root / f"result-{result_digest}.json"
    result_bytes = json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2).encode("utf-8") + b"\n"
    if result_evidence.exists() and result_evidence.read_bytes() != result_bytes:
        raise ForgeError("FAIL-EVIDENCE-INTEGRITY", "Result hash filename contains different bytes", "evidence")
    result_evidence.write_bytes(result_bytes)
    if completed.returncode or result.get("state") not in {"NoChange", "Promoted"}:
        raise ForgeError("FAIL-UNVERIFIED-RUNTIME", "WorldForge reconcile did not return NoChange or Promoted", "reconcile")
    return result


def _emit(value):
    print(json.dumps(value, sort_keys=True, separators=(",", ":")))


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("test-contract")
    sub.add_parser("verify-installed")
    sync = sub.add_parser("sync"); sync.add_argument("--package", required=True, type=Path); sync.add_argument("--manifest", required=True, type=Path)
    sub.add_parser("probe")
    build = sub.add_parser("build-request"); build.add_argument("--pins", required=True, type=Path); build.add_argument("--deadline", required=True); build.add_argument("--generation-ref", required=True); build.add_argument("--expected-pointer")
    run = sub.add_parser("run"); run.add_argument("--pins", required=True, type=Path); run.add_argument("--deadline", required=True); run.add_argument("--generation-ref", required=True); run.add_argument("--worldforge-checkout", required=True, type=Path); run.add_argument("--worldforge-python", required=True, type=Path)
    args = parser.parse_args(argv); root = args.root.resolve()
    try:
        if args.command == "test-contract":
            errors = validate_repository_contract(root)
            if errors: raise ForgeError(errors[0].split(":",1)[0], "; ".join(errors))
            _emit({"status":"Passed","deliverable_count":len(load_json(root/VERSION_ROOT/"inventory.json")["deliverables"])})
        elif args.command == "verify-installed":
            errors = verify_installed_plugin(root)
            if errors: raise ForgeError("FAIL-STALE-PLUGIN", "; ".join(errors), "vendor")
            _emit({"status":"Verified","plugin":"WorldForge","version":"0.2.0"})
        elif args.command == "sync": sync_plugin(root, args.package, args.manifest); _emit({"status":"Installed","plugin":"WorldForge","version":"0.2.0"})
        elif args.command == "probe":
            failures = probe_workstation(root)
            if failures: _emit({"status":"Rejected","failures":failures}); return 2
            _emit({"status":"Qualified"})
        elif args.command == "build-request": _emit(build_request(root,args.pins,args.deadline,args.generation_ref,args.expected_pointer))
        elif args.command == "run": _emit(run_operator(root,args.pins,args.deadline,args.generation_ref,args.worldforge_checkout,args.worldforge_python))
        return 0
    except ForgeError as exc:
        _emit({"status":"Rejected","failure":exc.as_dict()}); return 2
    except Exception as exc:
        failure = ForgeError("FAIL-UNVERIFIED-RUNTIME", f"Unexpected {type(exc).__name__}: {exc}", "operator")
        _emit({"status":"Rejected","failure":failure.as_dict()}); return 2


if __name__ == "__main__": raise SystemExit(main())
