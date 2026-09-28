// spec §4 逐执法线（D48/D49/D64）。断言 = IsOk + 字段值；错误线只验「响且不绿」，
// 消息不逐字钉（最小区分集纪律，spec §7）。

#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"

#include "Scan.h"

#include "Vase/Host/ManifestExpectation.h"
#include "Vase/PluginDescriptor.h"

#include "CatalogSandbox.h"

#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{

using testing_support::CatalogSandbox;
using vase::ManifestConfigField;
using vase::ManifestEntry;
using vase::ParseManifestFile;
using vase::Result;
using vase::ValueKind;

// M3/D93 用例的临时清单写手：temp 根下的一次性树（与 CatalogSandbox 同法，进程退出随静态树
// 清；残留只是 temp 垃圾，不判据）。整份 JSON 原样落盘，返回清单路径。
std::filesystem::path WriteTempManifest(std::string_view content)
{
    static CatalogSandbox gSandbox{"manifest-json-temp"};
    const auto ns = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string rel = "m" + std::to_string(ns) + "/plugin.json";
    gSandbox.WriteFile(rel, content);
    return gSandbox.Root / rel;
}

class ManifestJson : public ::testing::Test
{
public:
    CatalogSandbox Sandbox{"manifest-json"};

    static std::string Wrapped(std::string_view fields)
    {
        std::string out = R"({"schemaVersion":1,"id":"Vase.Probe")";
        if (!fields.empty())
        {
            out += ',';
            out += fields;
        }
        out += '}';
        return out;
    }

    Result<ManifestEntry> Parse(std::string_view fields, std::string_view sub = "probe") const
    {
        Sandbox.WriteFile(std::string(sub) + "/plugin.json", Wrapped(fields));
        return ParseManifestFile(Sandbox.Root / sub / "plugin.json", sub);
    }

    void ExpectReject(std::string_view fields) const
    {
        const auto parsed = Parse(fields);
        EXPECT_FALSE(parsed.IsOk()) << "accepted: " << Wrapped(fields);
    }
};

TEST_F(ManifestJson, MinimalFillsDefaults)
{
    const auto parsed = Parse("");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const ManifestEntry& entry = parsed.Value();
    EXPECT_EQ(entry.Id, "Vase.Probe");
    EXPECT_EQ(entry.DisplayName, "Vase.Probe");
    EXPECT_TRUE(entry.Version.empty());
    EXPECT_EQ(entry.Binary, "probe"); // 缺省 = 子目录名（D54）
    EXPECT_TRUE(entry.EnabledByDefault);
    EXPECT_TRUE(entry.Requires.empty());
    EXPECT_TRUE(entry.OptionalRequires.empty());
    EXPECT_TRUE(entry.Provides.empty());
    EXPECT_TRUE(entry.Config.empty());
}

TEST_F(ManifestJson, AcceptsFullShape)
{
    const auto parsed =
        Parse(R"("displayName":"探针","version":"1.2.0","binary":"ProbeBin","enabledByDefault":false)"
              R"(,"requires":[{"service":"Vase.World","version":1}])"
              R"(,"provides":[{"service":"Vase.P","version":2}])"
              R"(,"config":[{"key":"speed","type":"float","default":2,"min":1,"max":10,"displayName":"速度"}])");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const ManifestEntry& entry = parsed.Value();
    EXPECT_EQ(entry.DisplayName, "探针");
    EXPECT_EQ(entry.Version, "1.2.0");
    EXPECT_EQ(entry.Binary, "ProbeBin");
    EXPECT_EQ(entry.Subdirectory, "probe");
    EXPECT_FALSE(entry.EnabledByDefault);
    ASSERT_EQ(entry.Requires.size(), 1U);
    EXPECT_EQ(entry.Requires.begin()->Service, "Vase.World");
    EXPECT_EQ(entry.Requires.begin()->Version, 1U);
    ASSERT_EQ(entry.Config.size(), 1U);
    const ManifestConfigField& speed = *entry.Config.begin();
    EXPECT_EQ(speed.Kind, ValueKind::kFloat);
    EXPECT_EQ(speed.DefaultBits, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(2.0F))); // int→float 加宽
    EXPECT_TRUE(speed.HasMin);
    EXPECT_TRUE(speed.HasMax);
    EXPECT_EQ(speed.DisplayName, "速度");
}

TEST_F(ManifestJson, RejectsMissingOrEmptyId)
{
    Sandbox.WriteFile("pe/plugin.json", R"({"schemaVersion":1,"id":""})");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "pe" / "plugin.json", "pe").IsOk());
    Sandbox.WriteFile("ni/plugin.json", R"({"schemaVersion":1})");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "ni" / "plugin.json", "ni").IsOk()); // 缺 id
}

TEST_F(ManifestJson, RejectsNonIntegerSchemaVersion)
{
    Sandbox.WriteFile("p1/plugin.json", R"({"schemaVersion":1.0,"id":"Vase.Probe"})");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "p1" / "plugin.json", "p1").IsOk()); // 1.0 拒，D64
}

TEST_F(ManifestJson, RejectsMajorTwo)
{
    Sandbox.WriteFile("p2/plugin.json", R"({"schemaVersion":2,"id":"Vase.Probe"})");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "p2" / "plugin.json", "p2").IsOk());
}

TEST_F(ManifestJson, RejectsUnknownTopLevelField) { ExpectReject(R"("author":"someone")"); } // D49

TEST_F(ManifestJson, RejectsMalformedJson)
{
    Sandbox.WriteFile("p3/plugin.json", "{\"schemaVersion\":1,,}");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "p3" / "plugin.json", "p3").IsOk());
}

TEST_F(ManifestJson, RejectsBinaryPathChars) // D64 路径与点号闸
{
    ExpectReject(R"("binary":"../evil")");
    ExpectReject(R"("binary":"..")");
    ExpectReject(R"("binary":".")");
    ExpectReject(R"("binary":"")");
    ExpectReject(R"("binary":"sub\\dir")"); // JSON 里 \\ 解码为单个 \，真走 find_first_of 反斜杠分支
}

TEST_F(ManifestJson, RejectsDepEntryShapes)
{
    ExpectReject(R"("requires":[{"service":"S"}])");                                         // 缺 version
    ExpectReject(R"("requires":[{"service":"S","version":0}])");                             // version ≥1（D64）
    ExpectReject(R"("requires":[{"service":"S","version":1,"extra":1}])");                   // 条目 unknown 字段
    ExpectReject(R"("requires":[{"service":"S","version":1},{"service":"S","version":1}])"); // 重复（D64）
    ExpectReject(R"("requires":"not-an-array")");
    ExpectReject(R"("provides":"not-an-array")");
}

TEST_F(ManifestJson, RejectsConfigShapes)
{
    ExpectReject(R"("config":[{"key":"a","type":"int32"}])");               // 缺 default
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":"x"}])"); // 型不匹配
    ExpectReject(R"("config":[{"key":"a","type":"enum","default":1}])");    // enum 已合法但缺 choices（D80）
    ExpectReject(R"("config":[{"key":"a","type":"bool","default":true,"min":false}])");   // bool 禁 min/max
    ExpectReject(R"("config":[{"key":"a","type":"bool","default":true,"max":true}])");    // 只带 max 同闸
    ExpectReject(R"("config":[{"key":"","type":"int32","default":1}])");                  // 空 key
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":1,"min":5,"max":2}])"); // 倒挂（D64）
    ExpectReject(
        R"("config":[{"key":"a","type":"int32","default":1},{"key":"a","type":"int64","default":2}])"); // 重复 key
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":1,"nope":2}])");                      // 条目 unknown
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":9999999999}])");                      // int32 越界
}

TEST_F(ManifestJson, AccessorsMaterializePerKind)
{
    const auto parsed = Parse(R"("config":[{"key":"name","type":"string","default":"hello"},)"
                              R"({"key":"n","type":"int64","default":7,"min":1,"max":9}])");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const ManifestEntry& entry = parsed.Value();
    ASSERT_EQ(entry.Config.size(), 2U);
    const ManifestConfigField& nameField = *entry.Config.begin();
    const ManifestConfigField& nField = *std::next(entry.Config.begin(), 1);
    EXPECT_EQ(nameField.DefaultValue().Kind, ValueKind::kString);
    EXPECT_STREQ(nameField.DefaultValue().GetAs<const char*>(), "hello");
    EXPECT_EQ(nField.DefaultValue().GetAs<std::int64_t>(), 7);
    EXPECT_EQ(nField.MinValue().GetAs<std::int64_t>(), 1);
    EXPECT_EQ(nField.MaxValue().GetAs<std::int64_t>(), 9);
    EXPECT_EQ(nameField.MinValue().Kind, ValueKind::kNone); // !HasMin
    EXPECT_EQ(nameField.MaxValue().Kind, ValueKind::kNone);
}

// D80 的 enum 硬闸线。enum 自波 2 合法（D49 预留兑现）；复用上面 ManifestJson 的 Parse/沙箱形。
class ManifestEnum : public ManifestJson
{
public:
    static std::string MoodConfig(std::string_view entryFields)
    {
        return R"("config":[{"key":"mood","type":"enum",)" + std::string(entryFields) + "}]";
    }

    void ExpectRejectMsg(std::string_view fields, std::string_view token) const
    {
        const auto parsed = Parse(fields);
        EXPECT_FALSE(parsed.IsOk()) << "accepted: " << Wrapped(fields);
        if (!parsed.IsOk())
        {
            EXPECT_NE(parsed.GetError().Message().find(token), std::string::npos)
                << "token \"" << token << "\" absent: " << parsed.GetError().Message();
        }
    }
};

TEST_F(ManifestEnum, LegalEnumFieldParses)
{
    const auto parsed =
        Parse(MoodConfig(R"("default":"响亮","choices":[{"value":0,"label":"安静"},{"value":1,"label":"响亮"}])"));
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const ManifestEntry& entry = parsed.Value();
    ASSERT_EQ(entry.Config.size(), 1U);
    const ManifestConfigField& mood = *entry.Config.begin();
    EXPECT_EQ(mood.Kind, ValueKind::kEnum);
    ASSERT_EQ(mood.Choices.size(), 2U);
    EXPECT_EQ(mood.Choices.begin()->Value, 0);
    EXPECT_EQ(mood.Choices.begin()->Label, "安静");
    EXPECT_EQ(std::next(mood.Choices.begin(), 1)->Value, 1);
    EXPECT_EQ(std::next(mood.Choices.begin(), 1)->Label, "响亮");
    EXPECT_EQ(mood.DefaultValue().Kind, ValueKind::kString); // 中间形：default 存 label，换 value 归 Solve（T4）
    EXPECT_STREQ(mood.DefaultValue().GetAs<const char*>(), "响亮");
}

TEST_F(ManifestEnum, RequiresChoices)
{
    ExpectRejectMsg(MoodConfig(R"("default":"响亮")"), "\"choices\"");
    ExpectRejectMsg(MoodConfig(R"("default":"响亮","choices":[])"), "non-empty");
    ExpectRejectMsg(MoodConfig(R"("default":"响亮","choices":1)"), "array");
}

TEST_F(ManifestEnum, ChoiceShape)
{
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[42])"), "{value,label}");
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[{"value":0,"label":"a","x":1}])"), "got");
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[{"label":"a"}])"), "value");
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[{"value":0}])"), "label");
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[{"value":"0","label":"a"}])"), "range");
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[{"value":2147483648,"label":"a"}])"), "range");
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[{"value":-2147483649,"label":"a"}])"), "range");
}

TEST_F(ManifestEnum, ChoiceUniqueness)
{
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[{"value":0,"label":"a"},{"value":0,"label":"b"}])"),
                    "duplicate value");
    ExpectRejectMsg(MoodConfig(R"("default":"a","choices":[{"value":0,"label":"a"},{"value":1,"label":"a"}])"),
                    "duplicate label");
}

TEST_F(ManifestEnum, DefaultMustBeKnownLabel)
{
    ExpectRejectMsg(MoodConfig(R"("default":0,"choices":[{"value":0,"label":"安静"}])"), "label");
    ExpectRejectMsg(MoodConfig(R"("default":"疯狂","choices":[{"value":0,"label":"安静"}])"), "choices");
}

TEST_F(ManifestEnum, MinMaxForbidden)
{
    ExpectRejectMsg(MoodConfig(R"("default":"a","min":0,"choices":[{"value":0,"label":"a"}])"), "min/max");
    ExpectRejectMsg(MoodConfig(R"("default":"a","max":1,"choices":[{"value":0,"label":"a"}])"), "min/max");
}

TEST_F(ManifestEnum, ChoicesOnlyOnEnum)
{
    ExpectRejectMsg(R"("config":[{"key":"a","type":"int32","default":1,"choices":[{"value":0,"label":"a"}]}])",
                    "choices");
}

TEST_F(ManifestJson, ProcessStatesParsedInOrder)
{
    const auto file = WriteTempManifest(R"({
        "schemaVersion": 1, "id": "Vase.PS", "version": "0.0.1",
        "processStates": ["Vase.PS.Registry", "Vase.PS.Cache"]
    })");
    const auto parsed = vase::ParseManifestFile(file, "ps");
    ASSERT_TRUE(parsed.IsOk());
    ASSERT_EQ(parsed.Value().ProcessStates.size(), 2U);
    EXPECT_EQ(*parsed.Value().ProcessStates.begin(), "Vase.PS.Registry");
    EXPECT_EQ(*std::next(parsed.Value().ProcessStates.begin()), "Vase.PS.Cache");
}

TEST_F(ManifestJson, ProcessStatesMissingIsEmpty)
{
    const auto file = WriteTempManifest(R"({ "schemaVersion": 1, "id": "Vase.PS2", "version": "0.0.1" })");
    const auto parsed = vase::ParseManifestFile(file, "ps2");
    ASSERT_TRUE(parsed.IsOk()); // 缺键 = 空集，向后兼容（D93）
    EXPECT_TRUE(parsed.Value().ProcessStates.empty());
}

TEST_F(ManifestJson, ProcessStatesDuplicateRefused)
{
    const auto file = WriteTempManifest(R"({
        "schemaVersion": 1, "id": "Vase.PS3", "version": "0.0.1",
        "processStates": ["Vase.PS3.A", "Vase.PS3.A"]
    })");
    const auto parsed = vase::ParseManifestFile(file, "ps3");
    ASSERT_FALSE(parsed.IsOk());
    EXPECT_NE(parsed.GetError().Message().find("duplicate"), std::string::npos);
}

// M5/T5（D122/D134）：序列化面与解析面同源。写→读 round-trip 是键白名单同 TU 的可证伪证人。
TEST_F(ManifestJson, RoundTripsThroughWriteAndParse)
{
    vase::ManifestEntry entry;
    entry.Id = "Vase.RoundTrip";
    entry.DisplayName = "往返探针";
    entry.Version = "1.2.3";
    entry.Subdirectory = "RoundTrip";
    entry.Binary = "RoundTripProbe";
    entry.EnabledByDefault = false; // ← 保真字段：必须原样过一趟
    entry.Requires = {{.Service = "Vase.Other", .Version = 2}};
    entry.Provides = {{.Service = "Vase.RoundTrip.Service", .Version = 1}};
    entry.ProcessStates = {"Vase.RoundTrip.Loads"};

    const testing_support::CatalogSandbox sandbox("write-roundtrip");
    sandbox.CreateDir("RoundTrip"); // WriteManifestFile 不建目录树（scan 总落进既有子目录），测试自备
    const std::filesystem::path file = sandbox.Root / "RoundTrip" / "plugin.json";
    const auto written = vase::WriteManifestFile(file, entry);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();

    const auto parsed = vase::ParseManifestFile(file, "RoundTrip");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const vase::ManifestEntry& back = parsed.Value();
    EXPECT_EQ(back.Id, entry.Id);
    EXPECT_EQ(back.DisplayName, entry.DisplayName);
    EXPECT_EQ(back.Version, entry.Version);
    EXPECT_EQ(back.Binary, entry.Binary);
    EXPECT_EQ(back.EnabledByDefault, false);
    ASSERT_EQ(back.Requires.size(), 1U);
    EXPECT_EQ(back.Requires.begin()->Service, "Vase.Other");
    EXPECT_EQ(back.Requires.begin()->Version, 2U);
    ASSERT_EQ(back.Provides.size(), 1U);
    ASSERT_EQ(back.ProcessStates.size(), 1U);
    EXPECT_EQ(*back.ProcessStates.begin(), "Vase.RoundTrip.Loads");
}

// config 序列化支的专属证人：enum default-as-label、choices {value,label}、min/max 条件支
// 若不各过一次写→读，「同 TU 同源」的断言对这些支就是空的（T5 修复轮 1）。
TEST_F(ManifestJson, RoundTripsConfigAndEnumThroughWriteAndParse)
{
    vase::ManifestEntry entry;
    entry.Id = "Vase.RoundTrip.Cfg";
    entry.Binary = "CfgProbe";

    ManifestConfigField count;
    count.Key = "count";
    count.Kind = ValueKind::kInt32;
    count.DefaultBits = static_cast<std::uint64_t>(5U);
    count.MinBits = static_cast<std::uint64_t>(1U);
    count.MaxBits = static_cast<std::uint64_t>(10U);
    count.HasMin = true;
    count.HasMax = true;
    count.DisplayName = "数量";
    entry.Config.push_back(std::move(count));

    ManifestConfigField mood;
    mood.Key = "mood";
    mood.Kind = ValueKind::kEnum;
    mood.DefaultStr = "响亮"; // 中间形：写侧存 label、换算归 Solve（D80）
    mood.Choices = {{.Value = 0, .Label = "安静"}, {.Value = 1, .Label = "响亮"}};
    entry.Config.push_back(std::move(mood));

    const testing_support::CatalogSandbox sandbox("write-config-roundtrip");
    sandbox.CreateDir("Cfg"); // 目录树归调用方备（与另两条写用例同法）
    const std::filesystem::path file = sandbox.Root / "Cfg" / "plugin.json";
    const auto written = vase::WriteManifestFile(file, entry);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();

    const auto parsed = vase::ParseManifestFile(file, "Cfg");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const vase::ManifestEntry& back = parsed.Value();
    ASSERT_EQ(back.Config.size(), 2U);

    const ManifestConfigField& backCount = *back.Config.begin();
    EXPECT_EQ(backCount.Key, "count");
    EXPECT_EQ(backCount.Kind, ValueKind::kInt32);
    EXPECT_EQ(backCount.DisplayName, "数量");
    EXPECT_EQ(backCount.DefaultValue().GetAs<std::int32_t>(), 5);
    ASSERT_TRUE(backCount.HasMin);
    ASSERT_TRUE(backCount.HasMax);
    EXPECT_EQ(backCount.MinValue().GetAs<std::int32_t>(), 1);
    EXPECT_EQ(backCount.MaxValue().GetAs<std::int32_t>(), 10);

    const ManifestConfigField& backMood = *std::next(back.Config.begin(), 1);
    EXPECT_EQ(backMood.Key, "mood");
    EXPECT_EQ(backMood.Kind, ValueKind::kEnum);
    EXPECT_EQ(backMood.DefaultStr, "响亮");
    EXPECT_EQ(backMood.DefaultValue().Kind, ValueKind::kString); // 中间形过盘仍是 label 字串，不是数值
    EXPECT_STREQ(backMood.DefaultValue().GetAs<const char*>(), "响亮");
    ASSERT_EQ(backMood.Choices.size(), 2U);
    EXPECT_EQ(backMood.Choices.begin()->Value, 0);
    EXPECT_EQ(backMood.Choices.begin()->Label, "安静");
    EXPECT_EQ(std::next(backMood.Choices.begin(), 1)->Value, 1);
    EXPECT_EQ(std::next(backMood.Choices.begin(), 1)->Label, "响亮");
}

TEST_F(ManifestJson, WriteLeavesNoTempFileBehind)
{
    vase::ManifestEntry entry;
    entry.Id = "Vase.Temp";
    entry.DisplayName = "临时文件探针";
    entry.Binary = "TempProbe";

    const testing_support::CatalogSandbox sandbox("write-atomic");
    const std::filesystem::path dir = sandbox.Root / "Temp";
    sandbox.CreateDir("Temp"); // 同上：目录树归调用方备
    const auto written = vase::WriteManifestFile(dir / "plugin.json", entry);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();

    // 原子写（D134）：盘上只有 plugin.json，没有半个 JSON、也没有 .tmp 残留。
    EXPECT_TRUE(std::filesystem::exists(dir / "plugin.json"));
    EXPECT_FALSE(std::filesystem::exists(dir / "plugin.json.tmp"));
}

// 描述符 → 清单值的投影（M5/D122）：与 BuildExpectation（清单 → 期望）互为反方向的半条链。
// 三者串起来看：描述符 --投影--> 清单 --BuildExpectation--> 期望，故这条用例同时钉投影的正确性。
TEST_F(ManifestJson, ProjectsDescriptorMetaIntoManifestEntry)
{
    vase::PluginMeta meta;
    meta.Id = "Vase.Projected";
    meta.DisplayName = "投影探针";
    meta.Version = "2.0.0";
    meta.Requires = {};
    meta.Provides = {{.Name = "Vase.Projected.Service", .Version = 3}};

    const vase::ManifestEntry entry = tools::cli::ManifestEntryFromMeta(meta, "Projected", "ProjectedProbe");
    EXPECT_EQ(entry.Id, "Vase.Projected");
    EXPECT_EQ(entry.DisplayName, "投影探针");
    EXPECT_EQ(entry.Version, "2.0.0");
    EXPECT_EQ(entry.Subdirectory, "Projected");
    EXPECT_EQ(entry.Binary, "ProjectedProbe"); // 观察到的 stem，不是子目录名（D122）
    EXPECT_TRUE(entry.EnabledByDefault);       // 投影不碰清单独有字段——那是 Task 10 的合并职责
    ASSERT_EQ(entry.Provides.size(), 1U);
    EXPECT_EQ(entry.Provides.begin()->Service, "Vase.Projected.Service");
    EXPECT_EQ(entry.Provides.begin()->Version, 3U);

    // 借用已在投影处物化：meta 就地销毁，entry 仍自持全部字符串（寿命纪律在 §2.2）。
    meta = vase::PluginMeta{};
    EXPECT_EQ(entry.Provides.begin()->Service, "Vase.Projected.Service");
}

// 串链用例读真描述符：DescriptorTests.cpp 的宏入口与本文件同在一个镜像内，前置声明后于本图直调
// （同图引用不需导出宏；再展第二次不可——定名 extern "C" 入口必撞，约束理由见
// DescriptorTests.cpp『既有站点零改动』注）。

// NOLINTNEXTLINE(readability-identifier-naming) 名字是另一 TU 宏生成物的定名 extern "C" 符号，无代码级出路
extern "C" const vase::PluginDescriptor* VasePluginDesc_DescriptorProbePlugin();

TEST_F(ManifestJson, ProjectionAndExpectationAgreeOnARealDescriptor)
{
    const vase::PluginDescriptor* desc = VasePluginDesc_DescriptorProbePlugin();
    const vase::ManifestEntry entry = tools::cli::ManifestEntryFromMeta(*desc->Meta, "Probe", "Probe");
    const vase::ManifestExpectation expected = vase::BuildExpectation(entry);
    EXPECT_EQ(expected.Id, "Vase.DescriptorProbe");
    ASSERT_EQ(expected.Requires.size(), 2U);
    EXPECT_EQ(expected.Provides.begin()->Name, "Vase.Probe.Service");
    ASSERT_EQ(expected.Config.size(), 1U);
    EXPECT_EQ(expected.Config.begin()->Key, "Volume");
    EXPECT_EQ(expected.Config.begin()->Kind, vase::ValueKind::kFloat);
    // bugprone-unchecked-optional-access 不认 ASSERT 级 has_value（同 ConfigBlobTests 裁定），
    // 显式守卫后解引用；ASSERT_NE 兜住「投影丢了 default」这条判据本身。
    const auto& projectedDefault = expected.Config.begin()->Default;
    const auto* storage = projectedDefault.has_value() ? &*projectedDefault : nullptr;
    ASSERT_NE(storage, nullptr);
    // ProbeConfig 的 Volume 是 float（DescriptorTests.cpp 的 VASE_CONFIG 行），故 Storage 的
    // variant 实持 float——这一行同时钉「描述符的 Value → Storage 解码」正确；取错 alternative
    // 在无异常构建下是 abort，不是静默红。
    EXPECT_FLOAT_EQ(std::get<float>(*storage), 2.5F);
    ASSERT_EQ(expected.ProcessStates.size(), 1U);
    EXPECT_EQ(*expected.ProcessStates.begin(), "Vase.DescriptorProbe.State");
}

} // namespace
