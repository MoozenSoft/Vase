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

// 同 LoaderWindows.cpp：本 TU 的 Win32 API 实体在 misc-include-cleaner 眼里没有
// 「直接包含的提供者」（windows.h 是系统侧伞形头），代码级出路不成立。理由详见
// LoaderWindows.cpp 里那段。抑制范围限于本 TU。
// NOLINTBEGIN(misc-include-cleaner)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace vase::detail
{
namespace
{

// 已映射镜像的读取窗口。
//
// 计划给的是「固定 0x10000 字节窗口 + Require 越界检查」，但固定窗口有个真问题：
// Require 只知道 span 有多长，不知道映射有多长——小镜像（SizeOfImage < 64KB）上
// 拿 0x10000 去读会伸到映射之外，那是访问违例，不是能返回 Err 的东西。
// 故先按计划自己提示的「SizeOfImage 可从 OptionalHeader 读到后再收窄窗口」收窄一次：
// HMODULE 起点的**第一页必定映射着**（PE 头就在其中），在这一页里读 SizeOfImage 是安全的。
// 读不到（非 PE / 头部异常）就退回固定窗口——与计划同形。
constexpr std::size_t kHeaderProbeWindow = 0x1000U;
constexpr std::size_t kFallbackImageWindow = 0x10000U;
constexpr std::size_t kDosLfanewOffset = 0x3CU;
constexpr std::size_t kCoffHeaderSize = 20U;
constexpr std::size_t kSizeOfImageOffset = 56U;
constexpr std::uint16_t kPe32Magic = 0x010BU;
constexpr std::uint16_t kPe32PlusMagic = 0x020BU;

[[nodiscard]] std::uint16_t ReadU16At(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint16_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] std::uint32_t ReadU32At(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] std::size_t MappedImageWindow(const void* module)
{
    const auto* base = static_cast<const std::uint8_t*>(module);
    const std::span<const std::uint8_t> head{base, kHeaderProbeWindow};

    const std::size_t optionalHeader =
        static_cast<std::size_t>(ReadU32At(head, kDosLfanewOffset)) + sizeof(std::uint32_t) + kCoffHeaderSize;
    if (optionalHeader + kSizeOfImageOffset + sizeof(std::uint32_t) > head.size())
    {
        return kFallbackImageWindow;
    }
    const std::uint16_t magic = ReadU16At(head, optionalHeader);
    if (magic != kPe32Magic && magic != kPe32PlusMagic)
    {
        return kFallbackImageWindow;
    }
    const std::uint32_t sizeOfImage = ReadU32At(head, optionalHeader + kSizeOfImageOffset);
    return sizeOfImage == 0U ? kFallbackImageWindow : sizeOfImage;
}

[[nodiscard]] Result<ImageIdentity> PeIdentityFromMemory(const void* module)
{
    if (module == nullptr)
    {
        return Result<ImageIdentity>::Err(Error{"null module handle"});
    }
    const auto* base = static_cast<const std::uint8_t*>(module);
    return ParsePeCodeView(std::span<const std::uint8_t>{base, MappedImageWindow(module)}, /*loadedInMemory=*/true);
}

} // namespace

Result<ImageIdentity> MemoryIdentityPlatform(const void* raw, const std::filesystem::path& path)
{
    static_cast<void>(path); // Windows 侧不需要路径：HMODULE 就是映射基址
    return PeIdentityFromMemory(raw);
}

Result<ImageIdentity> FileIdentityPlatform(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return Result<ImageIdentity>::Err(Error{"cannot read file: " + path.string()});
    }
    return ParsePeCodeView(bytes, /*loadedInMemory=*/false);
}

// NOLINTEND(misc-include-cleaner)

} // namespace vase::detail
