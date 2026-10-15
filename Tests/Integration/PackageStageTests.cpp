#include "Detail/PackageStage.h"
#include "PackageBuild.h"

#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Result.h"

#include "CatalogSandbox.h"
#include "PackageBuildProbe.h"
#include "PackageDescriptorProbe.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <iterator>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <utility>

// Windows 伞形头的 include-cleaner 归因同 LoaderWindows.cpp。
// NOLINTBEGIN(misc-include-cleaner)
#include <windows.h>
// NOLINTEND(misc-include-cleaner)
#endif

namespace
{

// 读 staging 目录里的 sentinel：目录事务的判据是**字节**还在不在，不是回调次数。
std::string ReadSentinel(const std::filesystem::path& directory)
{
    std::ifstream input(directory / "sentinel");
    std::string line;
    std::getline(input, line);
    return line;
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string UppercaseName(std::string name)
{
    for (char& letter : name)
    {
        if (letter >= 'a' && letter <= 'z')
        {
            letter = static_cast<char>(letter - 'a' + 'A');
        }
    }
    return name;
}

tools::cli::PackageBuildOptions DynamicOptions(const testing_support::CatalogSandbox& sandbox)
{
    tools::cli::PackageBuildOptions options;
    options.SourceId = "VasePlugins";
    options.BinaryPath = VASE_FIXTURE_VASEPLUGINS;
    options.Members = {
        {.DescriptorSymbol = "VasePluginDesc_HelloPlugin", .SourceManifest = VASE_FIXTURE_HELLO_MANIFEST},
        {.DescriptorSymbol = "VasePluginDesc_DependentPlugin", .SourceManifest = VASE_FIXTURE_DEPENDENT_MANIFEST},
        {.DescriptorSymbol = "VasePluginDesc_FailingPlugin", .SourceManifest = VASE_FIXTURE_FAILING_MANIFEST},
    };
    options.OutputDirectory = sandbox.Root / "output";
    return options;
}

#ifdef _WIN32
// NOLINTBEGIN(misc-include-cleaner) Windows API 的伞形头归因同 LoaderWindows.cpp。
vase::Result<void> CreateJunction(const std::filesystem::path& link, const std::filesystem::path& target)
{
    std::wstring command = L"cmd.exe /d /c mklink /J \"" + link.wstring() + L"\" \"" + target.wstring() + L"\" >nul";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (::CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                         &process) == 0)
    {
        return vase::Result<void>::Err(
            vase::Error{"cannot create junction process: " + std::to_string(::GetLastError())});
    }
    const DWORD waited = ::WaitForSingleObject(process.hProcess, 30000);
    if (waited != WAIT_OBJECT_0)
    {
        ::TerminateProcess(process.hProcess, 1);
        ::WaitForSingleObject(process.hProcess, INFINITE);
    }
    DWORD exitCode = 1;
    const bool exitRead = ::GetExitCodeProcess(process.hProcess, &exitCode) != 0;
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    if (waited != WAIT_OBJECT_0 || !exitRead || exitCode != 0)
    {
        return vase::Result<void>::Err(vase::Error{"junction creation failed: " + std::to_string(exitCode)});
    }
    return vase::Result<void>::Ok();
}

// junction 必须先非递归摘除，CatalogSandbox 析构才可清理真实目标。
class JunctionGuard
{
public:
    explicit JunctionGuard(std::filesystem::path link)
        : Link(std::move(link))
    {
    }
    JunctionGuard(const JunctionGuard&) = delete;
    JunctionGuard& operator=(const JunctionGuard&) = delete;
    JunctionGuard(JunctionGuard&&) = delete;
    JunctionGuard& operator=(JunctionGuard&&) = delete;
    ~JunctionGuard() { ::RemoveDirectoryW(Link.c_str()); }

private:
    std::filesystem::path Link;
};
// NOLINTEND(misc-include-cleaner)
#endif

} // namespace

TEST(PackageStage, PreparationFailureKeepsOriginalDirectory)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage"};
    sandbox.WriteFile("output/sentinel", "original");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = sandbox.Root / "output";
    options.RuntimeFiles.push_back(sandbox.Root / "missing-dependency.dll");
    EXPECT_FALSE(tools::cli::BuildAndStagePackage(options).IsOk());
    EXPECT_EQ(ReadSentinel(options.OutputDirectory), "original");
}

TEST(PackageStage, SuccessReplacesStaleContentWithManifestAndLibrary)
{
    const testing_support::CatalogSandbox sandbox{"package-stage-success"};
    const std::string libraryName = vase::catalog_detail::LibraryFileName("VasePlugins");
    sandbox.WriteFile("output/sentinel", "original");
    sandbox.WriteFile("output/" + vase::catalog_detail::LibraryFileName("Stale"), "stale");
    tools::cli::PackageBuildOptions options;
    options.SourceId = "VasePlugins";
    options.BinaryPath = VASE_FIXTURE_VASEPLUGINS;
    options.Members = {
        {.DescriptorSymbol = "VasePluginDesc_HelloPlugin", .SourceManifest = VASE_FIXTURE_HELLO_MANIFEST},
        {.DescriptorSymbol = "VasePluginDesc_DependentPlugin", .SourceManifest = VASE_FIXTURE_DEPENDENT_MANIFEST},
        {.DescriptorSymbol = "VasePluginDesc_FailingPlugin", .SourceManifest = VASE_FIXTURE_FAILING_MANIFEST},
    };
    options.OutputDirectory = sandbox.Root / "output";
    const auto staged = tools::cli::BuildAndStagePackage(options);
    ASSERT_TRUE(staged.IsOk()) << staged.GetError().Message();
    EXPECT_FALSE(std::filesystem::exists(options.OutputDirectory / "sentinel"));
    EXPECT_FALSE(std::filesystem::exists(options.OutputDirectory / vase::catalog_detail::LibraryFileName("Stale")));
    EXPECT_TRUE(std::filesystem::exists(options.OutputDirectory / libraryName));
    const auto manifest = vase::ParsePackageManifestFile(options.OutputDirectory / "pack.json");
    ASSERT_TRUE(manifest.IsOk()) << manifest.GetError().Message();
    EXPECT_EQ(manifest.Value().SourceId, "VasePlugins");
    ASSERT_EQ(manifest.Value().Plugins.size(), 3U);
}

TEST(PackageStage, InstallRenameFailureRestoresOriginalDirectory)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage-rename"};
    sandbox.WriteFile("output/sentinel", "original");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = sandbox.Root / "output";
    int renames = 0;
    tools::cli::package_detail::SetRenameDirectoryForTest(
        [&renames](const std::filesystem::path& from, const std::filesystem::path& to) -> vase::Result<void>
        {
            ++renames;
            if (renames == 2) // 第二次 = 准备目录 → 输出目录那一跳；注入安装期失败
            {
                return vase::Result<void>::Err(vase::Error{"injected install rename failure"});
            }
            std::error_code ec;
            std::filesystem::rename(from, to, ec);
            if (ec)
            {
                return vase::Result<void>::Err(vase::Error{"rename failed: " + ec.message()});
            }
            return vase::Result<void>::Ok();
        });
    const auto staged = tools::cli::BuildAndStagePackage(options);
    tools::cli::package_detail::ResetRenameDirectoryForTest();
    EXPECT_FALSE(staged.IsOk());
    EXPECT_EQ(ReadSentinel(options.OutputDirectory), "original"); // 备份必须真的搬回来，不是只报了错
    EXPECT_GE(renames, 3);                                        // 第三跳 = 备份 → 输出目录的恢复
}

TEST(PackageStage, RejectsProtectedDirectoryAndInputAncestorOutputs)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage-guard"};
    sandbox.WriteFile("out/dependency.dat", "payload");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = sandbox.Root;
    options.ProtectedDirectories.push_back(sandbox.Root);
    EXPECT_FALSE(tools::cli::BuildAndStagePackage(options).IsOk());
    options.ProtectedDirectories.clear();
    options.OutputDirectory = sandbox.Root / "out";
    options.RuntimeFiles.push_back(sandbox.Root / "out/dependency.dat");
    EXPECT_FALSE(tools::cli::BuildAndStagePackage(options).IsOk());
}

TEST(PackageStage, RejectsSymlinkOutput)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage-link"};
    sandbox.CreateDir("real");
    std::error_code ec;
    std::filesystem::create_directory_symlink(sandbox.Root / "real", sandbox.Root / "link", ec);
    if (ec)
    {
        GTEST_SKIP() << "directory symlink unavailable: " << ec.message();
    }
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = sandbox.Root / "link";
    EXPECT_FALSE(tools::cli::BuildAndStagePackage(options).IsOk());
}

TEST(PackageStage, RejectsRuntimeNameCollidingWithPackageContent)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage-collide"};
    sandbox.WriteFile("output/sentinel", "original");
    sandbox.WriteFile("output/pack.json", "original manifest bytes");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.SourceId = "RuntimeReplacement";
    const auto replacement = tools::cli::GeneratePackageManifest(options);
    ASSERT_TRUE(replacement.IsOk());
    sandbox.CreateDir("shared");
    ASSERT_TRUE(vase::WritePackageManifestFile(sandbox.Root / "shared/pack.json", replacement.Value()).IsOk());
    options.SourceId = "Probe";
    options.OutputDirectory = sandbox.Root / "output";
    options.RuntimeFiles.push_back(sandbox.Root / "shared/pack.json");
    std::error_code ec;
    ASSERT_TRUE(std::filesystem::exists(options.RuntimeFiles.front(), ec));
    ASSERT_FALSE(ec) << ec.message();
    const auto staged = tools::cli::BuildAndStagePackage(options);
    EXPECT_FALSE(staged.IsOk());
    if (!staged.IsOk())
    {
        EXPECT_NE(staged.GetError().Message().find("name collides with package content"), std::string::npos);
    }
    EXPECT_EQ(ReadSentinel(options.OutputDirectory), "original");
    EXPECT_EQ(ReadFile(options.OutputDirectory / "pack.json"), "original manifest bytes");
}

TEST(PackageStage, RejectsJunctionOutput)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage-junction"};
    sandbox.WriteFile("real/sentinel", "original target bytes");
    sandbox.WriteFile("real/pack.json", "original target manifest");
    const std::filesystem::path link = sandbox.Root / "link";
#ifdef _WIN32
    const JunctionGuard cleanup{link};
    const auto junction = CreateJunction(link, sandbox.Root / "real");
    ASSERT_TRUE(junction.IsOk()) << junction.GetError().Message();
#else
    std::error_code ec;
    std::filesystem::create_directory_symlink(sandbox.Root / "real", link, ec);
    ASSERT_FALSE(ec) << ec.message();
#endif
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = link;
    const auto staged = tools::cli::BuildAndStagePackage(options);
    EXPECT_FALSE(staged.IsOk());
    if (!staged.IsOk())
    {
        EXPECT_NE(staged.GetError().Message().find("symlink or reparse point"), std::string::npos);
    }
    EXPECT_EQ(ReadSentinel(sandbox.Root / "real"), "original target bytes");
    EXPECT_EQ(ReadFile(sandbox.Root / "real/pack.json"), "original target manifest");
#ifdef _WIN32
    options.OutputDirectory = sandbox.Root / "invalid|output";
    const auto uninspectable = tools::cli::BuildAndStagePackage(options);
    EXPECT_FALSE(uninspectable.IsOk());
    if (!uninspectable.IsOk())
    {
        EXPECT_NE(uninspectable.GetError().Message().find("cannot inspect package stage output"), std::string::npos);
    }
#endif
}

TEST(PackageStage, RejectsRuntimeManifestNameAlias)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage-manifest-alias"};
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = sandbox.Root / "output";
    ASSERT_TRUE(tools::cli::BuildAndStagePackage(options).IsOk());
    const std::string original = ReadFile(options.OutputDirectory / "pack.json");
    options.SourceId = "RuntimeReplacement";
    const auto replacement = tools::cli::GeneratePackageManifest(options);
    ASSERT_TRUE(replacement.IsOk());
    sandbox.CreateDir("shared");
    ASSERT_TRUE(vase::WritePackageManifestFile(sandbox.Root / "shared/PACK.JSON", replacement.Value()).IsOk());
    options.SourceId = "Probe";
    options.RuntimeFiles.push_back(sandbox.Root / "shared/PACK.JSON");
    const auto staged = tools::cli::BuildAndStagePackage(options);
#if defined(_WIN32) || defined(__APPLE__)
    EXPECT_FALSE(staged.IsOk());
    if (!staged.IsOk())
    {
        EXPECT_NE(staged.GetError().Message().find("name collides with package content"), std::string::npos);
    }
#else
    EXPECT_TRUE(staged.IsOk());
    EXPECT_EQ(ReadFile(options.OutputDirectory / "PACK.JSON"), ReadFile(options.RuntimeFiles.front()));
#endif
    EXPECT_EQ(ReadFile(options.OutputDirectory / "pack.json"), original);
#ifdef _WIN32
    // Win32 去尾点形成另一种文件名别名，实际复制必须拒绝覆盖。
    options.RuntimeFiles.front() = sandbox.Root / "shared/PACK.JSON.";
    const auto trailingDot = tools::cli::BuildAndStagePackage(options);
    EXPECT_FALSE(trailingDot.IsOk());
    if (!trailingDot.IsOk())
    {
        EXPECT_NE(trailingDot.GetError().Message().find("name collides in staging directory"), std::string::npos);
    }
    EXPECT_EQ(ReadFile(options.OutputDirectory / "pack.json"), original);
#endif
}

TEST(PackageStage, RejectsRuntimeLibraryNameAlias)
{
    const testing_support::CatalogSandbox sandbox{"package-stage-library-alias"};
    auto options = DynamicOptions(sandbox);
    ASSERT_TRUE(tools::cli::BuildAndStagePackage(options).IsOk());
    const std::string libraryName = vase::catalog_detail::LibraryFileName("VasePlugins");
    const std::string originalManifest = ReadFile(options.OutputDirectory / "pack.json");
    const std::string originalLibrary = ReadFile(options.OutputDirectory / libraryName);
    const std::string alias = UppercaseName(libraryName);
    sandbox.WriteFile("shared/" + alias, "runtime replacement library bytes");
    options.RuntimeFiles.push_back(sandbox.Root / "shared" / alias);
    const auto staged = tools::cli::BuildAndStagePackage(options);
#if defined(_WIN32) || defined(__APPLE__)
    EXPECT_FALSE(staged.IsOk());
    if (!staged.IsOk())
    {
        EXPECT_NE(staged.GetError().Message().find("name collides with package content"), std::string::npos);
    }
#else
    EXPECT_TRUE(staged.IsOk());
    EXPECT_EQ(ReadFile(options.OutputDirectory / alias), "runtime replacement library bytes");
#endif
    EXPECT_EQ(ReadFile(options.OutputDirectory / "pack.json"), originalManifest);
    EXPECT_EQ(ReadFile(options.OutputDirectory / libraryName), originalLibrary);
}

TEST(PackageStage, RejectsDuplicateRuntimeNames)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage-runtime-alias"};
    sandbox.WriteFile("output/sentinel", "original");
    sandbox.WriteFile("first/runtime.dat", "first payload");
    sandbox.WriteFile("second/runtime.dat", "second payload");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = sandbox.Root / "output";
    options.RuntimeFiles = {sandbox.Root / "first/runtime.dat", sandbox.Root / "second/runtime.dat"};
    const auto duplicate = tools::cli::BuildAndStagePackage(options);
    EXPECT_FALSE(duplicate.IsOk());
    if (!duplicate.IsOk())
    {
        EXPECT_NE(duplicate.GetError().Message().find("duplicate runtime dependency name"), std::string::npos);
    }
    EXPECT_EQ(ReadSentinel(options.OutputDirectory), "original");

    sandbox.WriteFile("second/RUNTIME.DAT", "alias payload");
    options.RuntimeFiles.back() = sandbox.Root / "second/RUNTIME.DAT";
    const auto alias = tools::cli::BuildAndStagePackage(options);
#if defined(_WIN32) || defined(__APPLE__)
    EXPECT_FALSE(alias.IsOk());
    if (!alias.IsOk())
    {
        EXPECT_NE(alias.GetError().Message().find("duplicate runtime dependency name"), std::string::npos);
    }
    EXPECT_EQ(ReadSentinel(options.OutputDirectory), "original");
#else
    EXPECT_TRUE(alias.IsOk());
    EXPECT_EQ(ReadFile(options.OutputDirectory / "runtime.dat"), "first payload");
    EXPECT_EQ(ReadFile(options.OutputDirectory / "RUNTIME.DAT"), "alias payload");
#endif
}

TEST(PackageStage, RejectsProtectedAndInputDirectoryCaseAliases)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-stage-path-alias"};
    sandbox.WriteFile("guarded/sentinel", "original");
    sandbox.WriteFile("guarded/dependency.dat", "runtime payload");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = sandbox.Root / "FUTURE";
    options.ProtectedDirectories.push_back(sandbox.Root / "future");
    const auto futureProtected = tools::cli::BuildAndStagePackage(options);
#if defined(_WIN32) || defined(__APPLE__)
    EXPECT_FALSE(futureProtected.IsOk());
    if (!futureProtected.IsOk())
    {
        EXPECT_NE(futureProtected.GetError().Message().find("protected directory"), std::string::npos);
    }
#else
    EXPECT_TRUE(futureProtected.IsOk());
#endif
    options.ProtectedDirectories.clear();
    options.OutputDirectory = sandbox.Root / "FUTUREINPUT";
    options.RuntimeFiles.push_back(sandbox.Root / "futureinput/dependency.dat");
    const auto futureInput = tools::cli::BuildAndStagePackage(options);
    EXPECT_FALSE(futureInput.IsOk());
    if (!futureInput.IsOk())
    {
#if defined(_WIN32) || defined(__APPLE__)
        EXPECT_NE(futureInput.GetError().Message().find("ancestor of input file"), std::string::npos);
#else
        EXPECT_NE(futureInput.GetError().Message().find("missing runtime dependency"), std::string::npos);
#endif
    }
    options.RuntimeFiles.clear();
    options.OutputDirectory = sandbox.Root / "GUARDED";
    options.ProtectedDirectories.push_back(sandbox.Root / "guarded");
    const auto protectedAlias = tools::cli::BuildAndStagePackage(options);
#if defined(_WIN32) || defined(__APPLE__)
    EXPECT_FALSE(protectedAlias.IsOk());
    if (!protectedAlias.IsOk())
    {
        EXPECT_NE(protectedAlias.GetError().Message().find("protected directory"), std::string::npos);
    }
#else
    EXPECT_TRUE(protectedAlias.IsOk());
#endif
    options.ProtectedDirectories.clear();
    options.RuntimeFiles.push_back(sandbox.Root / "guarded/dependency.dat");
    const auto inputAlias = tools::cli::BuildAndStagePackage(options);
#if defined(_WIN32) || defined(__APPLE__)
    EXPECT_FALSE(inputAlias.IsOk());
    if (!inputAlias.IsOk())
    {
        EXPECT_NE(inputAlias.GetError().Message().find("ancestor of input file"), std::string::npos);
    }
#else
    EXPECT_TRUE(inputAlias.IsOk());
#endif
    EXPECT_EQ(ReadSentinel(sandbox.Root / "guarded"), "original");
    EXPECT_EQ(ReadFile(sandbox.Root / "guarded/dependency.dat"), "runtime payload");
}
