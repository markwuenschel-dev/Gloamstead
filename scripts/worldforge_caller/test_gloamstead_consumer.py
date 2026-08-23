#!/usr/bin/env python3
"""Assert that Gloamstead's WorldForge request is valid AND honest.

    cd "D:/Unreal Projects/Gloamstead5_8"
    PYTHONUTF8=1 python scripts/worldforge_caller/test_gloamstead_consumer.py

Exits non-zero on any failure.

WHY THE NEGATIVE CONTROLS ARE NOT OPTIONAL
------------------------------------------
Every positive assertion below is of the form "this validator returned no
failures". A validator that returned no failures because it was never really
looking would produce exactly the same green. So each rail this file cares about
is also fired DELIBERATELY against a deliberately-broken copy of the record, and
the test fails if the breakage is not caught. A harness that cannot go red has
not established that its green means anything.
"""

import copy
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import gloamstead_consumer as GC                          # noqa: E402

from consumers import adapter as ADP                      # noqa: E402
from wfcore import constraints as K                       # noqa: E402
from wfcore import tri                                    # noqa: E402
from wfcore.contracts import acceptance_criteria as ACR   # noqa: E402
from wfcore.contracts import asset_catalog as AC          # noqa: E402
from wfcore.contracts import consumer_profile as CP       # noqa: E402
from wfcore.contracts import revision_policy as RP        # noqa: E402
from wfcore.contracts import world_request as WR          # noqa: E402

_FAILURES = []
_PASSED = 0


def _check(name, ok, detail=""):
    global _PASSED
    if ok:
        _PASSED += 1
        print("  [ ok ] {}".format(name))
    else:
        _FAILURES.append((name, detail))
        print("  [FAIL] {}  {}".format(name, detail))


def _failing(checks):
    return [(n, d, c) for (n, ok, d, c) in checks if not ok]


def _validates(name, checks):
    bad = _failing(checks)
    _check(name, not bad,
           "" if not bad else "; ".join(
               "{} :: {}".format(n, d) for (n, d, _c) in bad[:4]))


def _rejects(name, checks, expect_check_substr):
    """A negative control: the named rail MUST be among the failures."""
    bad = _failing(checks)
    hit = [n for (n, _d, _c) in bad if expect_check_substr in n]
    _check(name, bool(hit),
           "expected a failing check containing {!r}; got {} failure(s): {}"
           .format(expect_check_substr, len(bad),
                   [n for (n, _d, _c) in bad[:6]]))


def _source_text():
    with open(GC.__file__, "r", encoding="utf-8") as fh:
        return fh.read()


# --------------------------------------------------------------------------- #
def main():
    adapter = GC.adapter()
    profile = GC.profile()
    catalog = GC.catalog()
    request = GC.request()
    policy = GC.policy()
    criteria = GC.criteria()

    print("\n-- 1. all six records validate STRICT against WorldForge --")
    _validates("adapter", ADP.validate_adapter(adapter, strict=True))
    _validates("consumer_profile",
               CP.validate_consumer_profile(profile, strict=True))
    _validates("asset_catalog", AC.validate_asset_catalog(catalog, strict=True))
    _validates("world_request", WR.validate_world_request(request, strict=True))
    _validates("revision_policy",
               RP.validate_revision_policy(policy, strict=True))
    _validates("acceptance_criteria",
               ACR.validate_acceptance_criteria(criteria, strict=True))
    _validates("constraint_set",
               K.validate_constraint_set(request["constraints"], strict=True))

    print("\n-- 2. provenance: a real caller asked, and says which one --")
    prov = adapter["provenance"]
    _check("origination is caller_originated",
           prov["origination"] == ADP.ORIGINATION_CALLER, repr(prov["origination"]))
    _check("ADP.is_caller_originated() is True",
           ADP.is_caller_originated(adapter) is True)
    _check("caller_provenance_verdict is SATISFIED",
           ADP.caller_provenance_verdict(adapter) == tri.SATISFIED,
           ADP.caller_provenance_verdict(adapter))
    _validates("a run labelled caller_originated is permitted",
               ADP.validate_run_provenance(adapter, ADP.ORIGINATION_CALLER))
    stmt = prov["statement"].lower()
    _check("statement names Gloamstead as the origin", "gloamstead" in stmt)
    _check("statement names the repository", GC.REPOSITORY.lower() in stmt)
    _check("statement carries the real commit sha", GC.COMMIT_SHA in prov["statement"])
    _check("authored_by names Gloamstead", "Gloamstead" in prov["authored_by"])
    _check("this is NOT the WorldForge demo value",
           prov["origination"] != ADP.ORIGINATION_WORLDFORGE_DEMO)
    _check("the demo statement was not copied",
           "demonstration" not in stmt)

    print("\n-- 3. the adapter carries no generation logic (record + SOURCE) --")
    src = _source_text()
    _validates("no-generation-logic scan over this module's own source",
               ADP.validate_adapter_has_no_generation_logic(
                   adapter, src, module_name="worldforge_caller.gloamstead_consumer"))
    scan = ADP.scan_source_for_generation_logic(src, "gloamstead_consumer")
    _check("source parsed", scan["parsed"] is True, scan["parse_error"] or "")
    _check("zero forbidden Core imports", not scan["forbidden_imports"],
           str(scan["forbidden_imports"]))
    _check("zero generative definitions", not scan["generative_definitions"],
           str(scan["generative_definitions"]))
    _check("record carries no GENERATION_LOGIC_FIELDS",
           not [f for f in ADP.GENERATION_LOGIC_FIELDS if f in adapter])

    print("\n-- 4. every unknown measure names who resolves it --")
    unknown_metrics = []
    for group, fields in (("player_metrics", CP.PLAYER_METRIC_FIELDS),
                          ("camera_metrics", CP.CAMERA_METRIC_FIELDS)):
        for fld in fields:
            if profile[group].get(fld) == "unknown":
                unknown_metrics.append("{}.{}".format(group, fld))
    # The exact set, not merely "some". These seven are the ones no text-readable
    # source in Gloamstead authors; anything MORE unknown would mean a real
    # measurement got dropped, and anything less would mean one got invented.
    EXPECTED_UNKNOWN = [
        "camera_metrics.far_clip_cm",
        "camera_metrics.horizontal_fov_deg",
        "camera_metrics.near_clip_cm",
        "player_metrics.eye_height_cm",
        "player_metrics.max_jump_height_cm",
        "player_metrics.max_step_height_cm",
        "player_metrics.max_walk_slope_deg",
    ]
    _check("exactly the genuinely-unmeasured fields are declared unknown",
           sorted(unknown_metrics) == EXPECTED_UNKNOWN,
           str(sorted(unknown_metrics)))
    owner = profile.get("unknown_resolution_owner")
    _check("profile names an unknown_resolution_owner",
           isinstance(owner, str) and bool(owner.strip()), repr(owner))
    _check("the measured metrics really are measured, not zeroed",
           profile["player_metrics"]["capsule_height_cm"] == 192.0
           and profile["player_metrics"]["capsule_radius_cm"] == 42.0)
    _check("adapter and profile agree on the metrics",
           adapter["player_metrics"] == profile["player_metrics"]
           and adapter["camera_metrics"] == profile["camera_metrics"])

    for c in request["constraints"]:
        if c["constraint_class"] == K.DECLARED_UNKNOWN:
            ro = c.get("resolution_owner")
            _check("DECLARED_UNKNOWN {} names a resolution_owner".format(
                c["constraint_id"]),
                isinstance(ro, str) and bool(ro.strip()), repr(ro))
    _check("environment unknown names its owner",
           request["environment"]["extent_m2"] == "unknown"
           and bool(request["environment"].get("resolution_owner")))

    print("\n-- 5. protected identities appear in BOTH policy and a constraint --")
    ps = [c for c in request["constraints"]
          if c["constraint_class"] == K.PROTECTED_SEMANTICS]
    _check("request carries a PROTECTED_SEMANTICS constraint", len(ps) >= 1)
    in_constraint = set()
    for c in ps:
        in_constraint |= set(c.get("protected_ids") or [])
    in_policy = set(policy["protected_content"])
    expected = set(GC.PROTECTED_IDENTITIES)
    _check("policy.protected_content covers every protected identity",
           expected <= in_policy, str(sorted(expected - in_policy)))
    _check("a PROTECTED_SEMANTICS constraint covers every protected identity",
           expected <= in_constraint, str(sorted(expected - in_constraint)))
    _check("adapter.protected_identities covers them too",
           expected <= set(adapter["protected_identities"]))
    _check("every policy protected_semantics member carries the right class",
           all(c["constraint_class"] == K.PROTECTED_SEMANTICS
               for c in policy["protected_semantics"]))

    print("\n-- 6. the taxonomy is used as a taxonomy --")
    by_class = {}
    for c in request["constraints"]:
        by_class[c["constraint_class"]] = by_class.get(c["constraint_class"], 0) + 1
    for klass in (K.HARD_INVARIANT, K.PROHIBITED_OUTCOME, K.PROTECTED_SEMANTICS,
                  K.BUDGET, K.TOLERANCE, K.DECLARED_UNKNOWN,
                  K.SOFT_PREFERENCE, K.OPTIMIZATION_TARGET):
        _check("request declares at least one {}".format(klass),
               by_class.get(klass, 0) >= 1, str(by_class))
    _check("soft/optimization constraints cannot block acceptance",
           not any(K.is_acceptance_load_bearing(c) for c in request["constraints"]
                   if c["constraint_class"] in K.SCORING_CLASSES))
    load_bearing = [c for c in request["constraints"]
                    if K.is_acceptance_load_bearing(c)]
    _check("pre-observation fold is UNKNOWN, never SATISFIED",
           K.fold_acceptance([(c, tri.UNKNOWN) for c in load_bearing])
           == tri.UNKNOWN)

    print("\n-- 7. the revision is bounded to what it actually needs --")
    _check("request_kind is revision", request["request_kind"] == WR.REVISION)
    _check("revision names its target",
           request["revision_target"] == GC.SUBJECT_MAP)
    _check("revision names its policy",
           request["revision_policy_id"] == policy["policy_id"])
    _check("subject is the real map from DefaultEngine.ini",
           request["subject"] == "/Game/Maps/Lvl_Gloamstead")
    _check("only marker geometry mutation is permitted",
           sorted(policy["permitted_mutations"])
           == ["add_geometry", "move_geometry", "remove_geometry"],
           str(policy["permitted_mutations"]))
    for kind in ("adjust_terrain_height", "adjust_lighting",
                 "replace_surface_material"):
        _check("{} is refused".format(kind),
               RP.mutation_verdict(policy, kind) == tri.VIOLATED)
    _check("add_geometry is permitted",
           RP.mutation_verdict(policy, "add_geometry") == tri.SATISFIED)
    _check("rollback is required with a real granularity",
           policy["rollback"]["rollback_required"] is True
           and policy["rollback"]["rollback_granularity"] != "none")

    print("\n-- 8. required affordances can actually fail --")
    lb_subjects = [str(c.get("subject", "")) for c in request["constraints"]
                   if K.is_acceptance_load_bearing(c)]
    for af in request["gameplay_affordances"]:
        if af["required"] is not True:
            continue
        aid = af["affordance_id"]
        _check("required affordance {} is a load-bearing subject".format(aid),
               any(aid == s or aid in s for s in lb_subjects), str(lb_subjects))

    print("\n-- 9. acceptance criteria bind to real, non-fictional hooks --")
    lb_ids = {c["constraint_id"] for c in criteria["constraints"]
              if K.is_acceptance_load_bearing(c)
              and c["constraint_class"] != K.DECLARED_UNKNOWN}
    evaluated = {r["constraint_id"] for r in criteria["evaluation_requirements"]}
    _check("every load-bearing non-unknown constraint has an evaluator",
           lb_ids <= evaluated, str(sorted(lb_ids - evaluated)))
    _check("every evaluation requirement names a real evaluator string",
           all(isinstance(r.get("evaluator"), str) and r["evaluator"].strip()
               for r in criteria["evaluation_requirements"]))
    hook_text = " ".join(
        str(r.get("evaluator", "")) + " " + str(r.get("detail", ""))
        for r in criteria["evaluation_requirements"])
    for real_hook in ("UGloamsteadSurveySubjectRegistry",
                      "URitualPlacementComponent::IsCurrentPlacementValid",
                      "SurveySubjectRegistryTests.cpp"):
        _check("criteria name the real hook {}".format(real_hook),
               real_hook in hook_text)
    _check("criteria request_id binds to this request",
           criteria["request_id"] == request["request_id"])
    _check("unknown_handling blocks",
           criteria["unknown_handling"] in ACR.UNKNOWN_HANDLINGS)
    _check("must_block_ids are all load-bearing",
           all(K.is_acceptance_load_bearing(c)
               for c in criteria["constraints"]
               if c["constraint_id"] in criteria["must_block_ids"]))
    verdict, blockers = ACR.acceptance_verdict(criteria, {})
    _check("with nothing observed, acceptance is UNKNOWN",
           verdict == tri.UNKNOWN and not tri.accepts(verdict))
    _check("the two declared unknowns are among the blockers",
           {"c_unknown_player_traversal_metrics",
            "c_unknown_ritual_restoration_radius"}
           <= {b["constraint_id"] for b in blockers})

    print("\n-- 10. the catalog claims no approval it did not get --")
    _check("catalog is a closed world", catalog["closed_world"] is True)
    approved = [e["asset_id"] for e in catalog["entries"]
                if e["authorization"] == AC.APPROVED]
    _check("no entry claims bare 'approved'", not approved, str(approved))
    for e in catalog["entries"]:
        if e["authorization"] == AC.APPROVED_WITH_CONDITIONS:
            _check("{} states its conditions".format(e["asset_id"]),
                   bool(e.get("conditions")))
        if e["authorization"] == AC.DENIED:
            _check("{} states its denial reason".format(e["asset_id"]),
                   bool(e.get("denial_reason")))
    _check("an unlisted asset is refused, not merely unknown",
           AC.authorization_of(catalog, "/Game/Nope/SM_Invented") == tri.VIOLATED)
    _check("the lantern motes are explicitly denied",
           AC.authorization_of(catalog, "/Game/Gloamstead/VFX/NS_LanternMotes")
           == tri.VIOLATED)
    _check("request's catalog_id resolves to this catalog",
           request["catalog_id"] == catalog["catalog_id"]
           and catalog["catalog_id"] in adapter["approved_catalog_ids"])

    print("\n-- 11. NEGATIVE CONTROLS: the harness can go red --")
    bad = copy.deepcopy(adapter)
    bad["provenance"]["origination"] = ADP.ORIGINATION_WORLDFORGE_DEMO
    _check("a demo-labelled adapter is not caller-originated",
           ADP.is_caller_originated(bad) is False)
    _rejects("upgrading a demo run to caller_originated is refused",
             ADP.validate_run_provenance(bad, ADP.ORIGINATION_CALLER),
             "no_upgrade_to_caller_originated")

    bad = copy.deepcopy(adapter)
    bad["placement_algorithm"] = {"kind": "poisson"}
    _rejects("a generation-logic field in the record is caught",
             ADP.validate_adapter_has_no_generation_logic(bad, src, "x"),
             "record_carries_no_generation_logic")

    _rejects("a generative import in the source is caught",
             ADP.validate_adapter_has_no_generation_logic(
                 adapter,
                 "from wfcore.planning import plan\n"
                 "def scatter_markers():\n    return 1\n", "x"),
             "imports_no_generation_machinery")
    _rejects("a generative def in the source is caught",
             ADP.validate_adapter_has_no_generation_logic(
                 adapter, "def place_markers():\n    return 1\n", "x"),
             "defines_no_generation_functions")
    _rejects("unscanned source is reported as NOT CHECKED, not as clean",
             ADP.validate_adapter_has_no_generation_logic(adapter, None, "x"),
             "source_scanned")

    bad = copy.deepcopy(profile)
    del bad["unknown_resolution_owner"]
    _rejects("an unknown metric with no owner is caught",
             CP.validate_consumer_profile(bad, strict=True),
             "unknown_metric_names_resolution_owner")

    bad = copy.deepcopy(profile)
    bad["player_metrics"]["max_step_height_cm"] = 0
    _rejects("a zero standing in for an unknown is caught",
             CP.validate_consumer_profile(bad, strict=True),
             "max_step_height_cm_measure")

    # The markers affordance is named by TWO load-bearing constraints -- the
    # HARD_INVARIANT that markers exist on the route, and the BUDGET that caps
    # them. Only the first can fail on ABSENCE (a budget of 8 is satisfied by
    # zero markers), so the invariant is what stops 'required' being decoration.
    # The rail's substring test is satisfied by either, so the control removes
    # both to show the rail really does fire.
    bad = copy.deepcopy(request)
    bad["constraints"] = [
        c for c in bad["constraints"]
        if not (K.is_acceptance_load_bearing(c)
                and GC.AFFORDANCE_MARKERS in str(c.get("subject", "")))]
    _rejects("an unbacked required affordance is caught",
             WR.validate_world_request(bad, strict=True),
             "required_affordance_is_load_bearing")
    _check("only the markers invariant can fail on ABSENCE of markers",
           [c["constraint_id"] for c in request["constraints"]
            if K.is_acceptance_load_bearing(c)
            and GC.AFFORDANCE_MARKERS in str(c.get("subject", ""))]
           == ["c_markers_on_route", "c_marker_instance_budget"])

    bad = copy.deepcopy(request)
    del bad["revision_target"]
    _rejects("a revision that names no target is caught",
             WR.validate_world_request(bad, strict=True),
             "revision_names_target")

    bad = copy.deepcopy(policy)
    bad["protected_semantics"][0]["constraint_class"] = K.SOFT_PREFERENCE
    _rejects("protection demoted to a preference is caught",
             RP.validate_revision_policy(bad, strict=True),
             "protected_semantics_carry_protected_class")

    bad = copy.deepcopy(policy)
    bad["permitted_mutations"] = bad["permitted_mutations"] + ["adjust_lighting"]
    _rejects("permitting and prohibiting the same kind is caught",
             RP.validate_revision_policy(bad, strict=True),
             "permit_prohibit_disjoint")

    bad = copy.deepcopy(criteria)
    bad["evaluation_requirements"] = [
        r for r in bad["evaluation_requirements"]
        if r["constraint_id"] != "c_route_traversable"]
    _rejects("a load-bearing constraint with no evaluator is caught",
             ACR.validate_acceptance_criteria(bad, strict=True),
             "every_load_bearing_constraint_is_evaluable")

    bad = copy.deepcopy(criteria)
    bad["constraints"] = bad["constraints"] + [{
        "constraint_id": "c_fake_gate",
        "constraint_class": K.SOFT_PREFERENCE,
        "subject": "visual.night_legibility",
        "detail": "a preference the consumer wrongly believes is a gate",
        "weight": 1.0,
    }]
    bad["must_block_ids"] = list(bad["must_block_ids"]) + ["c_fake_gate"]
    _rejects("a soft preference listed as a gate is caught",
             ACR.validate_acceptance_criteria(bad, strict=True),
             "must_block_ids_are_load_bearing")

    bad = copy.deepcopy(catalog)
    bad["entries"][0]["authorization"] = AC.APPROVED_WITH_CONDITIONS
    bad["entries"][0]["conditions"] = []
    _rejects("a conditional approval with no conditions is caught",
             AC.validate_asset_catalog(bad, strict=True),
             "conditional_approval_states_conditions")

    bad = copy.deepcopy(catalog)
    bad["closed_world"] = False
    _rejects("an open catalog is refused",
             AC.validate_asset_catalog(bad, strict=True),
             "catalog_is_closed_world")

    print("\n" + "=" * 72)
    print("passed {} check(s), {} failure(s)".format(_PASSED, len(_FAILURES)))
    if _FAILURES:
        print("\nFAILURES:")
        for (n, d) in _FAILURES:
            print("  - {}: {}".format(n, d))
        return 1
    print("Gloamstead's caller-originated request is valid and honest.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
