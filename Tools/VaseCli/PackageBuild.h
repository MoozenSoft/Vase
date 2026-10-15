#pragma once

#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tools::cli
{

// 成员描述符入口：静态来源由调用者直接给出该函数，动态来源按 DescriptorSymbol 从已加载包解析。
using DescriptorEntryPoint = const vase::PluginDescriptor* (*)();

// NOLINTBEGIN(readability-redundant-member-init) `= {}` NSDMI 是刻意的：clang 的
// -Wmissing-designated-field-initializers（默认开）会把指定初始化省略的字段判成 error，
// 带 NSDMI 才豁免——静态/动态两种读法各只需写自己那一个字段。
struct PackageMemberInput
{
    std::string DescriptorSymbol = {};         // 动态来源：组合库里该成员的描述符符号名
    DescriptorEntryPoint Descriptor = nullptr; // 静态来源：该成员的描述符函数
    std::filesystem::path SourceManifest = {}; // 空 = 未声明（默认启用）；非空但缺失是错误
};
// NOLINTEND(readability-redundant-member-init)

struct PackageBuildOptions
{
    std::string SourceId;
    vase::PackageSourceKind Kind = vase::PackageSourceKind::kDynamic;
    std::filesystem::path BinaryPath;
    vase::PackageEntryPoints Entries;
    std::vector<PackageMemberInput> Members;
    // 以下三项只归 staging（T11）：GeneratePackageManifest 不写盘、不看它们。
    std::filesystem::path OutputDirectory;
    std::vector<std::filesystem::path> RuntimeFiles;
    std::vector<std::filesystem::path> ProtectedDirectories;
};

// 只生成拥有形值；调用方负责写盘及 staging。
[[nodiscard]] vase::Result<vase::PackageManifest> GeneratePackageManifest(const PackageBuildOptions& options);

} // namespace tools::cli
