add_executable(VaseEmbedding main.cpp)

# 宿主侧只链 VaseHost（它 PUBLIC 带 VasePod），插件是运行期经 Loader 装进来的——
# 没有任何链接期依赖，这是 D11「插件不依赖 Host」的另一半。
target_link_libraries(VaseEmbedding
    PRIVATE VaseBuildOptions
            VaseHost)

# 提供方与消费方共用 Greeter.h，它住在 Samples/HelloCommon。
# AdoptExpectations.h（swapdemo 的 hello 期望工厂）住 Tests/TestingSupport——header-only、
# 无链接对象，开 include 即可；描述符事实不留第二份抄本（T12 裁定：include 优先于内联拷贝）。
target_include_directories(VaseEmbedding PRIVATE
    "${PROJECT_SOURCE_DIR}/Samples/HelloCommon"
    "${PROJECT_SOURCE_DIR}/Tests/TestingSupport")

# 插件二进制的位置由构建系统给出：主程序不猜路径，也不扫目录。
# CMAKE_PATH 的包法与 Tests 相同（直接塞字符串字面量会在 Windows 留下反斜杠转义事故）。
target_compile_definitions(VaseEmbedding PRIVATE
    VASE_HELLO_PATH="$<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPlugin>>")

# 判据 #1 的 CI 化：连跑 20 局，全 Clean 才退 0。
# COMMAND 写 target 名时 CMake 换成产物完整路径；插件路径由 $<TARGET_FILE> 直接给出（见上第 19 行），
# 本程序不猜也不扫目录——框架库那半边由宿主静态导入（见 CLAUDE.md「工具链 flag 是承重的」段）。
add_test(NAME EmbeddingLoop20 COMMAND VaseEmbedding loop 20)

# 前端字段集的机器守卫（M5 波末记账那笔）：这份打印此前**零 ctest 消费**——`loop 20` 不经过
# PrintEjectReport，清空那个函数体也全绿。值按平台变（§8.2），故只钉字段名序列；rc=0 一并承重。
add_test(NAME EmbeddingSwapDemo COMMAND VaseEmbedding swapdemo)
set_tests_properties(EmbeddingSwapDemo PROPERTIES
    PASS_REGULAR_EXPRESSION "eject Vase.Hello binaryUnloaded=[a-z]+ mappingRemoved=[a-z]+ reopenWritable=[a-z]+ mappingRemovalIsObservable=[a-z]+ reopenWritableIsMeaningful=[a-z]+")
