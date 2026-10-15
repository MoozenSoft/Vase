// 包 scan（T7，spec §5.2 命令矩阵 / §5.3 保真与失败）：显式 --package 模式与目录模式的包更新。
// 材料是真实 SHARED 组合库 VasePlugins（三员 Vase.Hello / Vase.Dependent / Vase.Failing），一律走真
// 文件系统（CatalogSandbox）；argv 腿经 tools::cli::Run，核心腿经 ScanPackage / RunScan。
#include "Scan.h"

#include "Cli.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/PackagePlan.h"

#include "CatalogSandbox.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <iterator>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <system_error> // chmod 证人支：permissions 的 error_code 重载
#include <unistd.h>     // geteuid：root 跑测试时 DAC 不咬，如实跳过（VaseCliDoctorTests 同例）
#endif

namespace
{

using testing_support::CatalogSandbox;
using tools::cli::PackageScanOptions;
using vase::ManifestEntry;
using vase::PackageManifest;

// 组合库的 target 名（Samples/VasePlugins）：包目录里的库文件名按本平台规则由它拼。
constexpr std::string_view kPackedStem = "VasePlugins";
constexpr const char* kPackageManifestName = "pack.json";

// 读全文件字节：「坏清单 / 冲突保持原文件」的判据（与 VaseCliDoctorTests 的 FlipMachineField 同形）。
std::string ReadAll(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string LibraryRel(std::string_view stem) { return vase::catalog_detail::LibraryFileName(stem); }

const ManifestEntry* FindMember(const PackageManifest& manifest, std::string_view id)
{
    const auto found = std::ranges::find(manifest.Plugins, id, &ManifestEntry::Id);
    return found == manifest.Plugins.end() ? nullptr : &*found;
}

std::vector<std::string> MemberIds(const PackageManifest& manifest)
{
    std::vector<std::string> ids;
    ids.reserve(manifest.Plugins.size());
    for (const ManifestEntry& member : manifest.Plugins)
    {
        ids.push_back(member.Id); // 文件序 = 写出序（T2 按 Id 写出），故这份读数同时钉排序
    }
    return ids;
}

// 一份合法 pack.json（只写用例关心的成员与策略）：binary 与二进制文件名一致，不夹带来源变化。
std::string DynamicPack(const std::string& sourceId, const std::string& members)
{
    return R"({"schemaVersion":1,"sourceId":")" + sourceId + R"(","source":{"kind":"dynamic","binary":")" +
           std::string{kPackedStem} + R"("},"plugins":[)" + members + "]}";
}

std::string MemberJson(const std::string& id, bool enabled)
{
    return R"({"id":")" + id + R"(","enabledByDefault":)" + (enabled ? "true" : "false") + "}";
}

// argv 腿必须经 tools::cli::Run（main 只做 argv → Run 的直通）：argv[0] 是程序名，Run 从 1 起读，
// 故这里补一格并把首个实参当命令。非 const data() 免掉 const_cast。
int RunArgv(std::vector<std::string> args, std::ostream& out, std::ostream& err)
{
    args.insert(args.begin(), "VaseCli");
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (std::string& arg : args)
    {
        argv.push_back(arg.data());
    }
    return tools::cli::Run(static_cast<int>(argv.size()), argv.data(), out, err);
}

class PackageCliScanFixture : public ::testing::Test
{
protected:
    // 根名带空格：显式模式要经得住含空格的路径。
    PackageCliScanFixture()
        : Sandbox("package cli scan")
    {
    }

    // 先成功生成一份合法 pack.json（Step 1 契约）：每个用例随后只改它关心的那一维。
    void SetUp() override
    {
        Sandbox.CopyFile("P/" + LibraryRel(kPackedStem), VASE_FIXTURE_VASEPLUGINS);
        Library = Sandbox.Root / "P" / LibraryRel(kPackedStem);
        PackPath = Library.parent_path() / kPackageManifestName;
        Options.BinaryPath = Library;
        Options.SourceId = "P";
        ASSERT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitOk) << Err.str();
        Out.str(std::string{});
        Out.clear();
        Err.str(std::string{});
        Err.clear();
    }

public:
    CatalogSandbox Sandbox;
    PackageScanOptions Options;
    std::filesystem::path Library;
    std::filesystem::path PackPath;
    std::ostringstream Out;
    std::ostringstream Err;
};

// gate 的过滤子按 ^PackageCliScan\. 选本套件（dispatch Task 7 Step 2/4），而 TEST_F 的套件名取自
// 第一个实参——别名让「套件名」与「fixture 名」各按各的契约落地。
using PackageCliScan = PackageCliScanFixture;

TEST_F(PackageCliScan, GeneratesEveryMemberFromTheBinary)
{
    const auto written = vase::ParsePackageManifestFile(PackPath);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    const PackageManifest& manifest = written.Value();
    EXPECT_EQ(manifest.SourceId, "P"); // 显式指定的 sourceId 就是写出来的来源事实
    EXPECT_EQ(manifest.Kind, vase::PackageSourceKind::kDynamic);
    EXPECT_EQ(manifest.Binary, kPackedStem);
    EXPECT_EQ(MemberIds(manifest), (std::vector<std::string>{"Vase.Dependent", "Vase.Failing", "Vase.Hello"}));
    // 元信息来自描述符而不是手写：Provides 的差异（Hello 一个、Failing 零个）就是这一点的证人。
    const ManifestEntry* hello = FindMember(manifest, "Vase.Hello");
    const ManifestEntry* failing = FindMember(manifest, "Vase.Failing");
    ASSERT_NE(hello, nullptr);
    ASSERT_NE(failing, nullptr);
    EXPECT_EQ(hello->Provides.size(), 1U);
    EXPECT_TRUE(failing->Provides.empty());
    EXPECT_TRUE(hello->EnabledByDefault); // 首次生成：全部默认启用
}

TEST_F(PackageCliScan, IgnoresOtherLibrariesInTheSameDirectory)
{
    // 命令矩阵（spec §5.2）：显式模式指定那一个库，其他库不参与成员发现——目录不再要求「只有一个库」。
    ASSERT_NE(Sandbox.Root.string().find(' '), std::string::npos); // 本 fixture 的路径就带空格
    Sandbox.CopyFile("P/" + LibraryRel("OtherLib"), VASE_FIXTURE_LOADPROBE);

    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitOk) << Err.str();
    const auto written = vase::ParsePackageManifestFile(PackPath);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    EXPECT_EQ(written.Value().Binary, kPackedStem);
    EXPECT_EQ(MemberIds(written.Value()), (std::vector<std::string>{"Vase.Dependent", "Vase.Failing", "Vase.Hello"}));
    EXPECT_EQ(FindMember(written.Value(), "Vase.LoadProbe"), nullptr); // 旁边的库一个成员也不进来
}

TEST_F(PackageCliScan, RefusesDirectoryThatAlreadyHoldsPluginJson)
{
    // spec §5.3：显式模式遇到 plugin.json 即拒——同一目录不产生两种清单，原 pack.json 一字节不动。
    const std::string before = ReadAll(PackPath);
    Sandbox.WriteFile("P/plugin.json", R"({"schemaVersion":1,"id":"Vase.Solo"})");

    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitCheckFailed);
    EXPECT_NE(Err.str().find("plugin.json"), std::string::npos);
    EXPECT_EQ(ReadAll(PackPath), before);
}

TEST_F(PackageCliScan, RefusesStaticArchive)
{
    // 命令矩阵 scan 的静态行：普通 CLI 扫不了归档（本平台库名形态之外即拒，spec §5.2）。
    const std::string before = ReadAll(PackPath);
    Sandbox.WriteFile("P/libPackLib.a", "not a dynamic library");
    PackageScanOptions options = Options;
    options.BinaryPath = Sandbox.Root / "P" / "libPackLib.a";

    EXPECT_EQ(tools::cli::ScanPackage(options, Out, Err), tools::cli::kExitUsage);
    EXPECT_NE(Err.str().find("static archive"), std::string::npos);
    EXPECT_EQ(ReadAll(PackPath), before);
}

TEST_F(PackageCliScan, PreservesPoliciesAndEnablesAddedMembers)
{
    // §5.3：按 Id 保留已有 enabledByDefault，新成员默认启用。Vase.Failing 缺席旧清单 = 新成员。
    Sandbox.WriteFile("P/pack.json",
                      DynamicPack("P", MemberJson("Vase.Hello", false) + "," + MemberJson("Vase.Dependent", true)));

    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitOk) << Err.str();
    const auto written = vase::ParsePackageManifestFile(PackPath);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    const ManifestEntry* hello = FindMember(written.Value(), "Vase.Hello");
    const ManifestEntry* dependent = FindMember(written.Value(), "Vase.Dependent");
    const ManifestEntry* failing = FindMember(written.Value(), "Vase.Failing");
    ASSERT_NE(hello, nullptr);
    ASSERT_NE(dependent, nullptr);
    ASSERT_NE(failing, nullptr);
    EXPECT_FALSE(hello->EnabledByDefault); // 人手策略保真
    EXPECT_TRUE(dependent->EnabledByDefault);
    EXPECT_TRUE(failing->EnabledByDefault);                                     // 新成员走缺省
    EXPECT_NE(Out.str().find("added member: Vase.Failing"), std::string::npos); // 加入被明确报告
}

TEST_F(PackageCliScan, ReportsRemovedAddedMembersAndSourceChange)
{
    // §5.3：原有成员删除、新成员加入、来源标识/形态变化都明确报告；写出的来源事实取自显式参数。
    Sandbox.WriteFile("P/pack.json",
                      R"({"schemaVersion":1,"sourceId":"OldPack","source":{"kind":"inProcess"},"plugins":[)" +
                          MemberJson("Vase.Hello", true) + "," + MemberJson("Vase.Gone", true) + "]}");

    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitOk) << Err.str();
    const std::string text = Out.str();
    EXPECT_NE(text.find("removed member: Vase.Gone"), std::string::npos);
    EXPECT_NE(text.find("added member: Vase.Dependent"), std::string::npos);
    EXPECT_NE(text.find("OldPack"), std::string::npos);
    EXPECT_NE(text.find("inProcess"), std::string::npos);
    EXPECT_NE(text.find("dynamic"), std::string::npos);
    const auto written = vase::ParsePackageManifestFile(PackPath);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    EXPECT_EQ(written.Value().SourceId, "P");
    EXPECT_EQ(written.Value().Kind, vase::PackageSourceKind::kDynamic);
    EXPECT_EQ(MemberIds(written.Value()), (std::vector<std::string>{"Vase.Dependent", "Vase.Failing", "Vase.Hello"}));
}

// brief 给定用例（逐字保留）：坏 pack.json 默认 rc=检查失败、字节不变，且归因必须点出 --regenerate。
TEST_F(PackageCliScan, CorruptManifestNeedsExplicitRebuild)
{
    Sandbox.WriteFile("P/pack.json", "{broken");
    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitCheckFailed);
    EXPECT_NE(Err.str().find("--regenerate"), std::string::npos);
    std::ifstream original(Sandbox.Root / "P/pack.json");
    std::string line;
    std::getline(original, line);
    EXPECT_EQ(line, "{broken");
}

TEST_F(PackageCliScan, RegenerateRebuildsCorruptManifestAndReportsReset)
{
    // 坏文档里的策略（含一个不存在的成员）一律不读：全部成员默认启用，且重置被明确报告。
    Sandbox.WriteFile("P/pack.json",
                      R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":"VasePlugins"},)"
                      R"("plugins":[{"id":"Vase.Hello","enabledByDefault":false},)"
                      R"({"id":"Vase.Dependent","enabledByDefault":false},)"
                      R"({"id":"Vase.Ghost","enabledByDefault":false}],"bogus":true})");
    Options.Regenerate = true;

    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitOk) << Err.str();
    const std::string text = Out.str();
    EXPECT_NE(text.find("regenerated"), std::string::npos);
    EXPECT_NE(text.find("reset"), std::string::npos);
    const auto written = vase::ParsePackageManifestFile(PackPath);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    const ManifestEntry* hello = FindMember(written.Value(), "Vase.Hello");
    ASSERT_NE(hello, nullptr);
    EXPECT_TRUE(hello->EnabledByDefault); // 坏文件里的 false 没有被读进来
    EXPECT_EQ(FindMember(written.Value(), "Vase.Ghost"), nullptr);
    EXPECT_EQ(MemberIds(written.Value()), (std::vector<std::string>{"Vase.Dependent", "Vase.Failing", "Vase.Hello"}));
}

TEST_F(PackageCliScan, RegenerateKeepsReadablePolicies)
{
    // --regenerate 的语义锚在「损坏」那一支（spec §5.3）：可读清单照常按 Id 合并，策略不重置。
    Sandbox.WriteFile("P/pack.json", DynamicPack("P", MemberJson("Vase.Hello", false)));
    Options.Regenerate = true;

    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitOk) << Err.str();
    EXPECT_EQ(Out.str().find("regenerated"), std::string::npos); // 没有重置发生就不报 regenerated
    const auto written = vase::ParsePackageManifestFile(PackPath);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    EXPECT_EQ(MemberIds(written.Value()), (std::vector<std::string>{"Vase.Dependent", "Vase.Failing", "Vase.Hello"}));
    const ManifestEntry* hello = FindMember(written.Value(), "Vase.Hello");
    ASSERT_NE(hello, nullptr);
    EXPECT_FALSE(hello->EnabledByDefault);
}

TEST_F(PackageCliScan, RegenerateStillFailsOnStaleHeaderVersion)
{
    // §5.3 末段：--regenerate 不绕过描述符/版本检查；失败时原文档字节不变、不报成功。
    Sandbox.CopyFile("P/" + LibraryRel(kPackedStem), VASE_FIXTURE_STALEENUM);
    const std::string before = ReadAll(PackPath);
    Options.Regenerate = true;

    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitCheckFailed);
    EXPECT_NE(Err.str().find("HeaderVersion mismatch"), std::string::npos);
    EXPECT_EQ(ReadAll(PackPath), before);
    EXPECT_EQ(Out.str().find("wrote"), std::string::npos);
    EXPECT_EQ(Out.str().find("regenerated"), std::string::npos);
}

TEST_F(PackageCliScan, RegenerateStillFailsWithoutEnumerationEntry)
{
    // 缺枚举入口的二进制（老形态）照旧能装载、只是扫不了：显式重建也不许把它当成一份包文档写出去。
    Sandbox.CopyFile("P/" + LibraryRel(kPackedStem), VASE_FIXTURE_NOENUMERATION);
    const std::string before = ReadAll(PackPath);
    Options.Regenerate = true;

    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitCheckFailed);
    EXPECT_NE(Err.str().find("VasePlugin_Descriptors"), std::string::npos);
    EXPECT_EQ(ReadAll(PackPath), before);
}

TEST_F(PackageCliScan, UnwritableTargetKeepsTheOriginalFile)
{
    // §5.3：写盘失败保留原件，且不报 wrote/regenerated——报告只跟着成功的写盘走。
    const std::string before = ReadAll(PackPath);
#ifdef _WIN32
    // 打开的句柄不含 FILE_SHARE_DELETE：rename 覆盖必失败（const 只约束本对象，不改打开时的共享模式）。
    const std::ifstream held(PackPath, std::ios::binary);
    ASSERT_TRUE(held.is_open());
#else
    const std::filesystem::path directory = PackPath.parent_path();
    if (geteuid() == 0)
    {
        GTEST_SKIP() << "running as root: DAC permissions would not bind the temp file creation";
    }
    std::error_code ec;
    std::filesystem::permissions(directory,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::replace, ec);
    ASSERT_FALSE(ec) << "chmod setup failed: " << ec.message();
#endif
    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitCheckFailed);
    EXPECT_EQ(Out.str().find("wrote"), std::string::npos);
#ifdef _WIN32
    // 失败点必须是覆盖那一步（而不是读或合并）：这样「保留原件」才是写失败路径的见证。
    EXPECT_NE(Err.str().find("rename failed"), std::string::npos);
#else
    EXPECT_NE(Err.str().find("cannot open for writing"), std::string::npos);
    std::filesystem::permissions(directory, std::filesystem::perms::owner_write, std::filesystem::perm_options::add,
                                 ec);
    ASSERT_FALSE(ec) << "permission restore failed: " << ec.message(); // 不恢复会绊住沙箱析构与后续跑
#endif
    EXPECT_EQ(ReadAll(PackPath), before);
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path{PackPath.string() + ".tmp"})); // 不留半个临时文件
}

TEST_F(PackageCliScan, RunDispatchesPackageMode)
{
    // argv 腿（D137）：main 只做 argv → Run 的直通，这里经 tools::cli::Run 走完整分发。
    EXPECT_EQ(RunArgv({"scan", "--package", Library.string(), "--source-id", "FromArgv"}, Out, Err),
              tools::cli::kExitOk)
        << Err.str();
    const auto written = vase::ParsePackageManifestFile(PackPath);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    EXPECT_EQ(written.Value().SourceId, "FromArgv"); // 值真的走到了写出口，不是「随便拿了个 rc」
    EXPECT_NE(Out.str().find("sourceId=FromArgv"), std::string::npos);
}

TEST_F(PackageCliScan, RunRejectsMissingSourceIdBeforeRegenerate)
{
    const std::string before = ReadAll(PackPath);

    EXPECT_EQ(RunArgv({"scan", "--package", Library.string(), "--source-id", "--regenerate"}, Out, Err),
              tools::cli::kExitUsage);
    EXPECT_NE(Err.str().find("usage: VaseCli scan"), std::string::npos);
    EXPECT_EQ(Out.str().find("wrote"), std::string::npos);
    EXPECT_EQ(Out.str().find("regenerated"), std::string::npos);
    EXPECT_EQ(ReadAll(PackPath), before);
}

TEST_F(PackageCliScan, RunRejectsMissingSourceIdBeforeReadingCorruptManifest)
{
    Sandbox.WriteFile("P/pack.json", "{broken");
    const std::string before = ReadAll(PackPath);

    EXPECT_EQ(RunArgv({"scan", "--package", Library.string(), "--source-id", "--regenerate"}, Out, Err),
              tools::cli::kExitUsage);
    EXPECT_NE(Err.str().find("usage: VaseCli scan"), std::string::npos);
    EXPECT_EQ(Out.str().find("wrote"), std::string::npos);
    EXPECT_EQ(Out.str().find("regenerated"), std::string::npos);
    EXPECT_EQ(ReadAll(PackPath), before);
}

TEST_F(PackageCliScan, RunRejectsEmptyOrOptionTokensInRequiredValuePositions)
{
    const std::string before = ReadAll(PackPath);
    const std::vector<std::vector<std::string>> malformed{
        {"scan", "--package", "", "--source-id", "P"},
        {"scan", "--package", "--package", "--source-id", "P"},
        {"scan", "--package", "--source-id", "--source-id", "P"},
        {"scan", "--package", "--regenerate", "--source-id", "P"},
        {"scan", "--package", Library.string(), "--source-id", ""},
        {"scan", "--package", Library.string(), "--source-id", "", "--regenerate"},
        {"scan", "--package", Library.string(), "--source-id", "--package"},
        {"scan", "--package", Library.string(), "--source-id", "--source-id"},
    };
    for (const std::vector<std::string>& args : malformed)
    {
        SCOPED_TRACE(::testing::PrintToString(args));
        Sandbox.WriteFile("P/pack.json", before);
        std::ostringstream out;
        std::ostringstream err;
        EXPECT_EQ(RunArgv(args, out, err), tools::cli::kExitUsage);
        EXPECT_NE(err.str().find("usage: VaseCli scan"), std::string::npos);
        EXPECT_EQ(out.str().find("wrote"), std::string::npos);
        EXPECT_EQ(out.str().find("regenerated"), std::string::npos);
        EXPECT_EQ(ReadAll(PackPath), before);
    }
}

TEST_F(PackageCliScan, RunRejectsMalformedPackageArguments)
{
    // 重名 / 缺值 / 多余 flag 一律用法档（Step 1 契约），且任一态都不许留下写盘痕迹。
    const std::string before = ReadAll(PackPath);
    const std::vector<std::vector<std::string>> malformed{
        {"scan", "--package"},
        {"scan", "--package", Library.string(), "--source-id"},
        {"scan", "--package", Library.string(), "--source-id", "P", "--regenerate", "extra"},
        {"scan", "--package", Library.string(), "--source-id", "P", "--source-id", "Q"},
        {"scan", Library.string(), "--regenerate"},
    };
    for (const std::vector<std::string>& args : malformed)
    {
        std::ostringstream out;
        std::ostringstream err;
        EXPECT_EQ(RunArgv(args, out, err), tools::cli::kExitUsage) << args.size() << " args: " << err.str();
        EXPECT_NE(err.str().find("usage: VaseCli scan"), std::string::npos);
    }
    EXPECT_EQ(ReadAll(PackPath), before);
}

TEST_F(PackageCliScan, RunRejectsMalformedSourceIdBeforeTouchingTheLibrary)
{
    // 畸形 --source-id 是用法错（D135 第二档）：必须在装载/物化之前拒。用一份真能装载的库作证——
    // 闸若拖到 WritePackageManifestFile 处，同一输入会先装载、再以检查档（rc=1）失败。
    const std::string before = ReadAll(PackPath);

    EXPECT_EQ(RunArgv({"scan", "--package", Library.string(), "--source-id", "bad id"}, Out, Err),
              tools::cli::kExitUsage);
    EXPECT_NE(Err.str().find("--source-id"), std::string::npos);
    EXPECT_EQ(Out.str().find("wrote"), std::string::npos);
    EXPECT_EQ(ReadAll(PackPath), before);

    // 闸也在文件探测之前：库路径不存在时先报的仍是用法档，而不是「读不了库」。
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(
        RunArgv({"scan", "--package", (Sandbox.Root / "P" / LibraryRel("missing")).string(), "--source-id", "bad id"},
                out, err),
        tools::cli::kExitUsage);
    EXPECT_NE(err.str().find("--source-id"), std::string::npos);
}

TEST_F(PackageCliScan, DirectoryModeUpdatesAnExistingPackageManifest)
{
    // §5.3：目录模式按 pack.json 里的来源信息更新，策略按 Id 保真——不数库、不转写 plugin.json。
    Sandbox.WriteFile("P/pack.json",
                      DynamicPack("P", MemberJson("Vase.Hello", false) + "," + MemberJson("Vase.Dependent", true) +
                                           "," + MemberJson("Vase.Failing", true)));
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(Sandbox.Root.string(), out, err), tools::cli::kExitOk) << err.str();
    EXPECT_NE(out.str().find("wrote P/pack.json"), std::string::npos);
    const auto written = vase::ParsePackageManifestFile(PackPath);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    EXPECT_EQ(written.Value().SourceId, "P");
    const ManifestEntry* hello = FindMember(written.Value(), "Vase.Hello");
    ASSERT_NE(hello, nullptr);
    EXPECT_FALSE(hello->EnabledByDefault);
    EXPECT_FALSE(std::filesystem::exists(Sandbox.Root / "P" / "plugin.json"));
}

TEST_F(PackageCliScan, DirectoryModePointsCombinedLibraryAtTheExplicitMode)
{
    // 组合库在目录模式里不再报「不支持」：点名显式模式，且绝不把单插件目录改写成包目录（spec §5.3）。
    Sandbox.CopyFile("Q/" + LibraryRel(kPackedStem), VASE_FIXTURE_VASEPLUGINS);
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(Sandbox.Root.string(), out, err), tools::cli::kExitCheckFailed) << err.str();
    EXPECT_NE(err.str().find("--package"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(Sandbox.Root / "Q" / "plugin.json"));
}

TEST_F(PackageCliScan, DirectoryModeReportsStaticPackageWithoutEnumeration)
{
    // 命令矩阵（spec §5.2）：静态包的成员枚举要宿主的显式入口，普通 CLI 做不到——如实报，不当成功。
    Sandbox.WriteFile("S/pack.json", R"({"schemaVersion":1,"sourceId":"StaticPack","source":{"kind":"inProcess"},)"
                                     R"("plugins":[{"id":"Vase.Static.A","enabledByDefault":true}]})");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(Sandbox.Root.string(), out, err), tools::cli::kExitCheckFailed) << err.str();
    EXPECT_NE(err.str().find("inProcess"), std::string::npos);
}

TEST_F(PackageCliScan, DirectoryModeReportsUnreadableManifestWithTheRebuildHint)
{
    // §5.3：坏 pack.json 默认保持原文件并报错——不做单插件那种自动恢复，归因点名显式重建。
    Sandbox.WriteFile("U/pack.json", "{broken");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(Sandbox.Root.string(), out, err), tools::cli::kExitCheckFailed) << err.str();
    EXPECT_NE(err.str().find("--regenerate"), std::string::npos);
    EXPECT_EQ(ReadAll(Sandbox.Root / "U" / "pack.json"), "{broken"); // 字节不变
}

TEST_F(PackageCliScan, DirectoryModeRejectsADirectoryHoldingBothManifests)
{
    // 与 Catalog 的 D50 同口径：同一目录两类清单并存是坏树，工具不隐式设优先级、也不改任何一份。
    Sandbox.WriteFile("B/plugin.json", R"({"schemaVersion":1,"id":"Vase.Both"})");
    Sandbox.WriteFile("B/pack.json", DynamicPack("Both", MemberJson("Vase.Both", true)));
    const std::string before = ReadAll(Sandbox.Root / "B" / "pack.json");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(Sandbox.Root.string(), out, err), tools::cli::kExitCheckFailed) << err.str();
    EXPECT_NE(err.str().find("both present"), std::string::npos);
    EXPECT_EQ(ReadAll(Sandbox.Root / "B" / "pack.json"), before);
}

} // namespace
