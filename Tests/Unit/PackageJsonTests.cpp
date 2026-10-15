#include "Vase/Catalog/PackageManifest.h"

#include "CatalogSandbox.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Config/Value.h"
#include "Vase/Host/PackagePlan.h"

#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::string ReadBytes(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

constexpr std::string_view kFullPackage =
    R"({"schemaVersion":1,"sourceId":"Bundle-1._","source":{"kind":"dynamic","binary":"Bundle"},"plugins":[
{"id":"Probe.Z","enabledByDefault":false},
{"id":"Probe.A","displayName":"探针","version":"1.2","requires":[{"service":"Probe.World","version":1}],"optionalRequires":[{"service":"Probe.Other","version":2}],"provides":[{"service":"Probe.A.Service","version":3}],"processStates":["Z","A"],"config":[
{"key":"Text","type":"string","default":"短"},
{"key":"Mood","type":"enum","default":"低","choices":[{"value":9,"label":"高"},{"value":-2,"label":"低"}]},
{"key":"Bool","type":"bool","default":true},
{"key":"I32","type":"int32","default":-123,"min":-200,"max":300},
{"key":"I64","type":"int64","default":-9223372036854775808},
{"key":"Float","type":"float","default":1.25,"min":0,"max":2},
{"key":"Double","type":"double","default":-2.5}
]}]})";

constexpr std::array<std::string_view, 32> kDeviceStems{
    "NUL",  "con",  "PRN",  "aux",  "COM1", "com2", "COM3",      "COM4",           "COM5",       "COM6",       "COM7",
    "COM8", "COM9", "LPT1", "lpt2", "LPT3", "LPT4", "LPT5",      "LPT6",           "LPT7",       "LPT8",       "LPT9",
    "COM¹", "COM²", "COM³", "LPT¹", "LPT²", "LPT³", "NUL.extra", "CoN.more.parts", "cOm9.extra", "LpT².extra",
};

std::string PackageWithBinary(std::string_view binary)
{
    return std::string(R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":")") +
           std::string(binary) + R"("},"plugins":[{"id":"Probe.A"}]})";
}

TEST(PackageJson, DynamicMembersKeepSinglePluginDefaults)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    sandbox.WriteFile(
        "P/pack.json",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":"ProbeBin"},"plugins":[{"id":"Probe.A"}]})");
    const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    EXPECT_EQ(parsed.Value().SourceId, "P");
    EXPECT_EQ(parsed.Value().Kind, vase::PackageSourceKind::kDynamic);
    EXPECT_EQ(parsed.Value().Binary, "ProbeBin");
    ASSERT_EQ(parsed.Value().Plugins.size(), 1U);
    const auto& member = parsed.Value().Plugins.front();
    EXPECT_EQ(member.Id, "Probe.A");
    EXPECT_EQ(member.DisplayName, "Probe.A");
    EXPECT_TRUE(member.Version.empty());
    EXPECT_TRUE(member.EnabledByDefault);
    EXPECT_TRUE(member.Binary.empty());
    EXPECT_TRUE(member.Requires.empty());
    EXPECT_TRUE(member.OptionalRequires.empty());
    EXPECT_TRUE(member.Provides.empty());
    EXPECT_TRUE(member.Config.empty());
    EXPECT_TRUE(member.ProcessStates.empty());
}

TEST(PackageJson, InProcessPackageNeedsNoBinary)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    sandbox.WriteFile(
        "P/pack.json",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A"}]})");
    const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    EXPECT_EQ(parsed.Value().Kind, vase::PackageSourceKind::kInProcess);
    EXPECT_TRUE(parsed.Value().Binary.empty());
    ASSERT_TRUE(vase::WritePackageManifestFile(sandbox.Root / "P/pack.json", parsed.Value()).IsOk());
    const auto reread = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(reread.IsOk());
    EXPECT_EQ(reread.Value().Kind, vase::PackageSourceKind::kInProcess);
    EXPECT_EQ(ReadBytes(sandbox.Root / "P/pack.json").find("binary"), std::string::npos);
}

TEST(PackageJson, RejectsInvalidRootAndSourceWithFileAttribution)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    const std::vector<std::string_view> inputs{
        "[]",
        "{",
        R"({"schemaVersion":1.0,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":2,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"A"}]})",
        R"({"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"","source":{"kind":"inProcess"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"@single/P","source":{"kind":"inProcess"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"..","source":{"kind":"inProcess"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"包","source":{"kind":"inProcess"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[]})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":{}})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"A"}],"extra":1})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess","extra":1},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess","binary":"Archive"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"other"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"P","source":[],"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":"a/b"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":"a\\b"},"plugins":[{"id":"A"}]})",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":""},"plugins":[{"id":"A"}]})",
    };
    for (const auto input : inputs)
    {
        SCOPED_TRACE(input);
        sandbox.WriteFile("P/pack.json", input);
        const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
        ASSERT_FALSE(parsed.IsOk());
        EXPECT_NE(parsed.GetError().Message().find("pack.json"), std::string::npos);
    }
}

TEST(PackageJson, RejectsInvalidMembersWithMemberAttribution)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    const std::vector<std::string_view> members{
        "0",
        R"({"id":""})",
        R"({"id":"Probe.A","extra":1})",
        R"({"id":"Probe.A","schemaVersion":1})",
        R"({"id":"Probe.A","binary":"Other"})",
        R"({"id":"Probe.A","sourceId":"Other"})",
        R"({"id":"Probe.A","source":{"kind":"inProcess"}})",
        R"({"id":"Probe.A","enabledByDefault":1})",
        R"({"id":"Probe.A","config":[{"key":"Mood","type":"enum","default":"unknown","choices":[{"value":1,"label":"valid"}]}]})",
        R"({"id":"Probe.A","requires":[{"service":"S","version":0}]})",
        R"({"id":"Probe.A","processStates":["state","state"]})",
    };
    for (const auto member : members)
    {
        SCOPED_TRACE(member);
        sandbox.WriteFile(
            "P/pack.json",
            std::string(R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[)") +
                std::string(member) + "]}");
        const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
        ASSERT_FALSE(parsed.IsOk());
        EXPECT_NE(parsed.GetError().Message().find("pack.json"), std::string::npos);
        EXPECT_NE(parsed.GetError().Message().find("plugins[0]"), std::string::npos);
        if (member.find("Probe.A") != std::string_view::npos)
        {
            EXPECT_NE(parsed.GetError().Message().find("Probe.A"), std::string::npos);
        }
    }
}

TEST(PackageJson, RejectsDuplicateMemberId)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    sandbox.WriteFile(
        "P/pack.json",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A"},{"id":"Probe.A"}]})");
    const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_FALSE(parsed.IsOk());
    EXPECT_NE(parsed.GetError().Message().find("plugins[1]"), std::string::npos);
    EXPECT_NE(parsed.GetError().Message().find("Probe.A"), std::string::npos);
}

TEST(PackageJson, RoundTripSortsMembersAndPreservesFieldValuesAndChoiceOrder)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    sandbox.WriteFile("P/pack.json", kFullPackage);
    auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(parsed.IsOk());
    for (auto& member : parsed.Value().Plugins)
    {
        member.Subdirectory = "internal-directory";
        member.Binary = "internal-binary";
    }
    ASSERT_TRUE(vase::WritePackageManifestFile(sandbox.Root / "P/pack.json", parsed.Value()).IsOk());
    const auto bytes = ReadBytes(sandbox.Root / "P/pack.json");
    EXPECT_EQ(bytes.find("internal-directory"), std::string::npos);
    EXPECT_EQ(bytes.find("internal-binary"), std::string::npos);
    const auto reread = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(reread.IsOk());
    ASSERT_EQ(reread.Value().Plugins.size(), 2U);
    const auto& member = reread.Value().Plugins.front();
    EXPECT_EQ(member.Id, "Probe.A");
    EXPECT_EQ(member.DisplayName, "探针");
    EXPECT_EQ(member.Version, "1.2");
    EXPECT_FALSE(reread.Value().Plugins.back().EnabledByDefault);
    ASSERT_EQ(member.Requires.size(), 1U);
    EXPECT_EQ(member.Requires.front().Service, "Probe.World");
    EXPECT_EQ(member.Requires.front().Version, 1U);
    ASSERT_EQ(member.OptionalRequires.size(), 1U);
    EXPECT_EQ(member.OptionalRequires.front().Version, 2U);
    ASSERT_EQ(member.Provides.size(), 1U);
    EXPECT_EQ(member.Provides.front().Version, 3U);
    EXPECT_EQ(member.ProcessStates, (std::vector<std::string>{"Z", "A"}));
    ASSERT_EQ(member.Config.size(), 7U);
    auto field = member.Config.begin();
    EXPECT_EQ(field->DefaultStr, "短");
    ++field;
    EXPECT_EQ(field->DefaultStr, "低");
    ASSERT_EQ(field->Choices.size(), 2U);
    EXPECT_EQ(field->Choices.front().Value, 9);
    EXPECT_EQ(field->Choices.front().Label, "高");
    EXPECT_EQ(field->Choices.back().Value, -2);
    ++field;
    EXPECT_TRUE(field->DefaultValue().GetAs<bool>());
    ++field;
    EXPECT_EQ(field->DefaultValue().GetAs<std::int32_t>(), -123);
    EXPECT_EQ(field->MinValue().GetAs<std::int32_t>(), -200);
    EXPECT_EQ(field->MaxValue().GetAs<std::int32_t>(), 300);
    ++field;
    EXPECT_EQ(field->DefaultValue().GetAs<std::int64_t>(), std::numeric_limits<std::int64_t>::min());
    ++field;
    EXPECT_EQ(field->DefaultValue().GetAs<float>(), 1.25F);
    EXPECT_EQ(field->MaxValue().GetAs<float>(), 2.0F);
    ++field;
    EXPECT_EQ(field->DefaultValue().GetAs<double>(), -2.5);
    ASSERT_TRUE(vase::WritePackageManifestFile(sandbox.Root / "P/pack.json", reread.Value()).IsOk());
    EXPECT_EQ(ReadBytes(sandbox.Root / "P/pack.json"), bytes);
}

TEST(PackageJson, InvalidWritePreservesOriginalBytes)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    sandbox.WriteFile("P/pack.json", kFullPackage);
    const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(parsed.IsOk());
    for (int mutation = 0; mutation < 13; ++mutation)
    {
        SCOPED_TRACE(mutation);
        auto package = parsed.Value();
        auto& member = package.Plugins.back();
        auto& field = member.Config.front();
        switch (mutation)
        {
        case 0:
            package.SourceId.clear();
            break;
        case 1:
            package.Kind = static_cast<vase::PackageSourceKind>(255);
            break;
        case 2:
            package.Binary = "../bad";
            break;
        case 3:
            package.Kind = vase::PackageSourceKind::kInProcess;
            break;
        case 4:
            package.Plugins.clear();
            break;
        case 5:
            package.Plugins.push_back(member);
            break;
        case 6:
            member.Id.clear();
            break;
        case 7:
            field.Kind = vase::ValueKind::kNone;
            break;
        case 8:
            field.Choices.push_back({.Value = 1, .Label = "bad"});
            break;
        case 9:
            field.HasMin = true;
            break;
        case 10:
            field = *std::next(member.Config.begin());
            field.DefaultStr = "unknown";
            break;
        case 11:
            field.Kind = vase::ValueKind::kDouble;
            field.DefaultBits = std::bit_cast<std::uint64_t>(std::numeric_limits<double>::infinity());
            break;
        case 12:
            member.DisplayName = std::string(1, static_cast<char>(0xFF));
            break;
        default:
            break;
        }
        const auto written = vase::WritePackageManifestFile(sandbox.Root / "P/pack.json", package);
        ASSERT_FALSE(written.IsOk());
        EXPECT_EQ(ReadBytes(sandbox.Root / "P/pack.json"), kFullPackage);
        EXPECT_FALSE(std::filesystem::exists(sandbox.Root / "P/pack.json.tmp"));
    }
}

TEST(PackageJson, SharedSinglePluginWriterRejectsInvalidFieldsBeforeReplace)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    constexpr std::string_view kOriginal = R"({"schemaVersion":1,"id":"Probe.A","binary":"Probe"})";
    sandbox.WriteFile("P/plugin.json", kOriginal);
    auto parsed = vase::ParseManifestFile(sandbox.Root / "P/plugin.json", "P");
    ASSERT_TRUE(parsed.IsOk());
    parsed.Value().Provides.push_back({.Service = "Probe.Service", .Version = 0});
    EXPECT_FALSE(vase::WriteManifestFile(sandbox.Root / "P/plugin.json", parsed.Value()).IsOk());
    EXPECT_EQ(ReadBytes(sandbox.Root / "P/plugin.json"), kOriginal);
}

TEST(PackageJson, StringDefaultsPreserveEmbeddedNullInBothFormats)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    sandbox.WriteFile(
        "P/pack.json",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"A","config":[{"key":"Text","type":"string","default":"a\u0000b"}]}]})");
    const auto package = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(package.IsOk());
    ASSERT_TRUE(vase::WritePackageManifestFile(sandbox.Root / "P/pack.json", package.Value()).IsOk());
    const auto reread = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(reread.IsOk());
    EXPECT_EQ(reread.Value().Plugins.front().Config.front().DefaultStr, std::string("a\0b", 3));
    auto single = package.Value().Plugins.front();
    single.Binary = "Probe";
    ASSERT_TRUE(vase::WriteManifestFile(sandbox.Root / "P/plugin.json", single).IsOk());
    const auto singleRead = vase::ParseManifestFile(sandbox.Root / "P/plugin.json", "P");
    ASSERT_TRUE(singleRead.IsOk());
    EXPECT_EQ(singleRead.Value().Config.front().DefaultStr, std::string("a\0b", 3));
}

TEST(PackageJson, DeviceBinaryStemRulesFollowPlatform)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
#ifdef _WIN32
    constexpr bool kAccepted = false;
#else
    constexpr bool kAccepted = true;
#endif
    for (const auto stem : kDeviceStems)
    {
        SCOPED_TRACE(stem);
        sandbox.WriteFile("P/pack.json", PackageWithBinary(stem));
        const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
        EXPECT_EQ(parsed.IsOk(), kAccepted);
        if (!parsed.IsOk())
        {
            EXPECT_NE(parsed.GetError().Message().find("pack.json"), std::string::npos);
        }
    }
}

TEST(PackageJson, DeviceBinaryWritesRespectPlatformAndPreserveRejectedOriginal)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    sandbox.WriteFile("P/pack.json", kFullPackage);
    const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
    ASSERT_TRUE(parsed.IsOk());
#ifdef _WIN32
    constexpr bool kAccepted = false;
#else
    constexpr bool kAccepted = true;
#endif
    for (const auto stem : kDeviceStems)
    {
        SCOPED_TRACE(stem);
        sandbox.WriteFile("P/pack.json", kFullPackage);
        auto package = parsed.Value();
        package.Binary = stem;
        const auto written = vase::WritePackageManifestFile(sandbox.Root / "P/pack.json", package);
        EXPECT_EQ(written.IsOk(), kAccepted);
        if constexpr (!kAccepted)
        {
            EXPECT_EQ(ReadBytes(sandbox.Root / "P/pack.json"), kFullPackage);
        }
        EXPECT_FALSE(std::filesystem::exists(sandbox.Root / "P/pack.json.tmp"));
    }
}

TEST(PackageJson, DeviceChecksAcceptOrdinaryStemsAndPreserveSinglePluginRules)
{
    const testing_support::CatalogSandbox sandbox{"package-json"};
    const std::array<std::string_view, 11> ordinary{
        "NULPlugin",
        "CONSOLE",
        "COM0",
        "COM10",
        "LPT0",
        "LPT10",
        "Addon.NUL",
        "COM¹Plugin",
        "LPT⁴",
        "Probe",
        "COM1PluginWithALongerOrdinaryName",
    };
    for (const auto stem : ordinary)
    {
        SCOPED_TRACE(stem);
        sandbox.WriteFile("P/pack.json", PackageWithBinary(stem));
        const auto parsed = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
        ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
        ASSERT_TRUE(vase::WritePackageManifestFile(sandbox.Root / "P/pack.json", parsed.Value()).IsOk());
        const auto reread = vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json");
        ASSERT_TRUE(reread.IsOk());
        EXPECT_EQ(reread.Value().Binary, stem);
    }
    for (const auto stem : kDeviceStems)
    {
        SCOPED_TRACE(stem);
        sandbox.WriteFile("P/plugin.json",
                          std::string(R"({"schemaVersion":1,"id":"Probe.A","binary":")") + std::string(stem) + R"("})");
        const auto parsed = vase::ParseManifestFile(sandbox.Root / "P/plugin.json", "P");
        ASSERT_TRUE(parsed.IsOk());
        EXPECT_TRUE(vase::WriteManifestFile(sandbox.Root / "P/plugin.json", parsed.Value()).IsOk());
    }
}

} // namespace
