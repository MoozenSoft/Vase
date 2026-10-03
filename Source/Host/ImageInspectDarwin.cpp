#include "Vase/Detail/ImageInspect.h"

#include "ImageInspectPlatform.h"
#include "Vase/Detail/Result.h"

#include <mach-o/dyld.h>
#include <mach-o/loader.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace vase::detail
{
namespace
{

// dyld 记的是**规范化路径**（实测 /tmp 记成 /private/tmp）——全等匹配前必须先规范化；canonical 失败退
// weakly_canonical，再失败退回原路径（§9.2：filesystem 一律 error_code 形态）。详见 spec D164。
// **对测试承重**：临时目录 /var/folders/... 真身是 /private/var/folders/...——不规范化则每条走装载的用例都落空。
[[nodiscard]] std::string Normalized(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path resolved = std::filesystem::canonical(path, ec);
    if (!ec)
    {
        return resolved.string();
    }
    std::error_code weakEc;
    const std::filesystem::path weak = std::filesystem::weakly_canonical(path, weakEc);
    return weakEc ? path.string() : weak.string();
}

[[nodiscard]] int FindImageIndex(const std::filesystem::path& path)
{
    const std::string target = Normalized(path);
    const std::uint32_t count = ::_dyld_image_count();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const char* name = ::_dyld_get_image_name(index);
        if (name != nullptr && target == name)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

} // namespace

bool IsImageMapped(const std::filesystem::path& path) { return FindImageIndex(path) >= 0; }

Result<ImageIdentity> MemoryIdentityPlatform(const void* raw, const std::filesystem::path& path)
{
    static_cast<void>(raw); // 与 Linux 同：由路径在 dyld 清单里定位，不用句柄
    const int index = FindImageIndex(path);
    if (index < 0)
    {
        return Result<ImageIdentity>::Err(Error{"image not mapped: " + path.string()});
    }
    const mach_header* header = ::_dyld_get_image_header(static_cast<std::uint32_t>(index));
    if (header == nullptr)
    {
        return Result<ImageIdentity>::Err(Error{"image header unavailable: " + path.string()});
    }
    // macOS 只跑 64 位镜像，dyld 的 API 回的是通用 header*——「宽类型收窄」没有免转写法。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* typed = reinterpret_cast<const mach_header_64*>(header);
    // 内存镜像的 load commands 紧跟 32 字节头，sizeofcmds 是它们的总长；解析器从前者之后起读。
    // 「对象表示 → 字节」只有 reinterpret_cast 一个写法（它就是它）。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::span<const std::uint8_t> image{reinterpret_cast<const std::uint8_t*>(header),
                                              sizeof(mach_header_64) + typed->sizeofcmds};
    return ParseMachOUuidFile(image);
}

Result<ImageIdentity> FileIdentityPlatform(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return Result<ImageIdentity>::Err(Error{"cannot read file: " + path.string()});
    }
    return ParseMachOUuidFile(bytes);
}

} // namespace vase::detail
