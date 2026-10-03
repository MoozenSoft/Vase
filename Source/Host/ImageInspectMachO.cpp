#include "Vase/Detail/ImageInspect.h"

#include "Detail/ImageBytes.h"
#include "Vase/Detail/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

// 手写常量与字段偏移，**不 include <mach-o/loader.h>**：本 TU 无条件编进 VaseHost
// （D161——八线的合成字节用例都要链接它），而系统头只在 Darwin 上存在。
namespace vase::detail
{
namespace
{

constexpr std::uint32_t kMachMagic64 = 0xFEEDFACFU; // MH_MAGIC_64（小端）

// mach_header_64：magic@0 ncmds@16(4) sizeofcmds@20(4)；头长 32。
constexpr std::size_t kMachHeader64Size = 32U;
constexpr std::size_t kMachNcmdsOffset = 16U;
constexpr std::size_t kMachSizeOfCmdsOffset = 20U;

// load_command：cmd@0(4) cmdsize@4(4)。
constexpr std::size_t kLoadCommandSize = 8U;

constexpr std::uint32_t kLcUuid = 0x1BU;
constexpr std::uint32_t kLcLoadDylib = 0x0CU;
constexpr std::uint32_t kLcLoadWeakDylib = 0x18U;

constexpr std::size_t kUuidCommandSize = 24U; // cmd(4) + cmdsize(4) + uuid(16)
constexpr std::size_t kDylibNameOffset = 8U;  // dylib_command 内 lc_str.name 的偏移
constexpr std::size_t kMachUuidBytes = 16U;

[[nodiscard]] Error MissingMachOUuidError()
{
    return Error{"no LC_UUID load command — 插件构建用了 -Wl,-no_uuid（见 "
                 "Cmake/Toolchains/macos-x64-clang-libcxx.cmake，§8.2 档三）"};
}

// 遍历 load commands。Call(cmd, 条目起点, cmdsize)。
// 返回 false = 「这不是一份能读的 Mach-O」，与「读得出但没有要找的东西」分开——
// 调用方据此选错误文案，不把「格式不对」混进「缺特征」。
template <typename Call>
[[nodiscard]] bool ForEachLoadCommand(std::span<const std::uint8_t> image, Call call)
{
    if (image.size() < kMachHeader64Size || ReadU32(image, 0) != kMachMagic64)
    {
        return false;
    }
    const std::uint32_t count = ReadU32(image, kMachNcmdsOffset);
    const std::uint32_t sizeOfCmds = ReadU32(image, kMachSizeOfCmdsOffset);
    if (!InBounds(kMachHeader64Size, sizeOfCmds, image.size()))
    {
        return false;
    }
    const std::size_t end = kMachHeader64Size + sizeOfCmds;
    std::size_t at = kMachHeader64Size;
    for (std::uint32_t index = 0; index < count && at + kLoadCommandSize <= end; ++index)
    {
        const std::uint32_t cmd = ReadU32(image, at);
        const std::uint32_t cmdSize = ReadU32(image, at + 4U);
        if (cmdSize < kLoadCommandSize || at + cmdSize > end)
        {
            return false; // 结构越界：当作「读不出可用的东西」，绝不越界读
        }
        call(cmd, at, cmdSize);
        at += cmdSize;
    }
    return true;
}

} // namespace

Result<ImageIdentity> ParseMachOUuidFile(std::span<const std::uint8_t> fileBytes)
{
    using Identity = Result<ImageIdentity>;

    std::vector<std::uint8_t> found;
    const bool walked =
        ForEachLoadCommand(fileBytes,
                           [&fileBytes, &found](std::uint32_t cmd, std::size_t at, std::uint32_t cmdSize)
                           {
                               if (cmd != kLcUuid || !found.empty() || cmdSize < kUuidCommandSize)
                               {
                                   return;
                               }
                               const std::span<const std::uint8_t> uuid =
                                   fileBytes.subspan(at + kLoadCommandSize, kMachUuidBytes);
                               found.assign(uuid.begin(), uuid.end());
                           });
    if (!walked)
    {
        return Identity::Err(Error{"not a Mach-O image"});
    }
    if (found.empty())
    {
        return Identity::Err(MissingMachOUuidError());
    }
    return Identity::Ok(ImageIdentity{.Kind = IdentityKind::kMachOUuid, .Bytes = std::move(found)});
}

Result<std::vector<std::string>> ParseMachODylibNamesFile(std::span<const std::uint8_t> fileBytes)
{
    using Names = Result<std::vector<std::string>>;

    std::vector<std::string> names;
    const bool walked =
        ForEachLoadCommand(fileBytes,
                           [&fileBytes, &names](std::uint32_t cmd, std::size_t at, std::uint32_t cmdSize)
                           {
                               // LC_ID_DYLIB 是**自名**不是导入，不收（与 DT_NEEDED / PE 导入表同口径）。
                               if (cmd != kLcLoadDylib && cmd != kLcLoadWeakDylib)
                               {
                                   return;
                               }
                               if (cmdSize < kDylibNameOffset + 4U)
                               {
                                   return; // 坏条目：cmdsize 装不下 lc_str.name 的 4 字节，先跳不读（绝不越界读）
                               }
                               const std::uint32_t nameOffset = ReadU32(fileBytes, at + kDylibNameOffset);
                               if (nameOffset >= cmdSize)
                               {
                                   return; // 坏条目：跳过而不是整体失败（与 PE 导入表的 break 同精神）
                               }
                               Result<std::string> name = ReadNulTerminated(fileBytes, at + nameOffset, at + cmdSize);
                               if (name.IsOk())
                               {
                                   names.push_back(std::move(name.Value()));
                               }
                           });
    if (!walked)
    {
        return Names::Err(Error{"not a Mach-O image"});
    }
    return Names::Ok(std::move(names));
}

} // namespace vase::detail
