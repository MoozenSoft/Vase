#include "Validate.h"

#include "Cli.h"
#include "Vase/Catalog/Analysis.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PackageInspect.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include <cstddef>
#include <filesystem>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

std::string PrefixOf(std::string_view id) { return std::string(id) + "."; }

bool HasPrefix(std::string_view text, std::string_view prefix)
{
    return text.size() > prefix.size() && text.starts_with(prefix); // 严格长于前缀：空尾巴不算
}

// 第④项判据 (a)（spec §3.2 末段）。只覆盖**服务**——事件名在描述符与清单里都没有载体（事实取证注⑨）。
// 逐条判定、不遇错即停：一次报全，人才修得动。这条随插件走，在比对循环里收集、于 check-4 节统一打印。
void AppendProvidesViolation(std::vector<std::string>& out, std::string_view pluginId, std::string_view serviceName)
{
    const std::string prefix = PrefixOf(pluginId);
    if (!HasPrefix(serviceName, prefix))
    {
        out.push_back(std::string(pluginId) + " provides \"" + std::string(serviceName) +
                      "\" which is not under its own id prefix \"" + prefix + "\"");
    }
}

// 描述符读法（动态来源与已绑定的静态来源）。
void AppendProvidesViolations(std::vector<std::string>& out, std::string_view pluginId, const vase::PluginMeta& meta)
{
    // MetaArray 无 begin/end，按仓内惯用法（Scan.cpp ConvertDeps）以 Begin()+std::next 迭代。
    for (std::size_t index = 0; index < meta.Provides.Size(); ++index)
    {
        const vase::ServiceRef& service = *std::next(meta.Provides.Begin(), static_cast<std::ptrdiff_t>(index));
        AppendProvidesViolation(out, pluginId, service.Name);
    }
}

// 清单读法（spec §5.2 静态列：普通 CLI 仍完成前缀检查，只是描述符那份读不到）。
void AppendProvidesViolations(std::vector<std::string>& out, std::string_view pluginId,
                              const std::vector<vase::ManifestDependency>& provides)
{
    for (const vase::ManifestDependency& service : provides)
    {
        AppendProvidesViolation(out, pluginId, service.Service);
    }
}

// 成员已由 InspectPackageMembers 检查；比对、输出和前缀收集均在动态借用卸载前完成。
bool CheckPackageMembers(const std::vector<vase::ManifestExpectation>& expected,
                         const std::vector<const vase::PluginDescriptor*>& members, std::string_view label,
                         std::vector<std::string>& namingViolations, std::ostream& out)
{
    const vase::Result<void> compared = vase::ComparePackage(expected, members);
    if (compared.IsOk())
    {
        out << "  " << label << ": ok (" << members.size() << " members)\n";
    }
    else
    {
        out << "  " << label << ": FAIL\n    " << compared.GetError().Message() << '\n';
    }
    for (const vase::PluginDescriptor* descriptor : members)
    {
        AppendProvidesViolations(namingViolations, descriptor->Meta->Id, *descriptor->Meta);
    }
    return compared.IsOk();
}

// 来源组名：单插件来源以它唯一成员的 Id 作组名（既有输出形状的锚），包来源以 sourceId 作组名；
// 成员级诊断仍由比对/枚举消息点名各自 Id。
std::string SourceLabel(const vase::CatalogSource& source)
{
    return source.IsPackage || source.MemberIds.empty() ? source.Id : *source.MemberIds.begin();
}

// 已链接调用者给出的静态双入口（T8）；argv 腿永远给不出绑定。
const vase::InProcessBinding* BindingFor(const std::vector<vase::InProcessBinding>& bindings, std::string_view sourceId)
{
    for (const vase::InProcessBinding& binding : bindings)
    {
        if (binding.SourceId == sourceId)
        {
            return &binding;
        }
    }
    return nullptr;
}

// 一个来源的全部成员期望（含被跳过的成员）：整包比对不按参与过滤（spec §4.3 末句）。
std::vector<vase::ManifestExpectation> ExpectationsFor(const vase::PluginCatalog& catalog,
                                                       const vase::CatalogSource& source)
{
    std::vector<vase::ManifestExpectation> expected;
    expected.reserve(source.MemberIds.size());
    for (const std::string& memberId : source.MemberIds)
    {
        const vase::ManifestEntry* entry = catalog.Find(memberId);
        if (entry != nullptr)
        {
            expected.push_back(vase::BuildExpectation(*entry));
        }
    }
    return expected;
}

// 无法完成描述符校验时的归因：点明缺什么、以及哪条路能补上（spec §5.2 静态列）。
std::string CannotVerifyText(std::string_view label)
{
    return std::string(label) +
           ": FAIL (cannot verify: the plain CLI has no host entry points for this in-process package"
           " — link the generated entries to complete the check)";
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
    // 按**来源**走（T8/spec §5.2）：一个物理库只枚举一次、整包比对一次；单插件来源维持单描述符契约。
    // 来源事实只读来源表——工具不重扫目录、不重解析清单（T4 的唯一维护处约束）。
    for (const vase::CatalogSource& source : catalog.Sources())
    {
        const std::string label = SourceLabel(source);
        const std::vector<vase::ManifestExpectation> expected = ExpectationsFor(catalog, source);
        if (expected.empty())
        {
            continue; // 来源表与快照同源，取不到是不可能的；防御性地跳过而非崩溃
        }

        if (source.Kind == vase::PackageSourceKind::kInProcess)
        {
            // 进程内来源的枚举入口只有宿主拿得到：普通 CLI 的描述符校验**无法执行**，
            // 明说并计入未通过（spec §5.2 静态列；「无法验证」不是成功）。
            const vase::InProcessBinding* binding = BindingFor(options.InProcessSources, source.Id);
            if (binding == nullptr)
            {
                out << "  " << CannotVerifyText(label) << '\n';
                failed = true;
                // 前缀面退到清单自陈的那份 provides：结构/依赖/前缀三项与描述符无关。
                for (const std::string& memberId : source.MemberIds)
                {
                    const vase::ManifestEntry* entry = catalog.Find(memberId);
                    if (entry != nullptr)
                    {
                        AppendProvidesViolations(namingViolations, memberId, entry->Provides);
                    }
                }
                continue;
            }
            const vase::Result<std::vector<const vase::PluginDescriptor*>> members =
                vase::InspectPackageMembers(binding->Entries);
            if (!members.IsOk())
            {
                out << "  " << label << ": FAIL\n    " << members.GetError().Message() << '\n';
                failed = true;
                continue;
            }
            if (!CheckPackageMembers(expected, members.Value(), label, namingViolations, out))
            {
                failed = true;
            }
            continue;
        }

        const vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(source.BinaryPath);
        if (!record.IsOk())
        {
            out << "  " << label << ": FAIL (binary not loadable: " << record.GetError().Message() << ")\n";
            failed = true;
            continue;
        }

        if (source.IsPackage)
        {
            // 动态包：一次枚举整包成员、一次双向等值比对（逐字段唯一走 CompareDescriptor）。
            const vase::Result<std::vector<const vase::PluginDescriptor*>> members =
                vase::InspectPackageMembers(*record.Value());
            if (!members.IsOk())
            {
                out << "  " << label << ": FAIL\n    " << members.GetError().Message() << '\n';
                loader.Unload(*record.Value());
                failed = true;
                continue;
            }
            if (!CheckPackageMembers(expected, members.Value(), label, namingViolations, out))
            {
                failed = true;
            }
            loader.Unload(*record.Value());
            continue;
        }

        const vase::Result<std::vector<const vase::PluginDescriptor*>> descriptors =
            vase::InspectDescriptors(*record.Value());
        if (!descriptors.IsOk() || descriptors.Value().size() != 1U)
        {
            out << "  " << label << ": FAIL ("
                << (descriptors.IsOk() ? std::string{"expected exactly one descriptor"}
                                       : descriptors.GetError().Message())
                << ")\n";
            loader.Unload(*record.Value());
            failed = true;
            continue;
        }
        // 借用物化点同 Scan.cpp：begin() 解引用取指针，比对时再解引用成对象引用。
        const vase::PluginDescriptor* single = *descriptors.Value().begin();
        const vase::Result<void> compared = vase::CompareDescriptor(expected.front(), *single);
        if (compared.IsOk())
        {
            out << "  " << label << ": ok\n";
        }
        else
        {
            out << "  " << label << ": FAIL\n    " << compared.GetError().Message() << '\n';
            failed = true;
        }
        // single 即 descriptors.Value()[0]：走既有具名物化点，避开 unchecked operator[]。
        // 攒着，循环后归 check-4 节统一打印
        AppendProvidesViolations(namingViolations, label, *single->Meta);
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

    // —— 第③项：依赖图 / 环 / 版本（D127/D131）。用 Analyze（T8/spec §5.2）：零装载、不看绑定，
    // 故缺静态绑定时依赖结论照样出来，不会被「没有入口」掩盖。无 preset = 目录全集按 enabledByDefault。
    vase::LoadRequest request;
    request.HostProvided = options.HostProvided;
    out << "check 3 (dependency graph):\n";
    const vase::Result<vase::AnalysisOutcome> analyzed = catalog.Analyze(request);
    if (!analyzed.IsOk())
    {
        // 环与被阻塞共用一个出口（D127）：原文透传，不另造「环」这个分类。
        out << "  FAIL: " << analyzed.GetError().Message() << '\n';
        failed = true;
    }
    else
    {
        for (const vase::AnalyzedPlugin& plugin : analyzed.Value().Ordered)
        {
            if (plugin.Decision == vase::LoadDecision::kLoad)
            {
                continue;
            }
            // 穷举 switch（无 default）：现三值各有文案；加枚举值时 clang 线的 -Wswitch 会顶出来。
            switch (plugin.Reason)
            {
            case vase::SkipReason::kDisabled:
                out << "  " << plugin.Id << ": skip (disabled)\n"; // 静态跳过不算未过（§4.4）
                break;
            case vase::SkipReason::kMissingDependency:
                out << "  " << plugin.Id << ": FAIL (missing dependency)\n";
                failed = true;
                break;
            case vase::SkipReason::kVersionMismatch:
                out << "  " << plugin.Id << ": FAIL (service version mismatch)\n";
                failed = true;
                break;
            }
        }
        for (const vase::SolveNote& note : analyzed.Value().Notes)
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
    // 解析腿已提入 Cli 层与 plan 共用（M6/D146）；文案逐字保留（MalformedHostProvides 用例钉着）。
    if (!ParseTrailingHostProvides(args, 2, options.HostProvided,
                                   "usage: VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n", err))
    {
        return kExitUsage;
    }
    return ValidateDirectory(options, out, err);
}

} // namespace tools::cli
