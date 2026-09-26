#pragma once

// §5.1 的输入形状，M2a 定形（D23）：提议形 + BinaryPath（Host 得知道文件在哪；M2b 由清单
// `binary` + 目录填）+ 三层合并后的 ResolvedConfig。kLoad 条目的数组序 = 加载序 = 关停逆序
// （§5.4）；应当是拓扑序——M2a 由手写者负责、装配预检兜底，M2b 起是 Solve 的构造性保证。
// Id 与 M1 同：借用计划拥有者持有的串，只在这次 CreatePod 调用期间有效。

#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/ManifestExpectation.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace vase
{

class Context; // 前向声明足矣：Stage0 回调只接引用

enum class LoadDecision : std::uint8_t
{
    kLoad,
    kSkip,
};

// 全部是静态跳过类（§4.4）。后两个值由 M2b 的 Solve 产出；M2a 手写计划至多用 kDisabled
// （「不进局但留在计划里示众」）。运行时跳过不进计划——它活在报告的 Skips 里（§5.1）。
enum class SkipReason : std::uint8_t
{
    kDisabled,
    kMissingDependency,
    kVersionMismatch,
};

struct LoadPlanEntry
{
    std::string_view Id;
    std::filesystem::path BinaryPath; // 绝对或相对 CWD；Host 内部绝对化（T6 原文）
    LoadDecision Decision = LoadDecision::kLoad;
    SkipReason Reason{};            // 仅 kSkip 有意义
    ConfigBlob ResolvedConfig = {}; // 缺字段回退 kFields 默认（D23）；空 = 全默认。
    // = {} 是必需的 NSDMI：省略尾字段的指定初始化点在本工具链是 error（同 T3 的 OptionalRequires{}）
    // NOLINTNEXTLINE(readability-redundant-member-init) NSDMI 为省略豁免所必需，同 ManifestExpectation.h。
    std::optional<ManifestExpectation> Expected = {}; // D67/D68：有值 = InspectBinary 闸后逐字段比对；
    // 空 = M2a 行为原样（手写计划的既有测试材料零改动）。拥有值形，plan 拷贝/移动自包含。
};

struct LoadPlan
{
    std::vector<LoadPlanEntry> Ordered;
};

struct PodOptions
{
    bool Strict = false;                  // §5.5 + D36：Strict 只对 Failed 触发；预检/静态跳过不算失败
    std::function<void(Context&)> Stage0; // §5.3 阶段 0：宿主服务注册点
};

} // namespace vase
