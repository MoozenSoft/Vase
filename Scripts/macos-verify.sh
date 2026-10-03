#!/bin/bash
# 干净树两线全量（macOS x64）。在 macOS 开发机上本地跑。
set -u

# pkg-config 是 macOS 独有的环境前提：缺它 vcpkg 的 gtest port 会无条件倒在
# vcpkg_fixup_pkgconfig，症状是「什么都编不出来」而不是一条清楚的缺依赖。
if ! command -v pkg-config >/dev/null 2>&1; then
    echo "!! pkg-config 不在 PATH 上。vcpkg 的 gtest port 需要它。" >&2
    echo "!! MacPorts: sudo port install pkgconf   /   Homebrew: brew install pkg-config" >&2
    exit 1
fi

cd ~/WindowsGit/Vase || exit 1

run() {
    echo ""
    echo "########## RUN: $* ##########"
    "$@"
    echo "########## RC=$? : $* ##########"
}

for p in macos-x64-clang-debug macos-x64-clang-release; do
    rm -rf build-macos/$p
done

run cmake --preset macos-x64-clang-debug
run cmake --build --preset macos-x64-clang-debug
run ctest --preset macos-x64-clang-debug
run ctest --preset macos-x64-clang-debug -N

run cmake --preset macos-x64-clang-release
run cmake --build --preset macos-x64-clang-release
run ctest --preset macos-x64-clang-release
run ctest --preset macos-x64-clang-release -N

echo ""
echo "########## ALL LINES DONE ##########"
