# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
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
#
# Windows counterpart of common/utils/fetch.sh - same modes, same flags, same
# checks, so src.mk can drive either host with one set of per-library variables.
# Read fetch.sh first: the policy is documented there and this file only mirrors it.
#
# gpg is not part of a stock Windows install, so on this host the OpenPGP check is
# usually skipped and the pinned SHA-256 carries the verification on its own. Put
# Gpg4win on PATH (or use a POSIX host) when refreshing a pin, and set
# SP_REQUIRE_SIGNATURES=1 to make its absence an error rather than a warning.

[CmdletBinding()]
param(
	[Parameter(Mandatory = $true, Position = 0)]
	[ValidateSet('tar', 'zip', 'file', 'clone')]
	[string] $Mode,

	[string] $Name,
	[string] $Url,
	[string] $Repo,
	[string] $Dest,
	[string] $Sha256,
	[string] $Sig,
	[string] $Key,
	[int]    $Strip = 1,
	[string] $Tag,
	[string] $Commit,
	[string] $Depth,
	[switch] $Submodules
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$TmpDir  = $env:SP_TMP_DIR
$KeysDir = $env:SP_KEYS_DIR
$RequireSignatures = ($env:SP_REQUIRE_SIGNATURES -eq '1')

function Die($msg) {
	Write-Host "fetch: ${Name}: $msg" -ForegroundColor Red
	exit 1
}
function Note($msg) { Write-Host "fetch: ${Name}: $msg" }

if (-not $Name)   { Write-Host 'fetch: -Name is required'; exit 1 }
if (-not $Dest)   { Die '-Dest is required' }
if (-not $TmpDir) { Die 'SP_TMP_DIR is not set' }

$Work = Join-Path $TmpDir $Name

function Have($exe) { $null -ne (Get-Command $exe -ErrorAction SilentlyContinue) }

function Get-Remote($u, $out) {
	if (Test-Path $out) { Remove-Item -Force $out }
	$attempt = 0
	while ($attempt -lt 3) {
		$attempt++
		try {
			# Invoke-WebRequest throws on any non-2xx status, which is exactly the
			# "download actually succeeded" check this whole script exists for.
			Invoke-WebRequest -Uri $u -OutFile $out -UseBasicParsing -TimeoutSec 300
			if ((Test-Path $out) -and ((Get-Item $out).Length -gt 0)) { return $true }
		} catch {
			if ($attempt -ge 3) { return $false }
			Start-Sleep -Seconds 2
		}
	}
	return $false
}

function Test-Sha256($path) {
	if (-not $Sha256) {
		Note 'WARNING: no SHA-256 pinned; the download is unverified'
		return
	}
	$got  = (Get-FileHash -Algorithm SHA256 -Path $path).Hash.ToLower()
	$want = $Sha256.ToLower()
	if ($got -ne $want) {
		Remove-Item -Force $path -ErrorAction SilentlyContinue
		Write-Host "fetch: ${Name}: SHA-256 MISMATCH - refusing to use this download" -ForegroundColor Red
		Write-Host "    url      $Url"
		Write-Host "    expected $want"
		Write-Host "    actual   $got"
		Write-Host '  Either the upstream file was replaced (re-check the release and the'
		Write-Host '  signature by hand before touching the pin) or the transfer was tampered'
		Write-Host "  with. Do not 'fix' this by pasting the new hash without checking."
		exit 1
	}
}

function Test-Signature($path) {
	if (-not $Sig) {
		if ($RequireSignatures) { Die 'SP_REQUIRE_SIGNATURES=1 but this source publishes no signature' }
		return
	}
	if ($Sig -match '^https?://') { $sigUrl = $Sig } else { $sigUrl = "$Url$Sig" }
	$keyFile = Join-Path $KeysDir "$Key.asc"

	if ((-not $Key) -or (-not (Test-Path $keyFile))) {
		if ($RequireSignatures) { Die "no pinned key for a signed source (expected $keyFile)" }
		Note 'WARNING: signature published but no pinned key; skipping'
		return
	}
	if (-not (Have 'gpg')) {
		if ($RequireSignatures) { Die 'SP_REQUIRE_SIGNATURES=1 but gpg is not installed' }
		Note 'WARNING: gpg not installed; signature not checked (SHA-256 pin still enforced)'
		return
	}

	$sigFile = "$path.sigfile"
	if (-not (Get-Remote $sigUrl $sigFile)) { Die "could not download the signature $sigUrl" }

	$gnupg = Join-Path $Work 'gnupg'
	if (Test-Path $gnupg) { Remove-Item -Recurse -Force $gnupg }
	New-Item -ItemType Directory -Force -Path $gnupg | Out-Null
	$ring = Join-Path $gnupg 'pinned.gpg'

	$oldHome = $env:GNUPGHOME
	$env:GNUPGHOME = $gnupg
	try {
		& gpg --batch --quiet --no-default-keyring --keyring $ring --import $keyFile 2>&1 | Out-Null
		if ($LASTEXITCODE -ne 0) { Die "could not import the pinned key $keyFile" }

		# Exit status only - gpg's messages are localised, so matching them against
		# "Good signature" quietly passes everything under a non-English locale.
		& gpg --batch --quiet --no-default-keyring --keyring $ring --trust-model always `
			--verify $sigFile $path 2>&1 | Out-Null
		if ($LASTEXITCODE -ne 0) {
			Write-Host "fetch: ${Name}: BAD OpenPGP SIGNATURE" -ForegroundColor Red
			Write-Host "    file $Url"
			Write-Host "    sig  $sigUrl"
			Write-Host "    key  $keyFile"
			Write-Host '  Either upstream rotated the signing key (verify the new fingerprint'
			Write-Host "  against the project's own site, then update toolchains/keys/) or this"
			Write-Host '  download is not what upstream published.'
			& gpg --batch --no-default-keyring --keyring $ring --verify $sigFile $path 2>&1 | Write-Host
			exit 1
		}
		Note "OpenPGP signature OK (key $Key)"
	} finally {
		$env:GNUPGHOME = $oldHome
		Remove-Item -Recurse -Force $gnupg -ErrorAction SilentlyContinue
	}
}

function Get-Verified {
	New-Item -ItemType Directory -Force -Path $Work | Out-Null
	$out = Join-Path $Work ([System.IO.Path]::GetFileName(([uri]$Url).AbsolutePath))
	if (-not (Get-Remote $Url $out)) { Die "download failed: $Url" }
	Test-Sha256 $out
	Test-Signature $out
	return $out
}

# Replace Dest with Stage in one step, so an interrupted run cannot leave a
# partially extracted tree that a later make would treat as a finished checkout.
function Install-Tree($stage) {
	if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
	$parent = Split-Path -Parent $Dest
	if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
	Move-Item -Path $stage -Destination $Dest -Force
	(Get-Item $Dest).LastWriteTime = Get-Date
}

function Invoke-Tar {
	if (-not $Url) { Die '-Url is required' }
	$file = Get-Verified

	# Listing the archive before unpacking turns a truncated or mislabelled file
	# into one clear error rather than a half-extracted directory.
	& tar -tf $file > $null 2>&1
	if ($LASTEXITCODE -ne 0) { Die "not a readable archive: $file" }

	$stage = Join-Path $Work 'unpack'
	if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
	New-Item -ItemType Directory -Force -Path $stage | Out-Null
	& tar -xf $file --strip-components=$Strip -C $stage
	if ($LASTEXITCODE -ne 0) { Die "could not unpack $file" }
	Install-Tree $stage
	Remove-Item -Force $file -ErrorAction SilentlyContinue
}

function Invoke-Zip {
	if (-not $Url) { Die '-Url is required' }
	$file = Get-Verified

	$stage = Join-Path $Work 'unzip'
	if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
	New-Item -ItemType Directory -Force -Path $stage | Out-Null
	try {
		Expand-Archive -Path $file -DestinationPath $stage -Force
	} catch {
		Die "not a readable zip archive: $file"
	}

	# Same effect as tar --strip-components=1, without naming the versioned
	# top-level directory (sqlite-amalgamation-NNNNNNN) anywhere in the makefile.
	if ($Strip -eq 1) {
		$entries = @(Get-ChildItem -Force $stage)
		if ($entries.Count -eq 1 -and $entries[0].PSIsContainer) {
			$stage = $entries[0].FullName
		} else {
			Die "expected exactly one top-level directory in the archive, found $($entries.Count)"
		}
	}
	Install-Tree $stage
	Remove-Item -Force $file -ErrorAction SilentlyContinue
}

function Invoke-File {
	if (-not $Url) { Die '-Url is required' }
	$file = Get-Verified
	$parent = Split-Path -Parent $Dest
	if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
	Move-Item -Path $file -Destination $Dest -Force
}

# Submodules are updated after the working tree is on the pinned commit, so they
# land on the revisions that commit records. No --depth here: the pinned submodule
# revision is not necessarily the tip of anything, and a shallow submodule fetch
# fails when it is not. `clone --recurse-submodules --depth N` behaved the same
# way - shallowness applies to submodules only with --shallow-submodules.
function Update-Submodules($tree, $pinnedTo) {
	if (-not $Submodules) { return }
	& git -C $tree submodule update --init --recursive --quiet
	if ($LASTEXITCODE -ne 0) { Die "could not update submodules at $pinnedTo" }
}

function Invoke-Clone {
	if (-not $Repo) { Die '-Repo is required' }
	if (-not (Have 'git')) { Die 'git is not installed' }

	$stage = Join-Path $Work 'clone'
	if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
	New-Item -ItemType Directory -Force -Path (Split-Path -Parent $stage) | Out-Null

	if ($Tag) {
		# Fetch the tag rather than `clone --branch <tag>`. clone --branch hands
		# git's clone path the ref's own object, and for an annotated tag that
		# object is the tag, not a commit, so git prints
		#
		#     warning: refs/tags/v1.6.58 fdc7185... is not a commit!
		#
		# for most of the sources here. The resulting clone is correct - git peels
		# the tag for HEAD anyway - but the warning is alarming and means nothing.
		# Fetching the tag explicitly does not go through that path. The refspec
		# copies the tag into the new repository too, so `git describe` still works
		# in the unpacked tree the way it did before; some upstream build scripts
		# read it for their version string.
		& git init --quiet $stage
		if ($LASTEXITCODE -ne 0) { Die 'git init failed' }
		& git -C $stage remote add origin $Repo
		if ($LASTEXITCODE -ne 0) { Die 'could not add the remote' }

		$fetchArgs = @('fetch', '--quiet')
		if ($Depth) { $fetchArgs += @('--depth', $Depth) }

		# A -Tag is normally a tag, but the option is documented as "tag or
		# branch", so fall back rather than fail on a branch name.
		$refspecs = @(
			"refs/tags/$Tag`:refs/tags/$Tag",
			"refs/heads/$Tag`:refs/remotes/origin/$Tag",
			$Tag
		)
		$fetched = $false
		foreach ($rs in $refspecs) {
			& git -C $stage @fetchArgs origin $rs 2>&1 | Out-Null
			if ($LASTEXITCODE -eq 0) { $fetched = $true; break }
		}
		if (-not $fetched) { Die "could not fetch '$Tag' from $Repo" }

		& git -c advice.detachedHead=false -C $stage checkout --quiet --detach FETCH_HEAD
		if ($LASTEXITCODE -ne 0) { Die "could not check out '$Tag'" }
	} else {
		# No tag to name: a plain clone does not go through the path that warns.
		# A bare commit pin needs the history, so --depth is dropped there.
		$cloneArgs = @('-c', 'advice.detachedHead=false', 'clone', '--quiet')
		if ($Depth -and (-not $Commit)) { $cloneArgs += @('--depth', $Depth) }

		& git @cloneArgs $Repo $stage
		if ($LASTEXITCODE -ne 0) { Die "git clone failed: $Repo" }

		if ($Commit) {
			& git -c advice.detachedHead=false -C $stage checkout --quiet $Commit
			if ($LASTEXITCODE -ne 0) { Die "pinned commit $Commit is not in $Repo" }
		}
	}

	# Only meaningful for the tag path: in the bare-commit path the checkout above
	# already named the commit, so there is nothing left to disagree with.
	if ($Commit -and $Tag) {
		$head = (& git -C $stage rev-parse HEAD).Trim()
		if ($LASTEXITCODE -ne 0) { Die 'could not read HEAD' }
		if ($head -ne $Commit) {
			# A tag is a mutable pointer; upstream re-tagging is exactly the event
			# this catches, and it must not pass silently.
			Write-Host "fetch: ${Name}: TAG MOVED - refusing this clone" -ForegroundColor Red
			Write-Host "    repo   $Repo"
			Write-Host "    tag    $Tag"
			Write-Host "    pinned $Commit"
			Write-Host "    actual $head"
			Write-Host '  Review what upstream changed under the tag before updating the pin.'
			Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
			exit 1
		}
	}

	$pinnedTo = if ($Commit) { $Commit } else { $Tag }
	Update-Submodules $stage $pinnedTo

	Install-Tree $stage
}

switch ($Mode) {
	'tar'   { Invoke-Tar }
	'zip'   { Invoke-Zip }
	'file'  { Invoke-File }
	'clone' { Invoke-Clone }
}
exit 0
