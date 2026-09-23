#include "Commands.h"

#include "Greeter.h"
#include "PlanFile.h"
#include "Shell.h"
#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/Pod/Pod.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace samples::bench
{
namespace
{

const char* BoolText(bool value) { return value ? "true" : "false"; }

const char* PhaseText(vase::Phase phase)
{
    switch (phase)
    {
    case vase::Phase::kLoad:
        return "load";
    case vase::Phase::kStart:
        return "start";
    case vase::Phase::kAdopt:
        return "adopt";
    case vase::Phase::kEject:
        return "eject";
    case vase::Phase::kUnload:
        return "unload";
    }
    return "?";
}

bool CheckArity(std::ostream& out, const std::vector<std::string>& args, std::size_t need, std::string_view usage)
{
    if (args.size() != need)
    {
        out << "usage: " << usage << "  (got " << args.size() << " argument(s))\n";
        return false;
    }
    return true;
}

// 失败清单的打印（§5.5：失败清单随时可被宿主读出）。返回「打印了没有」，MarkFailed 留给
// 调用点——那份「失败必须反映到退出码」的理由属于命令，不属于打印。
bool PrintFailures(std::ostream& out, const std::vector<vase::FailedPluginRecord>& failures)
{
    for (const vase::FailedPluginRecord& failure : failures)
    {
        out << "  failed: " << failure.Id << " [" << PhaseText(failure.Stage) << "] " << failure.Message << '\n';
    }
    return !failures.empty();
}

// 字段集合照抄 Samples/Embedding/main.cpp 的 PrintReport（已验收的那份），只换输出通道。
void PrintPodReport(std::ostream& out, const vase::PodReport& report)
{
    out << "clean=" << BoolText(report.Clean()) << " handleWasStale=" << BoolText(report.HandleWasStale) << '\n';
    out << "counters diff (baseline = pod creation):"
        << " effects=" << report.CountersDiff.Effects << " services=" << report.CountersDiff.Services
        << " subscriptions=" << report.CountersDiff.Subscriptions
        << " pluginInstances=" << report.CountersDiff.PluginInstances << " scopes=" << report.CountersDiff.Scopes
        << '\n';
    for (const vase::FailedPluginRecord& failure : report.Failures)
    {
        out << "failed: " << failure.Id << " [" << PhaseText(failure.Stage) << "] " << failure.Message << '\n';
    }
    for (const vase::ResidualEntry& residual : report.Residuals)
    {
        out << "residual: " << residual.OwnerLabel << " x" << residual.Count << '\n';
    }
    out << "hotswap log (" << report.HotSwapLog.size() << "):\n";
    for (const std::string& entry : report.HotSwapLog)
    {
        out << "  " << entry << '\n';
    }
}

// §9.2「每次进出必须有账可查」的演示形态（同 Samples/Embedding/main.cpp 那份）：把报告的每个
// 判据都打出来。两个 Is… 是**本平台哪个字段有判据力**的声明（§8.2 把这件事写进了结构）——
// 不打出来，Windows 上那个 mappingRemoved=false 就成了没解释的噪声，本该被读作失败。
void PrintEjectReport(std::ostream& out, const vase::EjectReport& report)
{
    out << "eject " << report.PluginId << " binaryUnloaded=" << BoolText(report.BinaryActuallyUnloaded)
        << " mappingRemoved=" << BoolText(report.MappingRemoved)
        << " reopenWritable=" << BoolText(report.ReopenWritable)
        << " mappingRemovalIsObservable=" << BoolText(report.MappingRemovalIsObservable)
        << " reopenWritableIsMeaningful=" << BoolText(report.ReopenWritableIsMeaningful) << '\n';
    if (!report.HotSwapNote.empty())
    {
        out << "  note: " << report.HotSwapNote << '\n';
    }
}

void PrintAdoptReport(std::ostream& out, const vase::AdoptReport& report)
{
    out << "adopt " << report.PluginId << " reusedResidentImage=" << BoolText(report.ReusedResidentImage)
        << " identityVerified=" << BoolText(report.IdentityVerified)
        << " importEnforcementPassed=" << BoolText(report.ImportEnforcementPassed)
        << " outgoingEdges=" << report.OutgoingEdges << '\n';
}

} // namespace

std::vector<shell::CommandSpec> Bench::Commands()
{
    std::vector<shell::CommandSpec> commands;
    const auto add = [&commands](std::string group, std::string name, std::vector<std::string> params, std::string help,
                                 std::function<void(std::ostream&, const std::vector<std::string>&)> handler)
    {
        commands.push_back(shell::CommandSpec{
            .Group = std::move(group),
            .Name = std::move(name),
            .Params = std::move(params),
            .Help = std::move(help),
            .Handler = std::move(handler),
        });
    };

    add("pod", "new", {"planFile"}, "Create a pod from a plan file",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdPodNew(out, args); });
    add("pod", "use", {"index"}, "Select the target pod",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdPodUse(out, args); });
    add("pod", "list", {}, "List live pods",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdPodList(out, args); });
    add("pod", "destroy", {}, "Destroy the active pod and print its report",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdPodDestroy(out, args); });
    // 进出挂根菜单（Group 留空）：与 pod 子菜单平级，子菜单名留给 pod 一个。
    add("", "eject", {"id"}, "Eject a plugin from the active pod",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdEject(out, args); });
    add("", "adopt", {"id"}, "Adopt a plugin into the active pod",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdAdopt(out, args); });
    add("", "swap", {"id", "rounds"}, "Eject + adopt the same plugin N times",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdSwap(out, args); });
    add("", "get", {}, "Print the greeting the active pod's Vase.Hello provider serves",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdGet(out, args); });
    add("", "emit", {"n"}, "Emit a GreetEvent with sequence number n into the active pod",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdEmit(out, args); });
    add("", "plugins", {}, "List the active pod's live plugin ids and failure records",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdPlugins(out, args); });
    // 磁盘面单开一个 file 子菜单（spec §3.3）：三条命令只有一起才成立——stage 不落盘、
    // install 落盘、show 看落盘结果。挂根菜单会让它们与进出命令混成一片。
    add("file", "stage", {"id", "srcPath"}, "Read src bytes into the session staging slot (does not overwrite)",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdFileStage(out, args); });
    add("file", "install", {"id"}, "Overwrite the id's registered path with the staged bytes",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdFileInstall(out, args); });
    add("file", "show", {"id"}, "Print the id's registered path, size, mtime and on-disk identity",
        [this](std::ostream& out, const std::vector<std::string>& args) { CmdFileShow(out, args); });
    return commands;
}

bool Bench::ParseIndex(std::string_view text, std::uint32_t& out)
{
    std::uint32_t value = 0;
    const char* const begin = text.data();
    const char* const end = std::next(begin, static_cast<std::ptrdiff_t>(text.size()));
    const std::from_chars_result parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
    {
        return false;
    }
    out = value;
    return true;
}

void Bench::CmdPodNew(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 1, "pod new <planFile>"))
    {
        MarkFailed();
        return;
    }
    // *begin() 而非 args[0]：非常量下标的 operator[] 过不了 cppcoreguidelines-pro-bounds-*。
    auto parsed = plan::ParsePlanFile(std::filesystem::path{*args.begin()});
    if (!parsed.IsOk())
    {
        out << "pod new: " << parsed.GetError().Message() << '\n';
        MarkFailed();
        return;
    }
    std::vector<plan::Entry> entries = std::move(parsed.Value());

    vase::LoadPlan plan2;
    for (const plan::Entry& entry : entries)
    {
        // LoadPlanEntry::Id 是 string_view，**只在这次 CreatePod 调用期间有效**：
        // entries 之后会被移进 Pods，届时地址变（SSO），那些 view 就都不能再用了。
        plan2.Ordered.push_back(vase::LoadPlanEntry{.Id = std::string_view{entry.Id}, .BinaryPath = entry.BinaryPath});
    }

    vase::PodOptions options;
    options.Strict = true; // 验证台不接受"半成的局"：一局要么完整要么失败
    auto created = Host.CreatePod(plan2, options);
    if (!created.IsOk())
    {
        out << "pod new: " << created.GetError().Message() << '\n';
        MarkFailed();
        return;
    }

    const vase::PodHandle handle = created.Value();
    LivePod slot;
    slot.Handle = handle;
    slot.PlanPath = std::filesystem::path{*args.begin()};
    slot.Entries = std::move(entries);
    Pods.insert_or_assign(handle.Index, std::move(slot));
    ActiveIndex = handle.Index;
    HasActive = true;

    out << "pod created index=" << handle.Index << " generation=" << handle.Generation << '\n';
    const vase::Pod* pod = Host.Resolve(handle);
    if (pod == nullptr)
    {
        out << "pod new: handle did not resolve immediately\n"; // 不该发生，但报告比崩溃有用
        MarkFailed();
        return;
    }
    for (const std::string& id : pod->PluginIds())
    {
        out << "  plugin: " << id << '\n';
    }
    // 这里不打印失败记录：Strict=true 下带失败记录的局根本不会建出来（规则 ① 由上面的
    // created 分支承担）。
}

void Bench::CmdPodDestroy(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 0, "pod destroy"))
    {
        MarkFailed();
        return;
    }
    if (!HasActive)
    {
        out << "pod destroy: no active pod\n"; // 与其它拒绝路径同形：响亮失败要有一行正文
        MarkFailed();
        return;
    }
    const LivePod* slot = Active();
    const vase::PodReport report = Host.DestroyPod(slot->Handle);
    out << "destroy index=" << report.PodIndex << '\n';
    PrintPodReport(out, report);
    if (!report.Clean())
    {
        MarkFailed(); // 规则 ②
    }
    Pods.erase(slot->Handle.Index);
    HasActive = false;
}

void Bench::CmdPodList(std::ostream& out, const std::vector<std::string>& args)
{
    static_cast<void>(args); // 本命令不吃参数，签名由 CommandSpec 的 handler 形状定死。
    out << "pods: " << Pods.size() << '\n';
    for (const auto& [index, pod] : Pods)
    {
        const vase::Pod* resolved = Host.Resolve(pod.Handle);
        out << "  [" << index << "] gen=" << pod.Handle.Generation << " plan=" << pod.PlanPath.string()
            << " plugins=" << (resolved == nullptr ? 0 : static_cast<int>(resolved->PluginCount()))
            << (index == ActiveIndex && HasActive ? "  <- active" : "") << '\n';
    }
}

void Bench::CmdPodUse(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 1, "pod use <index>"))
    {
        MarkFailed();
        return;
    }
    std::uint32_t index = 0;
    const auto found = Pods.find(ParseIndex(*args.begin(), index) ? index : UINT32_MAX);
    if (found == Pods.end())
    {
        out << "pod use: no live pod with index " << *args.begin() << '\n';
        MarkFailed();
        return;
    }
    ActiveIndex = found->first;
    HasActive = true;
    out << "active pod: index=" << ActiveIndex << " generation=" << found->second.Handle.Generation << '\n';
}

vase::Pod* Bench::ResolveActivePod(std::ostream& out, vase::PodHandle& handleOut)
{
    const LivePod* slot = Active();
    if (slot == nullptr)
    {
        out << "no active pod (use: pod new <planFile>)\n";
        return nullptr;
    }
    vase::Pod* pod = Host.Resolve(slot->Handle);
    if (pod == nullptr)
    {
        out << "active handle is stale\n"; // §5.1：句柄失效是预期内，不是错误——但命令确实没做成
        return nullptr;
    }
    handleOut = slot->Handle;
    return pod;
}

const plan::Entry* Bench::FindEntry(std::string_view id)
{
    const LivePod* slot = Active();
    if (slot == nullptr)
    {
        return nullptr;
    }
    for (const plan::Entry& entry : slot->Entries)
    {
        if (entry.Id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}

void Bench::CmdEject(std::ostream& out, const std::vector<std::string>& args)
{
    vase::PodHandle handle;
    const vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr || !CheckArity(out, args, 1, "eject <id>"))
    {
        MarkFailed();
        return;
    }
    // §5.6：Eject 失败即拒绝，拒绝的点名信息在 Error::Message 里（M1 不镀金成结构化拒绝类型）。
    // 冠词用 failed 不用 refused：Host 的 Err 文本前缀**不齐**——`plugin not in pod: <id>` 一类是
    // 光秃的（不加冠词则上下文全丢），而 `adopt refused: …` 一类自带前缀（照抄即叠成双前缀）。
    auto result = Host.EjectPlugin(handle, std::string_view{*args.begin()});
    if (!result.IsOk())
    {
        out << "eject failed: " << result.GetError().Message() << '\n';
        MarkFailed();
        return;
    }
    PrintEjectReport(out, result.Value());
}

void Bench::CmdAdopt(std::ostream& out, const std::vector<std::string>& args)
{
    vase::PodHandle handle;
    const vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr || !CheckArity(out, args, 1, "adopt <id>"))
    {
        MarkFailed();
        return;
    }
    // 先把「这个 Id 登记自哪份 plan」打出来：M1 的 KnownBinaries 是 Host 级、不按局分账
    // （spec §8.1），多局并存时它是用户最容易撞到的东西，报告里必须看得见来源。
    const plan::Entry* entry = FindEntry(*args.begin());
    out << "adopt " << *args.begin()
        << " registeredBy=" << (entry == nullptr ? "not-in-active-plan" : Active()->PlanPath.string()) << '\n';

    auto result = Host.AdoptPlugin(handle, std::string_view{*args.begin()});
    if (!result.IsOk())
    {
        out << "adopt failed: " << result.GetError().Message() << '\n';
        MarkFailed();
        return;
    }
    PrintAdoptReport(out, result.Value());
}

void Bench::CmdSwap(std::ostream& out, const std::vector<std::string>& args)
{
    vase::PodHandle handle;
    if (!CheckArity(out, args, 2, "swap <id> <rounds>") || ResolveActivePod(out, handle) == nullptr)
    {
        MarkFailed();
        return;
    }
    // rounds 必填、且必须为正：`swap <id>` 的"跑一次"写法被并掉了，两条命令两套 arity 不值当
    // （spec §3.3 那条待 Task 7 更新）。解析式同 ParseIndex：from_chars + 整串消费判定。
    int rounds = 0;
    const std::string& text = *std::next(args.begin(), 1);
    const char* const begin = text.data();
    const char* const end = std::next(begin, static_cast<std::ptrdiff_t>(text.size()));
    const std::from_chars_result parsed = std::from_chars(begin, end, rounds);
    if (parsed.ec != std::errc{} || parsed.ptr != end || rounds <= 0)
    {
        out << "swap: <rounds> must be a positive integer, got " << text << '\n';
        MarkFailed();
        return;
    }
    for (int round = 0; round < rounds; ++round)
    {
        out << "--- round " << round << " ---\n";
        auto ejected = Host.EjectPlugin(handle, std::string_view{*args.begin()});
        if (!ejected.IsOk())
        {
            out << "eject failed: " << ejected.GetError().Message() << '\n';
            MarkFailed();
            return;
        }
        PrintEjectReport(out, ejected.Value());
        auto adopted = Host.AdoptPlugin(handle, std::string_view{*args.begin()});
        if (!adopted.IsOk())
        {
            out << "adopt failed: " << adopted.GetError().Message() << '\n';
            MarkFailed();
            return;
        }
        PrintAdoptReport(out, adopted.Value());
    }
}

void Bench::CmdFileStage(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 2, "file stage <id> <srcPath>") || Active() == nullptr)
    {
        MarkFailed();
        return;
    }
    const std::string& sourcePath = *std::next(args.begin(), 1);
    std::ifstream source{sourcePath, std::ios::binary};
    if (!source.is_open())
    {
        out << "file stage: cannot open source: " << sourcePath << '\n';
        MarkFailed();
        return;
    }
    // 逐字节经 istreambuf_iterator 收（Source/Host/ImageInspectCommon.cpp 的 ReadImageFileBytes
    // 同款写法）：ifstream::read 要 char*，为取一个字节指针去 reinterpret_cast 不值当。
    const std::istreambuf_iterator<char> reader{source};
    const std::istreambuf_iterator<char> finish;
    std::vector<std::uint8_t> bytes;
    bytes.assign(reader, finish);
    if (bytes.empty())
    {
        // 一个字节都没读到就不许进暂存区：install 会把它当成「把 DLL 截成 0 字节」。
        out << "file stage: no bytes read from " << sourcePath << '\n';
        MarkFailed();
        return;
    }
    // 「为哪个 Id 暂的存」与「暂了哪些字节」绑成一个值，两者不可能错配。
    Staged = std::make_pair(*args.begin(), std::move(bytes));
    out << "staged " << Staged->second.size() << " bytes for " << Staged->first << " (not written yet)\n";
}

void Bench::CmdFileInstall(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 1, "file install <id>") || Active() == nullptr)
    {
        MarkFailed();
        return;
    }
    const std::string& id = *args.begin();
    if (!Staged || Staged->first != id)
    {
        out << "file install: nothing staged for " << id << '\n';
        MarkFailed();
        return;
    }
    // 本地引用紧贴校验点取：中间夹一次 FindEntry（非常量成员调用）之后，
    // bugprone-unchecked-optional-access 不再认「Staged 有值」这份事实。
    const std::vector<std::uint8_t>& staged = Staged->second;
    const plan::Entry* entry = FindEntry(id);
    if (entry == nullptr)
    {
        out << "file install: " << id << " is not in the active pod's plan\n";
        MarkFailed();
        return;
    }

    {
        // ofstream 默认就是 out（不带 app 即截断），故只需 binary 一个 flag——顺带避开
        // openmode 按位或上的 bugprone-signed-bitwise。暂存的是内存里的字节，用 ofstream
        // 而不是 filesystem::copy_file：磁盘上没有源文件可整份拷贝。
        std::ofstream target{entry->BinaryPath, std::ios::binary};
        if (!target.is_open())
        {
            // Windows 上这一支就是 T12 那个 sharing-violation 探针：镜像还映射着就写不开。
            out << "install " << id << " written=false path=" << entry->BinaryPath.string() << '\n';
            // 判据力声明同样按平台分叉（与成功路径那对一致）：Linux 正常走不到这一支
            // （truncate-in-place 总是开得成），到达只说明路径不可写，与 Eject 无关。
#ifdef _WIN32
            out << "  判据力：Windows → 写不开即「Eject 没真卸」（sharing violation）。本条**有**判据力\n";
#else
            out << "  判据力：Linux → 写不开只说明路径不可写（只读挂载 / 目录已删），非「Eject 没真卸」\n";
#endif
            MarkFailed();
            return;
        }
        // ostreambuf_iterator 走未格式化的 sputc：write() 要 const char*，而
        // reinterpret_cast 本仓库禁用。
        std::ranges::copy(staged, std::ostreambuf_iterator<char>(target));
        if (!target)
        {
            out << "install " << id << " written=false (short write)\n";
            MarkFailed();
            return;
        }
    }
    out << "install " << id << " written=true path=" << entry->BinaryPath.string() << '\n';
    // 这两行不是装饰，是这条命令存在的全部理由：同一个动作为什么在两平台证明的不是
    // 同一件事，读输出的人必须不看文档就知道（spec §4 / CLAUDE.md 规矩 6）。
#ifdef _WIN32
    out << "  判据力：Windows → 覆盖写不开即「Eject 没真卸」（sharing violation）。本条**有**判据力\n";
#else
    out << "  判据力：Linux → 覆盖是 truncate-in-place（同 inode），映射着也写得开。本条**为空转**\n";
#endif
}

void Bench::CmdFileShow(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 1, "file show <id>"))
    {
        MarkFailed();
        return;
    }
    const std::string& id = *args.begin();
    const plan::Entry* entry = FindEntry(id);
    if (entry == nullptr)
    {
        out << "file show: " << id << " is not in the active pod's plan\n";
        MarkFailed();
        return;
    }
    // 无异常编译下只有带 ec 的重载可用；而 ec 必须读——路径消失时 file_size 返回 (uintmax_t)-1、
    // last_write_time 返回 file_time_type::min()，不读就会把它们当有效读数打出去。
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(entry->BinaryPath, ec);
    const std::error_code sizeError = ec; // last_write_time 成功会 clear ec，先留下 file_size 的判定
    const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(entry->BinaryPath, ec);
    if (sizeError || ec)
    {
        out << "show " << id << " path=" << entry->BinaryPath.string() << " size=unavailable mtime=unavailable\n";
        MarkFailed(); // 读不到不是一次有效观察：报告不出来就是没做成
        return;
    }
    // 秒 + **平台自己的 file_clock 历元**（Windows 1601 / Linux 1970），只可与同平台读数比。
    // 原 mtimeNs 有两处错：100ns 滴答乘 100 在 Windows 上溢出打负数，字段名又既非 ns 又非 Unix
    // 历元。libc++ 无 clock_cast（实测编不过），故不折 Unix 秒，改让字段名自陈来历；秒粒度只除
    // 不乘，六条线都不溢出。
    const auto fileClockSeconds = std::chrono::duration_cast<std::chrono::seconds>(stamp.time_since_epoch()).count();
    out << "show " << id << " path=" << entry->BinaryPath.string() << " size=" << size
        << " mtimeFileClockS=" << fileClockSeconds << '\n';
    auto identity = vase::detail::Loader::FileIdentity(entry->BinaryPath);
    if (!identity.IsOk())
    {
        out << "  identity: unavailable — " << identity.GetError().Message() << '\n';
        return; // 指纹缺失是诊断信息，不是命令失败
    }
    const vase::detail::ImageIdentity& value = identity.Value();
    out << "  identity kind=" << (value.Kind == vase::detail::IdentityKind::kPdbCodeView ? "pdbCodeView" : "elfBuildId")
        << " bytes=";
    for (const std::uint8_t byte : value.Bytes)
    {
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
    }
    out << std::dec << '\n';
}

void Bench::CmdGet(std::ostream& out, const std::vector<std::string>& args)
{
    static_cast<void>(args); // 本命令不吃参数，签名由 CommandSpec 的 handler 形状定死。
    vase::PodHandle handle;
    vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr)
    {
        MarkFailed();
        return;
    }
    // TryGet 而不是 Get：Get 走 required=true，缺失即 ProgrammerError 终止。
    // eject 之后正好要用「拿不到」来表达注册已被撤掉。
    const auto* greeter = pod->Root().TryGet<samples::IGreeter>();
    if (greeter == nullptr)
    {
        out << "get: no provider for Vase.Hello.Greeter\n";
        return; // 这是一次有效观察，不是失败
    }
    out << "get: " << greeter->Greet() << '\n';
}

void Bench::CmdEmit(std::ostream& out, const std::vector<std::string>& args)
{
    vase::PodHandle handle;
    vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr || !CheckArity(out, args, 1, "emit <n>"))
    {
        MarkFailed();
        return;
    }
    // 解析式同 ParseIndex / CmdSwap：from_chars + 整串消费判定（`emit 1x` 不算数）。
    int seq = 0;
    const std::string& text = *args.begin();
    const char* const begin = text.data();
    const char* const end = std::next(begin, static_cast<std::ptrdiff_t>(text.size()));
    const std::from_chars_result parsed = std::from_chars(begin, end, seq);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
    {
        out << "emit: <n> must be an integer, got " << text << '\n';
        MarkFailed();
        return;
    }
    // 无人订阅时 Emit 是合法的空派发（§2.4）——eject 之后 `emit 2` 照样成功，不是失败。
    pod->Root().Emit(samples::GreetEvent{seq});
    out << "emitted GreetEvent seq=" << seq << '\n';
}

void Bench::CmdPlugins(std::ostream& out, const std::vector<std::string>& args)
{
    static_cast<void>(args); // 本命令不吃参数，签名由 CommandSpec 的 handler 形状定死。
    vase::PodHandle handle;
    const vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr)
    {
        MarkFailed();
        return;
    }
    out << "plugins live=" << pod->PluginCount() << '\n';
    for (const std::string& id : pod->PluginIds())
    {
        out << "  " << id << '\n';
    }
    // 本台建局走 Strict=true，带失败记录的局交付不出来，故这一支实践中为空；但仍保留——
    // 宽容（Source/Host/PluginHost.cpp 原话「宽容模式（默认）只落记录，Pod 照常交付」）才是
    // 框架默认，这里是唯一会把那种局判成非 clean 的地方，删掉即让改一个选项静默改掉退出码。
    if (PrintFailures(out, pod->Failures()))
    {
        MarkFailed();
    }
}

Bench::LivePod* Bench::Active()
{
    if (!HasActive)
    {
        return nullptr;
    }
    const auto found = Pods.find(ActiveIndex);
    return found == Pods.end() ? nullptr : &found->second;
}

void Bench::MarkFailed() { Failed = true; }

void Bench::UnmatchedCommand(std::ostream& out, const std::string& cmd)
{
    out << "wrong command: " << cmd << '\n'; // 文案是契约：VaseConsoleUnknownCommandReason 按子串匹配。
    MarkFailed();                            // 规则 ① 后半：根本没匹配上，与收到 Err 同判
}

void Bench::TearDownAll(std::ostream& out)
{
    // 升序拆，回放输出才稳定（unordered_map 本身无序）；~PluginHost 的 Debug 计数/活局断言
    // 吃的正是「宿主析构时有没有把局拆平」，漏一支就是 rc=3 崩溃而不是干净的非零。
    std::vector<std::uint32_t> indices;
    indices.reserve(Pods.size());
    for (const auto& entry : Pods)
    {
        indices.push_back(entry.first);
    }
    std::ranges::sort(indices);
    for (const std::uint32_t index : indices)
    {
        const auto it = Pods.find(index);
        if (it == Pods.end())
        {
            continue; // key 即从 Pods 抄来的；保险起见不作裸解引用
        }
        const vase::PodReport report = Host.DestroyPod(it->second.Handle);
        out << "destroy index=" << report.PodIndex << '\n';
        PrintPodReport(out, report);
        if (!report.Clean())
        {
            MarkFailed(); // 规则 ②
        }
        Pods.erase(it);
    }
    HasActive = false;
}

int Bench::Verdict(shell::ShellStop stop) const
{
    // ① 任何命令收到 Err、或根本没匹配上  ② 任何一次 destroy（含收尾拆局）不 Clean → 都记在 Failed 上
    // ③ 脚本结尾没走 exit  ④ 输入流出错
    // 活局在 Verdict 前已被 main 交来拆净（原规则 ③ 并入「exit 拆净所有局」，spec §3.3）。
    if (Failed)
    {
        return 1;
    }
    if (stop != shell::ShellStop::kByExitCommand)
    {
        return 1; // ③ 与 ④：判定从未发生，或这次运行本身不完整
    }
    return 0;
}

} // namespace samples::bench
