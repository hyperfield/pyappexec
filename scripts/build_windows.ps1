Param(
    [string]$Configuration = "Release",
    [string]$Triplet = "x64-windows",
    [string]$BuildDir = "build",
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [switch]$Clean,
    [string]$Arch = "amd64",
    [string]$HostArch = "amd64",
    [int]$Parallel = 0
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [string]$BasePath = (Get-Location).Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $Path))
}

$repoRoot = Resolve-FullPath -Path ".." -BasePath $PSScriptRoot

function Resolve-VcpkgRootFromCandidates {
    param(
        [Parameter(Mandatory = $true)]
        [System.Collections.IEnumerable]$Candidates,
        [Parameter(Mandatory = $true)]
        [string]$BasePath
    )

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $trimmed = $candidate.Trim('"')
        try {
            $fullPath = Resolve-FullPath -Path $trimmed -BasePath $BasePath
        } catch {
            continue
        }

        $toolchainCandidate = Join-Path $fullPath "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $toolchainCandidate) {
            return $fullPath
        }
    }

    return $null
}

$candidateRoots = @()
if ($PSBoundParameters.ContainsKey("VcpkgRoot")) {
    $candidateRoots += $VcpkgRoot
} elseif ($env:VCPKG_ROOT) {
    $candidateRoots += $env:VCPKG_ROOT
}

$candidateRoots += @(
    (Join-Path $env:SystemDrive "dev\vcpkg"),
    (Join-Path $repoRoot "vcpkg"),
    (Join-Path $repoRoot "..\vcpkg"),
    (Join-Path $env:USERPROFILE "vcpkg")
)

$candidateRoots = $candidateRoots | Where-Object { $_ } | Select-Object -Unique

$resolvedVcpkg = Resolve-VcpkgRootFromCandidates -Candidates $candidateRoots -BasePath $repoRoot
if (-not $resolvedVcpkg) {
    throw ("vcpkg root not found. Pass -VcpkgRoot or set VCPKG_ROOT. " +
        "Checked paths:`n - " + ($candidateRoots -join "`n - "))
}

$VcpkgRoot = $resolvedVcpkg
Write-Host "Using vcpkg root: $VcpkgRoot"

$toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $toolchain)) {
    throw "Invalid vcpkg root: toolchain file not found at '$toolchain'."
}

$buildPath = Resolve-FullPath -Path $BuildDir -BasePath $repoRoot
if ($Clean -and (Test-Path $buildPath)) {
    Write-Host "Removing existing build directory: $buildPath"
    Remove-Item -Recurse -Force $buildPath
}

if (-not (Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath | Out-Null
}

$env:VCPKG_ROOT = $VcpkgRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found at '$vswhere'. Install Visual Studio 2017+ with the VS Installer."
}

$vsDevCmd = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "Common7\Tools\VsDevCmd.bat" | Select-Object -First 1
if (-not $vsDevCmd) {
    throw "VsDevCmd.bat not found. Ensure Visual Studio with C++ workload is installed."
}
$vsDevCmd = $vsDevCmd.Trim()

function Invoke-WithVsDevEnv {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    $batch = "call `"$vsDevCmd`" -arch=$Arch -host_arch=$HostArch && $Command"
    Write-Host ""
    Write-Host ">> $Command"
    & cmd.exe /c $batch
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw ("Command failed with exit code {0}: {1}" -f $exitCode, $Command)
    }
}

$configureCmd = @(
    "cmake",
    "-S `"$repoRoot`"",
    "-B `"$buildPath`"",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_TOOLCHAIN_FILE=`"$toolchain`"",
    "-DVCPKG_TARGET_TRIPLET=$Triplet"
) -join " "

function Get-ParallelJobs {
    param([int]$Requested)
    if ($Requested -gt 0) { return $Requested }
    $count = [int]$env:NUMBER_OF_PROCESSORS
    if ($count -lt 1) { $count = 1 }
    return $count
}

$parallelJobs = Get-ParallelJobs -Requested $Parallel
$buildCmd = "cmake --build `"$buildPath`" --config $Configuration --parallel $parallelJobs"

Invoke-WithVsDevEnv -Command $configureCmd
Invoke-WithVsDevEnv -Command $buildCmd

Write-Host ""
Write-Host "Build artifacts are located in: $buildPath"
