#pragma once

// §5.1 的输入形状，M1 手写形态（D12）：LoadPlan 是普通 struct，M2 的
// PluginCatalog::Solve 产出同一个类型——装配路径零返工。
// Ordered 的数组序**就是**加载与关停的序（加载正序 / 关停逆序，§5.4）；
// M1 没有求解器，「Ordered 已按拓扑序排好」由手写者保证（M2 起是 Solve 的构造性保证）。

#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

namespace vase
{

class Context; // 前向声明足矣：Stage0 回调只接引用，不定义它

struct LoadPlanEntry
{
    std::string_view Id;
    std::filesystem::path BinaryPath; // 绝对或相对 CWD；Host 内部绝对化（T6）
};

struct LoadPlan
{
    std::vector<LoadPlanEntry> Ordered;
};

struct PodOptions
{
    bool Strict = false; // 5.5：任一插件失败即整局失败。M1 实现「无级联」的基本形
    // （任一 Failed → 已建 Pod 全拆 → Err）；级联拆除等 M2 依赖图。
    std::function<void(Context&)> Stage0; // §5.3 阶段 0：宿主服务注册点
};

} // namespace vase
