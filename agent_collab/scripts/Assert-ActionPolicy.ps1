#requires -Version 7.0
<#
.SYNOPSIS
  Decide whether the autonomous Orchestrator may take a proposed action, must ask the human first,
  or is forbidden -- enforcing the reversibility law from autonomy_policy.json.
.DESCRIPTION
  This is the gate that moves "is this next step safe?" out of model judgment and into a
  deterministic check. The law: the Orchestrator may take any action whose effect is LOCALLY
  REVERTIBLE, must stop-and-ask for anything that is not, and is hard-denied anything the policy
  forbids. Reversibility is determined HERE from the action type -- it is never asserted by the
  caller, so an untrusted runtime cannot label its way past the gate.

  Built-in classifications (policy may extend each list, never shrink the irreversible set):
    reversible   : local_commit, create_branch, create_worktree, remove_worktree, edit_file,
                   write_file, run_tests, run_build, run_lint, read, glob, grep,
                   merge_candidate_local, promote_candidate_local, move_handoff, write_state,
                   write_projection, normalize_result, plan, research
    ask_first    : dependency_install, schema_migration, db_migration, auth_change, security_change,
                   payment_change, trading_execution_change, file_delete, dir_delete, ci_cd_change,
                   secrets_change, env_change, broad_refactor
    never_without_human : remote_push, git_push, pr_create, pr_merge, deploy, publish, release,
                   external_purchase, external_message, external_api_sideeffect, image_generation,
                   send_email, post_webhook

  Decision order (first match wins):
    1. action in policy.forbidden_actions      -> deny
    2. autonomy_level == 0 (manual)             -> ask  (human approves every step)
    3. action in never_without_human            -> ask
    4. -Command matches an irreversible pattern -> ask  (catches a mislabeled action)
    5. risk_level == high                        -> ask
    6. action in ask_first                       -> ask
    7. action in reversible                      -> allow
    8. unknown action (reversibility unknown)    -> ask  (fail safe)
.PARAMETER ActionType
  The proposed action's type (controlled vocab above; unknown types fail safe to ask).
.PARAMETER RiskLevel
  low | medium | high. Default low.
.PARAMETER Command
  Optional raw command line; scanned against irreversible patterns to catch mislabeling.
.PARAMETER Policy
  Path to autonomy_policy.json. Default ../context/autonomy_policy.json relative to this script.
.OUTPUTS
  A decision object on stdout. Exit 0 = allow, 5 = ask (halt for human), 6 = deny (forbidden),
  2 = error.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)] [string] $ActionType,
  [ValidateSet('low','medium','high')] [string] $RiskLevel = 'low',
  [string] $Command,
  [string] $Policy
)

$ErrorActionPreference = 'Stop'

function Fail([string]$msg) { [Console]::Error.WriteLine("ERROR: $msg"); exit 2 }

try {
  if (-not $Policy) {
    $scriptDir = Split-Path -Parent $PSCommandPath
    $Policy = Join-Path $scriptDir '..\context\autonomy_policy.json'
  }

  # Policy is optional; absence means conservative defaults (level 1, no extra allow-lists).
  $policyObj = $null
  if (Test-Path -LiteralPath $Policy) {
    $policyObj = Get-Content -LiteralPath $Policy -Raw | ConvertFrom-Json
  }

  function PolicyList([string]$name) {
    if ($policyObj -and ($policyObj.PSObject.Properties.Name -contains $name) -and $policyObj.$name) {
      return @($policyObj.$name)
    }
    return @()
  }

  $level = 1
  if ($policyObj -and ($policyObj.PSObject.Properties.Name -contains 'autonomy_level')) {
    $level = [int]$policyObj.autonomy_level
  }

  $reversible = @(
    'local_commit','create_branch','create_worktree','remove_worktree','edit_file','write_file',
    'run_tests','run_build','run_lint','read','glob','grep','merge_candidate_local',
    'promote_candidate_local','move_handoff','write_state','write_projection','normalize_result',
    'plan','research'
  )
  $askFirst = @(
    'dependency_install','schema_migration','db_migration','auth_change','security_change',
    'payment_change','trading_execution_change','file_delete','dir_delete','ci_cd_change',
    'secrets_change','env_change','broad_refactor'
  ) + (PolicyList 'ask_first')
  $never = @(
    'remote_push','git_push','pr_create','pr_merge','deploy','publish','release',
    'external_purchase','external_message','external_api_sideeffect','image_generation',
    'send_email','post_webhook'
  ) + (PolicyList 'never_without_human')
  $forbidden = PolicyList 'forbidden_actions'

  $irrevPatterns = @(
    'git\s+push','git\s+remote\s+(add|set-url)','--force\b','-f\b.*push','git\s+reset\s+--hard',
    'git\s+rebase','git\s+commit\s+--amend','filter-branch','filter-repo',
    'rm\s+-rf','Remove-Item.*-Recurse','\b(npm|pnpm|yarn)\s+(install|add)\b','\bpip\s+install\b',
    '\bdotnet\s+add\b','\bdeploy\b','\bpublish\b','curl\s+.*\|\s*(sh|bash)','Invoke-WebRequest.*\|.*Invoke-Expression'
  ) + (PolicyList 'irreversible_command_patterns')

  $at = $ActionType.ToLowerInvariant()

  function Decide([string]$decision, [int]$code, [string]$reason) {
    [pscustomobject]@{
      decision       = $decision
      action_type    = $at
      risk_level     = $RiskLevel
      autonomy_level = $level
      requires_human = ($decision -ne 'allow')
      reason         = $reason
    } | ConvertTo-Json -Depth 4
    exit $code
  }

  if ($forbidden -contains $at)                         { Decide 'deny'  6 "action '$at' is in forbidden_actions" }
  if ($level -le 0)                                     { Decide 'ask'   5 "autonomy_level 0 (manual): every action requires human approval" }
  if ($never -contains $at)                             { Decide 'ask'   5 "action '$at' is never permitted without human approval (irreversible/external side effect)" }
  if ($Command) {
    foreach ($pat in $irrevPatterns) {
      if ($Command -match $pat) { Decide 'ask' 5 "command matches irreversible pattern /$pat/ : $Command" }
    }
  }
  if ($RiskLevel -eq 'high')                            { Decide 'ask'   5 "risk_level is high; halting for human review" }
  if ($askFirst -contains $at)                          { Decide 'ask'   5 "action '$at' is on the ask-first list" }
  if ($reversible -contains $at)                        { Decide 'allow' 0 "action '$at' is locally reversible and within autonomy level $level" }

  Decide 'ask' 5 "action '$at' has unknown reversibility; failing safe to human approval"
}
catch {
  Fail $_.Exception.Message
}
