#requires -Version 7
# Hostile negative suite: every known-bad fixture MUST be rejected by the expected rule. If a fake report
# passes, the validation layer is broken -> this script fails closed.
[CmdletBinding()]
param(
    [string]$FixturesDir,
    [switch]$Strict
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GloamsteadForge.Common.ps1')
$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $FixturesDir) { $FixturesDir = Join-Path $RepoRoot 'specs/gloamsteadforge/fixtures/bad' }
$Head = (git -C $RepoRoot rev-parse HEAD 2>$null); if ($Head) { $Head = $Head.Trim() }
# Fakes that claim a real matrix slot are bound to that scenario's authority (quiet / objective-bearing).
$ScenarioMap = Get-GFScenarioMap -MatrixPath (Join-Path $RepoRoot 'specs/gloamsteadforge/scenario_matrix.json')

# fixture basename -> expected rejection { code, kind }
$expect = @{
    'pcg_init_zero_points'         = @{ code = 'GF012'; kind = 'semantic'  }
    'restoration_invalid_index'    = @{ code = 'GF021'; kind = 'semantic'  }
    'success_without_cleanse'      = @{ code = 'GF023'; kind = 'semantic'  }
    'success_objective_unresolved' = @{ code = 'GF035'; kind = 'semantic'  }
    'success_no_target'            = @{ code = 'GF037'; kind = 'semantic'  }
    'dawn_not_consumed'            = @{ code = 'GF056'; kind = 'semantic'  }
    'state_not_mutated'            = @{ code = 'GF046'; kind = 'semantic'  }
    'success_no_reduction'         = @{ code = 'GF047'; kind = 'semantic'  }
    'failure_but_improved'         = @{ code = 'GF049'; kind = 'semantic'  }
    'unknown_night_type'           = @{ code = 'GF032'; kind = 'semantic'  }
    'success_with_failure_codes'   = @{ code = 'GF080'; kind = 'semantic'  }
    'saveload_not_roundtrip'       = @{ code = 'GF061'; kind = 'semantic'  }
    'dawn_outcome_mismatch'        = @{ code = 'GF057'; kind = 'semantic'  }
    'stale_commit'                 = @{ code = 'GF065'; kind = 'integrity' }
    # Matrix-slot fakes: claim a real scenario_id but violate its declared shape (self-declared quiet /
    # objective_kind:None to try to skip substantiation). Caught only because authority = the matrix.
    'matrix_objective_none_success' = @{ code = 'GF043'; kind = 'semantic' }
    'matrix_fake_quiet'             = @{ code = 'GF072'; kind = 'semantic' }
}

$files = @(Get-ChildItem -LiteralPath $FixturesDir -Filter *.json -File)
if ($files.Count -eq 0) { Write-Host "NEGATIVES: no bad fixtures under $FixturesDir" -ForegroundColor Yellow; exit 1 }

$fail = 0
foreach ($f in $files) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($f.Name)
    $r = Get-GFReport -Path $f.FullName
    $sc = $ScenarioMap[$r.scenario_id]   # non-null only for fakes that claim a real matrix slot

    $codes = @(Get-GFCodes -R $r -Scenario $sc)
    $intCodes = @(Get-GFIntegrityCodes -R $r -ExpectedCommit $Head)

    $exp = $expect[$name]
    if ($null -eq $exp) {
        # Unmapped bad fixture: require rejection by SOME validator.
        if ($codes.Count -eq 0 -and $intCodes.Count -eq 0) { $fail++; Write-Host "  LEAK (unmapped, accepted): $name" -ForegroundColor Red }
        else { Write-Host "  REJECT (unmapped): $name -> $(@($codes + $intCodes) -join ',')" -ForegroundColor Green }
        continue
    }

    $pool = if ($exp.kind -eq 'integrity') { $intCodes } else { $codes }
    if ($exp.code -in $pool) {
        Write-Host "  REJECT: $name -> $($exp.code) [$($exp.kind)]" -ForegroundColor Green
    }
    else {
        $fail++
        Write-Host "  LEAK: $name expected $($exp.code) [$($exp.kind)] but got semantic=[$($codes -join ',')] integrity=[$($intCodes -join ',')]" -ForegroundColor Red
    }
}
Write-Host "NEGATIVES: $($files.Count - $fail)/$($files.Count) bad fixtures correctly rejected"
if ($fail -gt 0) { exit 1 }  # negatives always fail closed
exit 0
