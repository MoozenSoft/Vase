#pragma once

// 包来源的共用形状（T1/T5/T6）：来源类别、静态绑定输入，以及「供 Host 建局前预检的整包信息」。
// 纯数据、不过 ABI 界的值形，故无导出宏（同 LoadPlan.h）。

#include "Vase/Host/ManifestExpectation.h"
#include "Vase/PluginDescriptor.h" // PackageEntryPoints

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vase
{

enum class PackageSourceKind : std::uint8_t
{
    kDynamic,
    kInProcess,
};

// 静态来源绑定的输入形（T5，spec §4.2）：Solve 只在本**次**同步调用借用 Entries 与 SourceId
// （SourceId 借调用方的串）；产出的计划复制函数指针，不留借用。
struct InProcessBinding
{
    std::string_view SourceId;
    PackageEntryPoints Entries;
};

// 参与包的整包信息（T5，spec §4.2）：Catalog 从来源表物化，供 Host 在建 Stage0/Pod 之前用
// 随后装配的同一镜像做整包校验。Members 含**被跳过的**包成员——只留本局 kLoad 集合就没有
// 「清单只列一员、实际多员」的比对面。全跳过包不进计划（免加载/绑定，global constraint）。
struct PackagePlanSource
{
    std::string SourceId;
    PackageSourceKind Kind = PackageSourceKind::kDynamic;
    std::filesystem::path BinaryPath; // 动态来源的库文件；进程内来源为空
    PackageEntryPoints Entries;       // 进程内来源的静态双入口；动态来源为空
    std::vector<ManifestExpectation> Members;
};

} // namespace vase
