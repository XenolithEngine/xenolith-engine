# Copyright (c) 2025 Stappler Team <admin@stappler.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# Windows PowerShell script equivalent to toolchains/Makefile
# Usage: powershell -File toolchains.ps1 [-Target <target_name>] [-GitTag <tag>]

param(
    [string]$Target = "",
    [string]$GitTag = "sdk-v0alpha0"
)

$ErrorActionPreference = "Stop"

# Base URL for releases
$BaseUrl = "https://stappler.dev/releases/$GitTag"

# Directories
$HostsDir = Join-Path $PSScriptRoot "..\runtime\toolchains\hosts"
$TargetsDir = Join-Path $PSScriptRoot "..\runtime\toolchains\targets"

# Alternative directories (if runtime/toolchains doesn't exist)
$AltHostsDir = Join-Path $PSScriptRoot "hosts"
$AltTargetsDir = Join-Path $PSScriptRoot "targets"

# Use alternative dirs if runtime toolchains don't exist
if (-not (Test-Path $HostsDir)) { $HostsDir = $AltHostsDir }
if (-not (Test-Path $TargetsDir)) { $TargetsDir = $AltTargetsDir }

# Detect host architecture
function Get-HostId {
    try {
        $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
        switch ($arch.ToString().ToUpper()) {
            "X64" { return "x86_64-pc-windows-msvc" }
            "X86" { return "x86-pc-windows-msvc" }
            "ARM64" { return "aarch64-pc-windows-msvc" }
            default { return "x86_64-pc-windows-msvc" }
        }
    } catch {
        return "x86_64-pc-windows-msvc"
    }
}

# Create directory if it doesn't exist
function Ensure-Directory($path) {
    if (-not (Test-Path $path)) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
    }
}

# Download file with progress
function Download-File($url, $destination) {
    Write-Host "Downloading: $(Split-Path $url -Leaf)"
    Ensure-Directory (Split-Path $destination -Parent)
    
    try {
        # Try Invoke-WebRequest first (available in PowerShell 5+)
        if ($PSVersionTable.PSVersion.Major -ge 5) {
            Invoke-WebRequest -Uri $url -OutFile $destination -UseBasicParsing
        } else {
            # Fallback for older PowerShell versions
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($url, $destination)
        }
    } catch {
        Write-Error "Failed to download: $_"
        exit 1
    }
}

# Extract tar.xz archive using Python if available, or 7-Zip
function Extract-Archive($archivePath, $extractDir) {
    Write-Host "Extracting: $(Split-Path $archivePath -Leaf)"
    Ensure-Directory $extractDir
    
    # Try 7-Zip first
    $sevenZipPath = "${env:ProgramFiles}\7-Zip\7z.exe"
    if (Test-Path $sevenZipPath) {
        & $sevenZipPath x $archivePath "-o$extractDir" -y | Out-Null
        return
    }
    
    # Try Python's tarfile module
    $pythonPath = Get-Command python -ErrorAction SilentlyContinue
    if ($pythonPath) {
        $pythonCmd = $pythonPath.Source
        $script = @"
import tarfile
import os
archive = r'$($archivePath.Replace('\', '\\\\'))'
extract_dir = r'$($extractDir.Replace('\', '\\\\'))'
with tarfile.open(archive, 'r:xz') as tar:
    tar.extractall(path=extract_dir)
"@
        & $pythonCmd -c $script 2>&1 | Out-Null
        return
    }
    
    # Try PowerShell Core (pwsh) which has native tar support
    $pwshPath = Get-Command pwsh -ErrorAction SilentlyContinue
    if ($pwshPath) {
        & $pwshPath -c "Expand-Archive -Path '$($archivePath)' -DestinationPath '$($extractDir)' -Force" 2>&1 | Out-Null
        return
    }
    
    Write-Error "Cannot extract archive: no suitable extraction tool found (need 7-Zip, Python, or PowerShell Core)"
    exit 1
}

# Clean directories
function Remove-Directories(@($dirs)) {
    foreach ($dir in $dirs) {
        if (Test-Path $dir) {
            Write-Host "Removing: $dir"
            Remove-Item -Recurse -Force -ErrorAction Ignore -Path $dir
        }
    }
}

# Download and extract hosts
function Install-Hosts($hostId) {
    $archiveName = "$hostId.tar.xz"
    $archivePath = Join-Path $HostsDir $archiveName
    
    if (-not (Test-Path $archivePath)) {
        Download-File "$BaseUrl/hosts/$archiveName" $archivePath
    }
    
    Extract-Archive $archivePath (Join-Path $HostsDir $hostId)
}

# Download and extract targets
function Install-Targets($targetId) {
    $archiveName = "$targetId.tar.xz"
    $archivePath = Join-Path $TargetsDir $archiveName
    
    if (-not (Test-Path $archivePath)) {
        Download-File "$BaseUrl/targets/$archiveName" $archivePath
    }
    
    Extract-Archive $archivePath (Join-Path $TargetsDir $targetId)
}

# Define all available targets
$AndroidTargets = @(
    "unknown-ndk-linux-android",
    "aarch64-unknown-linux-android",
    "armv7a-unknown-linux-androideabi",
    "i686-unknown-linux-android",
    "x86_64-unknown-linux-android"
)

$LinuxTargets = @(
    "x86_64-unknown-linux-gnu",
    "aarch64-unknown-linux-gnu",
    "riscv64-unknown-linux-gnu"
)

$AppleTargets = @(
    "x86_64-apple-macosx",
    "aarch64-apple-macosx",
    "x86_64-apple-macosx+sprt",
    "aarch64-apple-macosx+sprt",
    "aarch64-apple-ios",
    "x86_64-apple-ios-simulator",
    "aarch64-apple-ios-simulator"
)

$WindowsTargets = @(
    "x86_64-pc-windows-msvc",
    # "aarch64-pc-windows-msvc"  # Commented out - uncomment when needed
)

# Main execution
$hostId = Get-HostId

Write-Host "Detected host ID: $hostId"
Write-Host "Git Tag: $GitTag"
Write-Host ""

if ($Target -eq "") {
    Write-Host "No target specified, running 'all' (host + all targets)..."
    
    # Install host
    Install-Hosts $hostId
    
    # Install all platform targets
    foreach ($t in $AndroidTargets) { Install-Targets $t }
    foreach ($t in $LinuxTargets) { Install-Targets $t }
    foreach ($t in $AppleTargets) { Install-Targets $t }
    foreach ($t in $WindowsTargets) { Install-Targets $t }
    
} elseif ($Target -eq "host") {
    Write-Host "Installing host toolchain..."
    Install-Hosts $hostId
    
} elseif ($Target -eq "target" -or $Target -eq "all-targets") {
    Write-Host "Installing all target toolchains..."
    
    foreach ($t in $AndroidTargets) { Install-Targets $t }
    foreach ($t in $LinuxTargets) { Install-Targets $t }
    foreach ($t in $AppleTargets) { Install-Targets $t }
    foreach ($t in $WindowsTargets) { Install-Targets $t }
    
} elseif ($Target -eq "android" -or $Target -eq "target-android") {
    Write-Host "Installing Android targets..."
    foreach ($t in $AndroidTargets) { Install-Targets $t }
    
} elseif ($Target -eq "linux" -or $Target -eq "target-linux") {
    Write-Host "Installing Linux targets..."
    foreach ($t in $LinuxTargets) { Install-Targets $t }
    
} elseif ($Target -eq "apple" -or $Target -eq "target-apple") {
    Write-Host "Installing Apple targets..."
    foreach ($t in $AppleTargets) { Install-Targets $t }
    
} elseif ($Target -eq "windows" -or $Target -eq "target-windows") {
    Write-Host "Installing Windows targets..."
    foreach ($t in $WindowsTargets) { Install-Targets $t }
    
} elseif ($Target.StartsWith("target-")) {
    # Single specific target (e.g., "target-x86_64-pc-windows-msvc")
    $targetId = $Target.Substring(7)  # Remove "target-" prefix
    Write-Host "Installing single target: $targetId"
    Install-Targets $targetId
    
} elseif ($Target -eq "clean") {
    Write-Host "Cleaning up..."
    Remove-Directories @($HostsDir, $TargetsDir)
    
} else {
    # Try to interpret as a direct target name
    Write-Host "Installing target: $Target"
    Install-Targets $Target
}

Write-Host ""
Write-Host "Done!"