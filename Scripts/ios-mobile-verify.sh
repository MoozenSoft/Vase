#!/usr/bin/env bash
# Native x64 host manifests + real iPhoneOS arm64 static archives/final Mach-O link.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${VASE_IOS_SDK_ROOT:?Set to the real iPhoneOS18.2.sdk}"
: "${VCPKG_ROOT:?Mac vcpkg root missing}"
if [[ ! -f "$VASE_IOS_SDK_ROOT/SDKSettings.plist" || "$VASE_IOS_SDK_ROOT" != *iPhoneOS18.2.sdk ]]; then
    echo "FAIL: VASE_IOS_SDK_ROOT must be iPhoneOS18.2.sdk" >&2
    exit 3
fi

host="build-macos/mobile-manifest-host-macos"
tree="build-ios/ios-arm64-clang-release"
app="$tree/bin/VaseMobileIosLinkProbe.app"
# Restore the previous valid app resources before any host configure/build.
cmake "-DVASE_IOS_APP_BUNDLE=$app" -DVASE_IOS_RECOVER_ONLY=ON \
    -P Cmake/StageIosBundleResources.cmake
cmake --preset mobile-manifest-host-macos
cmake --build "$host" --target VaseCheckMobileManifests
cmake --preset ios-arm64-clang-release
cmake --build "$tree" --target VaseMobileIosBundleAssets -j 4

bin="$app/VaseMobileIosLinkProbe"
nm="$(command -v llvm-nm-mp-23)"
objdump="$(command -v llvm-objdump-mp-23)"
python3 Scripts/VerifyMobileSymbols.py --macho "$bin" --llvm-nm "$nm" --llvm-objdump "$objdump"

for archive in \
    "$tree/lib/libVasePod.a" \
    "$tree/lib/libVaseHost.a" \
    "$tree/lib/libVaseCatalog.a" \
    "$tree/lib/libVaseMobileScenarioCore.a" \
    "$tree/pack-objects/libVasePackageProvider.a" \
    "$tree/pack-objects/libVasePackageConsumer.a" \
    "$tree/pack-objects/libPackageProviderPlugin.Pack.a" \
    "$tree/pack-objects/libPackageConsumerPlugin.Pack.a"; do
    actual="$(lipo -archs "$archive")"
    if [[ "$actual" != arm64 ]]; then
        echo "FAIL: $archive architecture $actual instead of arm64" >&2
        exit 4
    fi
    echo "ARCHIVE_OK: $archive arm64"
done

for sourceId in Provider Consumer; do
    source="$host/mobile-resources/vase/$sourceId/pack.json"
    target="$app/vase/$sourceId/pack.json"
    cmp "$source" "$target"
    echo "BUNDLE_RESOURCE_OK: $sourceId byte-for-byte"
done

# Code-signing and real device runtime are intentionally not claimed.
echo "PASS: real iPhoneOS SDK18.2/minOS15.0, arm64 archives/final link, resources byte-identical"
