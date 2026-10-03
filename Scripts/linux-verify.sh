#!/bin/bash
# T14: 干净树两线全量（Linux）。经登录 shell 跑：WSL 的 VCPKG_ROOT 只由 /etc/profile.d/vcpkg.sh 提供。
# 从脚本自身位置定位仓库根：与 win-verify.cmd 的 `%~dp0..` 同形，分发脚本里不写机器路径。
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd) || exit 1
cd "$SCRIPT_DIR/.." || exit 1

run() {
    echo ""
    echo "########## RUN: $* ##########"
    "$@"
    echo "########## RC=$? : $* ##########"
}

for p in linux-x64-clang-debug linux-x64-clang-release; do
    rm -rf build-linux/$p
done

run cmake --preset linux-x64-clang-debug
run cmake --build --preset linux-x64-clang-debug
run ctest --preset linux-x64-clang-debug
run ctest --preset linux-x64-clang-debug -N

run cmake --preset linux-x64-clang-release
run cmake --build --preset linux-x64-clang-release
run ctest --preset linux-x64-clang-release
run ctest --preset linux-x64-clang-release -N

echo ""
echo "########## ALL LINES DONE ##########"
