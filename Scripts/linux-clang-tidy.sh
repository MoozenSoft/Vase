#!/bin/bash
# clang-tidy 门禁的 Linux 线（CLAUDE.md「静态检查与格式」里那条必须单独跑的）。
# 经登录 shell 跑——run-clang-tidy 要用到的工具链与 VCPKG_ROOT 由登录 shell 提供：
#
#     wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-clang-tidy.sh'
#
# 日志落在本脚本旁边而不是 /tmp：WSL 的 /tmp 随发行版关停就没了，而这三判据要连摘要行
# 一起读；放这里 Windows 侧能直接打开。它是 *.log，已被 .gitignore 盖住，不是漏 add 的产物。
#
# 退出码：三条判据全过才 0。run-clang-tidy 永远不因 warning 失败（.clang-tidy 的
# WarningsAsErrors 为空），只看它的退出码会把一屏正文 warning 读成全绿。

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd) || exit 1
cd "$SCRIPT_DIR/.." || exit 1
LOG="$SCRIPT_DIR/linux-clang-tidy.log"
BUILD_DIR=build-linux/linux-x64-clang-debug

# compile_commands.json 是 run-clang-tidy 唯一的输入；没有它时它的报错指向自己的
# 用法，看不出「这棵树还没配」。
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "[linux-clang-tidy] 缺 $BUILD_DIR/compile_commands.json —— 先跑 cmake --preset linux-x64-clang-debug" >&2
    exit 1
fi

run-clang-tidy -p "$BUILD_DIR" > "$LOG" 2>&1
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

# 摘要行。基数（49 个 TU / Suppressed 合计 133833 / NOLINT 命中 18）以 CLAUDE.md 的表
# 为准，这里不复制阈值，只把数打出来对。逐 TU 的明细不进 stdout，在日志里。
#
# 下面两个行数**天生不相等**，不是漏跑：compile_commands.json 有 50 条，而
# Tests/Unit/fixtures/LoadProbe/LoadProbe.cpp 同时编进 LoadProbe 与 UnloadProbe 两个
# fixture target，run-clang-tidy 按路径去重后是 49 个 TU（它打的 "49 files out of 49"
# 也是去重后的数）。所以 generated 行 50、Suppressed 行 49。
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
