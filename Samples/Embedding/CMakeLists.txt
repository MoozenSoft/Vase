add_executable(VaseEmbedding main.cpp)

# 宿主侧只链 VaseHost（它 PUBLIC 带 VasePod），插件是运行期经 Loader 装进来的——
# 没有任何链接期依赖，这是 D11「插件不依赖 Host」的另一半。
target_link_libraries(VaseEmbedding
    PRIVATE VaseBuildOptions
            VaseHost)

# 提供方与消费方共用 Greeter.h，它住在 Samples/HelloCommon。
target_include_directories(VaseEmbedding PRIVATE
    "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")

# 插件二进制的位置由构建系统给出：主程序不猜路径，也不扫目录。
# CMAKE_PATH 的包法与 Tests 相同（直接塞字符串字面量会在 Windows 留下反斜杠转义事故）。
target_compile_definitions(VaseEmbedding PRIVATE
    VASE_HELLO_PATH="$<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPlugin>>")

# 判据 #1 的 CI 化：连跑 20 局，全 Clean 才退 0。
# COMMAND 写 target 名时 CMake 换成产物完整路径；DLL 由可执行文件同目录（bin/）找到。
add_test(NAME EmbeddingLoop20 COMMAND VaseEmbedding loop 20)
