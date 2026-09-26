#include "AdoptExpectations.h"
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Plugin.h" // 服务接口标识（测试侧只借 kName/kVersion 查表，不跨界 new）
#include "Vase/Pod/Pod.h"

#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "../Integration/fixtures/M2Common.h"

namespace
{

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({.Id = id, .BinaryPath = path});
    }
    return plan;
}

int EchoValue(vase::PluginHost& host, const vase::PodHandle& h)
{
    vase::Pod* pod = host.Resolve(h);
    return pod->Root().Get<m2_fixture::IConfigEcho>().Value();
}

TEST(ConfigApply, BlobOverrideReachesTypedStruct)
{
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Echo", vase::Value::From<std::int32_t>(7));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(EchoValue(host, h), 7); // D23：blob 有键用 blob 值
    host.DestroyPod(h);
}

TEST(ConfigApply, EmptyBlobFallsBackToDescriptorDefaults)
{
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}})).Value();
    EXPECT_EQ(EchoValue(host, h), 1); // ConsumerConfig.Echo 的元组默认值
    host.DestroyPod(h);
}

TEST(ConfigApply, HostSuppliedStringConfigOutlivesPlanDonor)
{
    // R-F1 钉用例：宿主供的 string 配置必须在计划销毁后仍可读。Banner() 是**现场读**配置
    // 结构体里的借用指针（非 OnLoad 深拷）——修复前供给源是 entry.ResolvedConfig，计划一死
    // 即悬空（Debug 堆读出垃圾）；修复后单源 slot->Replays 覆盖实例存活期。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Banner", vase::Value::From<const char*>("host-string"));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    plan.Ordered.clear(); // 供体先死——此刻起读到的每个字节都由槽供给
    EXPECT_STREQ(host.Resolve(h)->Root().Get<m2_fixture::IConfigEcho>().Banner(), "host-string");
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(ConfigApply, KindMismatchIsPluginFailureNotAbort)
{
    // D32：宿主给的数据错 = 可恢复装配失败。float 灌 int32 字段 → Failed 记录，局照开。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Echo", vase::Value::From<float>(2.5F));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U);
    ASSERT_EQ(host.Resolve(h)->Failures().size(), 1U);
    // spec §6 契约：消息含插件 Id + 字段名 + 两侧 Kind（值 kFloat 灌 kInt32 字段）。
    const std::string& text = host.Resolve(h)->Failures().begin()->Message;
    EXPECT_NE(text.find("config field Echo"), std::string::npos);
    EXPECT_NE(text.find("Vase.ConfigConsumer"), std::string::npos);
    EXPECT_NE(text.find("kFloat"), std::string::npos);
    EXPECT_NE(text.find("kInt32"), std::string::npos);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(ConfigApplyEnum, OutOfChoicesBlobIsErrNotApply)
{
    // D79：手写 LoadPlan 绕开清单/Solve 的合法域——kEnum 值 99 ∉ {0,1} 灌 Mood 字段。
    // From<int32>(99) 再改 Kind 即域外位形（等价 Solve 侧的 EnumStored{99}）；成员名是 Mood。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    vase::Value rogue = vase::Value::From<std::int32_t>(99);
    rogue.Kind = vase::ValueKind::kEnum;
    plan.Ordered.begin()->ResolvedConfig.Set("Mood", rogue);
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U); // Err 不触 ProgrammerError：局照开，条目落 Failed
    ASSERT_EQ(host.Resolve(h)->Failures().size(), 1U);
    const vase::FailedPluginRecord& failure = *host.Resolve(h)->Failures().begin();
    EXPECT_EQ(failure.Stage, vase::Phase::kLoad);
    // 消息含 "not in choices" + 字段名 + 违例值（D32 家族同位：光看记录就要能定位）。
    EXPECT_NE(failure.Message.find("not in choices"), std::string::npos);
    EXPECT_NE(failure.Message.find("Mood"), std::string::npos);
    EXPECT_NE(failure.Message.find("99"), std::string::npos);
    EXPECT_NE(failure.Message.find("Vase.ConfigConsumer"), std::string::npos);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(ConfigApplyEnum, InChoicesApplies)
{
    // D79 的正半句（T5 挪来的第二例）：域内 kEnum 值 1 ∈ {0,1} 灌 Mood → 装配通过、
    // 值真的落进成员——MoodLabel 是现场读回，绿了就证明「闸放行 + Apply 写对」两件事。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    vase::Value legal = vase::Value::From<std::int32_t>(1);
    legal.Kind = vase::ValueKind::kEnum;
    plan.Ordered.begin()->ResolvedConfig.Set("Mood", legal);
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 1U);
    EXPECT_STREQ(host.Resolve(h)->Root().Get<m2_fixture::IConfigEcho>().MoodLabel(), "响亮");
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(ConfigApply, OutOfRangeAppliesAnyway)
{
    // D35 钉「无执法」：Meta 写着 0..10，999 照写不误。执法归 M2b Preset 校验。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Echo", vase::Value::From<std::int32_t>(999));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(EchoValue(host, h), 999);
    host.DestroyPod(h);
}

TEST(ConfigApply, LayoutMismatchTerminates)
{
    // D25 读取端防线：sizeof 与 ConfigInfo.StructSize 不符 → ProgrammerError（两构建 abort）。
    EXPECT_DEATH(
        {
            vase::PluginHost host;
            static_cast<void>(host.CreatePod(Plan({{"Vase.ConfigLayoutMisuse", VASE_FIXTURE_CONFIGLAYOUTMISUSE}})));
        },
        "layout mismatch");
    SUCCEED();
}

TEST(ConfigApply, AdoptReplaysPlanBlobAndNewIdGetsDefaults)
{
    // D34：eject+adopt 配置如如不动；从未入局的新 Id 走默认。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Echo", vase::Value::From<std::int32_t>(7));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(EchoValue(host, h), 7);
    const vase::ManifestExpectation consumer = testing_support::MakeConfigConsumerExpectation();
    vase::AdoptRequest consumerRequest;
    consumerRequest.Id = "Vase.ConfigConsumer";
    consumerRequest.BinaryPath = VASE_FIXTURE_CONFIGCONSUMER;
    consumerRequest.Expected = &consumer; // 借用止于两次同步调用
    ASSERT_TRUE(host.EjectPlugin(h, "Vase.ConfigConsumer").IsOk());
    ASSERT_TRUE(host.AdoptPlugin(h, consumerRequest).IsOk());
    EXPECT_EQ(EchoValue(host, h), 7); // 回放：与 eject 前一致（覆盖没丢）
    EXPECT_TRUE(host.DestroyPod(h).Clean());

    const vase::PodHandle second = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    ASSERT_TRUE(host.AdoptPlugin(second, consumerRequest).IsOk()); // 路径与期望由请求自带，与别局无关
    EXPECT_EQ(EchoValue(host, second), 1);                         // 本局回放表没它 → 默认层
    host.DestroyPod(second);
}

} // namespace
