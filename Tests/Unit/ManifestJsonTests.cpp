// spec §4 逐执法线（D48/D49/D64）。断言 = IsOk + 字段值；错误线只验「响且不绿」，
// 消息不逐字钉（最小区分集纪律，spec §7）。

#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"

#include "CatalogSandbox.h"

#include <bit>
#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <string_view>

namespace
{

using testing_support::CatalogSandbox;
using vase::ManifestConfigField;
using vase::ManifestEntry;
using vase::ParseManifestFile;
using vase::Result;
using vase::ValueKind;

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

} // namespace
