#!/usr/bin/env python3
"""gloamstead_consumer -- Gloamstead's caller-originated request to WorldForge.

WHO IS SPEAKING
---------------
Gloamstead. Not WorldForge. ``provenance.origination`` is
``adapter.ORIGINATION_CALLER`` and the provenance statement names this repository
and the commit it was authored at, because the whole value of the record is that
somebody outside WorldForge asked for something. WorldForge's own demonstration
consumers declare ``worldforge_authored_demonstration`` and must; this one is the
other case.

WHAT IS BEING ASKED FOR
-----------------------
A REVISION of ``/Game/Maps/Lvl_Gloamstead`` -- the map already exists and is
hand-authored (Config/DefaultEngine.ini:2-3 names it as both GameDefaultMap and
EditorStartupMap). The first-night slice has the player cross a corridor and a
plaza to the lantern they restored; the director's own comment says the night
must be "long enough for the player to actually cross the plaza to the lantern
they restored" (Source/Gloamstead/Systems/GloamsteadFirstNightDirector.h:83).
At night, with no added light permitted, that crossing is the part that does not
read. So: a small number of traversal wayfinding markers along the route from
``PlayerStart`` to the semantic subject ``courtyard.lantern.first`` -- and
nothing else touched.

WHAT THIS MODULE IS NOT
-----------------------
It is a THIN ADAPTER. It states identities, metrics, catalogs, protected
bindings and acceptance hooks. It contains no placement, composition,
provider-selection, generation, transaction, validation or repair logic, and it
imports none of ``wfcore.planning``, ``wfcore.analysis``, ``wfcore.providers``,
``wfcore.transaction``, ``wfcore.acceptance`` or ``wfcore.repair``. That is
checked mechanically against this file's own source by
``consumers.adapter.validate_adapter_has_no_generation_logic``, and the
accompanying test asserts it rather than trusting it.

WHAT GLOAMSTEAD DOES NOT KNOW, AND SAYS SO
------------------------------------------
Six measures are declared the literal ``"unknown"`` rather than filled with an
engine default. ``AGloamsteadCharacter`` is ``UCLASS(abstract)``
(GloamsteadCharacter.h:23-24) and its constructor sets the capsule, walk speed
and jump velocity but never ``BaseEyeHeight``, ``MaxStepHeight``,
``WalkableFloorAngle`` or any FOV; a repo-wide grep of Source/ and Config/ for
those four names returns nothing for this character (the single
``SetWalkableFloorAngle`` hit is Variant_SideScrolling/SideScrollingCharacter.cpp:40,
a variant this game does not ship). Writing UE's defaults in would have been a
guess wearing a measurement's clothes, and ``contracts.check_measure`` exists
precisely so that guess has somewhere honest to go instead.

Jump is a third case worth naming out loud: Gloamstead's character DOES jump
(JumpZVelocity=500, GloamsteadCharacter.cpp:33) but only the velocity is
authored -- the reachable height is a derivation nobody in this project has made,
so ``max_jump_height_cm`` is ``"unknown"`` too.
"""

import os
import sys

# WorldForge Core lives in a separate repository. Gloamstead imports its contract
# vocabulary; it does not vendor it, and it never edits it.
_WF_TOOLS = os.environ.get("WORLDFORGE_TOOLS", r"D:/Unreal Projects/WorldForge/tools")
if _WF_TOOLS not in sys.path:
    sys.path.insert(0, _WF_TOOLS)

from consumers import adapter as ADP                      # noqa: E402
from wfcore import constraints as K                       # noqa: E402
from wfcore.contracts import acceptance_criteria as ACR   # noqa: E402
from wfcore.contracts import asset_catalog as AC          # noqa: E402
from wfcore.contracts import consumer_profile as CP       # noqa: E402
from wfcore.contracts import revision_policy as RP        # noqa: E402
from wfcore.contracts import world_request as WR          # noqa: E402

# --------------------------------------------------------------------------- #
# identity
# --------------------------------------------------------------------------- #
CONSUMER_ID = "gloamstead"
ADAPTER_ID = "adapter_gloamstead_first_night_wayfinding"
CATALOG_ID = "catalog_gloamstead_first_night_markers"
REQUEST_ID = "request_gloamstead_first_night_wayfinding_0001"
POLICY_ID = "policy_gloamstead_first_night_wayfinding"
CRITERIA_ID = "criteria_gloamstead_first_night_wayfinding"

SUBJECT_MAP = "/Game/Maps/Lvl_Gloamstead"
ENGINE_VERSION = "5.8"
PROJECT_IDENTIFIER = "Gloamstead"

REPOSITORY = "https://github.com/markwuenschel-dev/Gloamstead.git"
COMMIT_SHA = "6628a02f44b91c2a17a29fe2d9d6139bae3d72f1"

CALLER_STATEMENT = (
    "This intent ORIGINATES FROM THE GLOAMSTEAD PROJECT. Gloamstead -- the game "
    "in repository {repo}, at commit {sha}, working tree D:\\Unreal "
    "Projects\\Gloamstead5_8 -- authored this profile, catalog, request, "
    "revision policy and acceptance criteria itself and is asking WorldForge to "
    "act on them. WorldForge did not write any of it and did not choose the "
    "subject: the map {subject} is Gloamstead's own hand-authored first-night "
    "level, and the thing being asked for is legibility of a route Gloamstead's "
    "own first-night director already depends on. Any artifact produced from "
    "this adapter may be labelled caller-originated, because a caller really "
    "did ask."
).format(repo=REPOSITORY, sha=COMMIT_SHA, subject=SUBJECT_MAP)

# Who inside Gloamstead resolves an unknown. Named once; every unknown points
# here, so a permanent acceptance blocker always has somebody attached to it.
UNKNOWN_OWNER = (
    "Gloamstead gameplay owner -- must measure the shipping Blueprint pawn "
    "(AGloamsteadCharacter is UCLASS(abstract), GloamsteadCharacter.h:23-24, so "
    "the live values are in uasset data no source grep can read) and record the "
    "result back into this adapter")

RITUAL_UNKNOWN_OWNER = (
    "Gloamstead ritual owner -- must read the authored RestorationRadius "
    "attribute off the PCG points for courtyard.lantern.first; the 800cm in "
    "RitualPlacementComponent.cpp:553 is the code FALLBACK, not the authored "
    "value")

# --------------------------------------------------------------------------- #
# the identity-bearing content a generator must never modify.
# Every path below was confirmed to exist on disk under Content/ at COMMIT_SHA.
# --------------------------------------------------------------------------- #
PROTECTED_IDENTITIES = [
    "/Game/Gloamstead/Blueprints/BP_VeilHeart",
    "/Game/Data/DA_VeilHeartWarningCatalog",
    "/Game/Gloamstead/Materials/MI_VeilHeart_Core",
    "/Game/Gloamstead/World/BP_FirstLanternAnchor",
    "/Game/Gloamstead/Restoration/FirstLantern/BP_Restored_LanternPost",
    "/Game/Gloamstead/VFX/NS_LanternMotes",
    "/Game/Gloamstead/World/BP_SanctuaryBootstrap",
    "/Game/Data/DA_Ritual_BellShrine",
    "/Game/Data/DA_Ritual_GardenBed",
    "/Game/Data/DA_Ritual_LanternPost",
    "/Game/Data/DA_Ritual_MirrorPillar",
    "/Game/Data/DA_Ritual_PathPoint",
]

# --------------------------------------------------------------------------- #
# the body and the lens, as Gloamstead actually authored them
# --------------------------------------------------------------------------- #
# InitCapsuleSize(42.f, 96.0f) at GloamsteadCharacter.cpp:20 -- half-height 96
# means a 192cm capsule. The four "unknown"s below are the honest ones; see the
# module docstring for the greps that establish they are genuinely unauthored.
PLAYER_METRICS = {
    "capsule_height_cm": 192.0,
    "capsule_radius_cm": 42.0,
    "eye_height_cm": "unknown",
    "max_step_height_cm": "unknown",
    "max_walk_slope_deg": "unknown",
    "max_jump_height_cm": "unknown",
}

# Third-person spring arm, TargetArmLength 400, bUsePawnControlRotation true
# (GloamsteadCharacter.cpp:43-44). Nothing in Source/ or Config/ sets an FOV or a
# clip plane, so all three are unknown rather than UE's 90/10/exp defaults.
CAMERA_METRICS = {
    "camera_mode": "third_person_close",
    "horizontal_fov_deg": "unknown",
    "near_clip_cm": "unknown",
    "far_clip_cm": "unknown",
}

# --------------------------------------------------------------------------- #
# semantic landmarks. Ids match Gloamstead's OWN declared survey subjects
# (Source/Gloamstead/Systems/GloamsteadSurveySubjectRegistry.cpp:19-63) wherever a
# declaration exists, so a WorldForge report and a Gloamstead survey artifact are
# talking about the same places.
# --------------------------------------------------------------------------- #
LANDMARKS = [
    {
        "landmark_id": "entry.player_start",
        "role": "entry",
        "must_be_reachable": True,
        "significance": (
            "the PlayerStart actor in Lvl_Gloamstead -- where the first night "
            "begins. It is an actor label, NOT a declared survey subject: the "
            "registry declares four subjects and this is not one of them"),
    },
    {
        "landmark_id": "courtyard.lantern.first",
        "role": "objective",
        "must_be_reachable": True,
        "significance": (
            "the lantern the player restores and then walks back to. Declared at "
            "GloamsteadSurveySubjectRegistry.cpp:57 with resolver kind "
            "RegisteredComponent and deliberately NO ActorClass -- the claim is "
            "made at runtime by URitualPlacementComponent "
            "(RitualPlacementComponent.cpp:32)"),
    },
    {
        "landmark_id": "sanctuary.heart",
        "role": "shelter",
        "must_be_reachable": True,
        "significance": (
            "AVeilHeart, placed as the Dais_Heart actor. Declared at "
            "GloamsteadSurveySubjectRegistry.cpp:26"),
    },
    {
        "landmark_id": "sanctuary.bootstrap",
        "role": "boundary",
        "must_be_reachable": False,
        "significance": (
            "AGloamsteadSanctuaryBootstrap, declared at "
            "GloamsteadSurveySubjectRegistry.cpp:34. Its UBoxComponent Bounds "
            "(GloamsteadSanctuaryBootstrap.h:44-45) is the sanctuary's extent"),
    },
]

# ``sanctuary.night_director`` is Gloamstead's fourth declared survey subject
# (GloamsteadSurveySubjectRegistry.cpp:42). It is deliberately absent from
# LANDMARKS: it is a director actor with no place semantics, and giving it a
# landmark role would be Gloamstead inventing spatial meaning it never authored.

AFFORDANCE_ROUTE = "afford_route_entry_to_first_lantern"
AFFORDANCE_MARKERS = "afford_wayfinding_markers"


# --------------------------------------------------------------------------- #
# the six records
# --------------------------------------------------------------------------- #
def adapter():
    """The thin adapter: who Gloamstead is, and how its evidence can be read."""
    return ADP.build_adapter(
        adapter_id=ADAPTER_ID,
        consumer_id=CONSUMER_ID,
        display_name="Gloamstead -- first-night wayfinding",
        created_by="Gloamstead",
        provenance={
            "origination": ADP.ORIGINATION_CALLER,
            "authored_by": (
                "Gloamstead (project module 'Gloamstead', UE {engine}), "
                "repository {repo}, commit {sha}").format(
                    engine=ENGINE_VERSION, repo=REPOSITORY, sha=COMMIT_SHA),
            "statement": CALLER_STATEMENT,
        },
        project_identity={
            "engine_version": ENGINE_VERSION,
            "project_identifier": PROJECT_IDENTIFIER,
            "subject_root": SUBJECT_MAP,
        },
        semantic_landmarks=[
            {"landmark_id": lm["landmark_id"], "role": lm["role"],
             "must_be_reachable": lm["must_be_reachable"]}
            for lm in LANDMARKS
        ],
        gameplay_anchors=[
            {"anchor_id": AFFORDANCE_ROUTE, "anchor_kind": "traversal",
             "required": True},
            {"anchor_id": AFFORDANCE_MARKERS, "anchor_kind": "navigation_anchor",
             "required": True},
        ],
        player_metrics=PLAYER_METRICS,
        camera_metrics=CAMERA_METRICS,
        approved_catalog_ids=[CATALOG_ID],
        protected_identities=list(PROTECTED_IDENTITIES),
        # A channel Gloamstead OFFERS, not a reader WorldForge implements. The
        # registry exposes no UFUNCTIONs on purpose
        # (GloamsteadSurveySubjectRegistry.h:49-52 and the note at
        # RitualPlacementComponent.h:60-62), so the only thing crossing the
        # boundary is a JSON artifact Gloamstead writes and WorldForge reads.
        runtime_state_access={
            "access_kind": "telemetry_report",
            "detail": (
                "UGloamsteadSurveySubjectRegistry (a UWorldSubsystem) resolves "
                "its declared subjects in a live world and writes "
                "procedural/reports/gloamstead_survey_subjects/"
                "survey_subject_report.json plus per-request artifacts under "
                ".../requests/<request_id>.json, via EmitReport() "
                "(GloamsteadSurveySubjectRegistry.h:136) and SurveyAndEmit() "
                "(:128). WorldForge READS those files. It cannot call in: the "
                "registry is plain C++ with no reflected surface, and the only "
                "BlueprintCallable path is UGloamsteadSurveySubjectComponent::"
                "RegisterWithRegistry (GloamsteadSurveySubjectComponent.h:52), "
                "which is Gloamstead registering itself, not WorldForge "
                "querying"),
        },
        acceptance_hooks=[
            {"constraint_id": "c_landmarks_stay_reachable",
             "evidence_kind": "runtime_observation",
             "hook_reference": (
                 "UGloamsteadSurveySubjectRegistry::SurveyAndEmit -> "
                 "procedural/reports/gloamstead_survey_subjects/requests/"
                 "<request_id>.json")},
            {"constraint_id": "c_route_traversable",
             "evidence_kind": "runtime_observation",
             "hook_reference": (
                 "UGloamsteadSurveySubjectRegistry::EmitReport -> "
                 "procedural/reports/gloamstead_survey_subjects/"
                 "survey_subject_report.json")},
            {"constraint_id": "c_markers_on_route",
             "evidence_kind": "human_review",
             "hook_reference": "Gloamstead level owner, 5.8 editor review"},
            {"constraint_id": "c_no_marker_in_restoration_envelope",
             "evidence_kind": "runtime_observation",
             "hook_reference": (
                 "URitualPlacementComponent::IsCurrentPlacementValid "
                 "(RitualPlacementComponent.h:42)")},
            {"constraint_id": "c_identity_untouched",
             "evidence_kind": "authoring_time_check",
             "hook_reference": (
                 "git diff {sha}..HEAD -- Content/Gloamstead "
                 "Content/Data").format(sha=COMMIT_SHA)},
            {"constraint_id": "c_marker_instance_budget",
             "evidence_kind": "external_measurement",
             "hook_reference": "WorldForge revision transaction record"},
        ],
        notes=(
            "Gloamstead declares four survey subjects; sanctuary.night_director "
            "is deliberately not carried as a landmark because it is a director "
            "actor with no place semantics."),
    )


def profile():
    """Standing facts about Gloamstead -- true of every request it will make."""
    return CP.build_consumer_profile(
        consumer_id=CONSUMER_ID,
        display_name="Gloamstead",
        created_by="Gloamstead",
        engine_identity={
            "engine_version": ENGINE_VERSION,
            "project_identifier": PROJECT_IDENTIFIER,
        },
        # What Gloamstead is willing to have done to it, from
        # wfcore.providers.base.CAPABILITIES. Terrain shaping, mesh synthesis,
        # material authoring and asset ingest are deliberately absent: this
        # project's identity lives in authored assets and it is not asking for
        # new ones.
        declared_capabilities=[
            "editor_authoring",
            "procedural_scatter",
            "scene_observation",
        ],
        game_type="narrative_exploration",
        # The first-night slice is greybox: its placed geometry is Wall_*,
        # Pillar_*, Ground_Plate StaticMeshActors built from the
        # /Game/LevelPrototyping kit, and Content/Gloamstead/ holds nine assets,
        # none of them a mesh.
        visual_language="blockout_greybox",
        # Gloamstead's character walks and jumps -- ACharacter::Jump is bound at
        # GloamsteadCharacter.cpp:65. "jump" originally had NO member in
        # CP.LOCOMOTION_MODES, so this list could only understate the player's
        # real mobility. The gap was reported rather than worked around, and
        # WorldForge added the member generically to Core; this is the consumer
        # that exposed it, now declaring it.
        locomotion_modes=["walk", "jump"],
        player_metrics=dict(PLAYER_METRICS),
        camera_metrics=dict(CAMERA_METRICS),
        unknown_resolution_owner=UNKNOWN_OWNER,
        standing_constraints=[
            {
                "constraint_id": "sc_identity_bearing_content_is_gloamsteads",
                "constraint_class": K.PROTECTED_SEMANTICS,
                "subject": "gloamstead.identity_bearing_content",
                "detail": (
                    "the VeilHeart, the first lantern and the ritual data "
                    "assets carry this game's identity; no WorldForge request, "
                    "now or later, authorises changing them"),
                "protected_ids": list(PROTECTED_IDENTITIES),
            },
            {
                "constraint_id": "sc_night_readability_without_added_light",
                "constraint_class": K.SOFT_PREFERENCE,
                "subject": "visual.night_legibility",
                "detail": (
                    "Gloamstead prefers space that reads at night through form "
                    "and contrast rather than through added light sources; the "
                    "dark IS the game"),
                "weight": 2.0,
            },
        ],
        notes=(
            "locomotion_modes now declares jump. It could not originally: "
            "CP.LOCOMOTION_MODES had no member for it, so this profile "
            "understated the player's real mobility. This consumer reported the "
            "gap rather than smuggling an out-of-vocabulary string past the "
            "enum, and WorldForge added the member generically to Core -- the "
            "first capability a real caller exposed. AGloamsteadCharacter jumps: "
            "JumpZVelocity=500 at GloamsteadCharacter.cpp:33, ACharacter::Jump "
            "bound at :65. max_jump_height_cm stays 'unknown' regardless, "
            "because only the VELOCITY is authored and the height is a "
            "derivation nobody in this project made. Sprint, crouch, climb, "
            "swim, vehicle and teleport are genuinely not implemented."),
    )


def catalog():
    """The CLOSED set WorldForge may build markers from.

    Not one entry is ``approved``, and that is the honest state rather than an
    oversight: no Gloamstead reviewer has looked at a wayfinding-marker asset,
    because until this request there were no wayfinding markers. Every path
    below was confirmed present on disk; what has not happened is the review.
    An ``approved`` here would be this adapter approving on a human's behalf,
    and ``authorization_of`` would fold it to SATISFIED and ship it.
    """
    return AC.build_asset_catalog(
        catalog_id=CATALOG_ID,
        consumer_id=CONSUMER_ID,
        created_by="Gloamstead",
        entries=[
            {
                "asset_id": "/Game/LevelPrototyping/Meshes/SM_Cylinder",
                "asset_role": "static_geometry",
                "authorization": AC.APPROVED_WITH_CONDITIONS,
                "source_reference": (
                    "Content/LevelPrototyping/Meshes/SM_Cylinder.uasset -- "
                    "present on disk at commit " + COMMIT_SHA),
                "style_tags": ["greybox_kit"],
                "conditions": [
                    "may be placed only as a wayfinding marker on the "
                    "entry.player_start -> courtyard.lantern.first route, never "
                    "as architecture",
                    "collision must not obstruct the walkable corridor; the "
                    "reachability invariant is measured against a 42cm-radius "
                    "capsule",
                    "Gloamstead art direction has not reviewed a cylinder "
                    "marker against the first-night silhouette language",
                ],
            },
            {
                "asset_id": "/Game/LevelPrototyping/Meshes/SM_Plane",
                "asset_role": "static_geometry",
                "authorization": AC.APPROVED_WITH_CONDITIONS,
                "source_reference": (
                    "Content/LevelPrototyping/Meshes/SM_Plane.uasset -- present "
                    "on disk at commit " + COMMIT_SHA),
                "style_tags": ["greybox_kit"],
                "conditions": [
                    "ground-mark stand-in only: Gloamstead owns no decal asset "
                    "of any kind, so a flat plane is the nearest thing that "
                    "exists",
                    "must sit flush with Ground_Plate and must never be "
                    "collidable",
                ],
            },
            {
                "asset_id": "/Game/LevelPrototyping/Materials/M_FlatCol",
                "asset_role": "surface_material",
                "authorization": AC.APPROVED_WITH_CONDITIONS,
                "source_reference": (
                    "Content/LevelPrototyping/Materials/M_FlatCol.uasset -- "
                    "present on disk at commit " + COMMIT_SHA),
                "style_tags": ["greybox_kit"],
                "conditions": [
                    "marker tint must be chosen so the marker reads at night "
                    "WITHOUT an added light source; adjust_lighting is "
                    "prohibited by this revision's policy",
                ],
            },
            {
                "asset_id": (
                    "/Game/LevelPrototyping/Interactable/JumpPad/Assets/Meshes/"
                    "SM_CircularGlow"),
                "asset_role": "marker",
                "authorization": AC.UNREVIEWED,
                "source_reference": (
                    "Content/LevelPrototyping/Interactable/JumpPad/Assets/"
                    "Meshes/SM_CircularGlow.uasset -- present on disk at commit "
                    + COMMIT_SHA),
                "style_tags": ["jumppad_sample"],
                "notes": (
                    "arrived with the Unreal LevelPrototyping JumpPad sample. "
                    "Nobody at Gloamstead has looked at it. Catalogued so it can "
                    "be planned around and discussed; it folds to UNKNOWN and "
                    "blocks acceptance until a human says a word about it"),
            },
            {
                "asset_id": "/Game/Gloamstead/VFX/NS_LanternMotes",
                "asset_role": "effect",
                "authorization": AC.DENIED,
                "source_reference": (
                    "Content/Gloamstead/VFX/NS_LanternMotes.uasset -- present on "
                    "disk at commit " + COMMIT_SHA),
                "denial_reason": (
                    "the motes are the restored lantern's signature. Reusing "
                    "them on a wayfinding marker would dilute the single thing "
                    "the player is walking toward, which is the opposite of what "
                    "this request asks for. Also protected content under "
                    + POLICY_ID),
            },
            {
                "asset_id": "/Game/Gloamstead/World/BP_FirstLanternAnchor",
                "asset_role": "marker",
                "authorization": AC.DENIED,
                "source_reference": (
                    "Content/Gloamstead/World/BP_FirstLanternAnchor.uasset -- "
                    "present on disk at commit " + COMMIT_SHA),
                "denial_reason": (
                    "this actor IS the semantic subject courtyard.lantern.first "
                    "(claimed at RitualPlacementComponent.cpp:32). Placing "
                    "copies of it as route markers would create additional "
                    "claimants for a subject the registry resolves by "
                    "registration, and the registry would then be resolving an "
                    "ambiguity Gloamstead never authored"),
            },
        ],
        notes=(
            "Closed world. Gloamstead owns no asset named *Marker*, *Beacon*, "
            "*Waypoint* or *Decal*, and no light asset of any kind -- verified "
            "by inventory of all 1,629 .uasset files under Content/ at commit "
            + COMMIT_SHA + ". Every usable entry here is therefore a greybox "
            "primitive under review, not an approved marker."),
    )


def request():
    """The revision Gloamstead is asking for. Small, bounded, identity-safe."""
    return WR.build_world_request(
        request_id=REQUEST_ID,
        consumer_id=CONSUMER_ID,
        catalog_id=CATALOG_ID,
        created_by="Gloamstead",
        subject=SUBJECT_MAP,
        request_kind=WR.REVISION,
        revision_target=SUBJECT_MAP,
        revision_policy_id=POLICY_ID,
        semantic_landmarks=list(LANDMARKS),
        gameplay_affordances=[
            {
                "affordance_id": AFFORDANCE_ROUTE,
                "affordance_kind": "traversal",
                "required": True,
                "detail": (
                    "one continuous on-foot route from entry.player_start "
                    "through the corridor (Wall_Corridor_N/S/Back), the gate "
                    "(Pillar_Gate_N/S) and the plaza (Wall_Plaza_*) to "
                    "courtyard.lantern.first"),
            },
            {
                "affordance_id": AFFORDANCE_MARKERS,
                "affordance_kind": "navigation_anchor",
                "required": True,
                "detail": (
                    "wayfinding marks along that route that let a player who "
                    "has never seen the level find the lantern in the dark"),
            },
        ],
        constraints=list(_constraints()),
        # This revision adds geometry, not inhabitants. 'none' with an empty role
        # list is the coherent pair, and the policy refuses every population
        # mutation kind so the two halves cannot drift apart.
        population={"density_class": "none", "population_roles": []},
        environment={
            # Gloamstead has never measured the playable extent of the
            # first-night slice. There is no scene-survey exporter that reports
            # bounds -- the survey registry emits only the four declared
            # subjects' transforms -- so this is unknown rather than estimated.
            "extent_m2": "unknown",
            # The traversable ground is the single Ground_Plate actor with
            # vertical Wall_* / Pillar_* blockers around it.
            "relief_class": "flat",
            # The first night. Sun, SkyLight and Fog are driven by the director
            # via the Gloamstead.FirstNight.* actor tags.
            "lighting_condition": "dark",
            "resolution_owner": (
                "Gloamstead level owner -- must measure the Ground_Plate extent "
                "in the 5.8 editor and record it here"),
        },
        notes=(
            "REVISION of an existing hand-authored map, not a new world. The "
            "ask is bounded on purpose: markers along one route, and nothing "
            "else touched."),
    )


def policy():
    """Exactly the authority this ask needs, and not one mutation kind more."""
    return RP.build_revision_policy(
        policy_id=POLICY_ID,
        consumer_id=CONSUMER_ID,
        created_by="Gloamstead",
        # An ALLOW-LIST. Marker geometry may be added, taken away, or nudged.
        # Everything absent is refused -- including every mutation kind Core
        # grows after this policy was written.
        permitted_mutations=["add_geometry", "remove_geometry", "move_geometry"],
        # Redundant to the allow-list, and carried anyway so the record shows
        # Gloamstead THOUGHT about terrain, light, surfaces and inhabitants and
        # said no, rather than merely not mentioning them.
        prohibited_mutations=[
            "adjust_terrain_height",
            "adjust_lighting",
            "replace_surface_material",
            "add_population",
            "remove_population",
            "move_population",
        ],
        protected_content=list(PROTECTED_IDENTITIES),
        protected_semantics=[
            {
                "constraint_id": "ps_gloamstead_identity_untouched",
                "constraint_class": K.PROTECTED_SEMANTICS,
                "subject": "gloamstead.identity_bearing_content",
                "detail": (
                    "the VeilHeart blueprint, material and warning catalog; the "
                    "first lantern's anchor, restored post and motes; the "
                    "sanctuary bootstrap; and all five ritual data assets keep "
                    "their identity, their package paths and their bytes across "
                    "this revision"),
                "protected_ids": list(PROTECTED_IDENTITIES),
            },
            {
                "constraint_id": "ps_survey_subject_claims_untouched",
                "constraint_class": K.PROTECTED_SEMANTICS,
                "subject": "gloamstead.survey_subject_claims",
                "detail": (
                    "the four subject ids Gloamstead declares at "
                    "GloamsteadSurveySubjectRegistry.cpp:19-63 must still "
                    "resolve to the same actors after the revision. A marker "
                    "that becomes a second claimant for "
                    "courtyard.lantern.first would make the registry resolve an "
                    "ambiguity nobody authored"),
                "protected_ids": [
                    "sanctuary.heart",
                    "sanctuary.bootstrap",
                    "sanctuary.night_director",
                    "courtyard.lantern.first",
                ],
            },
        ],
        rollback={
            "rollback_required": True,
            # Lvl_Gloamstead has no OFPA/external actors -- every actor lives
            # inside the single 95KB .umap -- so the smallest thing that can
            # actually be reverted on disk is the map as a whole.
            "rollback_granularity": "whole_revision",
            "max_revision_attempts": 2,
        },
        notes=(
            "adjust_navigation is absent from the allow-list on purpose: "
            "markers that need a navmesh rebuild are markers that changed "
            "traversal, which c_landmarks_stay_reachable exists to reject."),
    )


def criteria():
    """How Gloamstead decides yes -- a fold, with a named evaluator for each."""
    keep = tuple(K.ACCEPTANCE_LOAD_BEARING) + (K.TOLERANCE,)
    return ACR.build_acceptance_criteria(
        criteria_id=CRITERIA_ID,
        consumer_id=CONSUMER_ID,
        request_id=REQUEST_ID,
        created_by="Gloamstead",
        constraints=[c for c in _constraints() if c["constraint_class"] in keep],
        evaluation_requirements=[
            {
                "constraint_id": "c_landmarks_stay_reachable",
                "evidence_kind": "runtime_observation",
                "evaluator": (
                    "UGloamsteadSurveySubjectRegistry::SurveyAndEmit "
                    "(GloamsteadSurveySubjectRegistry.h:128)"),
                "detail": (
                    "the registry resolves each declared subject in a live "
                    "world and writes its status, resolver kind and transform "
                    "to procedural/reports/gloamstead_survey_subjects/requests/"
                    "<request_id>.json (schema GloamsteadSurveyRequest/v1). "
                    "That establishes WHERE each landmark is, before and after. "
                    "It does not by itself establish that a 192x42cm capsule "
                    "can walk between them -- Gloamstead owns closing that gap, "
                    "and c_unknown_player_traversal_metrics blocks acceptance "
                    "until it does"),
            },
            {
                "constraint_id": "c_route_traversable",
                "evidence_kind": "runtime_observation",
                "evaluator": (
                    "UGloamsteadSurveySubjectRegistry::EmitReport "
                    "(GloamsteadSurveySubjectRegistry.h:136), cross-checked by "
                    "the existing automation test "
                    "Gloamstead.SurveySubject.ResolvesPlacedActorsAndRefusesToGuess "
                    "(Source/Gloamstead/Tests/SurveySubjectRegistryTests.cpp:88)"),
                "detail": (
                    "survey_subject_report.json must still resolve "
                    "courtyard.lantern.first, and the walk from "
                    "entry.player_start must still complete inside the "
                    "director's NightDurationSeconds=45 budget at "
                    "MaxWalkSpeed=500 cm/s (GloamsteadCharacter.cpp:35). "
                    "Gloamstead's automation suite is EditorContext only -- it "
                    "builds worlds with UWorld::CreateWorld and runs no PIE and "
                    "no AFunctionalTest -- so the walk itself is measured by a "
                    "probe Gloamstead must add, not by a test that exists today"),
            },
            {
                "constraint_id": "c_markers_on_route",
                "evidence_kind": "human_review",
                "evaluator": (
                    "Gloamstead level owner, reviewing the revised "
                    + SUBJECT_MAP + " in the UE 5.8 editor"),
                "detail": (
                    "a person opens the map at night and checks that the "
                    "markers sit along the corridor->gate->plaza route and read "
                    "from the entry. This is human_review and not a probe "
                    "because 'the route reads' is the thing Gloamstead actually "
                    "wants and nothing in this project measures it"),
            },
            {
                "constraint_id": "c_no_marker_in_restoration_envelope",
                "evidence_kind": "runtime_observation",
                "evaluator": (
                    "URitualPlacementComponent::IsCurrentPlacementValid "
                    "(RitualPlacementComponent.h:42, impl .cpp:558-561) and "
                    "IsPointValidForPlacement (.h:180, impl .cpp:545-556)"),
                "detail": (
                    "the game's OWN definition of 'the ritual can still be "
                    "performed here'. The verdict at courtyard.lantern.first "
                    "must be identical before and after the revision. Note "
                    "there is no placement VOLUME to test against: validity is "
                    "Distance <= RestorationRadius * 1.25 "
                    "(RitualPlacementComponent.cpp:553-555) inside a 1600cm "
                    "search radius (:495)"),
            },
            {
                "constraint_id": "c_identity_untouched",
                "evidence_kind": "authoring_time_check",
                "evaluator": (
                    "git diff --stat " + COMMIT_SHA + "..HEAD -- "
                    "Content/Gloamstead Content/Data"),
                "detail": (
                    "every one of the twelve protected packages must be "
                    "byte-identical to its state at " + COMMIT_SHA + ". A diff "
                    "touching any of them fails this criterion outright, "
                    "whatever else the revision achieved"),
            },
            {
                "constraint_id": "c_marker_instance_budget",
                "evidence_kind": "external_measurement",
                "evaluator": (
                    "count of actors added to " + SUBJECT_MAP + " by the "
                    "WorldForge revision transaction record"),
                "detail": (
                    "Lvl_Gloamstead has no external-actor files on disk, so "
                    "the count is taken from WorldForge's own transaction "
                    "record rather than from a file listing"),
            },
        ],
        must_block_ids=[
            "c_landmarks_stay_reachable",
            "c_route_traversable",
            "c_markers_on_route",
            "c_no_marker_in_restoration_envelope",
            "c_identity_untouched",
            "c_marker_instance_budget",
            "c_unknown_player_traversal_metrics",
            "c_unknown_ritual_restoration_radius",
        ],
        unknown_handling="block_and_request_measurement",
    )


# --------------------------------------------------------------------------- #
# the constraint set, stated once and read by both request() and criteria()
# --------------------------------------------------------------------------- #
def _constraints():
    """Gloamstead's statement of what must, must not, and should hold.

    One list, two readers. Stating it twice would let the request and the
    criteria drift, and the drift would be invisible: both records would still
    validate, and the world would be judged against a set nobody asked for.
    """
    return [
        # ---- HARD: the two things that must survive the revision -------------
        {
            "constraint_id": "c_landmarks_stay_reachable",
            "constraint_class": K.HARD_INVARIANT,
            "subject": "navigation.reachability.declared_landmarks",
            "detail": (
                "every landmark in this request flagged must_be_reachable -- "
                "entry.player_start, courtyard.lantern.first, sanctuary.heart -- "
                "must remain reachable ON FOOT from entry.player_start after "
                "the revision, for the capsule Gloamstead actually ships: "
                "192cm tall, 42cm radius (InitCapsuleSize(42.f, 96.0f), "
                "GloamsteadCharacter.cpp:20), walking at 500 cm/s (:35). A "
                "marker that narrows a gap the capsule previously fitted "
                "through is a regression, not a decoration"),
        },
        {
            "constraint_id": "c_route_traversable",
            "constraint_class": K.HARD_INVARIANT,
            "subject": AFFORDANCE_ROUTE + ".on_foot",
            "detail": (
                "a continuous on-foot route must connect entry.player_start to "
                "courtyard.lantern.first. This is not a preference: "
                "GloamsteadFirstNightDirector.h:83 sizes the tutorial night "
                "around the player being able 'to actually cross the plaza to "
                "the lantern they restored', and reaching the light is what "
                "resolves the objective. If the route breaks, the first night "
                "cannot be completed"),
        },
        {
            "constraint_id": "c_markers_on_route",
            "constraint_class": K.HARD_INVARIANT,
            "subject": AFFORDANCE_MARKERS + ".placed_along_route",
            "detail": (
                "at least one wayfinding marker must exist after the revision, "
                "and every marker added must lie within the corridor between "
                "entry.player_start and courtyard.lantern.first. A revision "
                "that placed markers somewhere else in the sanctuary would have "
                "answered a request Gloamstead did not make. This is what makes "
                "the required affordance " + AFFORDANCE_MARKERS + " able to "
                "fail rather than decorative"),
        },
        # ---- PROHIBITED: the outcome that would break the ritual ------------
        {
            "constraint_id": "c_no_marker_in_restoration_envelope",
            "constraint_class": K.PROHIBITED_OUTCOME,
            "subject": "ritual.placement.restoration_envelope",
            "detail": (
                "no marker may block, overlap or otherwise alter the ritual "
                "placement envelope at courtyard.lantern.first. Stated "
                "precisely, because Gloamstead has no placement VOLUME to point "
                "at: URitualPlacementComponent decides validity by distance -- "
                "Distance <= RestorationRadius * 1.25 where RestorationRadius "
                "is a per-point PCG attribute falling back to 800cm "
                "(RitualPlacementComponent.cpp:553-555) -- inside a 1600cm "
                "search radius (:495). Any marker that changes the verdict of "
                "IsCurrentPlacementValid() at that point violates this"),
        },
        # ---- PROTECTED: what is Gloamstead's and stays Gloamstead's ---------
        {
            "constraint_id": "c_identity_untouched",
            "constraint_class": K.PROTECTED_SEMANTICS,
            "subject": "gloamstead.identity_bearing_content",
            "detail": (
                "the twelve identity-bearing packages listed in protected_ids "
                "must not be modified, moved, retargeted or reauthored. These "
                "are the VeilHeart, the first lantern and the ritual data "
                "assets: the content that makes this project Gloamstead rather "
                "than a courtyard"),
            "protected_ids": list(PROTECTED_IDENTITIES),
        },
        # ---- BUDGET: a small ceiling, with the arithmetic in the open -------
        {
            "constraint_id": "c_marker_instance_budget",
            "constraint_class": K.BUDGET,
            "subject": AFFORDANCE_MARKERS + ".instance_count",
            "detail": (
                "at most 8 marker instances may be added to " + SUBJECT_MAP + ". "
                "Eight because the route has three decision points -- leaving "
                "the corridor, crossing the gate between Pillar_Gate_N and "
                "Pillar_Gate_S, and committing across the plaza -- which at two "
                "marks each is six, plus one at the lantern approach and one "
                "spare. It is also a hard upper bound for a different reason: "
                "Lvl_Gloamstead currently holds 22 authored actor labels, and "
                "markers that approached that count would stop reading as "
                "signage and start reading as level, which is the failure this "
                "request is trying to avoid"),
            "limit": 8,
            "unit": "instances",
        },
        # ---- TOLERANCE: the slack in one hard invariant's comparison --------
        {
            "constraint_id": "c_route_clearance_tolerance",
            "constraint_class": K.TOLERANCE,
            "subject": AFFORDANCE_ROUTE + ".on_foot",
            "detail": (
                "corridor width along the route may be measured with 10cm of "
                "slack against the 42cm capsule radius. Below that, greybox "
                "mesh seams and floating-point transform noise read as "
                "obstructions and would fail c_route_traversable for reasons "
                "that have nothing to do with the markers"),
            "applies_to": "c_route_traversable",
            "limit": 10.0,
            "unit": "cm",
        },
        # ---- DECLARED_UNKNOWN: what Gloamstead has not decided --------------
        {
            "constraint_id": "c_unknown_player_traversal_metrics",
            "constraint_class": K.DECLARED_UNKNOWN,
            "subject": "player.traversal_metrics",
            "detail": (
                "Gloamstead does not know its own eye height, step height, "
                "walkable slope or jump height. AGloamsteadCharacter is "
                "UCLASS(abstract) (GloamsteadCharacter.h:23-24) and its "
                "constructor sets the capsule, walk speed and JumpZVelocity but "
                "never BaseEyeHeight, MaxStepHeight or WalkableFloorAngle; "
                "grepping Source/ and Config/ for those three names returns "
                "nothing for this character. Jump height in particular is a "
                "derivation from velocity that nobody here has made. Until "
                "these are measured on the shipping Blueprint pawn, any "
                "reachability verdict WorldForge returns is a verdict about a "
                "body Gloamstead has not described, so this blocks acceptance "
                "by construction"),
            "resolution_owner": UNKNOWN_OWNER,
        },
        {
            "constraint_id": "c_unknown_ritual_restoration_radius",
            "constraint_class": K.DECLARED_UNKNOWN,
            "subject": "ritual.placement.restoration_radius",
            "detail": (
                "the restoration envelope that "
                "c_no_marker_in_restoration_envelope protects has no known "
                "size. RestorationRadius is read per-PCG-point via "
                "GetFloatAttribute(Point, \"RestorationRadius\", 800.0f) "
                "(RitualPlacementComponent.cpp:553); 800cm is the code "
                "fallback, and the value actually authored on the "
                "courtyard.lantern.first points has never been read out. "
                "WorldForge must not be told to keep clear of a number "
                "Gloamstead invented"),
            "resolution_owner": RITUAL_UNKNOWN_OWNER,
        },
        # ---- SOFT / OPTIMIZATION: incapable of failing the build ------------
        {
            "constraint_id": "c_prefer_markers_that_read_without_added_light",
            "constraint_class": K.SOFT_PREFERENCE,
            "subject": "visual.night_legibility",
            "detail": (
                "prefer markers that read at night through silhouette, "
                "placement and contrast against Ground_Plate rather than "
                "through emissive intensity. Gloamstead's policy prohibits "
                "adjust_lighting outright, so this is about how the permitted "
                "geometry is shaped, not about adding light"),
            "weight": 2.0,
        },
        {
            "constraint_id": "c_minimise_marker_count",
            "constraint_class": K.OPTIMIZATION_TARGET,
            "subject": AFFORDANCE_MARKERS + ".instance_count",
            "detail": (
                "fewer markers at equal route legibility. The budget of 8 is a "
                "ceiling Gloamstead will accept, not a target it wants hit"),
            "direction": K.MINIMIZE,
            "weight": 1.0,
        },
    ]
