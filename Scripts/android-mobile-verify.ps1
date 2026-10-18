param(
    [switch]$Offline,
    [switch]$RunDeviceTests,
    [switch]$UseInstalledApks
)
if ($UseInstalledApks -and -not $RunDeviceTests) {
    throw 'UseInstalledApks requires RunDeviceTests'
}
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gradleRoot = Join-Path $root 'Samples/Mobile/Android/apk'
$hostBuild = Join-Path $root 'build-win/mobile-manifest-host'
$androidBuild = Join-Path $root 'build-android/android-arm64-clang-release'
$apkInputs = Join-Path $androidBuild 'apk-inputs'

function RunCommand([string]$name, [string]$exe, [string[]]$arguments) {
    Write-Host "RUN $name :: $exe $($arguments -join ' ')"
    & $exe @arguments
    $rc = $LASTEXITCODE
    Write-Host "$name RC=$rc"
    if ($rc -ne 0) { throw "$name failed with code $rc" }
}
if (-not $env:ANDROID_NDK_HOME -or -not (Test-Path (Join-Path $env:ANDROID_NDK_HOME 'source.properties'))) {
    throw 'ANDROID_NDK_HOME must point to installed NDK r30'
}
if (-not $env:ANDROID_HOME -or -not (Test-Path $env:ANDROID_HOME)) {
    throw 'ANDROID_HOME must point to Android SDK'
}
if (-not $env:JAVA_HOME -or -not (Test-Path (Join-Path $env:JAVA_HOME 'bin/java.exe'))) {
    throw 'JAVA_HOME must point to a JDK accepted by Gradle/AGP'
}
$env:PATH = (Join-Path $env:JAVA_HOME 'bin') + [IO.Path]::PathSeparator + $env:PATH
Write-Host "JAVA_HOME=$env:JAVA_HOME (Gradle validates the actual JDK version)"
# Recover an interrupted APK publication before any host configure or
# candidate regeneration can fail. The build target has the same dependency.
RunCommand 'recover prior APK inputs' cmake @(
    "-DVASE_MOBILE_OUTPUT=$apkInputs", '-DVASE_MOBILE_RECOVER_ONLY=ON',
    '-P', (Join-Path $root 'Cmake/StageMobileApkInputs.cmake'))
RunCommand 'native host config' cmake @('--preset', 'mobile-manifest-host')
RunCommand 'native host candidates' cmake @('--build', $hostBuild, '--target', 'VaseCheckMobileManifests')
RunCommand 'Android config' cmake @('--preset', 'android-arm64-clang-release')
RunCommand 'Android APK inputs' cmake @('--build', $androidBuild, '--target', 'VaseMobileApkInputs')
Push-Location $gradleRoot
try {
    $args = @('--no-daemon')
    if ($Offline) { $args += '--offline' }
    $args += @(':app:assembleDebug', ':app:assembleDebugAndroidTest')
    $gradle = if ($env:VASE_GRADLE_COMMAND) { $env:VASE_GRADLE_COMMAND } else { Join-Path $gradleRoot 'gradlew.bat' }
    if (-not (Test-Path $gradle)) { throw "Gradle command not found: $gradle" }
    RunCommand 'APK and instrumentation APK' $gradle $args
} finally { Pop-Location }

$apkFiles = @(
    (Join-Path $gradleRoot 'app/build/outputs/apk/debug/app-debug.apk'),
    (Join-Path $gradleRoot 'app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk')
)
foreach ($apk in $apkFiles) { if (-not (Test-Path $apk)) { throw "APK missing: $apk" } }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$readobj = Join-Path $env:ANDROID_NDK_HOME 'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-readobj.exe'
if (-not (Test-Path $readobj)) { throw "NDK llvm-readobj missing: $readobj" }
function GetElfBuildId([string]$path) {
    $lines = @(& $readobj --notes $path)
    if ($LASTEXITCODE -ne 0) { throw "llvm-readobj failed for $path" }
    $ids = @($lines | Select-String -Pattern '^\s*Build ID:\s*([0-9a-fA-F]+)\s*$')
    if ($ids.Count -ne 1) { throw "ELF must contain exactly one GNU Build ID: $path" }
    return $ids[0].Matches[0].Groups[1].Value.ToLowerInvariant()
}
function AssertApkNativeBuildId($zip, [string]$entryName, [string]$staged) {
    $entry = $zip.GetEntry($entryName)
    if ($null -eq $entry) { throw "APK native entry missing: $entryName" }
    $scratch = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
    try {
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $scratch)
        $expected = GetElfBuildId $staged
        $actual = GetElfBuildId $scratch
        if ($actual -ne $expected) { throw "APK native Build ID mismatch: $entryName (staged=$expected, packed=$actual)" }
    } finally {
        if (Test-Path $scratch) { Remove-Item -LiteralPath $scratch -Force }
    }
}
$archive = [IO.Compression.ZipFile]::OpenRead($apkFiles[0])
try {
    $names = @($archive.Entries | ForEach-Object FullName)
    $seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in $names) {
        if (-not $seen.Add($name)) { throw "APK has duplicate ZIP entry: $name" }
    }
    $expectedNative = @('lib/arm64-v8a/libVaseMobileJni.so', 'lib/arm64-v8a/libc++_shared.so')
    $unexpectedNative = @($names | Where-Object {
        $_ -match '^lib/' -and $_ -notmatch '/$' -and $_ -cnotin $expectedNative
    })
    if ($unexpectedNative.Count -ne 0) { throw "APK has unexpected native libraries: $($unexpectedNative -join ', ')" }
    foreach ($required in @('assets/vase/Provider/pack.json', 'assets/vase/Consumer/pack.json',
                             'lib/arm64-v8a/libVaseMobileJni.so', 'lib/arm64-v8a/libc++_shared.so')) {
        if ($required -cnotin $names) { throw "APK required entry missing: $required" }
    }
    if (@($names | Where-Object { $_ -match '/libc\+\+_shared\.so$' }).Count -ne 1) {
        throw 'APK must contain exactly one libc++_shared.so'
    }
    $expectedAssets = @('assets/vase/Provider/pack.json', 'assets/vase/Consumer/pack.json')
    $unexpectedAssets = @($names | Where-Object {
        $_ -match '^assets/vase/' -and $_ -notmatch '/$' -and $_ -cnotin $expectedAssets
    })
    if ($unexpectedAssets.Count -ne 0) {
        throw "APK contains unselected resource entries: $($unexpectedAssets -join ', ')"
    }
    foreach ($sourceId in @('Provider', 'Consumer')) {
        $path = "vase/$sourceId/pack.json"
        $entry = $archive.GetEntry("assets/$path")
        $source = Join-Path $hostBuild "mobile-resources/$path"
        if (-not (Test-Path $source)) { throw "Host candidate missing: $source" }
        $hash = [System.Security.Cryptography.SHA256]::Create()
        $stream = $entry.Open()
        $fileStream = [IO.File]::OpenRead($source)
        try {
            $actual = [BitConverter]::ToString($hash.ComputeHash($stream))
            $expected = [BitConverter]::ToString($hash.ComputeHash($fileStream))
            if ($actual -ne $expected) { throw "APK manifest differs from host candidate: $sourceId" }
        } finally { $stream.Dispose(); $fileStream.Dispose(); $hash.Dispose() }
    }
    foreach ($binary in @('libVaseMobileJni.so', 'libc++_shared.so')) {
        AssertApkNativeBuildId $archive "lib/arm64-v8a/$binary" (Join-Path $apkInputs "jniLibs/arm64-v8a/$binary")
    }
    Write-Host 'APK contents OK: 2 byte-identical manifests, JNI and one arm64 libc++ with matching ELF Build IDs'
} finally { $archive.Dispose() }

# The instrumentation APK must use only its own test resources, never silently
# bundle the target app's Provider/Consumer manifests.
$testArchive = [IO.Compression.ZipFile]::OpenRead($apkFiles[1])
try {
    $testEntries = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($entry in $testArchive.Entries) {
        if (-not $testEntries.Add($entry.FullName)) { throw "Instrumentation APK duplicate ZIP entry: $($entry.FullName)" }
        if ($entry.FullName -match '^assets/vase/') {
            throw "Instrumentation APK unexpectedly contains product resources: $($entry.FullName)"
        }
    }
} finally { $testArchive.Dispose() }

if ($RunDeviceTests) {
    $adb = Join-Path $env:ANDROID_HOME 'platform-tools/adb.exe'
    if (-not (Test-Path $adb)) { throw "adb not found: $adb" }
    $devices = @(& $adb devices | Where-Object { $_ -match '\tdevice$' })
    if ($devices.Count -ne 1) { throw 'Expected exactly one connected device. Instrumentation remains pending.' }
    $abi = (& $adb shell getprop ro.product.cpu.abilist).Trim()
    $api = (& $adb shell getprop ro.build.version.sdk).Trim()
    if ($abi.Split(',') -notcontains 'arm64-v8a' -or [int]$api -lt 29) { throw "Unsupported device: ABI=$abi API=$api" }

    $verifyScript = Join-Path $root 'Scripts/VerifyAndroidInstrumentation.py'
    $python = (Get-Command python -ErrorAction Stop).Source
    if (-not $UseInstalledApks) {
        # Connected tests can return Gradle RC0 after the device INSTALL failed.
        # Delete old results so a previously green report cannot hide a failed run.
        $reportDir = Join-Path $gradleRoot 'app/build/reports/androidTests/connected/debug'
        $resultDir = Join-Path $gradleRoot 'app/build/outputs/androidTest-results/connected/debug'
        foreach ($dir in @($reportDir, $resultDir)) {
            if (Test-Path $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
        }
        $testArgs = @('--no-daemon')
        if ($Offline) { $testArgs += '--offline' }
        $testArgs += ':app:connectedDebugAndroidTest'
        Push-Location $gradleRoot
        try { RunCommand 'connected instrumentation' $gradle $testArgs }
        finally { Pop-Location }
        RunCommand 'Gradle instrumentation report' $python @(
            '-B', $verifyScript, '--gradle-report', (Join-Path $reportDir 'index.html'),
            '--expected-tests', '8')
    }

    # Test only the current APK bits. In UseInstalledApks mode the user can
    # install both APKs on devices that disallow unattended USB installation.
    foreach ($item in @(
        @{ Package = 'com.moozen.vase.mobile'; File = $apkFiles[0] },
        @{ Package = 'com.moozen.vase.mobile.test'; File = $apkFiles[1] }
    )) {
        $installedPaths = @(& $adb shell pm path $item.Package | Where-Object { $_ -match '^package:' })
        if ($LASTEXITCODE -ne 0 -or $installedPaths.Count -ne 1) {
            throw "Expected exactly one installed APK for $($item.Package)"
        }
        $devicePath = $installedPaths[0].Substring('package:'.Length).Trim()
        $deviceHashLine = @(& $adb shell sha256sum $devicePath)
        if ($LASTEXITCODE -ne 0 -or $deviceHashLine.Count -ne 1 -or
            $deviceHashLine[0] -notmatch '^([0-9a-fA-F]{64})\s+') {
            throw "Could not hash installed APK for $($item.Package)"
        }
        $deviceHash = $Matches[1].ToLowerInvariant()
        $localHash = (Get-FileHash -LiteralPath $item.File -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($deviceHash -ne $localHash) {
            throw "Installed APK differs from freshly validated artifact: $($item.Package)"
        }
        Write-Host "Installed APK verified: $($item.Package) SHA256=$deviceHash"
    }

    $runner = 'com.moozen.vase.mobile.test/androidx.test.runner.AndroidJUnitRunner'
    $registered = @(& $adb shell pm list instrumentation | Where-Object {
        $_ -match '^instrumentation:com\.moozen\.vase\.mobile\.test/androidx\.test\.runner\.AndroidJUnitRunner '
    })
    if ($LASTEXITCODE -ne 0 -or $registered.Count -ne 1) {
        throw "Instrumentation runner is not installed: $runner"
    }
    $receipt = Join-Path $gradleRoot 'app/build/reports/androidTests/vase-device-instrumentation.log'
    $output = @(& $adb shell am instrument -w -r -e class 'com.moozen.vase.mobile.MobilePackageTest' $runner 2>&1)
    $adbRc = $LASTEXITCODE
    $output | Set-Content -LiteralPath $receipt -Encoding utf8
    if ($adbRc -ne 0) { throw "ADB instrumentation command failed RC=$adbRc; receipt=$receipt" }
    RunCommand 'ADB instrumentation acceptance' $python @(
        '-B', $verifyScript, '--adb-log', $receipt, '--expected-tests', '8')
}
