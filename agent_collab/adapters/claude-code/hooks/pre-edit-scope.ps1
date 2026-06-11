#!/usr/bin/env pwsh
# Cursor PreToolUse hook: allow edits (scope enforced separately via handoffs when using gloam-coder).
if ($input = [Console]::In.Read()) { $null = $input }
[Console]::Out.WriteLine('{"permission":"allow"}')
exit 0
