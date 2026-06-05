#Requires -Version 7.0
<#
.SYNOPSIS
    Create (or reuse) an isolated git worktree + task branch for one agent-collab task.

.DESCRIPTION
    Controller-side primitive for the agent-collab system. Creates a task branch
    'agent-collab/<slug>/task/<task_id>' off the work-branch HEAD (the baseRef:head intent,
    so the worktree carries already-integrated local commits) and checks it out into an
    isolated worktree under <WorktreeRoot>/<task_id>.

    This script has ONE job: produce the worktree and report the facts. It deliberately does
    NOT write task_state.json, scheduler_state.json, or any handoff/outbox file. The
    Orchestrator is the single writer of durable state; it consumes this script's JSON output
    and records base_commit / branch / worktree_path itself.

    This is for worktrees the controller creates (e.g. to apply a patch returned by a
    file-less runtime, or to pre-stage a coder tree). Claude Code subagents launched with
    'isolation: worktree' manage their own worktrees natively and do not use this script.

.PARAMETER TaskId
    Task identifier. Becomes the branch leaf and the worktree directory name.

.PARAMETER Slug
    Project slug. Branch = agent-collab/<Slug>/task/<TaskId>; work branch = agent-collab/<Slug>/work.

.PARAMETER BaseRef
    Ref to branch from. Default: the work branch if it exists, otherwise current HEAD.

.PARAMETER WorktreeRoot
    Directory (relative to repo toplevel unless absolute) holding worktrees. Default '.worktrees'.

.PARAMETER Reuse
    If the branch/worktree already exists, return it instead of failing.

.OUTPUTS
    One compact JSON object on stdout. Diagnostics go to the verbose/warning streams.
    Exit code 0 on success, 1 on failure (stdout empty on failure).

.EXAMPLE
    $wt = pwsh -NoProfile -File New-TaskWorktree.ps1 -TaskId task-auth-route -Slug marketmind | ConvertFrom-Json
    git -C $wt.worktree_path status
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]*$')]
    [string] $TaskId,

    [Parameter(Mandatory)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]*$')]
    [string] $Slug,

    [string] $BaseRef,

    [string] $WorktreeRoot = '.worktrees',

    [switch] $Reuse
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-Git {
    # Runs git, returns stdout as a single trimmed string, throws on non-zero exit.
    param([Parameter(Mandatory)][string[]] $GitArgs)
    $out = & git @GitArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArgs -join ' ') failed (exit $LASTEXITCODE): $($out -join [Environment]::NewLine)"
    }
    return ($out -join [Environment]::NewLine).Trim()
}

function Test-GitRefExists {
    param([Parameter(Mandatory)][string] $Ref)
    & git show-ref --verify --quiet $Ref *> $null
    return ($LASTEXITCODE -eq 0)
}

function Get-WorktreeForBranch {
    # Returns the worktree path currently checked out to refs/heads/<branch>, or $null.
    param([Parameter(Mandatory)][string] $Branch)
    $lines = (& git worktree list --porcelain 2>$null) -split "`n"
    $currentPath = $null
    foreach ($line in $lines) {
        $line = $line.TrimEnd("`r")
        if ($line -like 'worktree *') { $currentPath = $line.Substring(9) }
        elseif ($line -eq "branch refs/heads/$Branch") { return $currentPath }
    }
    return $null
}

try {
    # --- Preconditions -------------------------------------------------------
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw 'git was not found on PATH.'
    }

    $toplevel = Invoke-Git @('rev-parse', '--show-toplevel')

    $branch     = "agent-collab/$Slug/task/$TaskId"
    $workBranch = "agent-collab/$Slug/work"

    # Validate the resulting branch name is a legal ref.
    & git check-ref-format "refs/heads/$branch" *> $null
    if ($LASTEXITCODE -ne 0) { throw "Computed branch name is not a valid git ref: $branch" }

    # --- Resolve base ref ----------------------------------------------------
    if (-not $BaseRef) {
        $BaseRef = (Test-GitRefExists "refs/heads/$workBranch") ? $workBranch : 'HEAD'
    }
    Write-Verbose "Base ref: $BaseRef"
    $baseCommit = Invoke-Git @('rev-parse', '--verify', "$BaseRef^{commit}")

    # --- Resolve worktree path (absolute) ------------------------------------
    $rootPath = [System.IO.Path]::IsPathRooted($WorktreeRoot) `
        ? $WorktreeRoot `
        : (Join-Path $toplevel $WorktreeRoot)
    $worktreePath = Join-Path $rootPath $TaskId

    # --- Keep the worktree root out of git, without touching tracked .gitignore
    $commonDir = Invoke-Git @('rev-parse', '--git-common-dir')
    if (-not [System.IO.Path]::IsPathRooted($commonDir)) {
        $commonDir = Join-Path $toplevel $commonDir
    }
    $excludeFile = Join-Path $commonDir 'info/exclude'
    $ignoreLine  = ($WorktreeRoot.TrimEnd('/','\') + '/')
    if (Test-Path $excludeFile) {
        $existing = Get-Content -LiteralPath $excludeFile -ErrorAction SilentlyContinue
        if ($existing -notcontains $ignoreLine) {
            Add-Content -LiteralPath $excludeFile -Value $ignoreLine
            Write-Verbose "Added '$ignoreLine' to .git/info/exclude"
        }
    }

    $branchExists   = Test-GitRefExists "refs/heads/$branch"
    $existingWtForBr = $branchExists ? (Get-WorktreeForBranch $branch) : $null
    $reused = $false

    # --- Idempotency / conflict handling -------------------------------------
    if ($existingWtForBr) {
        if (-not $Reuse) {
            throw "Branch '$branch' is already checked out at '$existingWtForBr'. Re-run with -Reuse to return it."
        }
        Write-Verbose "Reusing existing worktree at $existingWtForBr"
        $worktreePath = $existingWtForBr
        $reused = $true
    }
    elseif (Test-Path -LiteralPath $worktreePath) {
        # A directory is in the way but git doesn't know it as this branch's worktree.
        throw "Path '$worktreePath' already exists but is not the worktree for '$branch'. Resolve manually (it may be stale; 'git worktree prune' if appropriate)."
    }
    else {
        if (-not $PSCmdlet.ShouldProcess($worktreePath, "git worktree add for branch '$branch' from $baseCommit")) {
            Write-Warning 'Aborted by -WhatIf/ShouldProcess; no worktree created.'
            return
        }
        New-Item -ItemType Directory -Force -Path $rootPath | Out-Null
        if ($branchExists) {
            # Branch exists but is not checked out anywhere: attach a worktree to it.
            Invoke-Git @('worktree', 'add', $worktreePath, $branch) | Out-Null
            $reused = $true
        }
        else {
            # Fresh branch off the base commit.
            Invoke-Git @('worktree', 'add', '-b', $branch, $worktreePath, $baseCommit) | Out-Null
        }
    }

    $headCommit = Invoke-Git @('-C', $worktreePath, 'rev-parse', 'HEAD')

    # --- Report (single-writer: caller persists this; we only emit it) -------
    $result = [ordered]@{
        task_id       = $TaskId
        slug          = $Slug
        branch        = $branch
        work_branch   = $workBranch
        base_ref      = $BaseRef
        base_commit   = $baseCommit
        head_commit   = $headCommit
        worktree_path = (Resolve-Path -LiteralPath $worktreePath).Path
        reused        = $reused
        created       = ([DateTime]::UtcNow.ToString('o'))
    }

    Write-Output ($result | ConvertTo-Json -Compress)
    exit 0
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}