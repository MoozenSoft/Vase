#include "Doctor.h"

#include "Cli.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <optional>

// NOLINTBEGIN(misc-include-cleaner) windows.h 是系统侧伞形头、符号实体散在各子头——
// 无代码级出路，实测推导与两类自相矛盾的诊断见 Source/Host/LoaderWindows.cpp 的头部注释。
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// NOLINTEND(misc-include-cleaner)
#endif

namespace
{

struct Finding
{
    // 构造而非指定初始化：省略尾字段的指定初始化点在本工具链是 error，构造函数是代码级出路。
    explicit Finding(std::string_view title)
        : Title(title)
    {
    }

    std::string_view Title;
    std::vector<std::string> Lines;
    std::size_t Failures = 0;
};

std::filesystem::path BinaryOf(const vase::PluginCatalog& catalog, const vase::ManifestEntry& entry)
{
    return catalog.Directory() / entry.Subdirectory / vase::catalog_detail::LibraryFileName(entry.Binary);
}

// ③ 写探针：创建→关→删；删除失败也算 FAIL（D150——探测不得留下不可见副作用）。
bool ProbeWrite(const std::filesystem::path& dir, std::string& detail)
{
    const auto ns = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path probe = dir / (".vase-probe-" + std::to_string(ns) + ".tmp");
    std::error_code ec;
    {
        std::ofstream stream(probe, std::ios::binary); // 默认即 trunc；openmode 相或会触发 signed-bitwise
        if (!stream.is_open())
        {
            // ofstream 失败不带 ec；errno 是唯一现场线索（两平台底层都是 open 族调用）。
            const std::error_code probeEc(errno, std::generic_category());
            detail = "cannot create \"" + probe.string() + "\": " + probeEc.message();
            return false;
        }
        stream.put('x');
    }
    std::filesystem::remove(probe, ec);
    if (ec)
    {
        detail = "probe file left behind: \"" + probe.string() + "\": " + ec.message();
        return false;
    }
    return true;
}

// ④ Windows 支：独占打开探针（形状照 Loader 的 PlatformReopenWritable，住工具层——D148）。
// nullopt = 未被锁（或文件不在——归①执法，不双报）；字符串 = 锁/探测发现。
#ifdef _WIN32
// NOLINTBEGIN(misc-include-cleaner) Win32 API 符号同上：伞形头之外没有直接包含的提供者。
std::optional<std::string> ProbeExclusiveOpen(const std::filesystem::path& binary)
{
    // 写开=install 语义（M6/D143④ 首测裁定：读开不见映射，实测 gle=0/32 对照）。
    auto* const handle = ::CreateFileW(binary.c_str(), GENERIC_WRITE, /*dwShareMode=*/0, nullptr, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(handle);
        return std::nullopt;
    }
    const DWORD lastError = ::GetLastError();
    if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND)
    {
        return std::nullopt;
    }
    if (lastError == ERROR_SHARING_VIOLATION)
    {
        return std::string{"locked — sharing violation (a previous unload may have kept the image resident)"};
    }
    return std::string{"probe failed (Win32 error "} + std::to_string(static_cast<unsigned long>(lastError)) + ")";
}
// NOLINTEND(misc-include-cleaner)
#endif

// 打 verdict 行并如实返回「是否任一 FAIL」——rc 判定与文本同一趟遍历，不留第二处读数。
bool JoinVerdict(const std::array<Finding*, 4>& checks, std::ostream& out)
{
    std::vector<std::size_t> failed;
    std::size_t number = 0;
    for (const Finding* check : checks)
    {
        ++number;
        if (check->Failures != 0U)
        {
            failed.push_back(number);
        }
    }
    if (failed.empty())
    {
        out << "doctor: ok\n";
        return false;
    }
    out << "doctor: FAIL (checks ";
    // 列表连缀：末位前 " and "，其余 ", "——两枚 FAIL 时与 spec §3.5 样例逐字同形。
    for (auto it = failed.begin(); it != failed.end(); ++it)
    {
        if (it != failed.begin())
        {
            out << (std::next(it) == failed.end() ? " and " : ", ");
        }
        out << *it;
    }
    out << " reported failures)\n";
    return true;
}

} // namespace

namespace tools::cli
{

int DoctorDirectory(const std::filesystem::path& pluginDirectory, std::ostream& out, std::ostream& err)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(pluginDirectory, ec))
    {
        err << "doctor: not a directory: \"" << pluginDirectory.string() << "\"\n";
        return kExitUsage;
    }

    vase::PluginCatalog catalog;
    const vase::Result<void> refreshed = catalog.Refresh(pluginDirectory);
    if (!refreshed.IsOk())
    {
        // 快照未建成 = FAIL 档（D144，D130 同律）；四项明说没跑。
        out << "doctor: snapshot not built: " << refreshed.GetError().Message() << '\n';
        out << "snapshot not built: checks 1-4 were not run\n";
        return kExitCheckFailed;
    }
    for (const vase::CatalogWarning& warning : catalog.Warnings())
    {
        out << "  warning: " << warning.Subdirectory << ": " << warning.Message << '\n'; // D128 info
    }
    if (catalog.Ids().empty())
    {
        out << "no plugins found in the snapshot\n";
        return kExitUsage; // D144：环境档补角，D135 同一律
    }

    out << "doctor: " << catalog.Directory().string() << '\n';
    Finding identity("check 1 (binary identity features):");
    Finding load("check 2 (load & header version):");
    Finding writability("check 3 (directory writability):");
    Finding locks("check 4 (file locks):");

    // —— ① 身份特征在场（磁盘读，零装载）。
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        if (entry == nullptr)
        {
            continue; // Ids() 与 Find() 同源，取不到是不可能的；防御性地跳过而非崩溃
        }
        const vase::Result<vase::detail::ImageIdentity> found =
            vase::detail::Loader::FileIdentity(BinaryOf(catalog, *entry));
        if (found.IsOk())
        {
            identity.Lines.push_back("  " + id + ": identity ok");
        }
        else
        {
            identity.Lines.push_back("  " + id + ": FAIL — " + found.GetError().Message() +
                                     " (tier-3 adopt would reject this binary)");
            ++identity.Failures;
        }
    }

    // —— ③ 目录可写（根 + 各插件子目录）。
    {
        std::string detail;
        if (ProbeWrite(catalog.Directory(), detail))
        {
            writability.Lines.emplace_back("  .: ok");
        }
        else
        {
            writability.Lines.push_back("  .: FAIL — " + detail);
            ++writability.Failures;
        }
    }
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        if (entry == nullptr)
        {
            continue; // Ids() 与 Find() 同源，取不到是不可能的；防御性地跳过而非崩溃
        }
        std::string detail;
        const std::filesystem::path sub = catalog.Directory() / entry->Subdirectory;
        if (ProbeWrite(sub, detail))
        {
            writability.Lines.push_back("  " + entry->Subdirectory + ": ok");
        }
        else
        {
            writability.Lines.push_back("  " + entry->Subdirectory + ": FAIL — " + detail);
            ++writability.Failures;
        }
    }

    // —— ④ 残留文件锁（在 ② 装载之前跑——D152 的免疫构造）。
#ifdef _WIN32
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        if (entry == nullptr)
        {
            continue; // Ids() 与 Find() 同源，取不到是不可能的；防御性地跳过而非崩溃
        }
        const std::filesystem::path binary = BinaryOf(catalog, *entry);
        std::error_code existsEc;
        if (!std::filesystem::exists(binary, existsEc))
        {
            // spec §3.4「文件不存在 → info 跳过」：缺件是 ① 的账（那边已 FAIL 点名），④ 不双报也不计成败。
            locks.Lines.push_back("  " + id + ": absent (check 1 owns it)");
            continue;
        }
        const std::optional<std::string> locked = ProbeExclusiveOpen(binary);
        if (locked.has_value())
        {
            locks.Lines.push_back("  " + id + ": FAIL — " + *locked);
            ++locks.Failures;
        }
        else
        {
            locks.Lines.push_back("  " + id + ": unlocked");
        }
    }
#else
    locks.Lines.emplace_back(
        "  lock probe: not observable on this platform — a resident mapping does not prevent overwrites here");
#endif

    // —— ② 装载读（全工具唯一装载步，置后——D152；D117 的代价由它单独承担）。
    load.Lines.push_back("  host header version: " + std::to_string(vase::kHeaderVersion));
    vase::detail::Loader loader;
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        if (entry == nullptr)
        {
            continue; // Ids() 与 Find() 同源，取不到是不可能的；防御性地跳过而非崩溃
        }
        const vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(BinaryOf(catalog, *entry));
        if (!record.IsOk())
        {
            load.Lines.push_back("  " + id + ": FAIL (" + record.GetError().Message() + ")");
            ++load.Failures;
            continue;
        }
        const vase::Result<std::vector<const vase::PluginDescriptor*>> descriptors =
            vase::InspectDescriptors(*record.Value());
        if (descriptors.IsOk())
        {
            load.Lines.push_back("  " + id + ": HeaderVersion matches (" + std::to_string(descriptors.Value().size()) +
                                 " descriptor(s))"); // D151：计数是 info，不假设 1
        }
        else
        {
            load.Lines.push_back("  " + id + ": FAIL (" + descriptors.GetError().Message() + ")");
            ++load.Failures;
        }
        loader.Unload(*record.Value()); // 借用止于此（Validate.cpp 同款）
    }

    // —— 固定序输出（收集序 ≠ 输出序，D152）。
    const std::array<Finding*, 4> checks{&identity, &load, &writability, &locks};
    for (const Finding* check : checks)
    {
        out << check->Title << '\n';
        for (const std::string& line : check->Lines)
        {
            out << line << '\n';
        }
    }
    return JoinVerdict(checks, out) ? kExitCheckFailed : kExitOk;
}

int RunDoctor(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    if (args.size() != 2U)
    {
        err << "usage: VaseCli doctor <插件目录>\n";
        return kExitUsage;
    }
    return DoctorDirectory(std::filesystem::path{*std::next(args.begin(), 1)}, out, err);
}

} // namespace tools::cli
