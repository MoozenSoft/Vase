add_executable(VaseConsole main.cpp Shell.cpp Commands.cpp PlanFile.cpp)

# 宿主侧只链 VaseHost（它 PUBLIC 带 VasePod）。VaseThirdPartyCli 的接入方式与
# 「为什么不是 add_subdirectory / vcpkg」写在 Cmake/VaseThirdParty.cmake。
target_link_libraries(VaseConsole
    PRIVATE VaseBuildOptions
            VaseHost
            VaseThirdPartyCli)

# get 要认识 samples::IGreeter（TryGet 的类型实参），它住在 Samples/HelloCommon。
# 命令层因此直接看见服务接口——这正是「宿主不需要插件的实现，只需要标识」的形态。
target_include_directories(VaseConsole PRIVATE "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")

# 回放资产用 file(GENERATE) 而不是 configure_file：产物路径要到 generate 期才求值，
# configure_file 拿不到 $<TARGET_FILE:...>（spec §7.2 的待实测项，此处即答案）。
set(VASE_CONSOLE_REPLAY_DIR "${CMAKE_CURRENT_BINARY_DIR}/replay")

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/exit_only.txt"
     CONTENT "exit\n")

add_test(NAME VaseConsoleExitOnly
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/exit_only.txt")

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/plan.txt"
     CONTENT "# 由 CMake 生成，不要手改：路径要到 generate 期才知道\nVase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPlugin>>\n")

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/pod_cycle.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
pod list
pod destroy
exit
")
add_test(NAME VaseConsolePodCycle
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/pod_cycle.txt")
# 默认判据（退出码 0）即足够：rc=0 已由规则 ② 蕴含 "clean=true"，
# 故此处**不配** PASS_REGULAR_EXPRESSION（配了反而吞掉退出码判定——实测它只看正则命中）。

# 规则 ③：脚本没走 exit → 非零。WILL_FAIL 断言的就是"这条必须失败"。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/no_exit.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
pod destroy
")
add_test(NAME VaseConsoleNoExitIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/no_exit.txt")
set_tests_properties(VaseConsoleNoExitIsFailure PROPERTIES WILL_FAIL TRUE)

# plan 里那条路径不存在 → 建局前就报错 → 非零
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/bad_plan.txt"
     CONTENT "Vase.Nope /definitely/not/here.dll
")
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/bad_plan_script.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/bad_plan.txt
exit
")
add_test(NAME VaseConsoleBadPlan
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/bad_plan_script.txt")
set_tests_properties(VaseConsoleBadPlan PROPERTIES WILL_FAIL TRUE)
# 兄弟用例：钉拒因文本（实测报错是消息契约，漂移要响）。
# ⚠ 它只看正则、**无视退出码**；码由上面 VaseConsoleBadPlan 钉。两者不可合并——
#    ctest 4.4.3 实测：同设 WILL_FAIL + PASS_REGULAR_EXPRESSION 时，命中反而判 Failed。
add_test(NAME VaseConsoleBadPlanReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/bad_plan_script.txt")
set_tests_properties(VaseConsoleBadPlanReason PROPERTIES
    PASS_REGULAR_EXPRESSION "binary not found")

# 规则 ① 后半：敲错的命令名必须算失败（实测修复前 rc=0）
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/unknown_command.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
pod destroy
ejct Vase.Hello
exit
")
add_test(NAME VaseConsoleUnknownCommandIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/unknown_command.txt")
set_tests_properties(VaseConsoleUnknownCommandIsFailure PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseConsoleUnknownCommandReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/unknown_command.txt")
set_tests_properties(VaseConsoleUnknownCommandReason PROPERTIES
    PASS_REGULAR_EXPRESSION "wrong command")

# 三档证据的端到端路径：eject + adopt 各三轮，全程不碰磁盘。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/swaploop.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
swap Vase.Hello 3
pod destroy
exit
")
add_test(NAME VaseConsoleSwapLoop
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/swaploop.txt")
# 兄弟用例（判据形状同 VaseConsoleBadPlan）：`hotswap log (6)` 是 3 轮进出的确切条数，
# 退出码表达不了「报告真的在动」。它只看正则、**无视退出码**，码由上面那条钉。
add_test(NAME VaseConsoleSwapLoopReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/swaploop.txt")
set_tests_properties(VaseConsoleSwapLoopReason PROPERTIES
    PASS_REGULAR_EXPRESSION "hotswap log \\(6\\)")

# —— 换件路径（spec §3.2 推论 1 / §12.1 的 A′）：本验证台相对 gtest 的唯一增量 ——
#
# **影子路径**：换件脚本登记的不是 $<TARGET_FILE:HelloPlugin>，而是 replay/ 下的一份拷贝。
# 理由是实测出来的：直接覆盖 bin/HelloPlugin.dll 会把它**永久**变成 prime 字节——覆盖把它
# 的 mtime 顶到比 .obj 新，ninja 判定它 up-to-date，`cmake --build` 报 "no work to do" 而
# 从不恢复它。于是同一份脚本第二次跑就不再演示「变化」（第一次 get 已经是 prime 串），
# 而验证台的全部价值在于这份记录是证据——一份内容取决于跑过几次的记录不是证据。
# 影子由 VaseConsoleSwapStage 在每次 ctest 开头从**任何用例都不会写**的 HelloPlugin.dll 刷回，
# 于是第 N 次的记录与第 1 次逐字相同。
# file stage 的**源**仍是 $<TARGET_FILE:HelloPluginPrime>：换的是影子上的字节，覆盖语义不变。
set(VASE_CONSOLE_SWAP_TARGET "${VASE_CONSOLE_REPLAY_DIR}/swap_target.dll")

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/swap_plan.txt"
     CONTENT "# 由 CMake 生成，不要手改：换件脚本登记的是影子路径\nVase.Hello ${VASE_CONSOLE_SWAP_TARGET}\n")
add_test(NAME VaseConsoleSwapStage
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:HelloPlugin> "${VASE_CONSOLE_SWAP_TARGET}")
# FIXTURES_SETUP 而不是 DEPENDS：DEPENDS 只在**选中集合之内**排序、不会把依赖拉进来，
# 所以「影子还没刷过」的树上窄跑 `-R VaseConsoleHotSwap` 会三连红——而选择子驱动的窄跑正是
# 本仓库验证的常态（CLAUDE.md 规矩 6，选择子字符串本身是承重的）。FIXTURES 会把 setup
# 用例自动并入任何需要它的选择集合，于是窄跑也自足。实测见 task-5-report.md §R9。
set_tests_properties(VaseConsoleSwapStage PROPERTIES FIXTURES_SETUP VaseConsoleSwap)
# copy_if_different 比的是**内容**：影子被上一轮换件写成 prime 之后，这一步把它刷回原始
# 字节；而它的源（HelloPlugin.dll）永不被任何用例写，所以「每次 ctest 开头影子 == 原始」
# 由构造成立，不靠任何清理约定。

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/swap_plan.txt
get
eject Vase.Hello
file stage Vase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPluginPrime>>
file install Vase.Hello
adopt Vase.Hello
get
file show Vase.Hello
pod destroy
exit
")
add_test(NAME VaseConsoleHotSwap
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
# 兄弟用例：换件是否**真的**发生，退出码看不见（CLI 不比较串）——只能钉文本。
# ⚠ 此条无视退出码，码由上面的 VaseConsoleHotSwap 钉（spec §2.19）。
# ⚠ 判据力按平台不对称：file install 在 Linux 上是 truncate-in-place 空转，swap 仍被
#    adopt 的档三身份比对证明（同 T12），两平台同跑得到的不是同一件事的两份拷贝。
add_test(NAME VaseConsoleHotSwapReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
# 兄弟用例 2：file show 是 Loader::FileIdentity 唯一的 CLI 消费者，它的报告形状是契约。
# 钉的是**平台中立**的那一段——identity kind 在 Windows 上是 pdbCodeView、Linux 上是
# elfBuildId，钉不得。同样只看正则、无视退出码。
add_test(NAME VaseConsoleHotSwapShowReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
# 兄弟用例 3：钉 PrintAdoptReport 真的跑过并打出了字段集（此前 adopt 只被退出码覆盖，
# 清空那个函数体也全绿——与 eject 的 VaseConsoleBehaviourEjectReason 同形，那条纪律还在这里守着）。
# 钉的是**平台中立**的那一段——identityVerified / importEnforcementPassed 是平台声明位（§4、§8.2），
# 钉不得；CmdAdopt 的前置行是 `adopt <id> registeredBy=`，只有 Report 行带 reusedResidentImage=，
# 故这个前缀同时证明是 PrintAdoptReport 打的。同样只看正则、无视退出码。
add_test(NAME VaseConsoleHotSwapAdoptReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
# 四条都跑同一份脚本，各自都会写影子上的字节（file install），故都要那个 fixture。
set_tests_properties(VaseConsoleHotSwap VaseConsoleHotSwapReason VaseConsoleHotSwapShowReason VaseConsoleHotSwapAdoptReason
    PROPERTIES
    FIXTURES_REQUIRED VaseConsoleSwap
    # 一把共享锁：这几条抢的是同一份影子，一条在写时另一条可能正映射着它，症状是随机的
    # sharing violation。今天六个 preset 都没有 -j、两个 verify 脚本也没有，所以是潜伏项，
    # 但封住它的成本就是这一行。
    RESOURCE_LOCK VaseConsoleSwapTarget)
set_tests_properties(VaseConsoleHotSwapReason PROPERTIES
    PASS_REGULAR_EXPRESSION "prime v2")
set_tests_properties(VaseConsoleHotSwapShowReason PROPERTIES
    PASS_REGULAR_EXPRESSION "show Vase.Hello path=.*size=[0-9]+ mtimeFileClockS=[0-9]+")
set_tests_properties(VaseConsoleHotSwapAdoptReason PROPERTIES
    PASS_REGULAR_EXPRESSION "adopt Vase.Hello reusedResidentImage=")

# —— 命令面收官：emit / plugins 与「取不到服务是有效观察」——
#
# 一条脚本同时压三件事：emit 派发、plugins 列表、eject 之后 get 拿不到提供方。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
emit 1
get
plugins
eject Vase.Hello
get
emit 2
adopt Vase.Hello
get
pod destroy
exit
")
add_test(NAME VaseConsoleBehaviour
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
# 兄弟用例：`no provider` 是一次**有效观察**（eject 后拿不到服务），不是失败，
# 所以退出码不会为它而变 —— 只能靠文本钉。⚠ 此条无视退出码（spec §2.19）。
add_test(NAME VaseConsoleBehaviourReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
set_tests_properties(VaseConsoleBehaviourReason PROPERTIES
    PASS_REGULAR_EXPRESSION "no provider")
# 兄弟用例 2：钉 PrintEjectReport 真的跑过并打出了字段集（此前 eject 只被退出码覆盖，
# 清空那个函数体也全绿）。钉的是平台中立的那一段——两个 Is… 声明字段按平台变（§8.2），钉不得。
# 同样只看正则、无视退出码。
add_test(NAME VaseConsoleBehaviourEjectReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
set_tests_properties(VaseConsoleBehaviourEjectReason PROPERTIES
    PASS_REGULAR_EXPRESSION "eject Vase.Hello binaryUnloaded=")
# 兄弟用例 3 / 4：钉本任务两条新命令各自的输出行——它们是命令面这一步的全部可观察产物，
# 没有它们，把 CmdPlugins / CmdEmit 的函数体换成 {} 也全绿（与兄弟用例 2 的成因同）。
# 钉的都是我们自己的固定前缀：单插件 plan 下 live 恒为 1、脚本第一条 emit 恒为 seq=1，
# 两平台同值。同样只看正则、无视退出码。
add_test(NAME VaseConsoleBehaviourPluginsReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
set_tests_properties(VaseConsoleBehaviourPluginsReason PROPERTIES
    PASS_REGULAR_EXPRESSION "plugins live=1")
add_test(NAME VaseConsoleBehaviourEmitReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
set_tests_properties(VaseConsoleBehaviourEmitReason PROPERTIES
    PASS_REGULAR_EXPRESSION "emitted GreetEvent seq=1")

# eject 一个不存在于任何 plan 的 id → 响亮拒绝 → 非零
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/unknown_id.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
eject Vase.Nowhere
exit
")
add_test(NAME VaseConsoleRefusedIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/unknown_id.txt")
set_tests_properties(VaseConsoleRefusedIsFailure PROPERTIES WILL_FAIL TRUE)
# 兄弟用例：钉拒因文本。⚠ 它只看正则、**无视退出码**（码由上面那条钉），
# 两者不可合并（同设 WILL_FAIL + PASS_REGULAR_EXPRESSION 时命中反而判 Failed，spec §2.19）。
# 钉 `plugin not in pod` 而不是 `eject refused`：后者只出在「被消费者挡住」那一条路径
# （§5.6 规则③），单插件脚本根本走不到；这条脚本走的是「本局无此 Id」那一条，其文案就是
# plugin not in pod。钉库的真实拒绝文本，才证到了 Host 的拒绝，而不是停在我们自己的前置检查上。
add_test(NAME VaseConsoleRefusedReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/unknown_id.txt")
set_tests_properties(VaseConsoleRefusedReason PROPERTIES
    PASS_REGULAR_EXPRESSION "plugin not in pod")
