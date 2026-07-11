#requires -Version 7
# Shared GloamsteadForge validation logic (Corrected Wave 3).
# Dot-source this from the validator scripts. Contains the enum sets, the scenario-matrix authority loader,
# the fail-closed semantic rule engine (Get-GFCodes), and the integrity rule engine (Get-GFIntegrityCodes).
#
# SECURITY MODEL: a report's own `quiet` / `objective_kind` are attacker-controlled and are NOT trusted as
# switches that disable substantiation. Authority for whether a scenario is quiet/objective-bearing comes
# from scenario_matrix.json (bound by scenario_id). Absent a matrix binding, validation defaults to STRICT
# (non-quiet), so a lone report cannot self-certify a benign night to skip the substantiation checks.

Set-StrictMode -Version Latest

$script:GFNightTypes    = @('Invalid','Tutorial','Corruption','Omen','Retrieval','SilencePossession','Mirror','Bargain','Fracture','TrueSiege')
$script:GFObjectiveKinds = @('None','CleanseCorruptionBloom','TutorialTeach')
$script:GFOutcomes      = @('None','Success','Partial','Failure')

function Get-GFReport {
    param([Parameter(Mandatory)][string]$Path)
    return (Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json)
}

function Get-GFScenarioMap {
    param([string]$MatrixPath)
    $map = @{}
    if ($MatrixPath -and (Test-Path -LiteralPath $MatrixPath)) {
        $m = Get-Content -Raw -LiteralPath $MatrixPath | ConvertFrom-Json
        foreach ($s in $m.scenarios) { $map[$s.scenario_id] = $s }
    }
    return $map
}

function Test-InRange01 {
    param([double]$V)
    return ($V -ge 0.0 -and $V -le 1.0)
}

# Fail-closed SEMANTIC rules over one parsed report. $Scenario (optional) is the matrix authority for
# quiet/objective-bearing. Returns an array of GF codes (empty = valid).
function Get-GFCodes {
    param(
        [Parameter(Mandatory)]$R,
        $Scenario
    )
    $codes = [System.Collections.Generic.List[string]]::new()

    $nl  = $R.night_loop
    $ss  = $R.sanctuary_state
    $dr  = $R.dawn_reflection
    $res = $R.restoration
    $sl  = $R.save_load

    # --- Authority (never trust the report's own quiet / objective self-declaration) ---
    if ($null -ne $Scenario) {
        $authQuiet      = [bool]$Scenario.quiet
        $authObjBearing = [bool]$Scenario.objective_bearing
        # The report's declared shape must match what the matrix says this scenario is.
        if ($authObjBearing -and $nl.objective_kind -eq 'None') { $codes.Add('GF043') | Out-Null }
        if ((-not $authObjBearing) -and $nl.objective_kind -ne 'None') { $codes.Add('GF043') | Out-Null }
        if ([bool]$R.quiet -ne $authQuiet) { $codes.Add('GF072') | Out-Null }
    }
    else {
        $authQuiet      = $false                              # strict: a lone report cannot self-certify quiet
        $authObjBearing = ($nl.objective_kind -ne 'None')
    }
    $isCleanse = ($nl.objective_kind -eq 'CleanseCorruptionBloom')

    # --- PCG init ---
    if ($nl.started -and -not $R.pcg_init.initialized) { $codes.Add('GF011') | Out-Null }
    if ($R.pcg_init.initialized -and $R.pcg_init.point_count -le 0) { $codes.Add('GF012') | Out-Null }
    if ($R.pcg_init.point_count -lt 0) { $codes.Add('GF013') | Out-Null }

    # --- Restoration ---
    if ($res.applied -and $res.point_index -lt 0) { $codes.Add('GF021') | Out-Null }
    if ((-not $res.attempted) -and $res.applied) { $codes.Add('GF022') | Out-Null }

    # --- Enum sanity (belt-and-suspenders with the schema) ---
    if ($nl.night_type -notin $script:GFNightTypes) { $codes.Add('GF032') | Out-Null }
    if ($nl.objective_kind -notin $script:GFObjectiveKinds) { $codes.Add('GF033') | Out-Null }
    if ($nl.outcome_result -notin $script:GFOutcomes) { $codes.Add('GF034') | Out-Null }

    # --- Started vs outcome / intentional end / tag ---
    if ((-not $nl.started) -and $nl.outcome_result -ne 'None') { $codes.Add('GF031') | Out-Null }
    if (-not $nl.ended_intentionally) { $codes.Add('GF038') | Out-Null }
    if ($nl.outcome_result -ne 'None' -and [string]::IsNullOrWhiteSpace([string]$nl.result_tag)) { $codes.Add('GF039') | Out-Null }

    # Intrinsic anti-fake: a night with NO objective applies no pressure -> it cannot mutate the sanctuary.
    if ($nl.objective_kind -eq 'None' -and $nl.started -and $ss.mutated) { $codes.Add('GF043') | Out-Null }

    # --- Objective resolution consistency (authoritative objective-bearing) ---
    if ($authObjBearing) {
        if ($nl.outcome_result -eq 'Success' -and -not $nl.objective_resolved) { $codes.Add('GF035') | Out-Null }
        if (($nl.outcome_result -in @('Partial','Failure')) -and $nl.objective_resolved) { $codes.Add('GF036') | Out-Null }
    }

    # --- Cleanse-specific substantiation ---
    if ($isCleanse) {
        if ($nl.target_point_index -lt 0) { $codes.Add('GF037') | Out-Null }
        if ($nl.outcome_result -eq 'Success' -and -not $res.applied) { $codes.Add('GF023') | Out-Null }
    }

    # The cleanse objective only exists on Corruption nights (closes the "Omen + cleanse, free-form tag" dodge).
    if ($isCleanse -and $nl.night_type -ne 'Corruption') { $codes.Add('GF043') | Out-Null }

    # --- Cleanse result-tag correctness (enforced for any cleanse objective, non-quiet) ---
    if ($isCleanse -and -not $authQuiet) {
        if ($nl.outcome_result -eq 'Success' -and $nl.result_tag -ne 'CorruptionCleansed') { $codes.Add('GF040') | Out-Null }
        if ($nl.outcome_result -eq 'Partial' -and $nl.result_tag -ne 'CorruptionLingers') { $codes.Add('GF041') | Out-Null }
        if ($nl.outcome_result -eq 'Failure' -and $nl.result_tag -ne 'CorruptionScar')  { $codes.Add('GF042') | Out-Null }
    }

    # --- Sanctuary mutation (authoritative quiet) ---
    if ((-not $authQuiet) -and $nl.started -and (-not $ss.mutated)) { $codes.Add('GF046') | Out-Null }
    if ($isCleanse -and $nl.outcome_result -eq 'Success' -and $ss.target_corruption_after -ge $ss.target_corruption_before) { $codes.Add('GF047') | Out-Null }
    if ($isCleanse -and $nl.outcome_result -eq 'Partial' -and $ss.target_corruption_after -ge $ss.target_corruption_before) { $codes.Add('GF048') | Out-Null }
    if ($isCleanse -and $nl.outcome_result -eq 'Failure' -and $ss.target_corruption_after -lt $ss.target_corruption_before) { $codes.Add('GF049') | Out-Null }
    foreach ($v in @($ss.avg_corruption_before,$ss.avg_corruption_after,$ss.target_corruption_before,$ss.target_corruption_after)) {
        if (-not (Test-InRange01 ([double]$v))) { $codes.Add('GF050') | Out-Null }
    }

    # --- Dawn reflection ---
    if (-not $dr.consumed_outcome) { $codes.Add('GF056') | Out-Null }
    if ($dr.outcome_result -ne $nl.outcome_result) { $codes.Add('GF057') | Out-Null }

    # --- Save/load ---
    if ([bool]$R.continuity -and -not $sl.checked) { $codes.Add('GF060') | Out-Null }
    if ($sl.checked -and -not $sl.roundtrip_ok) { $codes.Add('GF061') | Out-Null }

    # --- Success self-consistency / human playtest ---
    if ($nl.outcome_result -eq 'Success' -and @($R.failure_codes).Count -gt 0) { $codes.Add('GF080') | Out-Null }
    if ([bool]$R.human_playtest) { $codes.Add('GF078') | Out-Null }

    return $codes.ToArray()
}

# Integrity rules (freshness / provenance). Returns GF codes. ExpectedCommit defaults to repo HEAD.
function Get-GFIntegrityCodes {
    param(
        [Parameter(Mandatory)]$R,
        [string]$ExpectedCommit
    )
    $codes = [System.Collections.Generic.List[string]]::new()

    if (-not $ExpectedCommit) {
        $ExpectedCommit = (git rev-parse HEAD 2>$null)
        if ($ExpectedCommit) { $ExpectedCommit = $ExpectedCommit.Trim() }
    }

    if ($ExpectedCommit -and $R.git_commit -ne $ExpectedCommit) { $codes.Add('GF065') | Out-Null }
    if ([string]::IsNullOrWhiteSpace([string]$R.git_branch)) { $codes.Add('GF066') | Out-Null }

    $parsed = [datetime]::MinValue
    if (-not [datetime]::TryParse([string]$R.generated_at_utc, [ref]$parsed)) {
        $codes.Add('GF067') | Out-Null
    } elseif ($parsed -gt (Get-Date).ToUniversalTime().AddMinutes(5) -or $parsed.Year -le 1970) {
        $codes.Add('GF067') | Out-Null
    }

    return $codes.ToArray()
}
