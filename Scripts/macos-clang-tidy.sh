#!/bin/bash
# clang-tidy 门禁的 macOS 线（与 Linux 线同理：单独一条，别用别的线推它）。
# 在 macOS 开发机上跑；run-clang-tidy-mp-23 与 VCPKG_ROOT 由 zsh 的 .zshenv 提供，
# ssh 远程命令经 zsh 走、两者都可见。从 Windows 开发机经 ssh 触发时加 -o BatchMode=yes；
# 主机别名见本机 ssh config（不入库）：
#
#     ssh <host> 'bash ~/WindowsGit/Vase/Scripts/macos-clang-tidy.sh'
#
# 日志落在本脚本旁边：这三判据要连摘要行一起读，放这里 Windows 侧能直接打开。
# 它是 *.log，已被 .gitignore 盖住，不是漏 add 的产物。
#
# 退出码：三条判据全过才 0。run-clang-tidy 永远不因 warning 失败（.clang-tidy 的
# WarningsAsErrors 为空），只看它的退出码会把一屏正文 warning 读成全绿。

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd) || exit 1
cd "$SCRIPT_DIR/.." || exit 1
LOG="$SCRIPT_DIR/macos-clang-tidy.log"
BUILD_DIR=build-macos/macos-x64-clang-debug

# compile_commands.json 是 run-clang-tidy 唯一的输入；没有它时它的报错指向自己的
# 用法，看不出「这棵树还没配」。
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "[macos-clang-tidy] 缺 $BUILD_DIR/compile_commands.json —— 先跑 cmake --preset macos-x64-clang-debug" >&2
    exit 1
fi

# MacPorts 只装成 clang-tidy-mp-23、PATH 上没有裸名，而 run-clang-tidy 只按裸名找
# （本脚本首跑实测：rc=1 且正文双 0）。显式 -clang-tidy-binary 递进去。
TIDY_BIN=$(command -v clang-tidy-mp-23 || command -v clang-tidy || true)
if [ -z "$TIDY_BIN" ]; then
    echo "[macos-clang-tidy] 未找到 clang-tidy（MacPorts 名 clang-tidy-mp-23）" >&2
    exit 2
fi
run-clang-tidy-mp-23 -clang-tidy-binary "$TIDY_BIN" -p "$BUILD_DIR" > "$LOG" 2>&1
TIDY_RC=$?
echo "log=$LOG"
echo "1) run-clang-tidy 退出码 = $TIDY_RC"

BODY_ERR=$(grep -cE ': error: ' "$LOG")
BODY_WARN=$(grep -cE ': warning: ' "$LOG")
echo "2) 正文 error   = $BODY_ERR 条"
echo "3) 正文 warning = $BODY_WARN 条"
# 第三条只参考、不参与判定：凡是「文件:行:列」形态的正文行，含 note / remark。
# error 与 warning 都 0 而这一条不为 0 时，说明来了个新种类的诊断，值得去日志里看一眼。
echo "   （参考）正文形态行 = $(grep -cE '\.(cpp|h|hpp|cc|ixx):[0-9]+:[0-9]+:' "$LOG") 条"

# 摘要行。基数（各线 TU 数 / Suppressed 合计 / NOLINT 命中数）以 CLAUDE.md 的表为准，
# 这里不复制阈值，只把数打出来对。逐 TU 的明细不进 stdout，在日志里。
#
# 下面两个行数**天生不相等**，不是漏跑：compile_commands.json 的条目数比 TU 数多 1，而
# Tests/Unit/fixtures/LoadProbe/LoadProbe.cpp 同时编进 LoadProbe 与 UnloadProbe 两个
# fixture target，run-clang-tidy 按路径去重后少一个（它打的 "N files out of N"
# 也是去重后的数）。所以 generated 行比 Suppressed 行多 1。
echo "--- 摘要行 ---"
grep -E 'clang-tidy in [0-9]+ threads' "$LOG"
echo "摘要行数：warnings generated = $(grep -c 'warnings generated' "$LOG")，Suppressed = $(grep -c 'Suppressed [0-9]* warnings' "$LOG")"
grep -oE 'Suppressed [0-9]+ warnings' "$LOG" | awk '{s+=$2} END {printf "合计：suppressed = %d，", s+0}'
grep -oE '[0-9]+ NOLINT' "$LOG" | awk '{s+=$1} END {printf "NOLINT 命中 = %d\n", s+0}'

if [ "$BODY_ERR" -ne 0 ] || [ "$BODY_WARN" -ne 0 ]; then
    echo "--- 正文诊断所在处（前 10 条）---"
    grep -E '\.(cpp|h|hpp|cc|ixx):[0-9]+:[0-9]+:' "$LOG" | head -10
fi

if [ "$TIDY_RC" -eq 0 ] && [ "$BODY_ERR" -eq 0 ] && [ "$BODY_WARN" -eq 0 ]; then
    echo "########## TIDY GATE PASS ##########"
    exit 0
fi
echo "########## TIDY GATE FAIL ##########"
exit 1
