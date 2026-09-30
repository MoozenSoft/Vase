#include "Plan.h"

#include "Cli.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/PluginDescriptor.h"

#include <cstddef>
#include <filesystem>
#include <iterator>
#include <ostream>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

// 与 Console 各持一份的第二副本（M6/D149）：字符串逐字同 Console.cpp:197-225——
// 「同一套规则」钉的是 Solve 判定，不是输出形状；改判定的义务在源侧，不在这两份文案。
const char* SkipReasonText(vase::SkipReason reason)
{
    switch (reason)
    {
    case vase::SkipReason::kDisabled:
        return "disabled";
    case vase::SkipReason::kMissingDependency:
        return "missing-dependency";
    case vase::SkipReason::kVersionMismatch:
        return "version-mismatch";
    }
    return "?";
}

const char* SolveNoteKindText(vase::SolveNoteKind kind)
{
    switch (kind)
    {
    case vase::SolveNoteKind::kUnknownPluginId:
        return "unknownPluginId";
    case vase::SolveNoteKind::kUnknownConfigKey:
        return "unknownConfigKey";
    case vase::SolveNoteKind::kProviderSkipped:
        return "providerSkipped";
    case vase::SolveNoteKind::kVersionMismatchProvider:
        return "versionMismatchProvider";
    }
    return "?";
}

} // namespace

namespace tools::cli
{

int PlanDirectory(const PlanOptions& options, std::ostream& out, std::ostream& err)
{
    // 环境档先于判定档（spec §2.1）：目录不存在 ⇒ 快照根本无从谈起，2。
    std::error_code ec;
    if (!std::filesystem::is_directory(options.PluginDirectory, ec))
    {
        err << "plan: not a directory: \"" << options.PluginDirectory.string() << "\"\n";
        return kExitUsage;
    }

    vase::LoadRequest request;
    if (options.PresetFile.has_value())
    {
        // 非 const 是有内容的：const 路上 Value() 返回 const&，下面 std::move 会空转
        // （performance-move-const-arg 正为此而咬）。照 Console.cpp 的 LoadPreset 消费形。
        auto loaded = vase::LoadPreset(*options.PresetFile);
        if (!loaded.IsOk())
        {
            // preset 读不成 = 环境档（D142）；错误 Message 已带文件路径。
            err << "plan: " << loaded.GetError().Message() << '\n';
            return kExitUsage;
        }
        request.Preset = std::move(loaded.Value());
    }

    vase::PluginCatalog catalog;
    const vase::Result<void> refreshed = catalog.Refresh(options.PluginDirectory);
    if (!refreshed.IsOk())
    {
        // 快照未建成 = FAIL 档 1 不是环境档 2（D142，D130 同律）；没跑到的 Solve 明说没跑到。
        out << "plan: snapshot not built: " << refreshed.GetError().Message() << '\n';
        out << "snapshot not built: plan was not solved\n";
        return kExitCheckFailed;
    }

    if (catalog.Ids().empty())
    {
        out << "no plugins found in the snapshot\n"; // D135 第二档补角，validate 同位文案
        return kExitUsage;
    }

    request.HostProvided = std::span<const vase::ServiceRef>(options.HostProvided);
    const vase::Result<vase::SolveOutcome> solved = catalog.Solve(request);
    if (!solved.IsOk())
    {
        out << "plan: FAIL: " << solved.GetError().Message() << '\n'; // D127 原文透传同律
        return kExitCheckFailed;
    }

    const std::vector<vase::LoadPlanEntry>& ordered = solved.Value().Plan.Ordered;
    out << "plan: " << ordered.size() << " entries";
    if (options.PresetFile.has_value())
    {
        out << " (preset: \"" << options.PresetFile->string() << "\")";
    }
    out << '\n';

    std::size_t hardSkips = 0;
    std::size_t index = 1;
    for (const vase::LoadPlanEntry& entry : ordered)
    {
        if (entry.Decision == vase::LoadDecision::kLoad)
        {
            out << "  " << index << ". " << entry.Id << " load\n";
        }
        else
        {
            // 穷举 switch（无 default）：加枚举值时 -Wswitch 顶出来（Validate.cpp 同位先例）。
            switch (entry.Reason)
            {
            case vase::SkipReason::kDisabled:
                break; // 静态跳过不算未过（§4.4/D131）
            case vase::SkipReason::kMissingDependency:
            case vase::SkipReason::kVersionMismatch:
                ++hardSkips; // 两硬值同计入（合并两 case 过 bugprone-branch-clone，语义不变）
                break;
            }
            out << "  " << index << ". " << entry.Id << " skip " << SkipReasonText(entry.Reason) << '\n';
        }
        ++index;
    }
    for (const vase::SolveNote& note : solved.Value().Notes)
    {
        out << "  note: " << SolveNoteKindText(note.Kind) << " " << note.PluginId;
        if (!note.Key.empty())
        {
            out << " " << note.Key;
        }
        else if (!note.Cause.empty())
        {
            // 与 Console.cpp:417-420 同形——空 Key 的两 kind 归因在 Cause（D82）
            out << " " << note.Cause;
        }
        out << '\n';
    }

    if (hardSkips != 0U)
    {
        out << "plan: FAIL (" << hardSkips << (hardSkips == 1U ? " entry" : " entries")
            << " skipped with a hard reason)\n";
        return kExitCheckFailed;
    }
    out << "plan: ok\n";
    return kExitOk;
}

int RunPlan(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    if (args.size() < 2U)
    {
        err << "usage: VaseCli plan <插件目录> [presetFile] [--host-provides <name>@<version>]...\n";
        return kExitUsage;
    }
    PlanOptions options;
    options.PluginDirectory = *std::next(args.begin(), 1);
    std::size_t firstFlag = 2;
    // spec §6 风险 5：arg[2] 以 `--` 起 ⇒ flag 段开始、presetFile 缺省。
    if (args.size() >= 3U && !std::next(args.begin(), 2)->starts_with("--"))
    {
        options.PresetFile = std::filesystem::path{*std::next(args.begin(), 2)};
        firstFlag = 3;
    }
    if (!ParseTrailingHostProvides(
            args, firstFlag, options.HostProvided,
            "usage: VaseCli plan <插件目录> [presetFile] [--host-provides <name>@<version>]...\n", err))
    {
        return kExitUsage;
    }
    return PlanDirectory(options, out, err);
}

} // namespace tools::cli
