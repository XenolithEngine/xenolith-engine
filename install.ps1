<#
  Xenolith SDK installer for Windows — the irm|iex counterpart of install.sh:

    irm https://raw.githubusercontent.com/XenolithEngine/xenolith-engine/master/install.ps1 | iex

  Downloads the newest xenolith-cli release (tags "cli-v*", which never take the repository's
  "latest" pointer — that stays on the engine's sdk-v* releases), verifies its checksum, and
  installs it into ~\.local\bin (override with XENOLITH_BIN_DIR). Env overrides, mirroring
  install.sh:

    XENOLITH_CLI_VERSION   install a specific release tag (cli-v0.2.0)
    XENOLITH_CLI_TRIPLE    override the asset triple (e.g. x64 CLI on ARM64 Windows)
    XENOLITH_RELEASE_BASE  download base override (mirror/testing)
#>

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue' # Invoke-WebRequest progress is many-times slower than the download

$repo = 'XenolithEngine/xenolith-engine'
$api = "https://api.github.com/repos/$repo"

function Fail([string]$message) {
	Write-Host "install.ps1: $message" -ForegroundColor Red
	exit 1
}

# Windows PowerShell 5.1 may have TLS 1.2 off by default; PowerShell 7 already speaks it.
if ($PSVersionTable.PSEdition -eq 'Desktop') {
	[Net.ServicePointManager]::SecurityProtocol = `
		[Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
}

# --- platform -> asset triple ----------------------------------------------------

$triple = $env:XENOLITH_CLI_TRIPLE
if (-not $triple) {
	switch ($env:PROCESSOR_ARCHITECTURE) {
		'AMD64' { $triple = 'x86_64-pc-windows-msvc' }
		'ARM64' { $triple = 'aarch64-pc-windows-msvc' }
		default {
			Fail ("no prebuilt xenolith-cli for architecture '$($env:PROCESSOR_ARCHITECTURE)'. " +
				"Set XENOLITH_CLI_TRIPLE explicitly, or download from $api/releases " +
				'(assets named xenolith-cli-<triple>.tar.gz).')
		}
	}
}

# --- pick the release -------------------------------------------------------------

# The repository carries two release lines: sdk-v* (the engine, owns the "latest" pointer) and
# cli-v* (this tool). The CLI release is found by filtering tags, never via "latest".
$tag = $env:XENOLITH_CLI_VERSION
if ($tag) {
	if ($tag -notlike 'cli-v*') { Fail "XENOLITH_CLI_VERSION must start with 'cli-v', got '$tag'" }
	Write-Host "Using requested release $tag"
} else {
	$releases = Invoke-RestMethod -Uri "$api/releases?per_page=100"
	$rel = $releases | Where-Object { $_.tag_name -like 'cli-v*' } | Select-Object -First 1
	if (-not $rel) { Fail "no 'cli-v*' release found in $repo — has a CLI release been cut?" }
	$tag = $rel.tag_name
	Write-Host "Latest CLI release: $tag"
}

$asset = "xenolith-cli-$triple.tar.gz"
$baseUrl = if ($env:XENOLITH_RELEASE_BASE) { $env:XENOLITH_RELEASE_BASE }
	else { "https://github.com/$repo/releases/download/$tag" }

$tmp = Join-Path ([IO.Path]::GetTempPath()) ("xenolith-install." + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
	# --- download + verify --------------------------------------------------------

	Write-Host "Downloading $asset ..."
	try {
		Invoke-WebRequest -Uri "$baseUrl/$asset" -OutFile (Join-Path $tmp $asset)
		Invoke-WebRequest -Uri "$baseUrl/$asset.sha256" -OutFile (Join-Path $tmp "$asset.sha256")
	} catch {
		Fail "download failed from $baseUrl ($($_.Exception.Message)). " +
			"If this is an ARM64 machine, the aarch64 asset may not exist in $tag yet — " +
			"set XENOLITH_CLI_TRIPLE=x86_64-pc-windows-msvc to run the x64 build."
	}

	$expected = (Get-Content (Join-Path $tmp "$asset.sha256") -First 1) -split '\s+' | Select-Object -First 1
	$actual = (Get-FileHash -Algorithm SHA256 (Join-Path $tmp $asset)).Hash
	if ($actual -ne $expected) { Fail "checksum mismatch:`n  expected $expected`n  got      $actual" }

	# --- install ------------------------------------------------------------------

	# bsdtar ships with Windows 10 1803+; it reads .tar.gz natively.
	tar -xzf (Join-Path $tmp $asset) -C $tmp
	$bin = Get-ChildItem -Path $tmp -Filter 'xenolith-cli*' |
		Where-Object { $_.Name -eq 'xenolith-cli' -or $_.Name -eq 'xenolith-cli.exe' } |
		Select-Object -First 1
	if (-not $bin) { Fail 'archive did not contain a ./xenolith-cli(.exe) binary' }

	$binDir = if ($env:XENOLITH_BIN_DIR) { $env:XENOLITH_BIN_DIR } else { Join-Path $HOME '.local\bin' }
	New-Item -ItemType Directory -Path $binDir -Force | Out-Null

	# A copy, not a symlink: creating symlinks on Windows needs developer mode or admin.
	Copy-Item $bin.FullName (Join-Path $binDir $bin.Name) -Force
	$compatName = $bin.Name -replace '^xenolith-cli', 'xenolith-installer-cli'
	Copy-Item $bin.FullName (Join-Path $binDir $compatName) -Force

	# Add to the USER Path once; a new terminal picks it up (the current one gets it inline).
	$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
	if ($userPath -notlike "*$binDir*") {
		[Environment]::SetEnvironmentVariable('Path', "$userPath;$binDir", 'User')
		Write-Host "note: added $binDir to your user PATH — open a new terminal, or:"
		Write-Host "  `$env:Path += `";$binDir`""
	} else {
		$env:Path += ";$binDir"
	}

	Write-Host 'Installed:'
	& (Join-Path $binDir $bin.Name) --version
	Write-Host 'Next: xenolith-cli install   # engine + native toolchains'
} finally {
	Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
