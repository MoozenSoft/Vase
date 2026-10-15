#include "Vase/Host/PackageInspect.h"

#include "PackageDescriptorProbe.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace
{

void ExpectProbeRejection(std::string_view failure, std::string_view reason, std::string_view id = {})
{
    testing_support::ResetPackageProbe();
    testing_support::SetProbeFailure(failure);
    const auto result = vase::InspectPackageMembers(testing_support::ProbeEntries());
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find(reason), std::string::npos);
    if (!id.empty())
    {
        EXPECT_NE(result.GetError().Message().find(id), std::string::npos);
    }
}

TEST(PackageInspect, ValidMembersAreSortedAndComparedWithoutCreatingInstances)
{
    testing_support::ResetPackageProbe();
    const auto members = vase::InspectPackageMembers(testing_support::ProbeEntries());
    ASSERT_TRUE(members.IsOk()) << members.GetError().Message();
    ASSERT_EQ(members.Value().size(), 2U);
    EXPECT_EQ(members.Value().front()->Meta->Id, "Probe.A");
    EXPECT_EQ(members.Value().back()->Meta->Id, "Probe.B");
    const auto compared = vase::ComparePackage(testing_support::ProbeExpectations(), members.Value());
    ASSERT_TRUE(compared.IsOk()) << compared.GetError().Message();
    const auto counts = testing_support::ReadPackageProbeCounts();
    EXPECT_EQ(counts.Enumerate, 1U);
    EXPECT_EQ(counts.Lookup, 2U);
    EXPECT_EQ(counts.Create, 0U);
    EXPECT_EQ(counts.CreateConfig, 0U);
    EXPECT_EQ(counts.OnLoad, 0U);
    EXPECT_EQ(counts.Reset, 0U);
}

TEST(PackageInspect, RejectsLookupEnumerationMismatch)
{
    ExpectProbeRejection("lookup-mismatch", "lookup/enumeration mismatch", "Probe.A");
}
TEST(PackageInspect, RejectsDuplicateId) { ExpectProbeRejection("duplicate-id", "duplicate", "Probe.A"); }
TEST(PackageInspect, RejectsNullMeta) { ExpectProbeRejection("null-meta", "Meta"); }
TEST(PackageInspect, RejectsStaleHeaderBeforeMeta) { ExpectProbeRejection("stale-header", "HeaderVersion"); }
TEST(PackageInspect, RejectsEmptyEnumeration) { ExpectProbeRejection("empty-enumeration", "no descriptor"); }
TEST(PackageInspect, RejectsNullDescriptor) { ExpectProbeRejection("null-descriptor", "null descriptor"); }
TEST(PackageInspect, RejectsEmptyId) { ExpectProbeRejection("empty-id", "Id"); }

TEST(PackageInspect, RejectsExtraBinaryMember)
{
    testing_support::ResetPackageProbe();
    testing_support::SetProbeFailure("extra-member");
    const auto members = vase::InspectPackageMembers(testing_support::ProbeEntries());
    ASSERT_TRUE(members.IsOk()) << members.GetError().Message();
    const auto compared = vase::ComparePackage(testing_support::ProbeExpectations(), members.Value());
    ASSERT_FALSE(compared.IsOk());
    EXPECT_NE(compared.GetError().Message().find("Probe.Extra"), std::string::npos);
    EXPECT_NE(compared.GetError().Message().find("missing in manifest"), std::string::npos);
}

TEST(PackageInspect, RejectsExpectedSubsetOfCompleteEnumeration)
{
    testing_support::ResetPackageProbe();
    const auto members = vase::InspectPackageMembers(testing_support::ProbeEntries());
    ASSERT_TRUE(members.IsOk());
    auto expected = testing_support::ProbeExpectations();
    expected.pop_back();
    const auto compared = vase::ComparePackage(expected, members.Value());
    ASSERT_FALSE(compared.IsOk());
    EXPECT_NE(compared.GetError().Message().find("Probe.B"), std::string::npos);
    EXPECT_NE(compared.GetError().Message().find("missing in manifest"), std::string::npos);
}

TEST(PackageInspect, RejectsMissingBinaryMember)
{
    testing_support::ResetPackageProbe();
    const auto members = vase::InspectPackageMembers(testing_support::ProbeEntries());
    ASSERT_TRUE(members.IsOk());
    auto actual = members.Value();
    actual.pop_back();
    const auto compared = vase::ComparePackage(testing_support::ProbeExpectations(), actual);
    ASSERT_FALSE(compared.IsOk());
    EXPECT_NE(compared.GetError().Message().find("Probe.B"), std::string::npos);
    EXPECT_NE(compared.GetError().Message().find("missing in binary"), std::string::npos);
}

TEST(PackageInspect, ComparesFieldsThroughDescriptorRules)
{
    testing_support::ResetPackageProbe();
    const auto members = vase::InspectPackageMembers(testing_support::ProbeEntries());
    ASSERT_TRUE(members.IsOk());
    auto expected = testing_support::ProbeExpectations();
    expected.front().Version = "2.0.0";
    const auto compared = vase::ComparePackage(expected, members.Value());
    ASSERT_FALSE(compared.IsOk());
    EXPECT_NE(compared.GetError().Message().find("Probe.A"), std::string::npos);
    EXPECT_NE(compared.GetError().Message().find("version"), std::string::npos);
}

TEST(PackageInspect, ChecksCompleteIdSetBeforeComparingFields)
{
    testing_support::ResetPackageProbe();
    const auto members = vase::InspectPackageMembers(testing_support::ProbeEntries());
    ASSERT_TRUE(members.IsOk());
    auto expected = testing_support::ProbeExpectations();
    expected.pop_back();
    expected.front().Version = "2.0.0";
    const auto compared = vase::ComparePackage(expected, members.Value());
    ASSERT_FALSE(compared.IsOk());
    EXPECT_NE(compared.GetError().Message().find("Probe.B"), std::string::npos);
    EXPECT_NE(compared.GetError().Message().find("missing in manifest"), std::string::npos);
}

TEST(PackageInspect, RejectsDuplicateExpectedId)
{
    testing_support::ResetPackageProbe();
    const auto members = vase::InspectPackageMembers(testing_support::ProbeEntries());
    ASSERT_TRUE(members.IsOk());
    auto expected = testing_support::ProbeExpectations();
    expected.back().Id = "Probe.A";
    const auto compared = vase::ComparePackage(expected, members.Value());
    ASSERT_FALSE(compared.IsOk());
    EXPECT_NE(compared.GetError().Message().find("Probe.A"), std::string::npos);
    EXPECT_NE(compared.GetError().Message().find("duplicate package expectation"), std::string::npos);
}

TEST(PackageInspect, NullEnumerationEntryIsRejected)
{
    const auto result = vase::InspectDescriptors(static_cast<vase::PluginEnumerationEntryPoint>(nullptr));
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find("VasePlugin_Descriptors"), std::string::npos);
}

TEST(PackageInspect, MissingLookupEntryIsRejectedBeforeEnumeration)
{
    testing_support::ResetPackageProbe();
    auto entries = testing_support::ProbeEntries();
    entries.Lookup = nullptr;
    const auto result = vase::InspectPackageMembers(entries);
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find("VasePlugin_GetPlugin"), std::string::npos);
    EXPECT_EQ(testing_support::ReadPackageProbeCounts().Enumerate, 0U);
}

const vase::PluginDescriptor* const* NullTable(std::uint32_t* count)
{
    *count = 1;
    return nullptr;
}

TEST(PackageInspect, NullEnumerationTableIsRejected)
{
    const auto result = vase::InspectDescriptors(&NullTable);
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find("no descriptor"), std::string::npos);
}

TEST(PackageInspect, DirectComparisonRejectsNullDescriptor)
{
    const std::array<const vase::PluginDescriptor*, 1> actual{nullptr};
    const auto result = vase::ComparePackage({}, actual);
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find("null descriptor"), std::string::npos);
}

TEST(PackageInspect, DirectComparisonRejectsStaleHeaderBeforeMeta)
{
    const vase::PluginDescriptor stale{.HeaderVersion = vase::kHeaderVersion - 1U};
    const std::array actual{&stale};
    const auto result = vase::ComparePackage({}, actual);
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find("HeaderVersion"), std::string::npos);
}

TEST(PackageInspect, DirectComparisonRejectsNullMeta)
{
    const vase::PluginDescriptor nullMeta{.HeaderVersion = vase::kHeaderVersion};
    const std::array actual{&nullMeta};
    const auto result = vase::ComparePackage({}, actual);
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find("Meta"), std::string::npos);
}

TEST(PackageInspect, DirectComparisonRejectsEmptyId)
{
    const vase::PluginMeta emptyMeta{};
    const vase::PluginDescriptor emptyId{.HeaderVersion = vase::kHeaderVersion, .Meta = &emptyMeta};
    const std::array actual{&emptyId};
    const auto result = vase::ComparePackage({}, actual);
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find("Id"), std::string::npos);
}

TEST(PackageInspect, DirectComparisonRejectsDuplicateBinaryId)
{
    testing_support::ResetPackageProbe();
    const auto* descriptor = testing_support::ProbeEntries().Lookup("Probe.A");
    const std::array actual{descriptor, descriptor};
    const auto result = vase::ComparePackage(testing_support::ProbeExpectations(), actual);
    ASSERT_FALSE(result.IsOk());
    EXPECT_NE(result.GetError().Message().find("Probe.A"), std::string::npos);
    EXPECT_NE(result.GetError().Message().find("duplicate package member"), std::string::npos);
}

TEST(PackageInspect, BinaryRecordUsesTheSharedMemberChecks)
{
    vase::detail::Loader loader;
    const auto record = loader.EnsureResident(std::filesystem::path{VASE_FIXTURE_LOADPROBE});
    ASSERT_TRUE(record.IsOk()) << record.GetError().Message();
    const auto members = vase::InspectPackageMembers(*record.Value());
    ASSERT_TRUE(members.IsOk()) << members.GetError().Message();
    ASSERT_EQ(members.Value().size(), 1U);
    EXPECT_EQ(members.Value().front()->Meta->Id, "Vase.LoadProbe");
    loader.Unload(*record.Value());
}

TEST(PackageInspect, BinaryRecordRejectsMissingEnumeration)
{
    vase::detail::Loader loader;
    const auto record = loader.EnsureResident(std::filesystem::path{VASE_FIXTURE_NOENUMERATION});
    ASSERT_TRUE(record.IsOk()) << record.GetError().Message();
    const auto members = vase::InspectPackageMembers(*record.Value());
    ASSERT_FALSE(members.IsOk());
    EXPECT_NE(members.GetError().Message().find("VasePlugin_Descriptors"), std::string::npos);
    loader.Unload(*record.Value());
}

} // namespace
