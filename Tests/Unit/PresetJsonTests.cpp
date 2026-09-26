// spec §5 的结构/语法线（D59 第一段）。类型核对与 warn 族归 SolveTests（T5）。

#include "Vase/Catalog/Preset.h"

#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"

#include "CatalogSandbox.h"

#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace
{

using testing_support::CatalogSandbox;
using vase::LoadPreset;
using vase::Preset;
using vase::Result;
using vase::ValueKind;

// bugprone-unchecked-optional-access 不认 ASSERT/EXPECT 级 has_value 断言（同 ConfigBlobTests 的裁定）。
template <typename T>
T Unwrap(std::optional<T> found)
{
    if (!found.has_value())
    {
        ADD_FAILURE() << "Find miss";
        return T{};
    }
    return *found;
}

class PresetJson : public ::testing::Test
{
public:
    CatalogSandbox Sandbox{"preset-json"};

    [[nodiscard]] Result<Preset> Load(std::string_view text) const
    {
        Sandbox.WriteFile("Client.preset.json", text);
        return LoadPreset(Sandbox.Root / "Client.preset.json");
    }
};

TEST_F(PresetJson, AcceptsFullShape)
{
    const auto loaded = Load(R"({"schemaVersion":1,"displayName":"客户端","overrides":{)"
                             R"("Vase.Combat":{"config":{"friendlyFire":true,"repeats":3,"speed":1.5,"name":"x"}},)"
                             R"("Vase.Admin":{"enabled":false}}})");
    ASSERT_TRUE(loaded.IsOk()) << loaded.GetError().Message();
    const Preset& preset = loaded.Value();
    EXPECT_EQ(preset.SchemaVersion(), 1U);
    EXPECT_EQ(preset.DisplayName(), "客户端");
    ASSERT_EQ(preset.Entries().size(), 2U); // Id 字典序：Admin < Combat（JSON 解析器对象 = std::map，键字典序非文件序）
    const auto admin = preset.Entries().begin();
    const auto combat = std::next(preset.Entries().begin(), 1);
    EXPECT_EQ(admin->Id, "Vase.Admin");
    EXPECT_FALSE(Unwrap(admin->Enabled));
    EXPECT_EQ(combat->Id, "Vase.Combat");
    EXPECT_FALSE(combat->Enabled.has_value());
    const auto speed = Unwrap(combat->Config.Find("speed")); // D59：原样 int64/double/bool/string
    EXPECT_EQ(speed.Kind, ValueKind::kDouble);
    const auto repeats = Unwrap(combat->Config.Find("repeats"));
    EXPECT_EQ(repeats.Kind, ValueKind::kInt64);
    const auto name = Unwrap(combat->Config.Find("name"));
    EXPECT_STREQ(name.GetAs<const char*>(), "x");
}

TEST_F(PresetJson, RejectsMissingSchemaVersionOrOverrides)
{
    EXPECT_FALSE(Load(R"({"displayName":"x","overrides":{}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1})").IsOk());
}

TEST_F(PresetJson, RejectsMajorTwoAndFloatForm)
{
    EXPECT_FALSE(Load(R"({"schemaVersion":2,"overrides":{}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1.0,"overrides":{}})").IsOk());
}

TEST_F(PresetJson, RejectsNestingInConfig)
{
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"config":{"a":[1]}}}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"config":{"a":{"b":1}}}}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"config":{"a":null}}}})").IsOk());
}

TEST_F(PresetJson, RejectsConfigIntegerAboveInt64Max)
{
    // 合法 JSON，专打数值窄化分支：unsigned 越 int64::max 没有合法 int64 形，拒。
    const auto loaded = Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"config":{"a":18446744073709551615}}}})");
    ASSERT_FALSE(loaded.IsOk());
    EXPECT_NE(loaded.GetError().Message().find("integer too large"), std::string::npos);
}

TEST_F(PresetJson, LoadPresetMissingFileRejects)
{
    // T4-b：文件不存在 → Err（"cannot open file" 通道）——输入缺失也要响亮，不静默给空 Preset。
    EXPECT_FALSE(LoadPreset(Sandbox.Root / "no-such-client.preset.json").IsOk());
}

TEST_F(PresetJson, RejectsUnknownFieldAndBadShapes)
{
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"volume":2}}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"enabled":"yes"}}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":"nope"}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":[]})").IsOk());
}

// D83：根级只收 schemaVersion/displayName/overrides 三键，unknown 即结构错且点名 offending 键。
TEST(PresetRoot, UnknownRootKeyRejected)
{
    CatalogSandbox sandbox{"preset-root"};
    const auto load = [&sandbox](std::string_view text)
    {
        sandbox.WriteFile("Client.preset.json", text);
        return LoadPreset(sandbox.Root / "Client.preset.json");
    };
    const auto extra = load(R"({"schemaVersion":1,"overrides":{},"author":"someone"})");
    ASSERT_FALSE(extra.IsOk());
    EXPECT_NE(extra.GetError().Message().find("unknown top-level field"), std::string::npos);
    EXPECT_NE(extra.GetError().Message().find("\"author\""), std::string::npos);
    // 必填键的错拼也是根级 unknown，而非「缺失」——闸在缺失核对之前。
    const auto typo = load(R"({"schemaVersionn":1,"overrides":{}})");
    ASSERT_FALSE(typo.IsOk());
    EXPECT_NE(typo.GetError().Message().find("unknown top-level field"), std::string::npos);
    EXPECT_NE(typo.GetError().Message().find("\"schemaVersionn\""), std::string::npos);
}

} // namespace
