// 包读取命令（T8，spec §5.2）：真实动态组合库、探针双入口与真实 STATIC 包生成头。
// 清单复用 T7 物化件，一律走真文件系统；核心腿直调 validate / plan / doctor，argv 不接地址。
#include "Doctor.h"
#include "Plan.h"
#include "Validate.h"

#include "Cli.h"
#include "Scan.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"
#include "VasePackageProvider.entries.h"

#include "CatalogSandbox.h"
#include "PackageDescriptorProbe.h"

#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;

// 组合库的 target 名（Samples/VasePlugins）：包目录里的库文件名按本平台规则由它拼。
constexpr std::string_view kPackedStem = "VasePlugins";
constexpr const char* kPackManifestName = "pack.json";

std::string LibraryRel(std::string_view stem) { return vase::catalog_detail::LibraryFileName(stem); }

std::size_t CountOf(std::string_view text, std::string_view needle)
{
    std::size_t count = 0;
    for (std::size_t at = text.find(needle); at != std::string_view::npos; at = text.find(needle, at + needle.size()))
    {
        ++count;
    }
    return count;
}

std::filesystem::path PackPathOf(const CatalogSandbox& sandbox, std::string_view directory)
{
    return sandbox.Root / std::filesystem::path(directory) / kPackManifestName;
}

// 真组合库 + 由它物化出的包清单：复用 T7 的 ScanPackage，不另写第二套描述符物化路径。
void InstallDynamicPackage(const CatalogSandbox& sandbox, std::string_view directory)
{
    sandbox.CopyFile(std::string{directory} + "/" + LibraryRel(kPackedStem), VASE_FIXTURE_VASEPLUGINS);
    tools::cli::PackageScanOptions options;
    options.BinaryPath = sandbox.Root / std::filesystem::path(directory) / LibraryRel(kPackedStem);
    options.SourceId = std::string{directory};
    std::ostringstream out;
    std::ostringstream err;
    ASSERT_EQ(tools::cli::ScanPackage(options, out, err), tools::cli::kExitOk) << err.str();
}

// 复用 T7 的描述符物化件，入口可来自探针或真实 STATIC 包生成头。
void InstallStaticPackage(const CatalogSandbox& sandbox, std::string_view directory,
                          vase::PackageEntryPoints entries = testing_support::ProbeEntries())
{
    const vase::Result<std::vector<const vase::PluginDescriptor*>> descriptors =
        vase::InspectDescriptors(entries.Enumerate);
    ASSERT_TRUE(descriptors.IsOk()) << descriptors.GetError().Message();
    vase::PackageManifest manifest;
    manifest.SourceId = std::string{directory};
    manifest.Kind = vase::PackageSourceKind::kInProcess;
    for (const vase::PluginDescriptor* descriptor : descriptors.Value())
    {
        manifest.Plugins.push_back(
            tools::cli::ManifestEntryFromMeta(*descriptor->Meta, std::string{directory}, std::string{}));
    }
    sandbox.CreateDir(directory); // WritePackageManifestFile 只写盘、不建目录（WriteFile 才会建）
    const vase::Result<void> written = vase::WritePackageManifestFile(PackPathOf(sandbox, directory), manifest);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
}

vase::PackageManifest ReadManifest(const std::filesystem::path& file)
{
    const vase::Result<vase::PackageManifest> parsed = vase::ParsePackageManifestFile(file);
    if (!parsed.IsOk())
    {
        ADD_FAILURE() << parsed.GetError().Message();
        return {};
    }
    return parsed.Value();
}

void WriteManifest(const std::filesystem::path& file, const vase::PackageManifest& manifest)
{
    const vase::Result<void> written = vase::WritePackageManifestFile(file, manifest);
    if (!written.IsOk())
    {
        ADD_FAILURE() << written.GetError().Message();
    }
}

} // namespace

// brief 给定用例（逐字保留）：静态来源的 plan 无绑定时仍分析成功，且如实写出 inProcess 来源。
TEST(PackageCliRead, StaticPlanNeedsNoRuntimeBinding)
{
    const testing_support::CatalogSandbox sandbox{"static-plan"};
    sandbox.WriteFile(
        "P/pack.json",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A"}]})");
    tools::cli::PlanOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::PlanDirectory(options, out, err), tools::cli::kExitOk);
    EXPECT_NE(out.str().find("inProcess"), std::string::npos);
}

// —— validate ——

TEST(PackageCliRead, DynamicPackageSetMismatchFailsEvenWhenEveryMemberIsDisabled)
{
    // 整包错集：三员组合库只列两员，且全禁用。整包检查不按参与过滤（§4.3 末句），漏列仍要报。
    const CatalogSandbox sandbox("read-pack-set");
    InstallDynamicPackage(sandbox, "P");
    const std::filesystem::path pack = PackPathOf(sandbox, "P");
    vase::PackageManifest manifest = ReadManifest(pack);
    ASSERT_GT(manifest.Plugins.size(), std::size_t{1});
    std::erase_if(manifest.Plugins, [](const vase::ManifestEntry& member) { return member.Id == "Vase.Failing"; });
    for (vase::ManifestEntry& member : manifest.Plugins)
    {
        member.EnabledByDefault = false;
    }
    WriteManifest(pack, manifest);

    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitCheckFailed) << out.str() << err.str();
    EXPECT_NE(out.str().find("missing in manifest"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("Vase.Failing"), std::string::npos) << out.str(); // 成员级诊断仍点名 Id
    // 多员来源的检查组只出现一次：不是「每员一行」。
    EXPECT_EQ(CountOf(out.str(), "\n  P: "), 1U) << out.str();
}

TEST(PackageCliRead, MemberListedButAbsentFromTheBinaryIsNamed)
{
    // 双向等值的另一半：清单列了一个库里没有的成员。
    const CatalogSandbox sandbox("read-pack-ghost");
    InstallDynamicPackage(sandbox, "P");
    const std::filesystem::path pack = PackPathOf(sandbox, "P");
    vase::PackageManifest manifest = ReadManifest(pack);
    vase::ManifestEntry ghost;
    ghost.Id = "Vase.Ghost";
    ghost.DisplayName = "Ghost";
    manifest.Plugins.push_back(ghost);
    WriteManifest(pack, manifest);

    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitCheckFailed) << out.str() << err.str();
    EXPECT_NE(out.str().find("missing in binary"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("Vase.Ghost"), std::string::npos) << out.str();
}

TEST(PackageCliRead, DriftInADisabledMemberIsStillCaught)
{
    const CatalogSandbox sandbox("read-pack-drift");
    InstallDynamicPackage(sandbox, "P");
    const std::filesystem::path pack = PackPathOf(sandbox, "P");
    vase::PackageManifest manifest = ReadManifest(pack);
    for (vase::ManifestEntry& member : manifest.Plugins)
    {
        if (member.Id == "Vase.Hello")
        {
            member.DisplayName = "改过的名字";
            member.EnabledByDefault = false; // 禁用成员照比：不按 Preset 过滤
        }
    }
    WriteManifest(pack, manifest);

    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitCheckFailed) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Hello"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("displayName"), std::string::npos) << out.str();
}

TEST(PackageCliRead, StaticSourceWithoutEntriesCannotVerifyAndFails)
{
    // 普通 CLI 拿不到进程内包的枚举入口：描述符校验无法执行，明说且计入未通过。
    const CatalogSandbox sandbox("read-static-validate");
    InstallStaticPackage(sandbox, "P");

    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitCheckFailed) << out.str() << err.str();
    EXPECT_NE(out.str().find("cannot verify"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("  P: "), std::string::npos) << out.str();
    EXPECT_EQ(out.str().find("P: ok"), std::string::npos) << out.str(); // 不假报通过
}

TEST(PackageCliRead, StaticSourceWithoutEntriesStillReportsDependencyConclusions)
{
    // 依赖检查走 Analyze（零装载、不看绑定）：缺静态绑定不许掩盖依赖结论。
    const CatalogSandbox sandbox("read-static-deps");
    InstallStaticPackage(sandbox, "P");
    const std::filesystem::path pack = PackPathOf(sandbox, "P");
    vase::PackageManifest manifest = ReadManifest(pack);
    ASSERT_FALSE(manifest.Plugins.empty());
    manifest.Plugins.front().Requires.push_back(vase::ManifestDependency{.Service = "Vase.Absent.Svc", .Version = 1});
    WriteManifest(pack, manifest);

    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitCheckFailed) << out.str() << err.str();
    EXPECT_NE(out.str().find("check 3 (dependency graph):"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("FAIL (missing dependency)"), std::string::npos) << out.str();
}

TEST(PackageCliRead, StaticSourceWithHostEntriesPassesFullValidation)
{
    // 已链接调用者显式给出双入口 = 完整校验：整包比对真的跑过（正对照，见 PlanExecutesNoEntry）。
    const CatalogSandbox sandbox("read-static-bound");
    InstallStaticPackage(sandbox, "P");
    testing_support::ResetPackageProbe();
    const std::string sourceId = "P";
    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    options.InProcessSources.push_back(
        vase::InProcessBinding{.SourceId = sourceId, .Entries = testing_support::ProbeEntries()});

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("  P: ok (2 members)"), std::string::npos) << out.str();
    EXPECT_GT(testing_support::ReadPackageProbeCounts().Enumerate, std::size_t{0});
}

TEST(PackageCliRead, ValidationReportsOneGroupForAMultiMemberSource)
{
    const CatalogSandbox sandbox("read-validate-dedup");
    InstallDynamicPackage(sandbox, "P");
    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_EQ(CountOf(out.str(), "  P: ok (3 members)"), 1U) << out.str();
}

// —— plan ——

TEST(PackageCliRead, DynamicSourceWithoutItsFileStillAnalyzes)
{
    // 动态库不在盘上：plan 零装载，分析照常成功，并如实标出来源是动态文件。
    const CatalogSandbox sandbox("read-plan-missing");
    sandbox.WriteFile("P/pack.json",
                      R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":"VasePlugins"},)"
                      R"("plugins":[{"id":"Vase.Ghost","enabledByDefault":true}]})");
    tools::cli::PlanOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::PlanDirectory(options, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Ghost load"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("source P, dynamic"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find(LibraryRel(kPackedStem)), std::string::npos) << out.str();
}

TEST(PackageCliRead, PlanExecutesNoEntryOnAStaticSource)
{
    const CatalogSandbox sandbox("read-plan-static");
    InstallStaticPackage(sandbox, "P");
    testing_support::ResetPackageProbe(); // InstallStaticPackage 自己枚举过：计数从零起算
    tools::cli::PlanOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::PlanDirectory(options, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("source P, inProcess"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("host binding"), std::string::npos) << out.str();
    EXPECT_EQ(out.str().find("plan: ok"), std::string::npos) << out.str(); // 分析成功 != 拿到可执行计划
    const testing_support::PackageProbeCounts counts = testing_support::ReadPackageProbeCounts();
    EXPECT_EQ(counts.Lookup, std::size_t{0});
    EXPECT_EQ(counts.Enumerate, std::size_t{0});
}

TEST(PackageCliRead, AllDisabledStaticPlanReportsSourceWithoutHostBinding)
{
    const CatalogSandbox sandbox("read-plan-static-disabled");
    InstallStaticPackage(sandbox, "P");
    const std::filesystem::path pack = PackPathOf(sandbox, "P");
    vase::PackageManifest manifest = ReadManifest(pack);
    for (vase::ManifestEntry& member : manifest.Plugins)
    {
        member.EnabledByDefault = false;
    }
    WriteManifest(pack, manifest);
    testing_support::ResetPackageProbe();

    tools::cli::PlanOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::PlanDirectory(options, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_EQ(CountOf(out.str(), "skip disabled (source P, inProcess)"), manifest.Plugins.size()) << out.str();
    EXPECT_EQ(out.str().find("host binding"), std::string::npos) << out.str();
    EXPECT_EQ(testing_support::ReadPackageProbeCounts().Lookup, std::size_t{0});
    EXPECT_EQ(testing_support::ReadPackageProbeCounts().Enumerate, std::size_t{0});
}

TEST(PackageCliRead, DisabledDynamicPlanReportsItsSourceAndFile)
{
    const CatalogSandbox sandbox("read-plan-dynamic-disabled");
    sandbox.WriteFile("P/pack.json",
                      R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":"VasePlugins"},)"
                      R"("plugins":[{"id":"Vase.Ghost","enabledByDefault":false}]})");
    tools::cli::PlanOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::PlanDirectory(options, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Ghost skip disabled (source P, dynamic, binary"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find(LibraryRel(kPackedStem)), std::string::npos) << out.str();
}

TEST(PackageCliRead, PlanKeepsHardSkipLevelAndNoteCauseForPackages)
{
    // 硬跳过的失败级别与 Notes.Cause 归因在包来源上逐字保留（D82 两段制）。
    const CatalogSandbox sandbox("read-plan-hardskip");
    InstallDynamicPackage(sandbox, "P");
    sandbox.WriteFile("preset.json", R"({
        "schemaVersion": 1, "overrides": { "Vase.Hello": { "enabled": false } }
    })");
    tools::cli::PlanOptions options;
    options.PluginDirectory = sandbox.Root;
    options.PresetFile = sandbox.Root / "preset.json";
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::PlanDirectory(options, out, err), tools::cli::kExitCheckFailed) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Hello skip disabled"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("Vase.Dependent skip missing-dependency"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("note: providerSkipped Vase.Dependent Vase.Hello"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("plan: FAIL"), std::string::npos) << out.str();
}

// —— doctor ——

TEST(PackageCliRead, DoctorSeparatesNotApplicableFromCannotVerifyOnAStaticSource)
{
    const CatalogSandbox sandbox("read-static-doctor");
    InstallStaticPackage(sandbox, "P");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed)
        << out.str() << err.str();
    EXPECT_NE(out.str().find("not applicable"), std::string::npos) << out.str();        // 磁盘身份：不适用
    EXPECT_NE(out.str().find("cannot verify"), std::string::npos) << out.str();         // 装载/版号：无法验证
    EXPECT_EQ(out.str().find("HeaderVersion matches"), std::string::npos) << out.str(); // 不给假 ok
    EXPECT_NE(out.str().find("check 3 (directory writability):"), std::string::npos) << out.str();
    // 只有 ② 计入未通过——不适用项不拖退出码，无法验证必须拖。
    EXPECT_NE(out.str().find("doctor: FAIL (checks 2 reported failures)"), std::string::npos) << out.str();
}

TEST(PackageCliRead, DoctorWithHostEntriesCompletesTheVersionCheck)
{
    const CatalogSandbox sandbox("read-doctor-bound");
    InstallStaticPackage(sandbox, "P");
    testing_support::ResetPackageProbe();
    const std::string sourceId = "P";
    const std::vector<vase::InProcessBinding> bindings{
        vase::InProcessBinding{.SourceId = sourceId, .Entries = testing_support::ProbeEntries()},
    };
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, bindings, out, err), tools::cli::kExitOk)
        << out.str() << err.str();
    EXPECT_NE(out.str().find("P: HeaderVersion matches (2 descriptor(s))"), std::string::npos) << out.str();
    EXPECT_GT(testing_support::ReadPackageProbeCounts().Enumerate, std::size_t{0});
}

TEST(PackageCliRead, DoctorReportsEachPhysicalSourceOnce)
{
    // 三员一库：身份与版号各只有一组（按物理来源去重），成员 Id 不当成各自一行。
    const CatalogSandbox sandbox("read-doctor-dedup");
    InstallDynamicPackage(sandbox, "P");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_EQ(CountOf(out.str(), "identity ok"), 1U) << out.str();
    EXPECT_EQ(CountOf(out.str(), "HeaderVersion matches"), 1U) << out.str();
    EXPECT_EQ(out.str().find("Vase.Hello:"), std::string::npos) << out.str();
}

TEST(PackageCliRead, GeneratedStaticEntriesCompleteValidationAndDoctor)
{
    const CatalogSandbox sandbox("read-generated-static");
    InstallStaticPackage(sandbox, "P", vase::pack::vasepackageprovider::kEntryPoints);
    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    options.InProcessSources.push_back(
        vase::InProcessBinding{.SourceId = "P", .Entries = vase::pack::vasepackageprovider::kEntryPoints});

    std::ostringstream validateOut;
    std::ostringstream validateErr;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, validateOut, validateErr), tools::cli::kExitOk)
        << validateOut.str() << validateErr.str();
    EXPECT_NE(validateOut.str().find("P: ok (1 members)"), std::string::npos) << validateOut.str();

    std::ostringstream doctorOut;
    std::ostringstream doctorErr;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, options.InProcessSources, doctorOut, doctorErr),
              tools::cli::kExitOk)
        << doctorOut.str() << doctorErr.str();
    EXPECT_NE(doctorOut.str().find("P: HeaderVersion matches (1 descriptor(s))"), std::string::npos) << doctorOut.str();
}
