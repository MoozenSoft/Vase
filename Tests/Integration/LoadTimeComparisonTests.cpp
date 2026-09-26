// 加载期期望比对（判据 13；D67/D72/D89）：每例只篡改一个字段。基期望住 TestingSupport 工厂
// （与 AdoptTests 共用）；双服务逆序声明材料 = TwoProvidersPlugin（顺带 D89 全零 .Config）。
#include "AdoptExpectations.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

namespace
{

using testing_support::MakeHelloExpectation;
using vase::ConfigBlob;
using vase::ExpectedChoice;
using vase::ExpectedConfigField;
using vase::ExpectedService;
using vase::ManifestExpectation;
using vase::ValueKind;

// TwoProvidersPlugin 的描述符是 .Provides = {Second v2, First v1}（作者序刻意逆序）、无 .Config。
ManifestExpectation TwoProvidersExpectation()
{
    ManifestExpectation expected;
    expected.Id = "Vase.Test.TwoProviders";
    expected.DisplayName = "双服务比对材料";
    expected.Version = "0.0.1";
    expected.Provides = {
        ExpectedService{.Name = "Vase.Test.First", .Version = 1},
        ExpectedService{.Name = "Vase.Test.Second", .Version = 2},
    };
    return expected;
}

vase::LoadPlan SingleEntryPlan(std::string_view id, const std::filesystem::path& binaryPath,
                               std::optional<ManifestExpectation> expected)
{
    vase::LoadPlan plan;
    vase::LoadPlanEntry entry;
    entry.Id = id; // 借用 = 字面量（静态存储），与既有手写计划同法
    entry.BinaryPath = binaryPath;
    entry.Expected = std::move(expected);
    plan.Ordered.push_back(std::move(entry));
    return plan;
}

void ExpectLoaded(vase::PluginHost& host, const vase::LoadPlan& plan)
{
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk());
    const vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 1U);
    EXPECT_TRUE(pod->Failures().empty());
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}

void ExpectLoadRefused(vase::PluginHost& host, const vase::LoadPlan& plan, std::string_view id,
                       std::initializer_list<std::string_view> needles)
{
    // 宽容语义（D70）：比对失败 = Failed 记录（Phase::kLoad），局照开；断言形抄 FailureSemanticsTests。
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk());
    const vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 0U);
    ASSERT_EQ(pod->Failures().size(), 1U);
    const vase::FailedPluginRecord& failure = *pod->Failures().begin();
    EXPECT_EQ(failure.Id, id);
    EXPECT_EQ(failure.Stage, vase::Phase::kLoad);
    for (const std::string_view needle : needles)
    {
        EXPECT_NE(failure.Message.find(needle), std::string::npos) << failure.Message;
    }
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}

TEST(LoadTimeComparison, ExactMatchLoads)
{
    vase::PluginHost host;
    ExpectLoaded(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, MakeHelloExpectation()));
}

TEST(LoadTimeComparison, NullExpectedSkipsCompare)
{
    // D68 保护性断言：Expected 无值 = M2a 行为逐字节一致（既有全部手写计划用例由全量套件兜底）。
    vase::PluginHost host;
    ExpectLoaded(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, std::nullopt));
}

TEST(LoadTimeComparison, DisplayNameDriftRefused)
{
    vase::PluginHost host;
    ManifestExpectation expected = MakeHelloExpectation();
    expected.DisplayName = "示例插件X";
    ExpectLoadRefused(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, std::move(expected)), "Vase.Hello",
                      {"manifest/binary mismatch", "displayName", "示例插件X"});
}

TEST(LoadTimeComparison, ServiceSetOrderInsensitive)
{
    // 期望 First→Second、二进制声明 Second→First：(name,version) 多重集等值，序不敏感（D72）。
    vase::PluginHost host;
    ExpectLoaded(host, SingleEntryPlan("Vase.Test.TwoProviders", VASE_FIXTURE_TWOPROVIDERS, TwoProvidersExpectation()));
}

TEST(LoadTimeComparison, ServiceMissingRefused)
{
    // 期望多声明一条 → 拒，点名 provides[<service> v<n>]；缺员同判据由多重集走查两侧对称给出。
    vase::PluginHost host;
    ManifestExpectation expected = MakeHelloExpectation();
    expected.Provides.push_back(ExpectedService{.Name = "Vase.Ghost", .Version = 2});
    ExpectLoadRefused(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, std::move(expected)), "Vase.Hello",
                      {"manifest/binary mismatch", "provides[Vase.Ghost v2]"});
}

TEST(LoadTimeComparison, ConfigKindDriftRefused)
{
    vase::PluginHost host;
    ManifestExpectation expected = MakeHelloExpectation();
    ExpectedConfigField& repeats = *expected.Config.begin();
    repeats.Kind = ValueKind::kInt64;
    ExpectLoadRefused(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, std::move(expected)), "Vase.Hello",
                      {"manifest/binary mismatch", "config \"Repeats\".kind", "kInt64", "kInt32"});
}

TEST(LoadTimeComparison, ChoicesOrderSensitiveRefused)
{
    // choices 是唯一序敏感的集合面（编辑器按下拉顺序展示，换序即漂移，D72 末段）。
    vase::PluginHost host;
    ManifestExpectation expected = MakeHelloExpectation();
    ExpectedConfigField& mood = *std::next(expected.Config.begin(), static_cast<std::ptrdiff_t>(1));
    ExpectedChoice& first = *mood.Choices.begin();
    ExpectedChoice& second = *std::next(mood.Choices.begin());
    std::swap(first, second);
    ExpectLoadRefused(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, std::move(expected)), "Vase.Hello",
                      {"manifest/binary mismatch", "config \"MoodValue\".choices[0]"});
}

TEST(LoadTimeComparison, EmptyConfigVsManifestDeclinesRefused)
{
    // D89：描述符 .Config 全零（忘写的静默点）配带 config 的期望 → 拒——key 集双向等值天然覆盖。
    vase::PluginHost host;
    ManifestExpectation expected = TwoProvidersExpectation();
    expected.Config.push_back(ExpectedConfigField{
        .Key = "Repeats",
        .Kind = ValueKind::kInt32,
        .Default = ConfigBlob::Storage{std::int32_t{1}},
    });
    ExpectLoadRefused(host, SingleEntryPlan("Vase.Test.TwoProviders", VASE_FIXTURE_TWOPROVIDERS, std::move(expected)),
                      "Vase.Test.TwoProviders", {"manifest/binary mismatch", "config \"Repeats\""});
}

TEST(LoadTimeComparison, ExpectationOmitsConfigKeyRefused)
{
    // 反向臂（fix1 / 评审 Important）：期望**少了**一个 key、描述符多声明 → 走 binary-only 分支，
    // 钉 "missing in manifest" 全句；正向臂（上两条）不含它，两臂各留证人。
    vase::PluginHost host;
    ManifestExpectation expected = MakeHelloExpectation();
    expected.Config.erase(std::next(expected.Config.begin(), static_cast<std::ptrdiff_t>(1))); // 去 MoodValue
    ExpectLoadRefused(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, std::move(expected)), "Vase.Hello",
                      {"manifest/binary mismatch", "config \"MoodValue\" missing in manifest"});
}

TEST(LoadTimeComparison, ExpectationOmitsServiceRefused)
{
    // 同族反向：期望少一条 provides、二进制仍声明两条 → 归并走查 binary-only 臂。
    vase::PluginHost host;
    ManifestExpectation expected = TwoProvidersExpectation();
    expected.Provides.pop_back(); // 去 Second v2
    ExpectLoadRefused(host, SingleEntryPlan("Vase.Test.TwoProviders", VASE_FIXTURE_TWOPROVIDERS, std::move(expected)),
                      "Vase.Test.TwoProviders",
                      {"manifest/binary mismatch", "provides[Vase.Test.Second v2] missing in manifest"});
}

} // namespace
