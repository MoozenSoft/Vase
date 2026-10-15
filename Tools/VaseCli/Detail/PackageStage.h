#pragma once

// T11 内部件：发布目录事务（BuildAndStagePackage）与其测试接缝、生成器 TU 的共用入口。
// 只服务 Tools/VaseCli 自身与 vase_stage_pack 生成的生成器——接缝不进用户 CLI / 公开 API。

#include "PackageBuild.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include <filesystem>
#include <functional>
#include <span>
#include <string_view>

namespace tools::cli
{

// 单包目录事务（spec §3.1）：先在同一父目录的准备目录里生成清单、复制动态文件并校验，
// 全部成功才用两次改名替换原包目录；任何失败保留原目录内容。
[[nodiscard]] vase::Result<void> BuildAndStagePackage(const PackageBuildOptions& options);

} // namespace tools::cli

namespace tools::cli::package_detail
{

// 目录改名接缝：默认走 std::filesystem::rename；测试注入安装期失败（T11 Step 1）。
using RenameDirectoryFn = std::function<vase::Result<void>(const std::filesystem::path&, const std::filesystem::path&)>;

void SetRenameDirectoryForTest(RenameDirectoryFn fn);
void ResetRenameDirectoryForTest();

// NOLINTBEGIN(readability-redundant-member-init) `= {}` NSDMI 是刻意的：clang 的
// -Wmissing-designated-field-initializers（默认开）会把指定初始化省略的字段判成 error，
// 带 NSDMI 才豁免——静态/动态两种读法各只需写自己那一个字段（同 PackageMemberInput）。
struct StageMemberSpec
{
    std::string_view DescriptorSymbol = {};                // 动态：组合库内该成员的描述符符号名
    tools::cli::DescriptorEntryPoint Descriptor = nullptr; // 静态：该成员的描述符函数
    bool HasSourceManifest = false;                        // 是否按成员序消费一个 --manifest
};
// NOLINTEND(readability-redundant-member-init)

// 生成器 TU 的成员构造：生成物里因此只出现**平铺**的元素表，不出现嵌套花括号初始化表
// （后者是 readability-trailing-comma 的误报面，vase_pack_plugins 的分发表同律）。
constexpr StageMemberSpec DynamicStageMember(std::string_view descriptorSymbol, bool hasSourceManifest)
{
    return {.DescriptorSymbol = descriptorSymbol, .HasSourceManifest = hasSourceManifest};
}

constexpr StageMemberSpec InProcessStageMember(DescriptorEntryPoint descriptor, bool hasSourceManifest)
{
    return {.Descriptor = descriptor, .HasSourceManifest = hasSourceManifest};
}

// 动态包没有进程内入口：生成器把这一位传成空入口值。空花括号初始化表当实参会被
// readability-trailing-comma 误报（空表被判成带尾逗号），故这里起个名字。
inline constexpr vase::PackageEntryPoints kNoStageEntries{};

// 生成器 TU 的共用入口：解析 argv、按编译期绑定组装成员、跑 BuildAndStagePackage。
// 退出码：0 成功、1 staging 失败、2 用法错误。
int RunPackageStageGenerator(int argc, const char* const* argv, vase::PackageSourceKind kind,
                             vase::PackageEntryPoints entries, std::span<const StageMemberSpec> members);

} // namespace tools::cli::package_detail
