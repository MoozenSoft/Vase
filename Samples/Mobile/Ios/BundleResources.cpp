#include "BundleResources.h"

#include "MobileScenario.h"
#include "Vase/Detail/Result.h"
#include "VaseMobileResources.h"

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

#include <array>
#include <cstddef>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vase_mobile
{
namespace
{
using BundleString = std::unique_ptr<const __CFString, decltype(&CFRelease)>;
using BundleUrl = std::unique_ptr<const __CFURL, decltype(&CFRelease)>;

BundleString AsBundleString(std::string_view contents)
{
    const std::string bytes{contents};
    return BundleString{CFStringCreateWithCString(kCFAllocatorDefault, bytes.c_str(), kCFStringEncodingUTF8),
                        &CFRelease};
}

vase::Result<OwnedPackageResource> LoadBundleResource(CFBundleRef bundle, std::string_view relativeName)
{
    const std::size_t slash = relativeName.rfind('/');
    const std::size_t dot = relativeName.rfind('.');
    if (slash == std::string_view::npos || dot == std::string_view::npos || dot <= slash + 1)
    {
        return vase::Result<OwnedPackageResource>::Err(vase::Error{"invalid generated resource name"});
    }
    const BundleString subdirectory = AsBundleString(relativeName.substr(0, slash));
    const BundleString basename = AsBundleString(relativeName.substr(slash + 1, dot - slash - 1));
    const BundleString extension = AsBundleString(relativeName.substr(dot + 1));
    if (!subdirectory || !basename || !extension)
    {
        return vase::Result<OwnedPackageResource>::Err(vase::Error{"cannot allocate bundle resource name"});
    }
    const BundleUrl url{CFBundleCopyResourceURL(bundle, basename.get(), extension.get(), subdirectory.get()),
                        &CFRelease};
    if (!url)
    {
        return vase::Result<OwnedPackageResource>::Err(
            vase::Error{"missing iPhoneOS bundle resource " + std::string{relativeName}});
    }

    const BundleString filePath{CFURLCopyFileSystemPath(url.get(), kCFURLPOSIXPathStyle), &CFRelease};
    std::array<char, 4096> path{};
    if (!filePath ||
        CFStringGetCString(filePath.get(), path.data(), static_cast<CFIndex>(path.size()), kCFStringEncodingUTF8) == 0U)
    {
        return vase::Result<OwnedPackageResource>::Err(
            vase::Error{"cannot resolve iPhoneOS bundle path " + std::string{relativeName}});
    }

    std::ifstream input{path.data(), std::ios::binary | std::ios::ate};
    if (!input)
    {
        return vase::Result<OwnedPackageResource>::Err(
            vase::Error{"cannot open iPhoneOS bundle resource " + std::string{relativeName}});
    }
    const auto length = input.tellg();
    if (length < 0)
    {
        return vase::Result<OwnedPackageResource>::Err(
            vase::Error{"cannot stat iPhoneOS bundle resource " + std::string{relativeName}});
    }
    std::string contents(static_cast<std::size_t>(length), '\0');
    input.seekg(0);
    if (!contents.empty())
    {
        input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!input)
        {
            return vase::Result<OwnedPackageResource>::Err(
                vase::Error{"cannot read iPhoneOS bundle resource " + std::string{relativeName}});
        }
    }
    return vase::Result<OwnedPackageResource>::Ok(
        OwnedPackageResource{.Location = std::string{relativeName}, .Contents = std::move(contents)});
}
} // namespace

vase::Result<std::vector<OwnedPackageResource>> ReadBundlePackageResources()
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr)
    {
        return vase::Result<std::vector<OwnedPackageResource>>::Err(vase::Error{"iPhoneOS main bundle absent"});
    }
    std::vector<OwnedPackageResource> owned;
    owned.reserve(kPackageResources.size());
    for (const auto& resource : kPackageResources)
    {
        auto read = LoadBundleResource(bundle, resource.AssetName);
        if (!read.IsOk())
        {
            return vase::Result<std::vector<OwnedPackageResource>>::Err(read.GetError());
        }
        owned.push_back(std::move(read.Value()));
    }
    return vase::Result<std::vector<OwnedPackageResource>>::Ok(std::move(owned));
}
} // namespace vase_mobile
