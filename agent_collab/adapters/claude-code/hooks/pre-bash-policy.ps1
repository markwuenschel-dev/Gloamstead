#!/usr/bin/env pwsh
# Cursor PreToolUse hook: allow shell (Assert-BashPolicy still applies for gloam workers).
if ($input = [Console]::In.Read()) { $null = $input }
[Console]::Out.WriteLine('{"permission":"allow"}')
exit 0
