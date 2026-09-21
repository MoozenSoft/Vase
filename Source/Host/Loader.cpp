#include "Vase/Host/Loader.h"

#include "ImageInspectPlatform.h"
#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace vase::detail
{

Result<BinaryRecord*> Loader::EnsureResident(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec); // §9.2 清单：error_code 形态，永不抛
    if (ec)
    {
        abs = path;
    }
    const auto existing = std::ranges::find_if(Binaries, [&abs](const std::unique_ptr<BinaryRecord>& entry)
                                               { return entry->Path == abs; });
    if (existing != Binaries.end())
    {
        // 已驻留：复用同一记录，不触发二次平台加载（§8.1 表）。
        // 表本身是可变的，所以这里直接取非 const 指针——不需要 const_cast 那一手。
        return Result<BinaryRecord*>::Ok(existing->get());
    }

    std::string platformError;
    Result<void*> loaded = PlatformLoad(abs, platformError);
    if (!loaded.IsOk())
    {
        std::string message = "failed to load binary: " + abs.string() + " (" + platformError + ")";
        if (const std::string hint = DescribeLoadFailure(abs); !hint.empty())
        {
            message += " — missing dependency: " + hint; // POCO 借来的诊断：报「缺哪个」，不只 dlopen failed
        }
        return Result<BinaryRecord*>::Err(Error{std::move(message)});
    }

    // 计划原文这里写的是 `*loaded.Value()`。`Result<void*>::Value()` 返回的就是
    // `void*`（不是 `void**`），解引用它编译不过——实测诊断：
    //   error: indirection not permitted on operand of type 'void *'
    //   error: cannot initialize a member subobject of type 'void *' with an lvalue of type 'void'
    // 直接取那个句柄即可。
    Binaries.push_back(std::make_unique<BinaryRecord>(BinaryRecord{.Path = abs, .Raw = loaded.Value()}));
    return Result<BinaryRecord*>::Ok(Binaries.back().get());
}

UnloadEvidence Loader::Unload(const BinaryRecord& record)
{
    UnloadEvidence evidence;
    const auto it = std::ranges::find_if(Binaries, [&record](const std::unique_ptr<BinaryRecord>& entry)
                                         { return entry->Raw == record.Raw && entry->Path == record.Path; });
    if (it == Binaries.end())
    {
        return evidence; // 重复 Unload：全 false。调用方（Eject/Shutdown）必须读证据，不读=漏账
    }
    const BinaryRecord copy = **it;
    Binaries.erase(it); // 先摘表，后释放——释放触发的任何回调都看不到这个记录（防重入）
    PlatformFree(copy.Raw);

#ifdef _WIN32
    evidence.ReopenWritable = PlatformReopenWritable(copy.Path);
    evidence.ReopenWritableIsMeaningful = true;
    // MappingRemoved 在 Windows 不可观测（§8.2 档二：FreeLibrary 返回值不保证卸载干净），
    // 主判交给 ReopenWritable，但它只是辅助——最终防「假成功」的是档三（T11）。
#else
    evidence.MappingRemoved = PlatformMappingRemoved(copy.Path);
    evidence.MappingRemovalIsObservable = true;
    // Linux 上恒真（旧 inode 解除链接即可），记录但不作判据（§8.2）。
    evidence.ReopenWritable = PlatformReopenWritable(copy.Path);
    evidence.ReopenWritableIsMeaningful = false;
#endif
    return evidence;
}

Result<void*> Loader::Symbol(const BinaryRecord& record, std::string_view name)
{
    const std::string zeroTerminated(name);
    void* address = PlatformSymbol(record.Raw, zeroTerminated.c_str()); // 平台文件各一行实现
    if (address == nullptr)
    {
        return Result<void*>::Err(Error{"symbol not found: " + zeroTerminated});
    }
    return Result<void*>::Ok(address);
}

Result<ImageIdentity> Loader::MemoryIdentity(const BinaryRecord& record)
{
    return MemoryIdentityPlatform(record.Raw, record.Path);
}

Result<ImageIdentity> Loader::FileIdentity(const std::filesystem::path& path) { return FileIdentityPlatform(path); }

Result<std::vector<std::string>> Loader::ImportedLibraryNamesFromFile(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return Result<std::vector<std::string>>::Err(Error{"cannot read file: " + path.string()});
    }
#ifdef _WIN32
    return ParsePeImports(bytes, /*loadedInMemory=*/false);
#else
    return ParseElfNeededFile(bytes);
#endif
}

std::string Loader::DescribeLoadFailure(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return {};
    }
#ifdef _WIN32
    return FirstUnresolvableImport(bytes, /*isPe=*/true);
#else
    return FirstUnresolvableImport(bytes, /*isPe=*/false);
#endif
}

const BinaryRecord* Loader::FindResident(const std::filesystem::path& path) const
{
    // 逐字比较：表里存的是绝对化后的路径（见头文件的契约注释），相对路径查不到。
    const auto it = std::ranges::find_if(Binaries, [&path](const std::unique_ptr<BinaryRecord>& entry)
                                         { return entry->Path == path; });
    return it == Binaries.end() ? nullptr : it->get();
}

std::vector<BinaryRecord> Loader::AllResident() const
{
    std::vector<BinaryRecord> out;
    out.reserve(Binaries.size());
    for (const std::unique_ptr<BinaryRecord>& entry : Binaries)
    {
        out.push_back(*entry);
    }
    return out;
}

} // namespace vase::detail
