<#
.SYNOPSIS
    Unattended Windows entrypoint for the Stalthos Asset Toolkit.
    Runs a named stage sequence, captures JSON status per stage, propagates first non-zero exit code.

.DESCRIPTION
    This script wraps the `stalthos` console script (installed via `pip install -e .`).
    It resolves the Python environment, runs `stalthos doctor` to report tool availability,
    executes the requested pipeline stages for a given definition, and writes per-stage
    JSON status files to var/reports/status/.

    Designed for PowerShell 5.1 (Windows built-in) AND pwsh 7+ (PowerShell Core).
    Non-interactive, no prompts, safe for CI/CD and agent automation.

.PARAMETER Definition
    Path to the species/creature YAML definition file (required).

.PARAMETER Seed
    Root seed for deterministic generation (int or stable string label). Default: 0.

.PARAMETER Workspace
    Workspace root directory. Default: $env:STEALTHOS_HOME or .\var.

.PARAMETER Stages
    Comma-separated stage numbers or ranges (e.g., "1,3-5,8"). Default: "1-9" (all stages).
    Stage mapping:
      1 = research_species
      3 = generate_plant, build_creature, rig_creature
      4 = bake_materials
      5 = generate_plant_variants
      6 = generate_lods
      8 = render_turntable, render_growth_stages, validate_asset, benchmark_asset
      9 = publish_asset

.PARAMETER DryRun
    If set, prints the planned command sequence without executing.

.PARAMETER ContinueOnError
    If set, continues through all stages even after a failure. Default: stop on first error.

.PARAMETER Transcript
    Optional path to write a full PowerShell transcript log.

.PARAMETER BlenderPath
    Override Blender executable path (passed as --blender to each command).

.PARAMETER Quiet
    Suppress verbose output (only errors and final summary).

.EXAMPLE
    .\scripts\stalthos.ps1 -Definition species/plants/splitcrown.yaml -Seed 42

.EXAMPLE
    .\scripts\stalthos.ps1 -Definition species/creatures/smalltrotter.yaml -Seed 42 -Stages "3,4" -BlenderPath "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"

.EXAMPLE
    .\scripts\stalthos.ps1 -Definition species/plants/splitcrown.yaml -DryRun

.NOTES
    Requires: Python 3.11+, stalthos-toolkit installed (pip install -e .), Blender 4.3+.
    The `stalthos` console script must be on PATH (usually via pip user install).
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateNotNullOrEmpty()]
    [string]$Definition,

    [Parameter(Position = 1)]
    [string]$Seed = "0",

    [Parameter(Position = 2)]
    [string]$Workspace = "",

    [Parameter(Position = 3)]
    [string]$Stages = "1-9",

    [switch]$DryRun,

    [switch]$ContinueOnError,

    [string]$Transcript = "",

    [string]$BlenderPath = "",

    [switch]$Quiet
)

# --- Strict mode & error handling -----------------------------------------
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# --- Transcript -----------------------------------------------------------
if ($Transcript) {
    $transcriptDir = Split-Path $Transcript -Parent
    if ($transcriptDir -and -not (Test-Path $transcriptDir)) {
        New-Item -ItemType Directory -Path $transcriptDir -Force | Out-Null
    }
    Start-Transcript -Path $Transcript -Force | Out-Null
    Write-Host "[stalthos.ps1] Transcript started: $Transcript"
}

# --- Helper functions -----------------------------------------------------
function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    if (-not $Quiet -or $Level -eq "ERROR") {
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Write-Host "[$timestamp] [$Level] $Message"
    }
}

function Write-ErrorLog {
    param([string]$Message)
    Write-Log $Message "ERROR"
}

function Test-Command {
    param([string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    return $null -ne $cmd
}

function Resolve-StalthosCmd {
    # Try to find the stalthos console script
    $candidates = @(
        "stalthos",
        "stalthos.exe",
        (Join-Path $env:APPDATA "Python\Python314\Scripts\stalthos.exe"),
        (Join-Path $env:LOCALAPPDATA "Python\Python314\Scripts\stalthos.exe")
    )
    # Add PYTHONHOME-based path if PYTHONHOME is set
    if ($env:PYTHONHOME) {
        $candidates += Join-Path (Split-Path $env:PYTHONHOME -Parent) "Scripts\stalthos.exe"
    }
    foreach ($c in $candidates) {
        if (Test-Command $c) {
            return $c
        }
        # Also check if it's a full path that exists
        if (Test-Path $c) {
            return $c
        }
    }
    # Fallback: python -m stalthos_toolkit.cli
    if (Test-Command "python") {
        return "python -m stalthos_toolkit.cli"
    }
    if (Test-Command "python3") {
        return "python3 -m stalthos_toolkit.cli"
    }
    if (Test-Command "py") {
        return "py -m stalthos_toolkit.cli"
    }
    return $null
}

function Parse-Stages {
    param([string]$StageSpec)
    $stages = @()
    $parts = $StageSpec -split ","
    foreach ($part in $parts) {
        $part = $part.Trim()
        if ($part -match "^(\d+)-(\d+)$") {
            $start = [int]$matches[1]
            $end = [int]$matches[2]
            for ($i = $start; $i -le $end; $i++) { $stages += $i }
        } elseif ($part -match "^\d+$") {
            $stages += [int]$part
        } else {
            Write-ErrorLog "Invalid stage specification: $part"
            exit 2
        }
    }
    return $stages | Sort-Object -Unique
}

function Get-CommandsForStage {
    param([int]$Stage)
    switch ($Stage) {
        1 { return @("research_species") }
        3 { return @("generate_plant", "build_creature", "rig_creature") }
        4 { return @("bake_materials") }
        5 { return @("generate_plant_variants") }
        6 { return @("generate_lods") }
        8 { return @("render_turntable", "render_growth_stages", "validate_asset", "benchmark_asset") }
        9 { return @("publish_asset") }
        default { return @() }
    }
}

function Invoke-Stage {
    param(
        [string]$Command,
        [string]$DefinitionPath,
        [string]$SeedValue,
        [string]$WorkspacePath,
        [string]$BlenderExe,
        [string]$StatusDir,
        [string]$AssetId
    )

    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $statusFile = Join-Path $StatusDir "${Command}_${AssetId}_${timestamp}.json"

    # Global args first, then command, then subcommand-specific args
    $args = @(
        "--workspace", $WorkspacePath,
        "--quiet",
        "--seed", $SeedValue,
        $Command,
        "--definition", $DefinitionPath
    )
    if ($BlenderExe) {
        $args += "--blender", $BlenderExe
    }

    # Build display command line (global args, command, subcommand args)
    $globalArgs = @("--workspace", $WorkspacePath, "--quiet", "--seed", $SeedValue)
    $subCmdArgs = @("--definition", $DefinitionPath)
    if ($BlenderExe) { $subCmdArgs += "--blender", $BlenderExe }
    $cmdLine = "$stalthosCmd $($globalArgs -join ' ') $Command $($subCmdArgs -join ' ')"
    Write-Log "Running: $cmdLine"

    if ($DryRun) {
        Write-Log "[DRY RUN] Would execute: $cmdLine"
        return @{ ExitCode = 0; StatusFile = $statusFile; DryRun = $true }
    }

    # Execute and capture stdout (JSON) and stderr separately
    $stdout = ""
    $stderr = ""
    $exitCode = 0

    try {
        # Handle both direct executable and 'python -m module' forms
        $cmdParts = $stalthosCmd -split ' '
        $exe = $cmdParts[0]
        if ($cmdParts.Count -gt 1) {
            $extraArgs = $cmdParts[1..($cmdParts.Count - 1)]
        } else {
            $extraArgs = @()
        }
        $fullArgs = @($extraArgs) + $args
        
        # Use call operator with redirection - avoids Start-Process argument parsing issues
        $stdoutFile = Join-Path $env:TEMP "stalthos_stdout_$$.tmp"
        $stderrFile = Join-Path $env:TEMP "stalthos_stderr_$$.tmp"
        & $exe @fullArgs *>&1 > $stdoutFile 2> $stderrFile
        $exitCode = $LASTEXITCODE
        $stdout = Get-Content -Raw -Path $stdoutFile -ErrorAction SilentlyContinue
        $stderr = Get-Content -Raw -Path $stderrFile -ErrorAction SilentlyContinue
        Remove-Item $stdoutFile, $stderrFile -ErrorAction SilentlyContinue
    } catch {
        Write-ErrorLog "Failed to start process: $_"
        $exitCode = 1
        $stdout = "{}"
    }

    # Save status JSON to status directory
    if ($stdout) {
        $stdout | Out-File -FilePath $statusFile -Encoding utf8 -Force
    } else {
        # Fallback: create minimal status
        @{
            command = $Command
            ok = $false
            exit_code = $exitCode
            seed = [int]$SeedValue
            asset_id = $AssetId
            started_at = (Get-Date).ToString("o")
            duration_s = 0
            partial = $false
            artifacts = @()
            metrics = @{}
            provenance = @{}
            warnings = @("No JSON status captured from stdout")
            errors = @(@{ kind = "CaptureError"; message = "Empty stdout from command" })
            logs = @()
        } | ConvertTo-Json -Depth 10 | Out-File -FilePath $statusFile -Encoding utf8 -Force
    }

    if ($stderr -and -not $Quiet) {
        Write-Host "  stderr: $stderr"
    }

    # Parse exit code from JSON if possible
    try {
        $statusObj = $stdout | ConvertFrom-Json -ErrorAction Stop
        if ($statusObj.exit_code -ne $null) {
            $exitCode = [int]$statusObj.exit_code
        }
    } catch {
        # JSON parse failed, use process exit code
    }

    Write-Log "  Exit code: $exitCode"
    return @{ ExitCode = $exitCode; StatusFile = $statusFile; StatusJson = $stdout }
}

# --- Main -----------------------------------------------------------------
Write-Log "=== Stalthos Toolkit Pipeline Runner ==="
Write-Log "Definition: $Definition"
Write-Log "Seed: $Seed"
Write-Log "Stages: $Stages"
Write-Log "Workspace: $Workspace"
Write-Log "DryRun: $DryRun"
Write-Log "ContinueOnError: $ContinueOnError"

# Resolve stalthos command
$stalthosCmd = Resolve-StalthosCmd
if (-not $stalthosCmd) {
    Write-ErrorLog "Could not find 'stalthos' console script. Ensure stalthos-toolkit is installed (pip install -e .) and Scripts directory is on PATH."
    exit 1
}
Write-Log "Resolved stalthos command: $stalthosCmd"

# Resolve definition path
$defPath = (Resolve-Path $Definition -ErrorAction Stop).Path
Write-Log "Definition resolved: $defPath"

# Extract asset_id from definition filename (without extension)
$assetId = [System.IO.Path]::GetFileNameWithoutExtension($defPath)

# Resolve workspace
$resolvedWorkspace = $Workspace
if (-not $resolvedWorkspace) {
    $resolvedWorkspace = $env:STEALTHOS_HOME
}
if (-not $resolvedWorkspace) {
    $resolvedWorkspace = Join-Path (Get-Location) "var"
}
$workspacePath = (Resolve-Path $resolvedWorkspace -ErrorAction Stop).Path
Write-Log "Workspace: $workspacePath"

# Ensure status directory exists
$statusDir = Join-Path $workspacePath "reports\status"
if (-not (Test-Path $statusDir)) {
    New-Item -ItemType Directory -Path $statusDir -Force | Out-Null
}
Write-Log "Status directory: $statusDir"

# Activate only verified project-local tools, unless the caller supplied an
# explicit tool override. Use-Tools changes this PowerShell process only; a bad
# local installation is ignored so PATH-based discovery can still proceed.
$toolkitRoot = Split-Path -Parent $PSScriptRoot
$repositoryRoot = Split-Path -Parent $toolkitRoot
$projectToolsRoot = Join-Path $repositoryRoot "var\tools"
$projectToolsScript = Join-Path $PSScriptRoot "Use-Tools.ps1"
$projectToolSpecs = @(
    @{ Name = "gltfpack"; EnvName = "STEALTHOS_GLTFPACK"; Exe = (Join-Path $projectToolsRoot "gltfpack-1.3\gltfpack.exe"); Folder = (Join-Path $projectToolsRoot "gltfpack-1.3") },
    @{ Name = "compressonator"; EnvName = "STEALTHOS_COMPRESSONATOR"; Exe = (Join-Path $projectToolsRoot "compressonator-4.5.52\compressonatorcli-4.5.52-win64\compressonatorcli.exe"); Folder = (Join-Path $projectToolsRoot "compressonator-4.5.52") }
)
foreach ($projectTool in $projectToolSpecs) {
    $manifestPath = Join-Path $projectTool.Folder "stalthos-tool-provenance.json"
    $configuredPath = [Environment]::GetEnvironmentVariable($projectTool.EnvName, "Process")
    if (-not $configuredPath -and (Test-Path -LiteralPath $projectTool.Exe -PathType Leaf) -and
        (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        try {
            $null = & $projectToolsScript -Tool $projectTool.Name -ToolkitRoot $toolkitRoot -InstallRoot $projectToolsRoot
            Write-Log "Using verified project-local $($projectTool.Name)"
        } catch {
            Write-Log "Ignoring project-local $($projectTool.Name): $($_.Exception.Message)" "WARN"
        }
    }
}

# Run stalthos doctor first
Write-Log "Running stalthos doctor..."
$doctorArgs = @("--workspace", $workspacePath, "--quiet", "doctor")
if ($BlenderPath) { $doctorArgs += "--blender", $BlenderPath }
$doctorCmd = "$stalthosCmd $($doctorArgs -join ' ')"
Write-Log "  $doctorCmd"

if (-not $DryRun) {
    $doctorOut = & $stalthosCmd @doctorArgs 2>&1
    $doctorExit = $LASTEXITCODE
    Write-Log "  doctor exit code: $doctorExit"
    if (-not $Quiet) {
        Write-Host "  $doctorOut"
    }
} else {
    Write-Log "  [DRY RUN] Would run doctor"
}

# Parse stage list
$stageNumbers = Parse-Stages $Stages
Write-Log "Parsed stages: $($stageNumbers -join ', ')"

# Build command sequence
$commandSequence = @()
foreach ($stage in $stageNumbers) {
    $cmds = Get-CommandsForStage $stage
    foreach ($cmd in $cmds) {
        $commandSequence += @{ Stage = $stage; Command = $cmd }
    }
}
$cmdSeqStr = ""
foreach ($cs in $commandSequence) {
    if ($cmdSeqStr) { $cmdSeqStr += " -> " }
    $cmdSeqStr += "$($cs.Stage):$($cs.Command)"
}
Write-Log "Command sequence: $cmdSeqStr"

# Pipeline summary
$pipelineSummary = @{
    asset_id = $assetId
    definition = $defPath
    seed = $Seed
    workspace = $workspacePath
    started_at = (Get-Date).ToString("o")
    stages_requested = $stageNumbers
    commands_planned = $commandSequence.Command
    stages_executed = @()
    first_failure = $null
    overall_exit_code = 0
    status_files = @()
}

$firstNonZeroExit = 0

foreach ($step in $commandSequence) {
    $stageNum = $step.Stage
    $cmd = $step.Command

    Write-Log "=== Stage ${stageNum}: $cmd ==="
    $pipelineSummary.stages_executed += $stageNum

    $result = Invoke-Stage -Command $cmd -DefinitionPath $defPath -SeedValue $Seed -WorkspacePath $workspacePath -BlenderExe $BlenderPath -StatusDir $statusDir -AssetId $assetId
    $pipelineSummary.status_files += $result.StatusFile

    if ($result.ExitCode -ne 0) {
        if ($firstNonZeroExit -eq 0) {
            $firstNonZeroExit = $result.ExitCode
            $pipelineSummary.first_failure = @{
                stage = $stageNum
                command = $cmd
                exit_code = $result.ExitCode
                status_file = $result.StatusFile
            }
        }
        Write-Log "Stage $stageNum ($cmd) FAILED with exit code $($result.ExitCode)" "ERROR"
        if (-not $ContinueOnError) {
            Write-Log "Stopping pipeline (use -ContinueOnError to continue)."
            break
        }
    } else {
        Write-Log "Stage $stageNum ($cmd) OK"
    }
}

$pipelineSummary.overall_exit_code = $firstNonZeroExit
$pipelineSummary.completed_at = (Get-Date).ToString("o")

# Write pipeline summary
$summaryFile = Join-Path $statusDir "pipeline_${assetId}_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$pipelineSummary | ConvertTo-Json -Depth 10 | Out-File -FilePath $summaryFile -Encoding utf8 -Force
Write-Log "Pipeline summary written: $summaryFile"

# Final report
if ($firstNonZeroExit -eq 0) {
    Write-Log "=== PIPELINE SUCCESS (all stages passed) ==="
} else {
    Write-Log "=== PIPELINE FAILED (first failure: exit $firstNonZeroExit) ===" "ERROR"
    if ($pipelineSummary.first_failure) {
        Write-Log "  Failed stage: $($pipelineSummary.first_failure.stage) ($($pipelineSummary.first_failure.command))"
        Write-Log "  Status file: $($pipelineSummary.first_failure.status_file)"
    }
}

if ($Transcript) {
    Stop-Transcript | Out-Null
}

exit $firstNonZeroExit
