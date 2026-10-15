#include "Detail/PackageStage.h"

#include "PackageBuild.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
// Windows 伞形头的 include-cleaner 归因同 LoaderWindows.cpp。
// NOLINTBEGIN(misc-include-cleaner)
#include <windows.h>
// NOLINTEND(misc-include-cleaner)
#endif

namespace tools::cli
{
namespace
{

using Result = vase::Result<void>;

constexpr const char* kPackageManifestName = "pack.json";

vase::Error StageError(std::string message) { return vase::Error{std::move(message)}; }

package_detail::RenameDirectoryFn& RenameOverride()
{
    static package_detail::RenameDirectoryFn override; // 默认空 = 走真实 rename；测试注入后替代之
    return override;
}

Result RenameDirectory(const std::filesystem::path& from, const std::filesystem::path& to)
{
    if (RenameOverride())
    {
        return RenameOverride()(from, to);
    }
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec)
    {
        return Result::Err(StageError("rename " + from.string() + " -> " + to.string() + ": " + ec.message()));
    }
    return Result::Ok();
}

void RemoveTree(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec); // 尽力清理；temp/backup 名唯一，残留不会覆盖别的包
}

std::string UniqueToken()
{
    const auto ticks = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::to_string(ticks);
}

// 规范化：weakly_canonical 容忍尚不存在的后缀（staging 输出与临时目录都可能还不存在）。
std::filesystem::path Normalize(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
    return ec ? path.lexically_normal() : normalized;
}

// 发布路径在 Windows/macOS 上保守拒绝大小写别名，Linux 保留字节比较。
bool SamePathPart(const std::filesystem::path& left, const std::filesystem::path& right)
{
#ifdef _WIN32
    // NOLINTBEGIN(misc-include-cleaner) Windows API 的伞形头归因同 LoaderWindows.cpp。
    return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
    // NOLINTEND(misc-include-cleaner)
#elif defined(__APPLE__)
    const auto fold = [](std::string text)
    {
        for (char& letter : text)
        {
            if (letter >= 'A' && letter <= 'Z')
            {
                letter = static_cast<char>(letter - 'A' + 'a');
            }
        }
        return text;
    };
    return fold(left.native()) == fold(right.native());
#else
    return left == right;
#endif
}

// left 是 right 的祖先或与之同一路径（两侧都已 Normalize）。
bool IsAncestorOrEqual(const std::filesystem::path& left, const std::filesystem::path& right)
{
    auto leftPart = left.begin();
    auto rightPart = right.begin();
    for (; leftPart != left.end() && rightPart != right.end(); ++leftPart, ++rightPart)
    {
        if (!SamePathPart(*leftPart, *rightPart))
        {
            return false;
        }
    }
    return leftPart == left.end();
}

std::vector<std::filesystem::path> InputFiles(const PackageBuildOptions& options)
{
    std::vector<std::filesystem::path> files;
    if (!options.BinaryPath.empty())
    {
        files.push_back(options.BinaryPath);
    }
    files.insert(files.end(), options.RuntimeFiles.begin(), options.RuntimeFiles.end());
    for (const PackageMemberInput& member : options.Members)
    {
        if (!member.SourceManifest.empty())
        {
            files.push_back(member.SourceManifest);
        }
    }
    return files;
}

// 必须先查原始输出，再做会跟随链接的规范化（T11 Step 3）。
Result CheckOutputLink(const std::filesystem::path& rawOutput)
{
    std::filesystem::path path = rawOutput.lexically_normal();
    while (path != path.root_path() && path.filename().empty())
    {
        path = path.parent_path();
    }
#ifdef _WIN32
    // NOLINTBEGIN(misc-include-cleaner) Windows API 的伞形头归因同 LoaderWindows.cpp。
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD failure = ::GetLastError();
        if (failure != ERROR_FILE_NOT_FOUND && failure != ERROR_PATH_NOT_FOUND)
        {
            return Result::Err(StageError("cannot inspect package stage output: " + path.string() + " (Win32 error " +
                                          std::to_string(failure) + ")"));
        }
    }
    else if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
    {
        return Result::Err(StageError("package stage output must not be a symlink or reparse point: " + path.string()));
    }
    // NOLINTEND(misc-include-cleaner)
#else
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory)
    {
        return Result::Err(StageError("cannot inspect package stage output: " + path.string() + ": " + ec.message()));
    }
    if (std::filesystem::is_symlink(status))
    {
        return Result::Err(StageError("package stage output must not be a symlink or reparse point: " + path.string()));
    }
#endif
    return Result::Ok();
}

Result CheckOutputPath(const PackageBuildOptions& options, const std::filesystem::path& output)
{
    if (output == output.root_path())
    {
        return Result::Err(StageError("package stage refuses the filesystem root: " + output.string()));
    }
    // 受保护目录（CMake 侧注入项目根与构建根）：输出不得与它同一路径、也不得是它的祖先——
    // 那两种形态下「替换输出」会把仓库或构建树一起搬走。
    for (const std::filesystem::path& protectedDir : options.ProtectedDirectories)
    {
        const std::filesystem::path guarded = Normalize(protectedDir);
        if (IsAncestorOrEqual(output, guarded))
        {
            return Result::Err(
                StageError("package stage output would replace protected directory: " + guarded.string()));
        }
    }
    // 输出不得是任一输入文件的祖先：替换输出会把输入文件本身删掉。
    for (const std::filesystem::path& input : InputFiles(options))
    {
        if (IsAncestorOrEqual(output, Normalize(input)))
        {
            return Result::Err(StageError("package stage output is an ancestor of input file: " + input.string()));
        }
    }
    return Result::Ok();
}

// 动态包的主库名（与 GeneratePackageManifest 同一条命名契约）；文件名不合法时返回空串。
std::string PackageLibraryName(const std::filesystem::path& binary)
{
    const std::optional<std::string> stem = vase::catalog_detail::LibraryStem(binary.filename().string());
    if (!stem.has_value())
    {
        return {};
    }
    return vase::catalog_detail::LibraryFileName(stem.value());
}

Result CheckRuntimeNames(const PackageBuildOptions& options)
{
    std::string mainLibrary;
    if (options.Kind == vase::PackageSourceKind::kDynamic)
    {
        mainLibrary = PackageLibraryName(options.BinaryPath);
        if (mainLibrary.empty())
        {
            return Result::Err(StageError("invalid dynamic package library file name: " + options.BinaryPath.string()));
        }
    }
    std::vector<std::filesystem::path> names;
    for (const std::filesystem::path& runtime : options.RuntimeFiles)
    {
        const std::filesystem::path name = runtime.filename();
        if (SamePathPart(name, kPackageManifestName) || (!mainLibrary.empty() && SamePathPart(name, mainLibrary)))
        {
            return Result::Err(StageError("runtime dependency name collides with package content: " + name.string()));
        }
        for (const std::filesystem::path& known : names)
        {
            if (SamePathPart(name, known))
            {
                return Result::Err(StageError("duplicate runtime dependency name: " + name.string()));
            }
        }
        names.push_back(name);
    }
    return Result::Ok();
}

// 准备：在 temp 里生成清单、复制动态库与运行依赖，再把清单读回校验。原包目录此时一个字节都没动。
Result PrepareDirectory(const PackageBuildOptions& options, const vase::PackageManifest& manifest,
                        const std::filesystem::path& temp)
{
    std::error_code ec;
    std::filesystem::create_directories(temp, ec);
    if (ec)
    {
        return Result::Err(StageError("cannot create staging directory " + temp.string() + ": " + ec.message()));
    }
    Result written = vase::WritePackageManifestFile(temp / kPackageManifestName, manifest);
    if (!written.IsOk())
    {
        return written;
    }
    if (options.Kind == vase::PackageSourceKind::kDynamic)
    {
        const std::string libraryName = PackageLibraryName(options.BinaryPath);
        if (libraryName.empty())
        {
            return Result::Err(StageError("invalid dynamic package library file name: " + options.BinaryPath.string()));
        }
        const std::filesystem::path target = temp / libraryName;
        std::filesystem::copy_file(options.BinaryPath, target, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            return Result::Err(
                StageError("cannot stage package library " + options.BinaryPath.string() + ": " + ec.message()));
        }
    }
    for (const std::filesystem::path& runtime : options.RuntimeFiles)
    {
        const bool present = std::filesystem::exists(runtime, ec);
        if (ec || !present)
        {
            return Result::Err(StageError("missing runtime dependency: " + runtime.string()));
        }
        const std::filesystem::path target = temp / runtime.filename();
        // 禁止覆盖兜住实际文件系统的名称等价（如 APFS 的非 ASCII 别名）。
        std::filesystem::copy_file(runtime, target, std::filesystem::copy_options::none, ec);
        if (ec)
        {
            if (ec == std::errc::file_exists)
            {
                return Result::Err(StageError("runtime dependency name collides in staging directory: " +
                                              runtime.filename().string()));
            }
            return Result::Err(StageError("cannot stage runtime dependency " + runtime.string() + ": " + ec.message()));
        }
    }
    const auto staged = vase::ParsePackageManifestFile(temp / kPackageManifestName);
    if (!staged.IsOk())
    {
        return Result::Err(StageError("staged manifest unreadable: " + staged.GetError().Message()));
    }
    return Result::Ok();
}

struct GeneratorArgs
{
    std::string SourceId;
    std::filesystem::path OutputDirectory;
    std::filesystem::path BinaryPath;
    std::vector<std::filesystem::path> RuntimeFiles;
    std::vector<std::filesystem::path> ProtectedDirectories;
    std::vector<std::filesystem::path> SourceManifests;
};

// 形状固定：每个 flag 后面恰好一个值；不认识的 flag 与缺值都算用法错误，不猜意图。
// argv 一律经 std::next 取址（pro-bounds 两关的既有惯例，同 Cli.cpp 的 Arguments）。
bool ParseArgs(int argc, const char* const* argv, GeneratorArgs& out)
{
    int index = 1;
    while (index < argc)
    {
        const std::string_view flag{*std::next(argv, index)};
        if (index + 1 >= argc || !flag.starts_with("--"))
        {
            return false;
        }
        const std::string_view value{*std::next(argv, index + 1)};
        index += 2;
        if (flag == "--source-id")
        {
            out.SourceId = std::string(value);
        }
        else if (flag == "--output")
        {
            out.OutputDirectory = value;
        }
        else if (flag == "--binary")
        {
            out.BinaryPath = value;
        }
        else if (flag == "--runtime")
        {
            out.RuntimeFiles.emplace_back(value);
        }
        else if (flag == "--protected")
        {
            out.ProtectedDirectories.emplace_back(value);
        }
        else if (flag == "--manifest")
        {
            out.SourceManifests.emplace_back(value);
        }
        else
        {
            return false;
        }
    }
    return true;
}

// manifest 参数按成员序整份消费：HasSourceManifest 的成员各吃一个，多一个少一个都不算数。
bool BuildMembers(const GeneratorArgs& args, std::span<const package_detail::StageMemberSpec> specs,
                  std::vector<PackageMemberInput>& out)
{
    auto manifest = args.SourceManifests.begin();
    for (const package_detail::StageMemberSpec& spec : specs)
    {
        PackageMemberInput input{.DescriptorSymbol = std::string(spec.DescriptorSymbol), .Descriptor = spec.Descriptor};
        if (spec.HasSourceManifest)
        {
            if (manifest == args.SourceManifests.end())
            {
                return false;
            }
            input.SourceManifest = *manifest;
            ++manifest;
        }
        out.push_back(std::move(input));
    }
    return manifest == args.SourceManifests.end();
}

} // namespace

namespace package_detail
{

void SetRenameDirectoryForTest(RenameDirectoryFn fn) { RenameOverride() = std::move(fn); }

void ResetRenameDirectoryForTest() { RenameOverride() = nullptr; }

int RunPackageStageGenerator(int argc, const char* const* argv, vase::PackageSourceKind kind,
                             vase::PackageEntryPoints entries, std::span<const StageMemberSpec> members)
{
    GeneratorArgs args;
    PackageBuildOptions options;
    if (!ParseArgs(argc, argv, args) || !BuildMembers(args, members, options.Members))
    {
        vase::detail::WriteStderr(
            "usage: stage-generator --source-id <id> --output <dir> [--binary <path>] [--manifest <path>]... "
            "[--runtime <path>]... [--protected <dir>]...\n");
        return 2;
    }
    options.SourceId = std::move(args.SourceId);
    options.Kind = kind;
    options.BinaryPath = std::move(args.BinaryPath);
    options.Entries = entries;
    options.OutputDirectory = std::move(args.OutputDirectory);
    options.RuntimeFiles = std::move(args.RuntimeFiles);
    options.ProtectedDirectories = std::move(args.ProtectedDirectories);
    const Result staged = BuildAndStagePackage(options);
    if (!staged.IsOk())
    {
        vase::detail::WriteStderr("package staging failed: " + staged.GetError().Message() + "\n");
        return 1;
    }
    return 0;
}

} // namespace package_detail

vase::Result<void> BuildAndStagePackage(const PackageBuildOptions& options)
{
    if (options.OutputDirectory.empty())
    {
        return Result::Err(StageError("package stage requires an output directory"));
    }
    Result linkChecked = CheckOutputLink(options.OutputDirectory);
    if (!linkChecked.IsOk())
    {
        return linkChecked;
    }
    const std::filesystem::path output = Normalize(options.OutputDirectory);
    Result outputChecked = CheckOutputPath(options, output);
    if (!outputChecked.IsOk())
    {
        return outputChecked;
    }
    Result runtimeChecked = CheckRuntimeNames(options);
    if (!runtimeChecked.IsOk())
    {
        return runtimeChecked;
    }
    const auto manifest = GeneratePackageManifest(options);
    if (!manifest.IsOk())
    {
        return Result::Err(StageError("package manifest generation failed: " + manifest.GetError().Message()));
    }

    std::error_code ec;
    std::filesystem::create_directories(output.parent_path(), ec);
    const std::string name = output.filename().string();
    const std::string unique = UniqueToken();
    const std::filesystem::path temp = output.parent_path() / (name + ".stage-tmp-" + unique);
    const std::filesystem::path backup = output.parent_path() / (name + ".stage-bak-" + unique);

    Result prepared = PrepareDirectory(options, manifest.Value(), temp);
    if (!prepared.IsOk())
    {
        RemoveTree(temp);
        return prepared;
    }

    const std::filesystem::file_status existing = std::filesystem::symlink_status(output, ec);
    const bool hadOutput = !ec && std::filesystem::exists(existing);
    if (hadOutput)
    {
        const Result moved = RenameDirectory(output, backup);
        if (!moved.IsOk())
        {
            RemoveTree(temp);
            return Result::Err(
                StageError("cannot move existing package directory aside: " + moved.GetError().Message()));
        }
    }
    Result installed = RenameDirectory(temp, output);
    if (!installed.IsOk())
    {
        if (!hadOutput)
        {
            RemoveTree(temp);
            return installed;
        }
        const Result restored = RenameDirectory(backup, output);
        if (!restored.IsOk())
        {
            return Result::Err(StageError("package staging install failed (" + installed.GetError().Message() +
                                          "); original kept at " + backup.string() +
                                          " (restore failed: " + restored.GetError().Message() + ")"));
        }
        RemoveTree(temp);
        return installed;
    }
    if (hadOutput)
    {
        RemoveTree(backup);
    }
    return Result::Ok();
}

} // namespace tools::cli
