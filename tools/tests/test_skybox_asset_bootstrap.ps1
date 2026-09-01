$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projectPath = Join-Path $PSScriptRoot 'native\SkyboxAssetValidationTests.vcxproj'
$managerSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\Common\SkyboxAssetManager.cpp')
$startDownloadOffset = $managerSource.IndexOf('void StartDownload(bool forceRetry)', [StringComparison]::Ordinal)
$workerLaunchOffset = $managerSource.IndexOf('DownloadWorker', $startDownloadOffset, [StringComparison]::Ordinal)
Assert-True ($startDownloadOffset -ge 0 -and $workerLaunchOffset -gt $startDownloadOffset) `
    'could not locate the asynchronous skybox download launch path'
$startDownloadPath = $managerSource.Substring($startDownloadOffset, $workerLaunchOffset - $startDownloadOffset)
Assert-True ($startDownloadPath -notmatch 'AllRequiredAssetsAreValid\s*\(') `
    'EnsureSkyboxAssetsAsync must not hash the full IBL install on its caller thread before launching the worker'
Assert-True ($managerSource -notmatch '\.detach\s*\(') `
    'the skybox download worker must remain joinable for safe application shutdown'
Assert-True ($managerSource -match 'SkyboxAssetManager::Shutdown\s*\(' -and
    $managerSource -match '\.Cancel\s*\(' -and $managerSource -match '\.join\s*\(') `
    'skybox shutdown must cancel the active child process and join the worker thread'
$finalInstallOffset = $managerSource.IndexOf('if (!InstallExtractedAssetsToAllRoots', [StringComparison]::Ordinal)
$finalSuccessOffset = $managerSource.IndexOf(
    'SetStatus(SkyboxAssetDownloadState::Succeeded', $finalInstallOffset, [StringComparison]::Ordinal)
Assert-True ($finalInstallOffset -ge 0 -and $finalSuccessOffset -gt $finalInstallOffset) `
    'could not locate the final skybox installation success path'
$finalSuccessPath = $managerSource.Substring($finalInstallOffset, $finalSuccessOffset - $finalInstallOffset)
Assert-True ($finalSuccessPath -match '}\s*if \(ShutdownRequested\(\)\)\s*return;\s*MarkAllSetsValidated\(SkyboxRoot\(\)\);\s*g_completedGeneration') `
    'the worker must not publish successful installation state after shutdown begins'

$hasSetOffset = $managerSource.IndexOf('bool SkyboxAssetManager::HasIBLAssetSet', [StringComparison]::Ordinal)
$ensureOffset = $managerSource.IndexOf('void SkyboxAssetManager::EnsureSkyboxAssetsAsync', $hasSetOffset, [StringComparison]::Ordinal)
Assert-True ($hasSetOffset -ge 0 -and $ensureOffset -gt $hasSetOffset) `
    'could not locate the IBL set availability path'
$hasSetPath = $managerSource.Substring($hasSetOffset, $ensureOffset - $hasSetOffset)
Assert-True ($hasSetPath -notmatch 'VerifySetPrefix\s*\(') `
    'HasIBLAssetSet must use the worker-populated validation cache instead of hashing hundreds of MiB on the caller thread'

$iblCallers = @(
    'Dx11\31_IBL\App.cpp',
    'Dx11\32_Sound_FMOD\App.cpp',
    'Dx11\33_Sound_Animation_Camera_Motion\App.cpp',
    'Dx11\34_ToneMapping\App.cpp',
    'Dx11\35_DeferredRendering\App.cpp',
    'Dx11\36_AdvancedAnim_Sound_Click\App_Utilities.inl'
)
foreach ($relativeCaller in $iblCallers) {
    $callerSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativeCaller)
    $availabilityOffset = $callerSource.IndexOf('if (!SkyboxAssetManager::HasIBLAssetSet(path))', [StringComparison]::Ordinal)
    $loadOffset = $callerSource.IndexOf('const bool loaded', $availabilityOffset, [StringComparison]::Ordinal)
    Assert-True ($availabilityOffset -ge 0 -and $loadOffset -gt $availabilityOffset) `
        "could not locate the guarded IBL load path in $relativeCaller"
    $guardPath = $callerSource.Substring($availabilityOffset, $loadOffset - $availabilityOffset)
    Assert-True ($guardPath -match 'return\s*;') `
        "unverified IBL files must not be loaded before worker validation in $relativeCaller"
}

$shutdownCallers = @(
    'Dx11\31_IBL\App.cpp',
    'Dx11\32_Sound_FMOD\App.cpp',
    'Dx11\33_Sound_Animation_Camera_Motion\App.cpp',
    'Dx11\34_ToneMapping\App.cpp',
    'Dx11\35_DeferredRendering\App.cpp',
    'Dx11\36_AdvancedAnim_Sound_Click\App_Lifecycle.inl'
)
foreach ($relativeCaller in $shutdownCallers) {
    $callerSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativeCaller)
    $uninitializeOffset = $callerSource.IndexOf('void App::OnUninitialize()', [StringComparison]::Ordinal)
    $shutdownOffset = $callerSource.IndexOf('SkyboxAssetManager::Shutdown();', $uninitializeOffset, [StringComparison]::Ordinal)
    $resourceShutdownOffset = $callerSource.IndexOf('ImGui_ImplDX11_Shutdown();', $uninitializeOffset, [StringComparison]::Ordinal)
    Assert-True ($uninitializeOffset -ge 0 -and $shutdownOffset -gt $uninitializeOffset -and
        $resourceShutdownOffset -gt $shutdownOffset) `
        "skybox worker shutdown must precede graphics resource destruction in $relativeCaller"
}

$uiRenderers = @(
    @{ path = 'Dx11\31_IBL\App.cpp'; function = 'void App::OnRender()' },
    @{ path = 'Dx11\32_Sound_FMOD\App.cpp'; function = 'void App::OnRender()' },
    @{ path = 'Dx11\33_Sound_Animation_Camera_Motion\App.cpp'; function = 'void App::OnRender()' },
    @{ path = 'Dx11\34_ToneMapping\App.cpp'; function = 'void App::OnRender()' },
    @{ path = 'Dx11\35_DeferredRendering\App.cpp'; function = 'void App::PassUI()' },
    @{ path = 'Dx11\36_AdvancedAnim_Sound_Click\App_RenderPasses.inl'; function = 'void App::PassUI()' }
)
foreach ($renderer in $uiRenderers) {
    $relativeRenderer = $renderer.path
    $rendererSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativeRenderer)
    $functionOffset = $rendererSource.IndexOf($renderer.function, [StringComparison]::Ordinal)
    $generationOffset = $rendererSource.IndexOf(
        'SkyboxAssetManager::GetCompletedGeneration()', $functionOffset, [StringComparison]::Ordinal)
    $imguiFrameOffset = $rendererSource.IndexOf(
        'ImGui_ImplDX11_NewFrame()', $functionOffset, [StringComparison]::Ordinal)
    Assert-True ($functionOffset -ge 0 -and $generationOffset -ge 0 -and $imguiFrameOffset -ge 0) `
        "could not locate skybox generation handling and the ImGui frame in $relativeRenderer"
    Assert-True ($generationOffset -lt $imguiFrameOffset) `
        "skybox SRVs must be refreshed before ImGui records texture references in $relativeRenderer"
}

$captureSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\capture_readme_media.ps1')
Assert-True ($captureSource -match 'Assert-ProjectSkyboxPreflight[\s\S]*?-SkyboxRoot\s+\(\[IO\.Path\]::GetFullPath\(\(Join-Path\s+\$RuntimeDir') `
    'README capture must derive its skybox preflight root from the launched runtime directory'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
    $msbuild = @(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1)[0]
}
if ([string]::IsNullOrWhiteSpace([string]$msbuild)) {
    $msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($null -ne $msbuildCommand) { $msbuild = $msbuildCommand.Source }
}
Assert-True (-not [string]::IsNullOrWhiteSpace([string]$msbuild) -and
    (Test-Path -LiteralPath $msbuild -PathType Leaf)) `
    'Visual Studio MSBuild is required for the native skybox validation test'

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('D3D11-SkyboxValidationTests-' + [guid]::NewGuid().ToString('N'))
$outDir = Join-Path $tempRoot 'out'
$intDir = Join-Path $tempRoot 'obj'
New-Item -ItemType Directory -Force -Path $outDir, $intDir | Out-Null

try {
    $fixtureRoot = Join-Path $tempRoot 'fixture-skybox'
    $fixtureSample = Join-Path $fixtureRoot 'Sample'
    New-Item -ItemType Directory -Force -Path $fixtureSample | Out-Null
    $fixtureAssets = @(
        @{ name = 'BakerSampleDiffuseHDR.dds'; byte = [byte][char]'a'; sha256 = 'ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb' },
        @{ name = 'BakerSampleSpecularHDR.dds'; byte = [byte][char]'b'; sha256 = '3e23e8160039594a33894f6564e1b1348bbd7a0088d42c4acb73eeaed59c009d' },
        @{ name = 'BakerSampleBrdf.dds'; byte = [byte][char]'c'; sha256 = '2e7d2c03a9507ae265ecf5b5356885a53393a2029d241394997265a1a25aefc6' },
        @{ name = 'BakerSampleEnvHDR.dds'; byte = [byte][char]'d'; sha256 = '18ac3e7343f016890c510e93f935261169d9e3f565436429830faf0934f4f8e4' }
    )
    foreach ($asset in $fixtureAssets) {
        [IO.File]::WriteAllBytes((Join-Path $fixtureSample $asset.name), [byte[]]@($asset.byte))
    }

    $fixtureManifest = Join-Path $tempRoot 'fixture-manifest.json'
    $manifestAssets = @($fixtureAssets | ForEach-Object {
        [ordered]@{
            set = 'Sample'
            relativePath = 'Sample/' + $_.name
            size = 1
            sha256 = $_.sha256
        }
    })
    [IO.File]::WriteAllText(
        $fixtureManifest,
        ([ordered]@{ assets = $manifestAssets } | ConvertTo-Json -Depth 5),
        [Text.UTF8Encoding]::new($false))

    $verifier = Join-Path $repoRoot 'tools\verify_skybox_assets.ps1'
    $verifyOutput = & pwsh -NoProfile -File $verifier -SkyboxRoot $fixtureRoot -SetName Sample -ManifestPath $fixtureManifest 2>&1
    Assert-True ($LASTEXITCODE -eq 0) "valid fixture skybox set was rejected: $($verifyOutput -join [Environment]::NewLine)"

    [IO.File]::WriteAllBytes(
        (Join-Path $fixtureSample 'BakerSampleDiffuseHDR.dds'),
        [byte[]]@([byte][char]'x'))
    $verifyOutput = & pwsh -NoProfile -File $verifier -SkyboxRoot $fixtureRoot -SetName Sample -ManifestPath $fixtureManifest 2>&1
    Assert-True ($LASTEXITCODE -ne 0) 'same-size fixture corruption was accepted by the capture preflight'
    Assert-True (($verifyOutput -join "`n") -match 'SHA-256 mismatch') 'capture preflight did not explain the fixture hash failure'

    . (Join-Path $repoRoot 'tools\readme_media_common.ps1')
    $captureProject = [pscustomobject]@{ number = '36'; skyboxIblSet = 'Sample' }
    $preflightRejectedCorruption = $false
    try {
        Assert-ProjectSkyboxPreflight -Project $captureProject -RepoRoot $repoRoot `
            -SkyboxRoot $fixtureRoot -ManifestPath $fixtureManifest
    }
    catch {
        $preflightRejectedCorruption = $_.Exception.Message -match 'SHA-256 mismatch'
    }
    Assert-True $preflightRejectedCorruption 'project capture preflight did not reject a same-size corrupt IBL file'

    [IO.File]::WriteAllBytes(
        (Join-Path $fixtureSample 'BakerSampleDiffuseHDR.dds'),
        [byte[]]@([byte][char]'a'))
    Assert-ProjectSkyboxPreflight -Project $captureProject -RepoRoot $repoRoot `
        -SkyboxRoot $fixtureRoot -ManifestPath $fixtureManifest

    $unrelatedProject = [pscustomobject]@{ number = '04' }
    Assert-ProjectSkyboxPreflight -Project $unrelatedProject -RepoRoot $repoRoot `
        -SkyboxRoot (Join-Path $tempRoot 'missing-unrelated-root') -ManifestPath $fixtureManifest

    & $msbuild $projectPath /t:Build /p:Configuration=Debug /p:Platform=x64 `
        "/p:RepoRoot=$repoRoot\" "/p:OutDir=$outDir\" "/p:IntDir=$intDir\" /m:1 /v:minimal
    if ($LASTEXITCODE -ne 0) {
        throw "native skybox validation test failed to build (exit $LASTEXITCODE)"
    }

    $testExe = Join-Path $outDir 'SkyboxAssetValidationTests.exe'
    Assert-True (Test-Path -LiteralPath $testExe -PathType Leaf) 'native skybox validation test executable missing'
    & $testExe
    if ($LASTEXITCODE -ne 0) {
        throw "native skybox validation behavior test failed (exit $LASTEXITCODE)"
    }
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

'Skybox asset bootstrap behavior tests passed'
