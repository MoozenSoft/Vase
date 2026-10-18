#include "Vase/Host/Loader.h"

#include "Vase/Detail/Result.h"

#include <filesystem>
#include <string>

namespace vase::detail
{

Result<void*> Loader::PlatformLoad(const std::filesystem::path& /*path*/, std::string& outError)
{
    outError = "linked-in-only build does not support file loading";
    return Result<void*>::Err(Error{outError});
}

void Loader::PlatformFree(void* /*handle*/) {}

void* Loader::PlatformSymbol(void* /*handle*/, const char* /*symbolName*/) { return nullptr; }

bool Loader::PlatformReopenWritable(const std::filesystem::path& /*path*/) { return false; }

bool Loader::PlatformMappingRemoved(const std::filesystem::path& /*path*/) { return false; }

} // namespace vase::detail
