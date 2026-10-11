// 静态链入的端到端见证（spec §5.1 #2）：组合库的真静态形**链进本可执行**。
// 与 VasePackTests 的六条恰相反——那些刻意经 Loader 按路径装载（D204），
// 本文件刻意**真链**：宿主取 &VasePlugin_GetPlugin 这个动作本身即 §8.6 要的硬引用。
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Pod.h"

#include <string>
#include <vector>

#ifdef _WIN32
// NOLINTBEGIN(misc-include-cleaner) windows.h 是系统侧伞形头、符号实体散在各子头（canonical 见
// Source/Host/LoaderWindows.cpp）。本区一直盖到下面的 Windows 观测支——那里的 GetProcAddress /
// GetModuleHandleW 同样只由这个伞形头提供。
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#endif

#include <gtest/gtest.h>

// 分发表入口（§8.1 的唯一一跳）：静态链入时它就是本可执行里的一个普通符号。
// 名字是**字面契约**（`Cmake/VasePackDispatch.cpp.in` 与 VASE_PLUGIN 同名生成）。
// NOLINTNEXTLINE(readability-identifier-naming) 名字是 ABI 契约字面、分发表按此名取，无代码级出路
extern "C" const vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id);

// 保活证的观测手段按平台三分岔（链接器对「弱未定义会否拉取归档成员」各说各话），但每一支的承重位
// 与对照组都走**同一套机制**——只测一枚的话，一个恒「不在」的探针会让承重位假通过。机制与理由
// 见 spec §5.2 / D241。（Apple ld 会拉弱未定义，故 macOS 支走 dlsym；Windows 支读产物导出表。）
// NOLINTBEGIN(readability-identifier-naming) 名字是 VASE_PLUGIN 生成物的定名 extern "C" 符号，无代码级出路
#if !defined(_WIN32) && !defined(__APPLE__)
extern "C" const vase::PluginDescriptor* VasePluginDesc_KeepAliveProbePlugin() __attribute__((weak));
extern "C" const vase::PluginDescriptor* VasePluginDesc_HelloPlugin() __attribute__((weak));
#endif
// NOLINTEND(readability-identifier-naming)

namespace
{

// 链接期硬引用：不写这一笔，静态库会被整个丢掉（§8.6 末条 iOS 例外，本波由类型承载）。
constexpr vase::PluginEntryPoint kPackedEntry = &VasePlugin_GetPlugin;

// 承重位：探针**不在**（未提取）。三平台各说各话，但读数同义。
bool KeepAliveProbeAbsent()
{
#ifdef _WIN32
    return ::GetProcAddress(::GetModuleHandleW(nullptr), "VasePluginDesc_KeepAliveProbePlugin") == nullptr;
#elif defined(__APPLE__)
    return ::dlsym(RTLD_SELF, "VasePluginDesc_KeepAliveProbePlugin") == nullptr;
#else
    return VasePluginDesc_KeepAliveProbePlugin == nullptr;
#endif
}

// 对照组：被提取时同一套机制给出「在」——探针坏成恒「不在」时它会转红。
bool ListedDescriptorPresent()
{
#ifdef _WIN32
    return ::GetProcAddress(::GetModuleHandleW(nullptr), "VasePluginDesc_HelloPlugin") != nullptr;
#elif defined(__APPLE__)
    return ::dlsym(RTLD_SELF, "VasePluginDesc_HelloPlugin") != nullptr;
#else
    return VasePluginDesc_HelloPlugin != nullptr;
#endif
}

// NOLINTEND(misc-include-cleaner)

} // namespace

TEST(StaticLink, HostAssemblesStaticallyLinkedCombinedLibrary)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    const std::vector<std::string> ids{"Vase.Hello", "Vase.Dependent", "Vase.Failing"};
    for (const std::string& id : ids)
    {
        vase::LoadPlanEntry entry{};
        entry.Id = id;
        entry.InProcessEntry = kPackedEntry;
        plan.Ordered.push_back(entry);
    }

    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message(); // 宽容语义：Failed 不翻整局（D70 同形）
    const vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);

    // FailingPlugin 的 OnStart 返回 Err → 只落 Failed 记录、实例不进活集合；另两只照常活。
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.Failing");
    EXPECT_EQ(pod->Failures().begin()->Stage, vase::Phase::kStart);

    // 三个 Id 各自的实例都在（Failing 按设计只剩记录）。
    EXPECT_TRUE(pod->HasPlugin("Vase.Hello"));
    EXPECT_TRUE(pod->HasPlugin("Vase.Dependent"));
    EXPECT_FALSE(pod->HasPlugin("Vase.Failing"));
    EXPECT_EQ(pod->PluginCount(), 2U);

    // 服务解析走通：Dependent 的 OnStart 凭声明 Get<IGreeter>（Get 失败在本项目是 terminate，
    // 它活着即链通）；账本那一条边正对应该解析。
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 1U);

    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}

TEST(StaticLink, EjectIsRefusedOnStaticallyLinkedPlugins)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    vase::LoadPlanEntry entry{};
    entry.Id = "Vase.Hello";
    entry.InProcessEntry = kPackedEntry;
    plan.Ordered.push_back(entry);
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(created.Value(), "Vase.Hello");
    ASSERT_FALSE(ejected.IsOk());
    EXPECT_NE(ejected.GetError().Message().find("linked-in binary cannot be ejected"), std::string::npos);

    // 早闸在任何拆除之前：实例仍在（下一句 DestroyPod 能干净拆掉就是证据）
    ASSERT_NE(host.Resolve(created.Value()), nullptr);
    static_cast<void>(host.DestroyPod(created.Value()));
}

TEST(StaticLink, ListedPackObjectIsExtracted) // 对照组 / 证伪线
{
    // 已列入 PLUGINS 的插件对象必被提取（分发表 TU 硬引用它）。本句**红了**就说明探针机制
    // 本身坏了（恒报「不在」），承重位那条随即无意义——所以它是承重位的前提。
    EXPECT_TRUE(ListedDescriptorPresent());
}

TEST(StaticLink, UnlistedPackObjectIsNotExtracted) // 承重位
{
    EXPECT_TRUE(KeepAliveProbeAbsent()) << "未列入 PLUGINS 的 .Pack 对象被提取了——§8.6 的保活链不成立";
}
