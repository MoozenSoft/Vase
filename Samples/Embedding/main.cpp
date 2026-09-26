// VaseEmbedding —— 验证宿主（§10.1：没有 UI、没有业务，只有演示命令）。
// 它证明的是「干净地起、干净地灭、可重复无数次」：play 跑一局并把报告打出来，
// loop <N> 连跑 N 局，任一局不 Clean 就非零退出（CI 化的判据 #1）。

#include "AdoptExpectations.h" // 期望工厂住 Tests/TestingSupport（header-only；include 路径在 CMake 里开）——
                               // hello 描述符漂移时测试证人与本演示同一家盯住，不留第二份抄本。
#include "Greeter.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

// —— 宿主侧的服务：只在这里定义，插件不认它（宿主服务与插件服务的登记方式相同）。——
class IHostMarker
{
public:
    static constexpr std::string_view kName = "Samples.Embedding.Marker";
    static constexpr std::uint32_t kVersion = 1;

    IHostMarker() = default;
    IHostMarker(const IHostMarker&) = delete;
    IHostMarker& operator=(const IHostMarker&) = delete;
    IHostMarker(IHostMarker&&) = delete;
    IHostMarker& operator=(IHostMarker&&) = delete;
    virtual ~IHostMarker() = default;

    [[nodiscard]] virtual std::string_view Marker() const = 0;
};

class HostMarker final : public IHostMarker
{
public:
    [[nodiscard]] std::string_view Marker() const override { return "VaseEmbedding"; }
};

// 登记进根 Context 的实例必须活得比 Pod 久——借用一个函数内 static。
IHostMarker& Marker()
{
    static HostMarker marker;
    return marker;
}

// §5.3 阶段 0：宿主服务的注册点。根 Context 上的 Provide 判为 kHost（§6.1），不落账本边。
void RegisterHostServices(vase::Context& ctx) { ctx.Provide<IHostMarker>(Marker()); }

// 进程唯一 Host 用函数内 static（§1.4）；它的析构要逐个卸载驻留镜像。
vase::PluginHost& Host()
{
    static vase::PluginHost host;
    return host;
}

// 路径不写成命名空间级对象：`std::filesystem::path` 的构造可能分配，
// 静态初始化那一类会被 cert-err58-cpp 盯上。用函数返回，与 LoaderTests 同写法。
std::filesystem::path HelloBinaryPath() { return std::filesystem::path{VASE_HELLO_PATH}; }

// 无变参的输出：printf 一族过不了 cppcoreguidelines-pro-type-vararg（与 Fail.h 同写法）。
void Out(std::string_view text) { static_cast<void>(std::fwrite(text.data(), 1, text.size(), stdout)); }

vase::LoadPlan HelloPlan()
{
    vase::LoadPlan plan;
    plan.Ordered.push_back(vase::LoadPlanEntry{.Id = "Vase.Hello", .BinaryPath = HelloBinaryPath()});
    return plan;
}

vase::PodOptions HostOptions()
{
    vase::PodOptions options;
    options.Strict = true;
    options.Stage0 = &RegisterHostServices;
    return options;
}

void PrintReport(const vase::PodReport& report)
{
    Out(std::string{"clean="} + (report.Clean() ? "true" : "false") + "\n");

    std::string line{"counters diff (baseline = pod creation):"};
    line += " effects=" + std::to_string(report.CountersDiff.Effects);
    line += " services=" + std::to_string(report.CountersDiff.Services);
    line += " subscriptions=" + std::to_string(report.CountersDiff.Subscriptions);
    line += " pluginInstances=" + std::to_string(report.CountersDiff.PluginInstances);
    line += " scopes=" + std::to_string(report.CountersDiff.Scopes);
    line += '\n';
    Out(line);

    for (const vase::FailedPluginRecord& failure : report.Failures)
    {
        Out("failed: " + failure.Id + " (" + failure.Message + ")\n");
    }
    for (const vase::ResidualEntry& residual : report.Residuals)
    {
        Out("residual: " + residual.OwnerLabel + " x" + std::to_string(residual.Count) + "\n");
    }
}

// 手写十进制解析：std::stoi 在本项目不可用（_HAS_EXCEPTIONS=0，解析失败直接终止进程）。
bool ParseCount(std::string_view text, int& out)
{
    int value = 0;
    const char* const begin = text.data();
    const char* const end = std::next(begin, static_cast<std::ptrdiff_t>(text.size()));
    const std::from_chars_result parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end || value <= 0)
    {
        return false;
    }
    out = value;
    return true;
}

const char* BoolText(bool value) { return value ? "true" : "false"; }

// §9.2「每次进出必须有账可查」的演示形态：把报告的每个判据都打出来，不看文档也知道
// 本平台哪个字段有判据力（UnloadEvidence 的 IsMeaningful / IsObservable 自己声明）。
void PrintEjectReport(const vase::EjectReport& report)
{
    std::string line{"eject " + report.PluginId};
    line += " binaryUnloaded=";
    line += BoolText(report.BinaryActuallyUnloaded);
    line += " mappingRemoved=";
    line += BoolText(report.MappingRemoved);
    line += " reopenWritable=";
    line += BoolText(report.ReopenWritable);
    // 两个 Is… 是本平台「哪个字段有判据力」的**声明**（§8.2 把这件事写进了结构）：不打出来，
    // Windows 上那个 mappingRemoved=false 就成了没解释的噪声——它本来就不该被读作失败。
    line += " mappingRemovalIsObservable=";
    line += BoolText(report.MappingRemovalIsObservable);
    line += " reopenWritableIsMeaningful=";
    line += BoolText(report.ReopenWritableIsMeaningful);
    line += '\n';
    Out(line);
    if (!report.HotSwapNote.empty())
    {
        Out("  note: " + report.HotSwapNote + "\n");
    }
}

void PrintAdoptReport(const vase::AdoptReport& report)
{
    std::string line{"adopt " + report.PluginId};
    line += " reusedResidentImage=";
    line += BoolText(report.ReusedResidentImage);
    line += " identityVerified=";
    line += BoolText(report.IdentityVerified);
    line += " importEnforcementPassed=";
    line += BoolText(report.ImportEnforcementPassed);
    line += " outgoingEdges=" + std::to_string(report.OutgoingEdges);
    line += '\n';
    Out(line);
}

int Usage()
{
    Out("usage: VaseEmbedding play | loop <N> | swapdemo\n");
    return 2;
}

int Play()
{
    vase::PluginHost& host = Host();
    const vase::Result<vase::PodHandle> created = host.CreatePod(HelloPlan(), HostOptions());
    if (!created.IsOk())
    {
        Out("CreatePod failed: " + created.GetError().Message() + "\n");
        return 1;
    }

    vase::Pod& pod = *host.Resolve(created.Value());
    pod.Root().Emit(samples::GreetEvent{1});
    // 宿主根 Context 的解析：不落账本边、不受「凭声明」限制（§5.6 括注）
    Out(std::string{"IGreeter says: "} + std::string{pod.Root().Get<samples::IGreeter>().Greet()} + "\n");

    const vase::PodReport report = host.DestroyPod(created.Value());
    PrintReport(report);
    return report.Clean() ? 0 : 1;
}

int Loop(int count)
{
    vase::PluginHost& host = Host();
    for (int cycle = 0; cycle < count; ++cycle)
    {
        const vase::Result<vase::PodHandle> created = host.CreatePod(HelloPlan(), HostOptions());
        if (!created.IsOk())
        {
            Out("cycle " + std::to_string(cycle) + ": CreatePod failed: " + created.GetError().Message() + "\n");
            return 1;
        }
        const vase::PodReport report = host.DestroyPod(created.Value());
        if (!report.Clean())
        {
            Out("cycle " + std::to_string(cycle) + ": not clean\n");
            PrintReport(report);
            return 1;
        }
    }
    Out(std::to_string(count) + " cycles, all clean\n");
    return 0;
}

// 热插拔一遍：play → eject → adopt → stop，每一步的报告都打出来（T10/T11 的端到端演示）。
// 与 play 的差别只在中间两步——首尾同形，说明进出不改「干净地起、干净地灭」这件事。
int SwapDemo()
{
    vase::PluginHost& host = Host();
    const vase::Result<vase::PodHandle> created = host.CreatePod(HelloPlan(), HostOptions());
    if (!created.IsOk())
    {
        Out("CreatePod failed: " + created.GetError().Message() + "\n");
        return 1;
    }
    const vase::PodHandle pod = created.Value();
    vase::Pod& running = *host.Resolve(pod);

    running.Root().Emit(samples::GreetEvent{1});
    Out(std::string{"play: IGreeter says "} + std::string{running.Root().Get<samples::IGreeter>().Greet()} + "\n");

    // D21 后 Ok ≠ 拆净/入净：执法拒绝走 Status。本演示的局只有 Vase.Hello——无消费者
    // （宿主侧解析不落边）也无同名提供方，拒绝支结构性不可达，故不展开 Status 检查。
    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(pod, "Vase.Hello");
    if (!ejected.IsOk())
    {
        Out("EjectPlugin failed: " + ejected.GetError().Message() + "\n");
        host.DestroyPod(pod);
        return 1;
    }
    PrintEjectReport(ejected.Value());
    Out("  plugins after eject: " + std::to_string(running.PluginCount()) + "\n");

    // T12 单轨：Adopt 必须自带期望（D69）。期望工厂住在 Tests/TestingSupport（header-only，
    // 链头不链库）——与 gtest 证人共读一份描述符事实，Hello 漂移时两边一起红。
    const vase::ManifestExpectation hello = testing_support::MakeHelloExpectation();
    vase::AdoptRequest request;
    request.Id = "Vase.Hello";
    request.BinaryPath = HelloBinaryPath();
    request.Expected = &hello; // 借用止于这一次同步调用
    const vase::Result<vase::AdoptReport> adopted = host.AdoptPlugin(pod, request);
    if (!adopted.IsOk())
    {
        Out("AdoptPlugin failed: " + adopted.GetError().Message() + "\n");
        host.DestroyPod(pod);
        return 1;
    }
    PrintAdoptReport(adopted.Value());
    Out("  plugins after adopt: " + std::to_string(running.PluginCount()) + "\n");

    const vase::PodReport report = host.DestroyPod(pod);
    PrintReport(report);
    return report.Clean() ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        return Usage();
    }
    // argv[i] 的下标写法过不了 cppcoreguidelines-pro-bounds-*，用 std::next（与 MetaArray 同写法）。
    const std::string_view command{*std::next(argv, 1)};
    if (command == "play")
    {
        return Play();
    }
    if (command == "loop")
    {
        int count = 0;
        if (argc < 3 || !ParseCount(*std::next(argv, 2), count))
        {
            return Usage();
        }
        return Loop(count);
    }
    if (command == "swapdemo")
    {
        return SwapDemo();
    }
    return Usage();
}
