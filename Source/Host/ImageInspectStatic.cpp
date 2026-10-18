#include "ImageInspectPlatform.h"

#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace vase::detail
{

Result<ImageIdentity> MemoryIdentityPlatform(const void* /*raw*/, const std::filesystem::path& /*path*/)
{
    return Result<ImageIdentity>::Err(Error{"linked-in-only build does not support memory identity inspection"});
}

Result<ImageIdentity> FileIdentityPlatform(const std::filesystem::path& /*path*/)
{
    return Result<ImageIdentity>::Err(Error{"linked-in-only build does not support file identity inspection"});
}

bool IsImageMapped(const std::filesystem::path& /*path*/) { return false; }

ImageFormat PlatformImageFormat()
{
#ifdef __ANDROID__
    return ImageFormat::kElf;
#elif defined(__APPLE__)
    return ImageFormat::kMachO;
#else
#error "ImageInspectStatic requires an Android or iOS target"
#endif
}

Result<std::vector<std::string>> ParseImportedLibraryNames(std::span<const std::uint8_t> /*fileBytes*/)
{
    return Result<std::vector<std::string>>::Err(Error{"linked-in-only build does not support import inspection"});
}

} // namespace vase::detail
