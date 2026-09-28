#include "Validate.h"

#include "Cli.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/PluginDescriptor.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

// "name@version"：版本必须 ≥1（与 D64 的解析期闸同口径）；名字不得为空。
std::optional<vase::ServiceRef> ParseHostProvided(std::string_view text)
{
    const std::size_t at = text.rfind('@');
    if (at == std::string_view::npos || at == 0U || at + 1U >= text.size())
    {
        return std::nullopt;
    }
    std::uint32_t version = 0;
    const std::string_view digits = text.substr(at + 1U);
    // 指针形态照 Console::ParseIndex（data() 先落命名变量再进 from_chars；data()+size() 吃 pro-bounds）。
    const char* const begin = digits.data();
    const char* const end = std::next(begin, static_cast<std::ptrdiff_t>(digits.size()));
    const auto parsed = std::from_chars(begin, end, version);
    if (parsed.ec != std::errc{} || parsed.ptr != end || version < 1U)
    {
        return std::nullopt;
    }
    return vase::ServiceRef{.Name = text.substr(0, at), .Version = version};
}

std::string PrefixOf(std::string_view id) { return std::string(id) + "."; }

bool HasPrefix(std::string_view text, std::string_view prefix)
{
    return text.size() > prefix.size() && text.starts_with(prefix); // 严格长于前缀：空尾巴不算
}

// 第④项判据 (a)（spec §3.2 末段）。只覆盖**服务**——事件名在描述符与清单里都没有载体（事实取证注⑨）。
// 逐条判定、不遇错即停：一次报全，人才修得动。这条随插件走，在比对循环里收集、于 check-4 节统一打印。
std::vector<std::string> ProvidesPrefixViolations(std::string_view pluginId, const vase::PluginMeta& meta)
{
    std::vector<std::string> out;
    const std::string prefix = PrefixOf(pluginId);
    // MetaArray 无 begin/end，按仓内惯用法（Scan.cpp ConvertDeps）以 Begin()+std::next 迭代。
    for (std::size_t index = 0; index < meta.Provides.Size(); ++index)
    {
        const vase::ServiceRef& service = *std::next(meta.Provides.Begin(), static_cast<std::ptrdiff_t>(index));
        if (!HasPrefix(service.Name, prefix))
        {
            out.push_back(std::string(pluginId) + " provides \"" + std::string(service.Name) +
                          "\" which is not under its own id prefix \"" + prefix + "\"");
        }
    }
    return out;
}

// 第④项判据 (b)：反向——宿主提供的名字不得落在任何插件 Id 之下（占用他人命名空间是明确的错）。
// 该判定对快照是循环不变的 ⇒ 全树跑一次（修复轮 1：原先随每只插件各跑，squat 被重复打印 N 遍；
// 且插件全走 continue 早退时一条都不打）。宿主名字不做自家前缀自查——宿主没有 Id 可对照（D132）。
std::vector<std::string> HostSquatViolations(const std::vector<std::string>& allPluginIds,
                                             const std::vector<vase::ServiceRef>& hostProvided)
{
    std::vector<std::string> out;
    for (const std::string& otherId : allPluginIds)
    {
        const std::string prefix = PrefixOf(otherId); // 前缀每 Id 算一次，不按 (hosted, id) 对重铸
        for (const vase::ServiceRef& hosted : hostProvided)
        {
            if (HasPrefix(hosted.Name, prefix))
            {
                out.push_back("host-provided \"" + std::string(hosted.Name) + "\" squats the namespace of plugin \"" +
                              otherId + "\"");
            }
        }
    }
    return out;
}

} // namespace

namespace tools::cli
{

int ValidateDirectory(const ValidateOptions& options, std::ostream& out, std::ostream& err)
{
    // 目录不存在 = 环境错误（D135 第二档），不是第②项没过——快照根本还没开始建。
    std::error_code ec;
    if (!std::filesystem::is_directory(options.PluginDirectory, ec))
    {
        err << "validate: not a directory: \"" << options.PluginDirectory.string() << "\"\n";
        return kExitUsage;
    }

    vase::PluginCatalog catalog;
    const vase::Result<void> refreshed = catalog.Refresh(options.PluginDirectory);
    if (!refreshed.IsOk())
    {
        // 第②项（D130）：快照没建成 ⇒ 只报这一项，明说其余三项没跑。
        out << "check 2 (duplicate plugin id): FAIL\n";
        out << "  " << refreshed.GetError().Message() << '\n';
        out << "snapshot not built: checks 1/3/4 were not run\n";
        return kExitCheckFailed;
    }
    out << "check 2 (duplicate plugin id): ok\n";

    // D128：子目录缺 plugin.json 是**常态**（§4.3），Catalog 已按 D50 跳过后记在 Warnings 里。
    // 打印但**不计入退出码**——validate 报的是「存在的东西不对」，不是「少了个东西」。
    for (const vase::CatalogWarning& warning : catalog.Warnings())
    {
        out << "  warning: " << warning.Subdirectory << ": " << warning.Message << '\n';
    }

    // D135 第二档补角：零个插件的树不是「全过」——快照建成了，但没有任何东西可查。
    if (catalog.Ids().empty())
    {
        out << "no plugins found in the snapshot\n";
        return kExitUsage;
    }

    vase::detail::Loader loader;
    bool failed = false;
    // 第④项 (a) 逐插件收集、(b) 循环不变，二者都留到比对循环之后并入 check-4 节打印（逐项分节，
    // spec §3.5）——违规文案与 rc 语义不变，且不打断 check-1 与其逐插件 ok/FAIL 的相邻。
    std::vector<std::string> namingViolations;
    out << "check 1 (manifest vs binary):\n";
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        if (entry == nullptr)
        {
            continue; // Ids() 与 Find() 同源，取不到是不可能的；防御性地跳过而非崩溃
        }
        const std::filesystem::path binary =
            catalog.Directory() / entry->Subdirectory / vase::catalog_detail::LibraryFileName(entry->Binary);
        vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(binary);
        if (!record.IsOk())
        {
            out << "  " << id << ": FAIL (binary not loadable: " << record.GetError().Message() << ")\n";
            failed = true;
            continue;
        }
        const vase::Result<std::vector<const vase::PluginDescriptor*>> descriptors =
            vase::InspectDescriptors(*record.Value());
        if (!descriptors.IsOk() || descriptors.Value().size() != 1U)
        {
            out << "  " << id << ": FAIL ("
                << (descriptors.IsOk() ? std::string{"expected exactly one descriptor"}
                                       : descriptors.GetError().Message())
                << ")\n";
            loader.Unload(*record.Value());
            failed = true;
            continue;
        }
        const vase::ManifestExpectation expected = vase::BuildExpectation(*entry);
        // 借用物化点同 Scan.cpp：begin() 解引用取指针，比对时再解引用成对象引用。
        const vase::PluginDescriptor* single = *descriptors.Value().begin();
        const vase::Result<void> compared = vase::CompareDescriptor(expected, *single);
        if (compared.IsOk())
        {
            out << "  " << id << ": ok\n";
        }
        else
        {
            out << "  " << id << ": FAIL\n    " << compared.GetError().Message() << '\n';
            failed = true;
        }
        // single 即 descriptors.Value()[0]：走既有具名物化点，避开 unchecked operator[]。
        for (const std::string& violation : ProvidesPrefixViolations(id, *single->Meta))
        {
            namingViolations.push_back(violation); // 攒着，循环后归 check-4 节统一打印
        }
        loader.Unload(*record.Value()); // 借用止于此：比对照着驻留期做完才卸
    }

    // —— 第④项：宿主越界 (b) + 逐插件 provides 违规 (a) 同归一节、逐项分节（spec §3.5）。
    out << "check 4 (service naming prefix):\n";
    for (const std::string& violation : HostSquatViolations(catalog.Ids(), options.HostProvided))
    {
        out << "  " << violation << '\n';
        failed = true;
    }
    for (const std::string& violation : namingViolations)
    {
        out << "  " << violation << '\n';
        failed = true;
    }

    // —— 第③项：依赖图 / 环 / 版本（D127/D131）。Solve 无 preset = 目录全集按 enabledByDefault。
    vase::LoadRequest request;
    request.HostProvided = options.HostProvided;
    out << "check 3 (dependency graph):\n";
    const vase::Result<vase::SolveOutcome> solved = catalog.Solve(request);
    if (!solved.IsOk())
    {
        // 环与被阻塞共用一个出口（D127）：原文透传，不另造「环」这个分类。
        out << "  FAIL: " << solved.GetError().Message() << '\n';
        failed = true;
    }
    else
    {
        for (const vase::LoadPlanEntry& entry : solved.Value().Plan.Ordered)
        {
            if (entry.Decision == vase::LoadDecision::kLoad)
            {
                continue;
            }
            // 穷举 switch（无 default）：现三值各有文案；加枚举值时 clang 线的 -Wswitch 会顶出来。
            switch (entry.Reason)
            {
            case vase::SkipReason::kDisabled:
                out << "  " << entry.Id << ": skip (disabled)\n"; // 静态跳过不算未过（§4.4）
                break;
            case vase::SkipReason::kMissingDependency:
                out << "  " << entry.Id << ": FAIL (missing dependency)\n";
                failed = true;
                break;
            case vase::SkipReason::kVersionMismatch:
                out << "  " << entry.Id << ": FAIL (service version mismatch)\n";
                failed = true;
                break;
            }
        }
        for (const vase::SolveNote& note : solved.Value().Notes)
        {
            out << "  note: " << note.Message << '\n';
        }
    }

    return failed ? kExitCheckFailed : kExitOk;
}

int RunValidate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    if (args.size() < 2U)
    {
        err << "usage: VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n";
        return kExitUsage;
    }
    ValidateOptions options;
    options.PluginDirectory = *std::next(args.begin(), 1); // std::next 形态：pro-bounds 检查的既有惯例（同 Cli.cpp）
    // HostProvided 的 Name 借 args 里的串（D60 窗 = 本次调用）：args 存活覆盖整个调用，安全。
    // 一次吃「flag + 值」两格：步进放 header 的 std::advance(it, 2)（header 与体内双 ++it 触发 -Wfor-loop-analysis）。
    for (auto it = std::next(args.begin(), 2); it != args.end(); std::advance(it, 2))
    {
        if (*it != "--host-provides" || std::next(it) == args.end())
        {
            err << "usage: VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n";
            return kExitUsage;
        }
        const std::optional<vase::ServiceRef> provided = ParseHostProvided(*std::next(it));
        if (!provided.has_value())
        {
            err << "invalid --host-provides value: " << *std::next(it) << " (expected <name>@<version>=1)\n";
            return kExitUsage;
        }
        options.HostProvided.push_back(*provided);
    }
    return ValidateDirectory(options, out, err);
}

} // namespace tools::cli
