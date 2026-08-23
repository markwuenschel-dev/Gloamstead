#!/usr/bin/env python3
"""Project Gloamstead Cycle II intent into WorldForge's generic compiler input.

This is a caller-owned bridge.  It deliberately does *not* ask WorldForge to
interpret warnings, ritual meaning, evidence, or gameplay outcomes.  The only
input to WorldForge is a canonical generic plan; the semantic material remains
in ``intent-provenance.json`` next to the resulting planned descriptors.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Iterable


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INTENT = REPO_ROOT / "specs/world/cycle-2-corruption-neglect.world.json"
DEFAULT_SCHEMA = REPO_ROOT / "specs/world/gloamstead_world_spec.schema.json"
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "artifacts/worldforge/cycle2"
COMPILER_OUTPUTS = (
    "manifest.json",
    "terrain-slice.json",
    "poi-descriptors.json",
    "placement-variants.json",
    "material-variants.json",
    "survey-requests.json",
)
SIDE_CAR = "intent-provenance.json"
RECEIPT = "bridge-receipt.json"
MATERIALIZATION_REQUEST = "materialization-request.json"
SEMANTIC_SCHEMA_SHA256 = "09fe85ffb949f3f470ae8f9fb897041750d40e1a8eb1d1501590a619544fa2d2"
STATE_WRITE_LEASE_REVISION = "97b1af6f5fa3fb1498095cd0925d29845d079df3"
STATE_WRITE_LEASE_HASHES = {
    "Plugins/WorldForge/Source/WorldForgeCore/Public/WorldStateSubsystem.h": "BC63B5A0AC6714E40B998A0693AB7DD505AEDAE5A49C8BAEFCCEAB41BB518DEF",
    "Plugins/WorldForge/Source/WorldForgeCore/Private/WorldStateSubsystem.cpp": "44FF7A9B599E929BD3862F95C78DBC20A1B6811354E9F65DBBB88A69634975BD",
    "Plugins/WorldForge/Source/WorldForgeCore/Private/Tests/WorldStateWriteReservationTests.cpp": "CC70BA41AE45FA10A8680BCCDA57881F932FFC643CF39F4B57B9194F385E18E7",
}


class BridgeError(ValueError):
    """A caller-visible boundary, contract, or invocation failure."""


def _pointer(parts: tuple[Any, ...]) -> str:
    if not parts:
        return "$"
    return "$" + "".join("[{}]".format(part) if isinstance(part, int)
                       else ".{}".format(part) for part in parts)


def _reject_constant(value: str) -> None:
    raise BridgeError("$: non-finite JSON constant is not allowed: {}".format(value))


def _no_duplicates(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise BridgeError("$: duplicate JSON object key {!r}".format(key))
        result[key] = value
    return result


def _load_object(path: Path, label: str) -> tuple[dict[str, Any], bytes]:
    if not path.is_file():
        raise BridgeError("$: {} does not exist or is not a file: {}".format(label, path))
    try:
        raw = path.read_bytes()
        value = json.loads(raw.decode("utf-8"), object_pairs_hook=_no_duplicates,
                           parse_constant=_reject_constant)
    except UnicodeDecodeError as error:
        raise BridgeError("$: {} is not UTF-8 JSON: {}".format(label, error)) from error
    except json.JSONDecodeError as error:
        raise BridgeError("$: invalid {} JSON at line {}, column {}: {}".format(
            label, error.lineno, error.colno, error.msg)) from error
    if not isinstance(value, dict):
        raise BridgeError("$: {} must be a JSON object".format(label))
    return value, raw


def _require_object(value: Any, path: tuple[Any, ...]) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise BridgeError("{}: expected object".format(_pointer(path)))
    return value


def _require_array(value: Any, path: tuple[Any, ...], size: int | None = None) -> list[Any]:
    if not isinstance(value, list):
        raise BridgeError("{}: expected array".format(_pointer(path)))
    if size is not None and len(value) != size:
        raise BridgeError("{}: expected exactly {} items".format(_pointer(path), size))
    return value


def _require_keys(value: dict[str, Any], path: tuple[Any, ...], expected: set[str]) -> None:
    unknown, missing = sorted(set(value) - expected), sorted(expected - set(value))
    if unknown:
        raise BridgeError("{}: unknown field(s): {}".format(_pointer(path), ", ".join(unknown)))
    if missing:
        raise BridgeError("{}: missing field(s): {}".format(_pointer(path), ", ".join(missing)))


def _require_equal(value: Any, expected: Any, path: tuple[Any, ...]) -> None:
    if value != expected:
        raise BridgeError("{}: expected {!r}, got {!r}".format(_pointer(path), expected, value))


def _require_string(value: Any, path: tuple[Any, ...]) -> str:
    if not isinstance(value, str) or not value:
        raise BridgeError("{}: expected non-empty string".format(_pointer(path)))
    return value


def _require_number_sequence(value: Any, expected: list[float], path: tuple[Any, ...], *, positive: bool = False) -> None:
    values = _require_array(value, path, len(expected))
    if any(not isinstance(item, (int, float)) or isinstance(item, bool) for item in values):
        raise BridgeError("{}: expected numeric values".format(_pointer(path)))
    if positive and any(item <= 0 for item in values):
        raise BridgeError("{}: expected positive values".format(_pointer(path)))
    if list(values) != expected:
        raise BridgeError("{}: expected {!r}, got {!r}".format(_pointer(path), expected, values))


def validate_cycle2_intent(intent: dict[str, Any]) -> None:
    """Reject every unspecified Cycle II variation before invoking WorldForge."""
    top = {"specVersion", "worldId", "map", "output", "anchors", "poi", "generationInput",
           "subjects", "evidence", "reactiveCategories", "worldState"}
    _require_keys(intent, (), top)
    if not isinstance(intent["specVersion"], int) or isinstance(intent["specVersion"], bool):
        raise BridgeError("$.specVersion: expected integer")
    _require_equal(intent["specVersion"], 1, ("specVersion",))
    _require_equal(intent["worldId"], "Cycle2_Garden", ("worldId",))

    world_map = _require_object(intent["map"], ("map",))
    _require_keys(world_map, ("map",), {"asset", "anchorId"})
    _require_equal(world_map["asset"], "/Game/Maps/Lvl_Gloamstead", ("map", "asset"))
    _require_equal(world_map["anchorId"], "Cycle2_Garden.Anchor", ("map", "anchorId"))

    output = _require_object(intent["output"], ("output",))
    _require_keys(output, ("output",), {"root"})
    _require_equal(output["root"], "/Game/Generated/WorldForge/Cycle2/", ("output", "root"))

    anchors = _require_array(intent["anchors"], ("anchors",), 1)
    anchor = _require_object(anchors[0], ("anchors", 0))
    _require_keys(anchor, ("anchors", 0), {"anchorId", "mapAsset", "surveyId"})
    _require_equal(anchor["anchorId"], "Cycle2_Garden.Anchor", ("anchors", 0, "anchorId"))
    _require_equal(anchor["mapAsset"], "/Game/Maps/Lvl_Gloamstead", ("anchors", 0, "mapAsset"))
    _require_equal(anchor["surveyId"], "cycle2-garden-anchor", ("anchors", 0, "surveyId"))

    poi = _require_object(intent["poi"], ("poi",))
    _require_keys(poi, ("poi",), {"poiId", "anchorId", "anchorTransform", "bounds"})
    _require_equal(poi["poiId"], "Cycle2_Garden", ("poi", "poiId"))
    _require_equal(poi["anchorId"], "Cycle2_Garden.Anchor", ("poi", "anchorId"))
    transform = _require_object(poi["anchorTransform"], ("poi", "anchorTransform"))
    _require_keys(transform, ("poi", "anchorTransform"), {"coordinateSpace", "translation"})
    _require_equal(transform["coordinateSpace"], "sanctuary_bootstrap_local",
                   ("poi", "anchorTransform", "coordinateSpace"))
    _require_number_sequence(transform["translation"], [480.0, 160.0, 0.0],
                             ("poi", "anchorTransform", "translation"))
    bounds = _require_object(poi["bounds"], ("poi", "bounds"))
    _require_keys(bounds, ("poi", "bounds"), {"shape", "halfExtents"})
    _require_equal(bounds["shape"], "box", ("poi", "bounds", "shape"))
    _require_number_sequence(bounds["halfExtents"], [240.0, 280.0, 160.0],
                             ("poi", "bounds", "halfExtents"), positive=True)

    generation = _require_object(intent["generationInput"], ("generationInput",))
    _require_keys(generation, ("generationInput",), {"seed", "inputVersion"})
    if not isinstance(generation["seed"], int) or isinstance(generation["seed"], bool):
        raise BridgeError("$.generationInput.seed: expected integer")
    _require_equal(generation["seed"], 42, ("generationInput", "seed"))
    _require_equal(generation["inputVersion"], "gloamstead-cycle2-corruption-neglect.v1",
                   ("generationInput", "inputVersion"))

    subjects = _require_array(intent["subjects"], ("subjects",), 1)
    subject = _require_object(subjects[0], ("subjects", 0))
    _require_keys(subject, ("subjects", 0),
                  {"subjectId", "warningId", "ritualType", "restorationTag", "surveyId"})
    for key, expected in {"subjectId": "Cycle2_Garden", "warningId": "GardenRot",
                          "ritualType": "GardenBed", "restorationTag": "GardenBed",
                          "surveyId": "cycle2-garden-subject"}.items():
        _require_equal(subject[key], expected, ("subjects", 0, key))

    evidence = _require_object(intent["evidence"], ("evidence",))
    _require_keys(evidence, ("evidence",), {"supportBindings", "dawnReport"})
    bindings = _require_array(evidence["supportBindings"], ("evidence", "supportBindings"), 3)
    expected_bindings = {
        "GardenRot.WitheredVines": ("environmental", "Cycle2_Garden.Environment"),
        "GardenRot.ColdSoil": ("object_reaction", "Cycle2_Garden.ObjectReaction"),
        "GardenRot.BellMoths": ("audio", "Cycle2_Garden.Audio"),
    }
    found: set[str] = set()
    for index, binding_value in enumerate(bindings):
        binding = _require_object(binding_value, ("evidence", "supportBindings", index))
        _require_keys(binding, ("evidence", "supportBindings", index), {"supportId", "surface", "surfaceId"})
        support_id = _require_string(binding["supportId"], ("evidence", "supportBindings", index, "supportId"))
        if support_id in found:
            raise BridgeError("$.evidence.supportBindings[{}].supportId: duplicate support id {!r}".format(index, support_id))
        found.add(support_id)
        if support_id not in expected_bindings:
            raise BridgeError("$.evidence.supportBindings[{}].supportId: unexpected support id {!r}".format(index, support_id))
        surface, surface_id = expected_bindings[support_id]
        _require_equal(binding["surface"], surface, ("evidence", "supportBindings", index, "surface"))
        _require_equal(binding["surfaceId"], surface_id, ("evidence", "supportBindings", index, "surfaceId"))
    if found != set(expected_bindings):
        raise BridgeError("$.evidence.supportBindings: missing required support binding(s): {}".format(
            ", ".join(sorted(set(expected_bindings) - found))))
    dawn = _require_object(evidence["dawnReport"], ("evidence", "dawnReport"))
    _require_keys(dawn, ("evidence", "dawnReport"), {"surface", "surfaceId", "supportIds"})
    _require_equal(dawn["surface"], "dawn_report", ("evidence", "dawnReport", "surface"))
    _require_equal(dawn["surfaceId"], "Cycle2_Garden.DawnReport", ("evidence", "dawnReport", "surfaceId"))
    support_ids = _require_array(dawn["supportIds"], ("evidence", "dawnReport", "supportIds"), 3)
    if len(set(support_ids)) != 3 or set(support_ids) != set(expected_bindings):
        raise BridgeError("$.evidence.dawnReport.supportIds: must bind each GardenRot support exactly once")

    categories = _require_array(intent["reactiveCategories"], ("reactiveCategories",), 4)
    expected_categories = {"foliage", "ruins", "paths", "lighting_materials"}
    found_categories: set[str] = set()
    for index, category_value in enumerate(categories):
        category = _require_object(category_value, ("reactiveCategories", index))
        _require_keys(category, ("reactiveCategories", index), {"category", "stateKey"})
        category_name = _require_string(category["category"], ("reactiveCategories", index, "category"))
        if category_name in found_categories:
            raise BridgeError("$.reactiveCategories[{}].category: duplicate category {!r}".format(index, category_name))
        found_categories.add(category_name)
        _require_equal(category["stateKey"], "restoration_level", ("reactiveCategories", index, "stateKey"))
    if found_categories != expected_categories:
        raise BridgeError("$.reactiveCategories: must contain exactly {}".format(
            ", ".join(sorted(expected_categories))))

    state = _require_object(intent["worldState"], ("worldState",))
    _require_keys(state, ("worldState",), {"scope", "contextId", "key", "scenarios"})
    _require_equal(state["scope"], "Region", ("worldState", "scope"))
    _require_equal(state["contextId"], "Cycle2_Garden", ("worldState", "contextId"))
    _require_equal(state["key"], "restoration_level", ("worldState", "key"))
    scenarios = _require_array(state["scenarios"], ("worldState", "scenarios"), 2)
    if any(not isinstance(item, (int, float)) or isinstance(item, bool) for item in scenarios):
        raise BridgeError("$.worldState.scenarios: expected numeric values")
    if len(set(scenarios)) != 2 or set(scenarios) != {0.0, 1.0}:
        raise BridgeError("$.worldState.scenarios: must contain exactly 0.0 and 1.0")


def _canonical_json(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def _sha256(value: bytes | Path) -> str:
    if isinstance(value, Path):
        value = value.read_bytes()
    return hashlib.sha256(value).hexdigest()


def _validate_pinned_semantic_schema(schema: dict[str, Any], raw: bytes) -> None:
    """The Cycle II schema is a fixed caller contract, not a caller override."""
    if _sha256(raw) != SEMANTIC_SCHEMA_SHA256:
        raise BridgeError("$: semantic schema SHA-256 does not match the pinned Cycle II contract")
    if schema.get("$id") != "https://gloamstead.local/specs/world/gloamstead_world_spec.schema.json":
        raise BridgeError("$.$id: semantic schema does not identify the pinned Cycle II contract")
    if schema.get("additionalProperties") is not False:
        raise BridgeError("$.additionalProperties: pinned semantic schema must reject unknown fields")


def _git_commit(repository: Path, label: str) -> str:
    try:
        result = subprocess.run(["git", "-c", "safe.directory={}".format(repository.as_posix()),
                                 "-C", str(repository), "rev-parse", "HEAD"],
                                capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise BridgeError("$: cannot resolve {} commit: {}".format(label, error)) from error
    commit = result.stdout.strip()
    if len(commit) != 40 or any(character not in "0123456789abcdef" for character in commit):
        raise BridgeError("$: invalid {} commit returned by git".format(label))
    return commit


def _git_paths_clean(repository: Path, paths: list[Path], label: str) -> bool:
    try:
        relative_paths = [str(path.resolve().relative_to(repository.resolve())) for path in paths]
    except ValueError:
        # A caller may deliberately stage an immutable copy of its intent for
        # a reproducibility test.  It cannot truthfully claim repository
        # cleanliness, but that must not prevent the compile from recording
        # the scoped source as external.
        return False
    try:
        result = subprocess.run(["git", "-c", "safe.directory={}".format(repository.as_posix()),
                                 "-C", str(repository), "status", "--porcelain=v1", "--untracked-files=all",
                                 "--", *relative_paths], capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise BridgeError("$: cannot inspect {} source cleanliness: {}".format(label, error)) from error
    return not bool(result.stdout.strip())


def _repository_path_label(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_ROOT.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def _safe_output_root(path: Path, intent: Path, schema: Path) -> Path:
    output_root = path.resolve()
    if output_root == Path(output_root.anchor):
        raise BridgeError("$: output root must not be a filesystem root")
    content_root = (REPO_ROOT / "Content").resolve()
    try:
        output_root.relative_to(content_root)
    except ValueError:
        pass
    else:
        raise BridgeError("$: output root must not be inside Content/")
    for label, input_path in (("intent", intent.resolve()), ("schema", schema.resolve())):
        if input_path == output_root:
            raise BridgeError("$: output root collides with {} input".format(label))
        try:
            input_path.relative_to(output_root)
        except ValueError:
            pass
        else:
            raise BridgeError("$: output root may not contain {} input".format(label))
    return output_root


def _prepare_empty_publish_root(output_root: Path) -> None:
    if output_root.exists():
        raise BridgeError("$: output root already exists; atomic publish requires an absent destination")
    output_root.parent.mkdir(parents=True, exist_ok=True)


def _safe_compiler(path: Path) -> Path:
    compiler = path.resolve()
    if not compiler.is_file():
        raise BridgeError("$: WorldForge compiler does not exist or is not a file: {}".format(path))
    if compiler.suffix.lower() != ".py":
        raise BridgeError("$: WorldForge compiler must be a Python file: {}".format(path))
    return compiler


def _safe_materialization_preparer(path: Path | None, compiler: Path) -> Path:
    preparer = (compiler.with_name("prepare_authored_world_materialization.py")
                if path is None else path).resolve()
    if not preparer.is_file():
        raise BridgeError(
            "$: WorldForge materialization preparer does not exist or is not a file: {}".format(preparer))
    if preparer.suffix.lower() != ".py":
        raise BridgeError(
            "$: WorldForge materialization preparer must be a Python file: {}".format(preparer))
    return preparer


def _normalise(intent: dict[str, Any]) -> dict[str, Any]:
    """Create generic records from explicit spatial and state fields only."""
    anchor, poi, state = intent["anchors"][0], intent["poi"], intent["worldState"]
    return {
        "world_id": intent["worldId"],
        "input_version": intent["generationInput"]["inputVersion"],
        "seed": intent["generationInput"]["seed"],
        "authored_anchors": [{
            "anchor_id": anchor["anchorId"], "map_asset": anchor["mapAsset"],
            "survey_id": anchor["surveyId"],
        }],
        "points_of_interest": [{
            "poi_id": poi["poiId"], "anchor_id": poi["anchorId"],
            "coordinate_space": poi["anchorTransform"]["coordinateSpace"],
            "translation": poi["anchorTransform"]["translation"],
            "bounds_shape": poi["bounds"]["shape"], "bounds_half_extents": poi["bounds"]["halfExtents"],
        }],
        "terrain_slice": {
            "map_asset": intent["map"]["asset"], "map_anchor_id": intent["map"]["anchorId"],
            "authored_anchor": {"anchor_id": anchor["anchorId"], "map_asset": anchor["mapAsset"],
                                "survey_id": anchor["surveyId"]},
            "point_of_interest": {
                "poi_id": poi["poiId"], "anchor_id": poi["anchorId"],
                "coordinate_space": poi["anchorTransform"]["coordinateSpace"],
                "translation": poi["anchorTransform"]["translation"],
                "bounds_shape": poi["bounds"]["shape"], "bounds_half_extents": poi["bounds"]["halfExtents"],
            },
        },
        "reactive_categories": sorted([
            {"id": record["category"], "values": {"state_key": record["stateKey"]}}
            for record in intent["reactiveCategories"]
        ], key=lambda record: record["id"]),
        "world_state_scenarios": [
            {"id": "restoration-level-{}".format(int(value)), "state_values": {
                "scope": state["scope"], "context_id": state["contextId"],
                "key": state["key"], "value": value,
            }}
            for value in sorted(state["scenarios"])
        ],
    }


GENERIC_SCHEMA = {
    "type": "object", "additionalProperties": False,
    "required": ["world_id", "input_version", "seed", "authored_anchors", "points_of_interest",
                 "terrain_slice", "reactive_categories", "world_state_scenarios"],
    "properties": {
        "world_id": {"type": "string"}, "input_version": {"type": "string"}, "seed": {"type": "integer"},
        "authored_anchors": {"type": "array", "items": {}},
        "points_of_interest": {"type": "array", "items": {}},
        "terrain_slice": {"type": "object"},
        "reactive_categories": {"type": "array", "items": {
            "type": "object", "additionalProperties": False, "required": ["id", "values"],
            "properties": {"id": {"type": "string"}, "values": {"type": "object"}},
        }},
        "world_state_scenarios": {"type": "array", "items": {
            "type": "object", "additionalProperties": False, "required": ["id", "state_values"],
            "properties": {"id": {"type": "string"}, "state_values": {"type": "object"}},
        }},
    },
}


def _write_atomic(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=".cycle2-bridge-", suffix=".tmp", dir=str(path.parent))
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(payload)
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def _safe_artifact_path(output_root: Path, name: str) -> Path:
    if name not in {*COMPILER_OUTPUTS, SIDE_CAR, RECEIPT, MATERIALIZATION_REQUEST} or Path(name).name != name:
        raise BridgeError("$: unsafe bridge artifact name {!r}".format(name))
    candidate = (output_root / name).resolve()
    try:
        candidate.relative_to(output_root)
    except ValueError as error:
        raise BridgeError("$: bridge artifact escapes output root") from error
    return candidate


def _run_compiler(compiler: Path, spec: Path, schema: Path, output_root: Path) -> None:
    try:
        result = subprocess.run([sys.executable, str(compiler), "--spec", str(spec), "--schema", str(schema),
                                 "--output-root", str(output_root)], capture_output=True, text=True, check=False)
    except OSError as error:
        raise BridgeError("$: failed to invoke WorldForge compiler: {}".format(error)) from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise BridgeError("$: WorldForge compiler failed with exit {}: {}".format(result.returncode, detail))


def _run_materialization_preparer(preparer: Path, input_root: Path, generated_root: str,
                                  output_path: Path) -> None:
    try:
        result = subprocess.run(
            [sys.executable, str(preparer), "--input-root", str(input_root),
             "--generated-root", generated_root, "--output", str(output_path)],
            capture_output=True, text=True, check=False,
        )
    except OSError as error:
        raise BridgeError("$: failed to invoke WorldForge materialization preparer: {}".format(error)) from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise BridgeError(
            "$: WorldForge materialization preparer failed with exit {}: {}".format(
                result.returncode, detail))


def compile_cycle2_world(intent_path: Path, schema_path: Path, compiler_path: Path, output_root: Path,
                         materialization_preparer_path: Path | None = None) -> dict[str, Any]:
    intent_path, schema_path = intent_path.resolve(), schema_path.resolve()
    intent, intent_bytes = _load_object(intent_path, "intent")
    schema, schema_bytes = _load_object(schema_path, "schema")
    _validate_pinned_semantic_schema(schema, schema_bytes)
    validate_cycle2_intent(intent)
    output_root = _safe_output_root(output_root, intent_path, schema_path)
    _prepare_empty_publish_root(output_root)
    compiler_path = _safe_compiler(compiler_path)
    normalized = _normalise(intent)
    normalized_bytes, compiler_schema_bytes = _canonical_json(normalized), _canonical_json(GENERIC_SCHEMA)
    gloamstead_commit = _git_commit(REPO_ROOT, "Gloamstead")
    compiler_root = compiler_path.parents[2]
    compiler_commit = _git_commit(compiler_root, "WorldForge compiler")
    source_provenance = {
        "gloamstead_source_revision": gloamstead_commit,
        "gloamstead_source_clean": _git_paths_clean(REPO_ROOT, [intent_path, schema_path], "Gloamstead"),
        "worldforge_source_revision": compiler_commit,
        "worldforge_source_clean": _git_paths_clean(compiler_root, [compiler_path], "WorldForge compiler"),
        "world_spec_path": _repository_path_label(intent_path),
        "world_spec_sha256": _sha256(intent_bytes),
        "schema_path": _repository_path_label(schema_path),
        "schema_sha256": _sha256(schema_bytes),
    }
    poi, state = intent["poi"], intent["worldState"]
    contract_provenance = {
        "poi_id": poi["poiId"], "poi_anchor_id": poi["anchorId"],
        "poi_coordinate_space": poi["anchorTransform"]["coordinateSpace"],
        "poi_anchor_translation": poi["anchorTransform"]["translation"],
        "poi_box_half_extents": poi["bounds"]["halfExtents"],
        "poi_box_contained_by_sanctuary_bootstrap_half_extents": [800.0, 800.0, 400.0],
        "generation_input_seed": intent["generationInput"]["seed"],
        "generation_input_version": intent["generationInput"]["inputVersion"],
        "worldforge_state_address": {"scope": state["scope"], "context_id": state["contextId"], "key": state["key"]},
        "worldforge_state_scenarios": [
            {"label": "untouched" if value == 0.0 else "restored", "value": value}
            for value in sorted(state["scenarios"])
        ],
    }
    with tempfile.TemporaryDirectory(prefix=".gloamstead-cycle2-stage-", dir=str(output_root.parent)) as staging_name:
        staging_root = Path(staging_name)
        with tempfile.TemporaryDirectory(prefix="gloamstead-cycle2-worldforge-") as temporary:
            temporary_root = Path(temporary)
            canonical_spec, canonical_schema = (temporary_root / "canonical-world.json",
                                                 temporary_root / "canonical-world.schema.json")
            canonical_spec.write_bytes(normalized_bytes)
            canonical_schema.write_bytes(compiler_schema_bytes)
            _run_compiler(compiler_path, canonical_spec, canonical_schema, staging_root)
        missing = [name for name in COMPILER_OUTPUTS if not _safe_artifact_path(staging_root, name).is_file()]
        if missing:
            raise BridgeError("$: WorldForge compiler did not produce required artifact(s): {}".format(
                ", ".join(missing)))
        materialization_preparer = _safe_materialization_preparer(materialization_preparer_path, compiler_path)
        preparer_root = materialization_preparer.parents[2]
        preparer_commit = _git_commit(preparer_root, "WorldForge materialization preparer")
        source_provenance.update({
            "worldforge_materialization_source_revision": preparer_commit,
            "worldforge_materialization_source_clean": _git_paths_clean(
                preparer_root, [materialization_preparer], "WorldForge materialization preparer"),
        })
        compiler_manifest, _ = _load_object(_safe_artifact_path(staging_root, "manifest.json"), "WorldForge manifest")
        worldforge_provenance = compiler_manifest.get("worldforge_provenance")
        if not isinstance(worldforge_provenance, dict):
            raise BridgeError("$.worldforge_provenance: compiler manifest omitted provenance")
        if worldforge_provenance.get("source_commit") != compiler_commit:
            raise BridgeError("$.worldforge_provenance.source_commit: compiler manifest revision differs from invoked compiler")
        compiler_tree_dirty = worldforge_provenance.get("source_tree_dirty")
        if not isinstance(compiler_tree_dirty, bool):
            raise BridgeError("$.worldforge_provenance.source_tree_dirty: compiler manifest must report boolean cleanliness")
        with tempfile.TemporaryDirectory(prefix=".worldforge-materialization-request-",
                                          dir=str(output_root.parent)) as request_name:
            request_path = Path(request_name) / MATERIALIZATION_REQUEST
            _run_materialization_preparer(
                materialization_preparer, staging_root, intent["output"]["root"], request_path)
            materialization_request, materialization_request_bytes = _load_object(
                request_path, "WorldForge materialization request")
            _require_equal(materialization_request.get("artifact_kind"),
                           "authored_world_materialization_request",
                           ("materialization-request", "artifact_kind"))
            _require_equal(materialization_request.get("execution_status"),
                           "not_materialized", ("materialization-request", "execution_status"))
            _require_equal(materialization_request.get("observation_status"),
                           "not_observed", ("materialization-request", "observation_status"))
            _require_equal(materialization_request.get("materialization_claim"),
                           "none", ("materialization-request", "materialization_claim"))
            _require_equal(materialization_request.get("generated_root"),
                           intent["output"]["root"].rstrip("/"),
                           ("materialization-request", "generated_root"))
            _write_atomic(_safe_artifact_path(staging_root, MATERIALIZATION_REQUEST),
                          materialization_request_bytes)
        materialization_request_hash = _sha256(
            _safe_artifact_path(staging_root, MATERIALIZATION_REQUEST))
        output_hashes = {name: _sha256(_safe_artifact_path(staging_root, name)) for name in COMPILER_OUTPUTS}
        output_set_hash = _sha256(_canonical_json(output_hashes))
        provenance = {
        "artifact_kind": "gloamstead_cycle2_intent_provenance",
        "execution_status": "not_materialized",
        "observation_status": "not_observed",
        "source": source_provenance,
        "contract": contract_provenance,
        "worldforge_compiler_manifest_source_tree_dirty": compiler_tree_dirty,
        "semantic_intent_retained_by_gloamstead": {
            "subjects": intent["subjects"], "evidence": intent["evidence"],
        },
        "compiler_boundary": "WorldForge received only generic spatial and state records; it did not interpret warning, ritual, evidence, or lore meaning.",
        }
        _write_atomic(_safe_artifact_path(staging_root, SIDE_CAR), _canonical_json(provenance))
        receipt = {
        "artifact_kind": "gloamstead_worldforge_bridge_receipt",
        "execution_status": "not_materialized",
        "observation_status": "not_observed",
        "materialization_claim": "none",
        "source": {
            "intent_sha256": source_provenance["world_spec_sha256"],
            "schema_sha256": source_provenance["schema_sha256"],
            "gloamstead_commit": gloamstead_commit,
        },
        **source_provenance,
        **contract_provenance,
        "normalized_spec_sha256": _sha256(normalized_bytes),
        "generator_revision": compiler_commit,
        "generator_parameters": {},
        "worldforge_state_write_lease_source_revision": STATE_WRITE_LEASE_REVISION,
        "worldforge_state_write_lease_source_sha256": STATE_WRITE_LEASE_HASHES,
        "compiler": {"path": str(compiler_path), "commit": compiler_commit,
                     "manifest_source_tree_dirty": compiler_tree_dirty},
        "materialization_request": {
            "path": MATERIALIZATION_REQUEST,
            "sha256": materialization_request_hash,
            "preparer": str(materialization_preparer),
            "commit": preparer_commit,
            "source_clean": source_provenance["worldforge_materialization_source_clean"],
        },
        "outputs": {
            "artifacts": output_hashes,
            "materialization_request_sha256": materialization_request_hash,
            "output_manifest_sha256": output_hashes["manifest.json"],
            "output_set_sha256": output_set_hash,
        },
        "scope": "planned generic descriptors only; no UE materialization, map load, NeoStack activity, or observed survey is claimed.",
        }
        _write_atomic(_safe_artifact_path(staging_root, RECEIPT), _canonical_json(receipt))
        try:
            os.replace(staging_root, output_root)
        except OSError as error:
            raise BridgeError("$: cannot atomically publish staged output root: {}".format(error)) from error
    return receipt


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compile strict Gloamstead Cycle II intent through WorldForge.")
    parser.add_argument("--intent", type=Path, default=DEFAULT_INTENT)
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA)
    parser.add_argument("--worldforge-compiler", type=Path,
                        default=os.environ.get("WORLDFORGE_COMPILER"),
                        help="External compile_authored_world.py (or WORLDFORGE_COMPILER).")
    parser.add_argument("--worldforge-materialization-preparer", type=Path,
                        default=os.environ.get("WORLDFORGE_MATERIALIZATION_PREPARER"),
                        help="External prepare_authored_world_materialization.py (or environment variable).")
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    args = parser.parse_args(argv)
    if args.worldforge_compiler is None:
        print("ERROR: $: --worldforge-compiler or WORLDFORGE_COMPILER is required", file=sys.stderr)
        return 2
    try:
        receipt = compile_cycle2_world(args.intent, args.schema, args.worldforge_compiler, args.output_root,
                                       args.worldforge_materialization_preparer)
    except BridgeError as error:
        print("ERROR: {}".format(error), file=sys.stderr)
        return 2
    print("COMPILED: 6 planned WorldForge artifacts plus {}, {}, and {} under {}".format(
        MATERIALIZATION_REQUEST, SIDE_CAR, RECEIPT, args.output_root))
    print("RECEIPT: {}".format(_safe_artifact_path(args.output_root.resolve(), RECEIPT)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
