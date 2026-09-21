#include "Vase/Detail/ImageInspect.h"

#include "ImageInspectPlatform.h"
#include "Vase/Detail/Result.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <elf.h>
#include <link.h>

namespace vase::detail
{
namespace
{

// dl_iterate_phdr 的回调上下文：按 dlpi_name 全等匹配目标，命中后把该镜像所有
// PT_NOTE 区域的字节拼进 Notes。
struct NoteScan
{
    const char* Target = nullptr;
    std::vector<std::uint8_t> Notes;
    bool Found = false;
};

// 参数类型由 dl_iterate_phdr 的函数指针类型**规定**（int (*)(struct dl_phdr_info*, size_t, void*)），
// 改成 const 指针就不再是它的回调类型、传不进去；本回调也确实只读该结构。
// NOLINTNEXTLINE(misc-const-correctness)
int CollectNotesFromMatchingImage(struct dl_phdr_info* info, std::size_t /*size*/, void* data)
{
    auto* scan = static_cast<NoteScan*>(data);
    if (info == nullptr || scan == nullptr || scan->Target == nullptr || info->dlpi_name == nullptr ||
        std::strcmp(info->dlpi_name, scan->Target) != 0)
    {
        return 0;
    }
    scan->Found = true;
    const std::span<const ElfW(Phdr)> headers{info->dlpi_phdr, static_cast<std::size_t>(info->dlpi_phnum)};
    for (const ElfW(Phdr) & header : headers)
    {
        if (header.p_type != PT_NOTE)
        {
            continue;
        }
        const auto address = static_cast<std::uintptr_t>(info->dlpi_addr + header.p_vaddr);
        // dlpi_addr 是整数地址，「地址 → 指针」只有 reinterpret_cast 一个写法（bit_cast
        // 只是换个名字做同一件事且更难读）。抑制写在**上一行**而不是行尾：这一行的代码
        // 加行尾注释会超过 120 列，clang-format 一折行，注释就落到诊断所在行之外、失效了
        // （T4 已实测过这条）。performance-no-int-to-ptr 与 reinterpret-cast 是同一次转换
        // 的两条剑：整数→指针这条转换没法「改成别的写法」（它就是它）。

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
        const auto* raw = reinterpret_cast<const std::uint8_t*>(address);
        const std::span<const std::uint8_t> region{raw, static_cast<std::size_t>(header.p_memsz)};
        scan->Notes.insert(scan->Notes.end(), region.begin(), region.end());
    }
    return 1; // 命中即停
}

void RunNoteScan(const std::filesystem::path& path, NoteScan& scan)
{
    const std::string target = path.string();
    scan.Target = target.c_str();
    static_cast<void>(::dl_iterate_phdr(&CollectNotesFromMatchingImage, &scan));
}

} // namespace

bool IsImageMapped(const std::filesystem::path& path)
{
    NoteScan scan;
    RunNoteScan(path, scan);
    return scan.Found;
}

Result<ImageIdentity> MemoryIdentityPlatform(const void* raw, const std::filesystem::path& path)
{
    static_cast<void>(raw); // Linux 侧由路径在映射清单里定位，不用句柄
    NoteScan scan;
    RunNoteScan(path, scan);
    if (!scan.Found)
    {
        return Result<ImageIdentity>::Err(Error{"image not mapped: " + path.string()});
    }
    return ExtractBuildIdFromNotes(scan.Notes);
}

Result<ImageIdentity> FileIdentityPlatform(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return Result<ImageIdentity>::Err(Error{"cannot read file: " + path.string()});
    }
    return ParseElfBuildIdFile(bytes);
}

} // namespace vase::detail
