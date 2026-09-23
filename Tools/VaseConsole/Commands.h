#pragma once

#include "PlanFile.h"
#include "Shell.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace samples::bench
{

class Bench
{
public:
    Bench() = default;
    Bench(const Bench&) = delete;
    Bench& operator=(const Bench&) = delete;
    Bench(Bench&&) = delete;
    Bench& operator=(Bench&&) = delete;
    ~Bench() = default;

    [[nodiscard]] std::vector<shell::CommandSpec> Commands();
    [[nodiscard]] int Verdict(shell::ShellStop stop) const;

    // 拆净所有活局（spec §3.3「exit 拆净所有局」+ ~PluginHost 的 Debug 断言要求「宿主析构无活局」）：
    // main 在 RunShell 之后、Verdict 之前无条件调用；逐局 DestroyPod + 打完整报告，Clean 照常走规则 ②。
    void TearDownAll(std::ostream& out);
    // 接进 RunShell 的未匹配回调：敲错一条线同样算失败（规则 ① 后半）。
    void UnmatchedCommand(std::ostream& out, const std::string& cmd);

private:
    // 只存 PodHandle（两个 uint32）、字符串与字节：**装不下实例级对象**（铁律 §1.2）。
    // Pod* 一律每次现 Resolve，不缓存（§1.4 句柄语义 / 9.3 宿主纪律）。
    struct LivePod
    {
        vase::PodHandle Handle;
        std::filesystem::path PlanPath;
        std::vector<plan::Entry> Entries;
    };

    void CmdPodNew(std::ostream& out, const std::vector<std::string>& args);
    void CmdPodUse(std::ostream& out, const std::vector<std::string>& args);
    void CmdPodList(std::ostream& out, const std::vector<std::string>& args);
    void CmdPodDestroy(std::ostream& out, const std::vector<std::string>& args);
    void CmdEject(std::ostream& out, const std::vector<std::string>& args);
    void CmdAdopt(std::ostream& out, const std::vector<std::string>& args);
    void CmdSwap(std::ostream& out, const std::vector<std::string>& args);
    void CmdFileStage(std::ostream& out, const std::vector<std::string>& args);
    void CmdFileInstall(std::ostream& out, const std::vector<std::string>& args);
    void CmdFileShow(std::ostream& out, const std::vector<std::string>& args);
    void CmdGet(std::ostream& out, const std::vector<std::string>& args);
    void CmdEmit(std::ostream& out, const std::vector<std::string>& args);
    void CmdPlugins(std::ostream& out, const std::vector<std::string>& args);

    // 「为哪个 Id 暂了哪些字节」，绑成一个值以免两者错配。**槽是会话级的、不随 pod 生命周期**：
    // pod new / destroy 都不清它，故同一 Id 的字节可能被装到之后某个 pod 的登记路径上（CmdFileInstall）。
    std::optional<std::pair<std::string, std::vector<std::uint8_t>>> Staged;

    [[nodiscard]] LivePod* Active();
    void MarkFailed();
    [[nodiscard]] static bool ParseIndex(std::string_view text, std::uint32_t& out);

    // 命令处理器的公共前置：没有活动局 / 句柄已失效，就把话说清并返回 nullptr。
    // handleOut 只在返回非空时被写——所有 Eject/Adopt 调用用的都是它，**不从 Pod* 反推句柄**
    // （Pod 没有公开的 Handle() 访问器，也不该有：句柄是宿主的账）。
    [[nodiscard]] vase::Pod* ResolveActivePod(std::ostream& out, vase::PodHandle& handleOut);
    [[nodiscard]] const plan::Entry* FindEntry(std::string_view id); // 在 Active()->Entries 里按 Id 找

    vase::PluginHost Host;
    std::unordered_map<std::uint32_t, LivePod> Pods; // key = PodHandle::Index（销毁时擦除）
    std::uint32_t ActiveIndex = 0;
    bool HasActive = false;
    bool Failed = false;
};

} // namespace samples::bench
