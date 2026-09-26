add_executable(VaseConsole main.cpp Shell.cpp Console.cpp PlanFile.cpp)

# 宿主侧链 VaseHost（它 PUBLIC 带 VasePod）+ VaseCatalog（T12/D45 链接方向：catalog 在上，
# 主链 pod new 与 AdoptInto 都住在它里面；raw 旁路不碰清单面）。VaseThirdPartyCli 的接入方式与
# 「为什么不是 add_subdirectory / vcpkg」写在 Cmake/VaseThirdParty.cmake。
target_link_libraries(VaseConsole
    PRIVATE VaseBuildOptions
            VaseHost
            VaseCatalog
            VaseThirdPartyCli)

# get 要认识 samples::IGreeter（TryGet 的类型实参），它住在 Samples/HelloCommon。
# 命令层因此直接看见服务接口——这正是「宿主不需要插件的实现，只需要标识」的形态。
target_include_directories(VaseConsole PRIVATE "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")

# 回放资产用 file(GENERATE) 而不是 configure_file：产物路径要到 generate 期才求值，
# configure_file 拿不到 $<TARGET_FILE:...>（spec §7.2 的待实测项，此处即答案）。
set(VASE_CONSOLE_REPLAY_DIR "${CMAKE_CURRENT_BINARY_DIR}/replay")
set(VASE_CONSOLE_MANIFEST_DIR "${VASE_CONSOLE_REPLAY_DIR}/manifests")
set(VASE_CONSOLE_PRESET_DIR "${VASE_CONSOLE_REPLAY_DIR}/preset")
set(VASE_CONSOLE_HELLO_MANIFEST "${PROJECT_SOURCE_DIR}/Tests/Integration/fixtures/manifests/hello/plugin.json")
set(VASE_CONSOLE_SHARED_MANIFEST "${PROJECT_SOURCE_DIR}/Tests/Integration/fixtures/manifests/shared_provider/plugin.json")

# —— 生成的清单材料（比对生效后的 D57 逐字义务：全部按各插件 .cpp 描述符抄）——

# HelloPluginPrime 的清单（D87 双 install 的另一半）：结构抄 fixtures/manifests/hello/plugin.json，
# 只动三处漂移值——displayName「示例插件（prime 版）」、Mood 默认「响亮」（kLoud=1）；
# binary 保持 HelloPlugin——换件写的是影子上的同一格。
file(GENERATE OUTPUT "${VASE_CONSOLE_MANIFEST_DIR}/hello_prime.json"
     CONTENT "{
    \"schemaVersion\": 1,
    \"id\": \"Vase.Hello\",
    \"displayName\": \"示例插件（prime 版）\",
    \"version\": \"0.1.0\",
    \"binary\": \"HelloPlugin\",
    \"provides\": [ { \"service\": \"Vase.Hello.Greeter\", \"version\": 1 } ],
    \"config\": [
        { \"key\": \"Repeats\", \"type\": \"int32\", \"default\": 1, \"displayName\": \"问候次数\" },
        { \"key\": \"MoodValue\", \"type\": \"enum\", \"displayName\": \"情绪\",
          \"choices\": [ { \"value\": 0, \"label\": \"安静\" }, { \"value\": 1, \"label\": \"响亮\" } ],
          \"default\": \"响亮\" }
    ]
}
")

# CollisionProviderPlugin 的清单（fixtures/manifests/ 下没有它，随 adopt_blocked 重造在此生成；
# 展示字段按 Tests/Integration/fixtures/CollisionProviderPlugin/CollisionProviderPlugin.cpp 逐字）。
file(GENERATE OUTPUT "${VASE_CONSOLE_MANIFEST_DIR}/collision_provider.json"
     CONTENT "{
    \"schemaVersion\": 1,
    \"id\": \"Vase.CollisionProvider\",
    \"displayName\": \"重复提供者探针\",
    \"version\": \"0.0.1\",
    \"binary\": \"CollisionProviderPlugin\",
    \"provides\": [ { \"service\": \"Vase.Test.Shared\", \"version\": 1 } ]
}
")

# SharedProvider 的 enabledByDefault=false 变体（solve 展示面的 skip 行）。
file(GENERATE OUTPUT "${VASE_CONSOLE_MANIFEST_DIR}/shared_provider_disabled.json"
     CONTENT "{
    \"schemaVersion\": 1,
    \"id\": \"Vase.SharedProvider\",
    \"displayName\": \"共享服务提供方探针\",
    \"version\": \"0.0.1\",
    \"binary\": \"SharedProviderPlugin\",
    \"enabledByDefault\": false,
    \"provides\": [ { \"service\": \"Vase.Test.Shared\", \"version\": 1 } ]
}
")

# adopt_blocked 的两份 preset（D82 面的旁证）：同目录双插件的快照，各局只放一只进局——
# 双 kLoad 会在计划级碰撞执法处整局拒掉，到不了 Adopt（M1 时代的两份 plan 同因）。
file(GENERATE OUTPUT "${VASE_CONSOLE_PRESET_DIR}/only_collision.json"
     CONTENT "{
    \"schemaVersion\": 1,
    \"displayName\": \"只放 CollisionProvider 进局\",
    \"overrides\": { \"Vase.SharedProvider\": { \"enabled\": false } }
}
")
file(GENERATE OUTPUT "${VASE_CONSOLE_PRESET_DIR}/only_shared.json"
     CONTENT "{
    \"schemaVersion\": 1,
    \"displayName\": \"只放 SharedProvider 进局\",
    \"overrides\": { \"Vase.CollisionProvider\": { \"enabled\": false } }
}
")

# —— catalog 主链的回放树：每次 ctest 由 VaseConsoleCatalogStage 从**任何用例都不写**的
# 源刷回已知初始态（影子纪律：内容取决于跑过几次的记录不是证据）。
# 每个写盘家族独占一棵根目录——一个家族跑完留下的脏态不许成为下一个家族的起点。
set(VASE_STAGE_SCRIPT "")
function(vase_console_stage_line subdir src dst_name)
    set(VASE_STAGE_SCRIPT "${VASE_STAGE_SCRIPT}
file(MAKE_DIRECTORY \"${VASE_CONSOLE_REPLAY_DIR}/${subdir}\")
execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different \"${src}\" \"${VASE_CONSOLE_REPLAY_DIR}/${subdir}/${dst_name}\" RESULT_VARIABLE vase_rc)
if(NOT vase_rc EQUAL 0)
  message(FATAL_ERROR \"catalog staging failed: ${src}\")
endif()
" PARENT_SCOPE)
endfunction()

# 双 install 同步链（synced 家族）：自持一棵 cat_hot——跑完留 (prime,prime) 的**一致**脏态，
# swaploop / behaviour 在同一家族之后起跑时比对照样过（两态皆自洽）。
vase_console_stage_line(cat_hot/HelloShadow $<TARGET_FILE:HelloPlugin> $<TARGET_FILE_NAME:HelloPlugin>)
vase_console_stage_line(cat_hot/HelloShadow ${VASE_CONSOLE_HELLO_MANIFEST} plugin.json)
# 局内 [安静] 证人（ReplayReason）独占一棵 cat_q：每棵根每次刷回只被它自己的用例写，
# 「从 (base,base) 起跑」就不再是注册顺序的函数（T14c 自足化，R9 先例对齐）。
vase_console_stage_line(cat_q/HelloShadow $<TARGET_FILE:HelloPlugin> $<TARGET_FILE_NAME:HelloPlugin>)
vase_console_stage_line(cat_q/HelloShadow ${VASE_CONSOLE_HELLO_MANIFEST} plugin.json)
# 换件不更清单 → adopt 拒（mismatch 家族）：IsFailure 与 Reason 各一棵根，起点互不污染。
vase_console_stage_line(cat_mm1/HelloSwap $<TARGET_FILE:HelloPlugin> $<TARGET_FILE_NAME:HelloPlugin>)
vase_console_stage_line(cat_mm1/HelloSwap ${VASE_CONSOLE_HELLO_MANIFEST} plugin.json)
vase_console_stage_line(cat_mm2/HelloSwap $<TARGET_FILE:HelloPlugin> $<TARGET_FILE_NAME:HelloPlugin>)
vase_console_stage_line(cat_mm2/HelloSwap ${VASE_CONSOLE_HELLO_MANIFEST} plugin.json)
# adopt_blocked 重造局：同目录双插件 + 两份 preset。
vase_console_stage_line(cat_ab/SharedProvider $<TARGET_FILE:SharedProviderPlugin> $<TARGET_FILE_NAME:SharedProviderPlugin>)
vase_console_stage_line(cat_ab/SharedProvider ${VASE_CONSOLE_SHARED_MANIFEST} plugin.json)
vase_console_stage_line(cat_ab/CollisionProvider $<TARGET_FILE:CollisionProviderPlugin> $<TARGET_FILE_NAME:CollisionProviderPlugin>)
vase_console_stage_line(cat_ab/CollisionProvider ${VASE_CONSOLE_MANIFEST_DIR}/collision_provider.json plugin.json)
# solve 展示面（只读）：一 load 一 skip。
vase_console_stage_line(cat_show/HelloView $<TARGET_FILE:HelloPlugin> $<TARGET_FILE_NAME:HelloPlugin>)
vase_console_stage_line(cat_show/HelloView ${VASE_CONSOLE_HELLO_MANIFEST} plugin.json)
vase_console_stage_line(cat_show/SharedOff $<TARGET_FILE:SharedProviderPlugin> $<TARGET_FILE_NAME:SharedProviderPlugin>)
vase_console_stage_line(cat_show/SharedOff ${VASE_CONSOLE_MANIFEST_DIR}/shared_provider_disabled.json plugin.json)

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/stage_catalog.cmake" CONTENT "${VASE_STAGE_SCRIPT}")

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/exit_only.txt"
     CONTENT "exit\n")

add_test(NAME VaseConsoleExitOnly
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/exit_only.txt")

add_test(NAME VaseConsoleCatalogStage
         COMMAND ${CMAKE_COMMAND} -P "${VASE_CONSOLE_REPLAY_DIR}/stage_catalog.cmake")
# FIXTURES_SETUP：任何 catalog 家族的窄跑（-R 驱动）都自带刷回，不靠「先跑过别的」的残留。
set_tests_properties(VaseConsoleCatalogStage PROPERTIES FIXTURES_SETUP VaseConsoleCatalog)

# 默认判据（退出码 0）即足够：rc=0 已由规则 ② 蕴含 "clean=true"，
# 故此处**不配** PASS_REGULAR_EXPRESSION（配了反而吞掉退出码判定——实测它只看正则命中）。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/plan.txt"
     CONTENT "# 由 CMake 生成，不要手改：路径要到 generate 期才知道\nVase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPlugin>>\n")

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/pod_cycle.txt"
     CONTENT "pod new-raw ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
pod list
pod destroy
exit
")
add_test(NAME VaseConsolePodCycle
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/pod_cycle.txt")

# 规则 ③：脚本没走 exit → 非零。WILL_FAIL 断言的就是"这条必须失败"。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/no_exit.txt"
     CONTENT "pod new-raw ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
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
     CONTENT "pod new-raw ${VASE_CONSOLE_REPLAY_DIR}/bad_plan.txt
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
     CONTENT "pod new-raw ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
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

# —— 三档证据的端到端路径：eject + adopt 各三轮，全程不碰磁盘（T12 起吃 catalog 主链——
# raw 局的 adopt 已无路径账可查，只剩响亮拒绝，见下面的 RawAdoptRefused 家族）。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/swaploop.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_hot
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
set_tests_properties(VaseConsoleSwapLoop VaseConsoleSwapLoopReason PROPERTIES
    FIXTURES_REQUIRED VaseConsoleCatalog
    RESOURCE_LOCK VaseConsoleCatalogTree)
set_tests_properties(VaseConsoleSwapLoopReason PROPERTIES
    PASS_REGULAR_EXPRESSION "hotswap log \\(6\\)")

# —— D87 双 install 同步链（synced 家族）：本验证台相对 gtest 的唯一增量再进一格——
# 换件必须**二进制与清单两半同覆盖**，adopt 的加载期比对才放行。影子由
# VaseConsoleCatalogStage 每次 ctest 开头刷回（内容与换件源永不被用例写）。
# ⚠ 判据力按平台不对称（规矩 6）：file install 在 Windows 上是 sharing-violation 探针、
#   在 Linux 上是 truncate-in-place 空转，swap 仍被 adopt 的档三身份比对证明。
# T14c 自足化：换件腿之后**拆局重建**（盘上此时已是 prime 双半，重 scan 铸新 plan）——
# 局内 label 跟建局清单走（adopted 实例按 D34 回放 plan 时 blob，非缺陷），重建局才打「响亮」。
# 两级证人由此都吃**脚本自己**造出的态，不再依赖同族用例的先后（R9：窄 -R 跑必须自足）。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_hot
get
eject Vase.Hello
file stage Vase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPluginPrime>>
file install Vase.Hello
file stage-manifest Vase.Hello ${VASE_CONSOLE_MANIFEST_DIR}/hello_prime.json
file install-manifest Vase.Hello
adopt Vase.Hello
get
file show Vase.Hello
pod destroy
pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_hot
get
pod destroy
exit
")
# 族名一律带 Adopt（T14 更名）：五条全被规矩 6 的窄选择子 `-R 'HotSwap|Eject|Adopt'` 拉进热插拔验证。
add_test(NAME VaseConsoleAdoptManifestSynced
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
# 兄弟用例：换件是否**真的**发生，退出码看不见（CLI 不比较串）——只能钉文本。
# 本条钉重建局的 `prime v2 x1 [响亮]`：fresh (base,base) 起跑出自脚本重建腿，(prime,prime)
# 起跑出自第一条 get——两态恒真，单跑自足（needle 不与 ReplayReason 的 [安静] 互撞）。
# ⚠ 此条无视退出码，码由上面的 VaseConsoleAdoptManifestSynced 钉（spec §2.19）。
add_test(NAME VaseConsoleAdoptManifestSyncedReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
# 兄弟用例 1b（T14c）：D34 回放语义的诚实证人——cat_q 每次从 (base,base) 刷回、只被本用例写，
# 换件腿后的 get 恒打 `prime v2 x1 [安静]`（局内 label 跟建局清单走；响亮归重建腿/§7 正文）。
add_test(NAME VaseConsoleAdoptManifestSyncedReplayReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/quiet_swap.txt")
# quiet_swap：局内 [安静] 证人专用腿，吃 cat_q 自持根（脚本自己把该根推进 (prime,base→prime)
# 脏态，但每次 ctest 刷回后只被本用例写 → 起跑恒为 (base,base)，与同族先后无关）。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/quiet_swap.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_q
get
eject Vase.Hello
file stage Vase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPluginPrime>>
file install Vase.Hello
file stage-manifest Vase.Hello ${VASE_CONSOLE_MANIFEST_DIR}/hello_prime.json
file install-manifest Vase.Hello
adopt Vase.Hello
get
pod destroy
exit
")
# 兄弟用例 2：file show 是 Loader::FileIdentity 唯一的 CLI 消费者，它的报告形状是契约。
# 钉的是**平台中立**的那一段——identity kind 在 Windows 上是 pdbCodeView、Linux 上是
# elfBuildId，钉不得。同样只看正则、无视退出码。
add_test(NAME VaseConsoleAdoptManifestSyncedShowReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
# 兄弟用例 3：钉 PrintAdoptReport 真的跑过并打出了字段集（此前 adopt 只被退出码覆盖，
# 清空那个函数体也全绿——与 eject 的 VaseConsoleBehaviourEjectReason 同形，那条纪律还在这里守着）。
# T12 起行尾多一截 manifestVerified=true：它既证函数体，也证单轨后比对真的跟着成功态走。
# reusedResidentImage= 只出在 Report 行（CmdAdopt 的前置行是 registeredBy=），前缀即函数指纹。
add_test(NAME VaseConsoleAdoptManifestSyncedAdoptReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
set_tests_properties(VaseConsoleAdoptManifestSynced VaseConsoleAdoptManifestSyncedReason
    VaseConsoleAdoptManifestSyncedReplayReason VaseConsoleAdoptManifestSyncedShowReason
    VaseConsoleAdoptManifestSyncedAdoptReason
    PROPERTIES
    FIXTURES_REQUIRED VaseConsoleCatalog
    RESOURCE_LOCK VaseConsoleCatalogTree)
set_tests_properties(VaseConsoleAdoptManifestSyncedReason PROPERTIES
    PASS_REGULAR_EXPRESSION "prime v2 x1 \\[响亮\\]")
set_tests_properties(VaseConsoleAdoptManifestSyncedReplayReason PROPERTIES
    PASS_REGULAR_EXPRESSION "prime v2 x1 \\[安静\\]")
set_tests_properties(VaseConsoleAdoptManifestSyncedShowReason PROPERTIES
    PASS_REGULAR_EXPRESSION "show Vase.Hello path=.*size=[0-9]+ mtimeFileClockS=[0-9]+")
set_tests_properties(VaseConsoleAdoptManifestSyncedAdoptReason PROPERTIES
    PASS_REGULAR_EXPRESSION "adopt Vase.Hello reusedResidentImage=.* manifestVerified=true")

# —— D87 的反半句：换件只覆盖二进制、不更清单 → adopt 被加载期比对拒。
# IsFailure 与 Reason 各吃一棵刚刷回（base,base）的根：先跑的家族留下的 (prime,base)
# 脏态不许改变后跑那一条的拒绝路径（起点决定证人钉的是 adopt 还是 CreatePod）。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/mismatch_1.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_mm1
eject Vase.Hello
file stage Vase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPluginPrime>>
file install Vase.Hello
adopt Vase.Hello
exit
")
add_test(NAME VaseConsoleAdoptManifestMismatchIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/mismatch_1.txt")
set_tests_properties(VaseConsoleAdoptManifestMismatchIsFailure PROPERTIES
    WILL_FAIL TRUE
    FIXTURES_REQUIRED VaseConsoleCatalog
    RESOURCE_LOCK VaseConsoleCatalogTree)
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/mismatch_2.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_mm2
eject Vase.Hello
file stage Vase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPluginPrime>>
file install Vase.Hello
adopt Vase.Hello
exit
")
# 兄弟用例：钉拒因 token（spec §6：一个子串即够，字段细节留给人）。只看正则、无视退出码。
# 用例名含 Adopt：规矩 6 的窄选择子把它拉进热插拔验证。
add_test(NAME VaseConsoleAdoptManifestMismatchReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/mismatch_2.txt")
set_tests_properties(VaseConsoleAdoptManifestMismatchReason PROPERTIES
    PASS_REGULAR_EXPRESSION "manifest/binary mismatch"
    FIXTURES_REQUIRED VaseConsoleCatalog
    RESOURCE_LOCK VaseConsoleCatalogTree)

# —— D85 的 raw 旁路面：raw 局没有清单来源，adopt 与清单换件命令都响亮拒绝。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/raw_adopt_refused.txt"
     CONTENT "pod new-raw ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
adopt Vase.Hello
file stage-manifest Vase.Hello ${VASE_CONSOLE_MANIFEST_DIR}/hello_prime.json
pod destroy
exit
")
add_test(NAME VaseConsoleRawAdoptRefusedIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/raw_adopt_refused.txt")
set_tests_properties(VaseConsoleRawAdoptRefusedIsFailure PROPERTIES WILL_FAIL TRUE)
# 兄弟用例：钉 D85 契约文本（含 adopt 与 stage-manifest 两条拒绝行，同 token）。
# 用例名含 Adopt：进规矩 6 的窄选择子。
add_test(NAME VaseConsoleRawAdoptRefusedReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/raw_adopt_refused.txt")
set_tests_properties(VaseConsoleRawAdoptRefusedReason PROPERTIES
    PASS_REGULAR_EXPRESSION "catalog-backed")

# —— solve 展示面：catalog refresh + catalog solve 的输出形状（只读，不写影子）。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/solve_showcase.txt"
     CONTENT "catalog refresh ${VASE_CONSOLE_REPLAY_DIR}/cat_show
catalog solve
exit
")
add_test(NAME VaseConsoleSolveShowcase
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/solve_showcase.txt")
add_test(NAME VaseConsoleSolveShowcaseReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/solve_showcase.txt")
set_tests_properties(VaseConsoleSolveShowcaseReason PROPERTIES
    PASS_REGULAR_EXPRESSION "Vase\\.Hello load")
# 兄弟用例 2：enabledByDefault=false 走静态跳过——skip 行是 Solve 决策面的另一半。
add_test(NAME VaseConsoleSolveShowcaseSkipReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/solve_showcase.txt")
set_tests_properties(VaseConsoleSolveShowcaseSkipReason PROPERTIES
    PASS_REGULAR_EXPRESSION "Vase\\.SharedProvider skip disabled")
set_tests_properties(VaseConsoleSolveShowcase VaseConsoleSolveShowcaseReason
    VaseConsoleSolveShowcaseSkipReason
    PROPERTIES FIXTURES_REQUIRED VaseConsoleCatalog)

# —— 命令面收官：emit / plugins 与「取不到服务是有效观察」（T12 起吃 catalog 主链：
# 脚本里有条 adopt，raw 局走不动了；不写盘，起点态 base/prime 皆自洽）。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_hot
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
# 兄弟用例 2：钉 PrintEjectReport 真的跑过并打出了字段集（此前 eject 只被退出码覆盖，
# 清空那个函数体也全绿）。钉的是平台中立的那一段——两个 Is… 声明字段按平台变（§8.2），钉不得。
# 同样只看正则、无视退出码。
add_test(NAME VaseConsoleBehaviourEjectReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
# 兄弟用例 3 / 4：钉命令各自的输出行——清空对应函数体也全绿的那类缺口就靠它们封。
# 钉的都是我们自己的固定文本：单插件局下 live 恒为 1、脚本第一条 emit 恒为 seq=1，两平台同值。
add_test(NAME VaseConsoleBehaviourPluginsReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
add_test(NAME VaseConsoleBehaviourEmitReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
set_tests_properties(VaseConsoleBehaviour VaseConsoleBehaviourReason VaseConsoleBehaviourEjectReason
    VaseConsoleBehaviourPluginsReason VaseConsoleBehaviourEmitReason
    PROPERTIES
    FIXTURES_REQUIRED VaseConsoleCatalog
    RESOURCE_LOCK VaseConsoleCatalogTree)
set_tests_properties(VaseConsoleBehaviourReason PROPERTIES
    PASS_REGULAR_EXPRESSION "no provider")
set_tests_properties(VaseConsoleBehaviourEjectReason PROPERTIES
    PASS_REGULAR_EXPRESSION "eject Vase.Hello binaryUnloaded=")
set_tests_properties(VaseConsoleBehaviourPluginsReason PROPERTIES
    PASS_REGULAR_EXPRESSION "plugins live=1")
set_tests_properties(VaseConsoleBehaviourEmitReason PROPERTIES
    PASS_REGULAR_EXPRESSION "emitted GreetEvent seq=1")

# eject 一个不存在于任何局/快照的 id → 响亮拒绝 → 非零（eject 不依赖 catalog，raw 照旧）
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/unknown_id.txt"
     CONTENT "pod new-raw ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
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

# —— R9-2（T9 移交）：CmdEject Status 拒绝分支的回放证人（eject 不依赖 catalog，留 raw）。
# 这对的 plan 是提供方+消费者：eject 提供方必被账本拦下（D21 的 Ok + Status），
# 点名单要**真的打印出来**才算字段驱动——退出码只说「不干净」，说不出是谁挡的。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/blocked_plan.txt"
     CONTENT "# 由 CMake 生成，不要手改：提供方在前、消费者在后（装配预检要求拓扑序）\nVase.SharedProvider $<PATH:CMAKE_PATH,$<TARGET_FILE:SharedProviderPlugin>>\nVase.SharedConsumer2 $<PATH:CMAKE_PATH,$<TARGET_FILE:SharedConsumer2Plugin>>\n")
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/eject_blocked.txt"
     CONTENT "pod new-raw ${VASE_CONSOLE_REPLAY_DIR}/blocked_plan.txt
eject Vase.SharedProvider
exit
")
add_test(NAME VaseConsoleEjectBlockedIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/eject_blocked.txt")
set_tests_properties(VaseConsoleEjectBlockedIsFailure PROPERTIES WILL_FAIL TRUE)
# 兄弟用例：钉拒绝行进名了消费者（判据从 EjectReport::Consumers 字段现拼）。
# ⚠ 只看正则、**无视退出码**，码由上面那条钉——两者不可合并（同 VaseConsoleBadPlan 系列的
#    ctest 4.4.3 实测规则）。用例名带 Eject，规矩 6 的窄选择子 `-R 'HotSwap|Eject|Adopt'`
#    能把这对一并拉进热插拔验证。
add_test(NAME VaseConsoleEjectBlockedReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/eject_blocked.txt")
set_tests_properties(VaseConsoleEjectBlockedReason PROPERTIES
    PASS_REGULAR_EXPRESSION "eject refused: Vase\\.SharedProvider is provided by \\[Vase\\.SharedConsumer2\\]")

# —— R10-2（T10 裁定）的 T12 重造：console adopt 拒绝分支的端到端证人。
# M1 形（两份 raw plan 借 Host 级路径账跨局捞 Id）随 KnownBinaries 退役——这就是需求方
# 预告过「语义撑不过退役」的那对，诚实替代是**同目录双插件 + 两份 preset**：
# 快照里两只都在（adopt 才找得到 Id），计划里各局只放一只（双 kLoad 会在计划级碰撞执法
# 处整局拒掉，到不了 Adopt）。preset 走 LoadPreset 主链，顺带是 preset 文件面的活体消费者。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/adopt_blocked.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_ab ${VASE_CONSOLE_PRESET_DIR}/only_collision.json
pod destroy
pod new ${VASE_CONSOLE_REPLAY_DIR}/cat_ab ${VASE_CONSOLE_PRESET_DIR}/only_shared.json
adopt Vase.CollisionProvider
exit
")
add_test(NAME VaseConsoleAdoptBlockedIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/adopt_blocked.txt")
# 兄弟用例：钉点名行——服务、版本、现提供者归因三段都在（谁挡的要看得见）。
# ⚠ 只看正则、**无视退出码**，码由上面那条钉（不可合并规则同 VaseConsoleBadPlan 系列）。
# 用例名带 Adopt：规矩 6 的窄选择子 `-R 'HotSwap|Eject|Adopt'` 能把这对拉进热插拔验证。
add_test(NAME VaseConsoleAdoptBlockedReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/adopt_blocked.txt")
set_tests_properties(VaseConsoleAdoptBlockedIsFailure VaseConsoleAdoptBlockedReason PROPERTIES
    FIXTURES_REQUIRED VaseConsoleCatalog
    RESOURCE_LOCK VaseConsoleCatalogTree)
set_tests_properties(VaseConsoleAdoptBlockedIsFailure PROPERTIES WILL_FAIL TRUE)
set_tests_properties(VaseConsoleAdoptBlockedReason PROPERTIES
    PASS_REGULAR_EXPRESSION "adopt refused: provides collision \\[Vase\\.Test\\.Shared@1 by Vase\\.SharedProvider\\]")
