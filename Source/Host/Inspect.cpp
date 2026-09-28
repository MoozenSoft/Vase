#include "Vase/Host/Inspect.h"

#include "Detail/InspectInternal.h"

#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace vase::detail
{

Result<void> CheckHeaderVersion(const PluginDescriptor& desc)
{
    if (desc.HeaderVersion != kHeaderVersion)
    {
        return Result<void>::Err(Error{"HeaderVersion mismatch: binary " + std::to_string(desc.HeaderVersion) +
                                       ", host " + std::to_string(kHeaderVersion)});
    }
    return Result<void>::Ok();
}

} // namespace vase::detail

namespace
{

// 枚举入口的签名（§8.1 家族同形）：与 VASE_PLUGIN 生成的 VasePlugin_Descriptors 一致。
using EnumerationEntryPoint = const vase::PluginDescriptor* const* (*)(std::uint32_t*);

} // namespace

namespace vase
{

Result<std::vector<const PluginDescriptor*>> InspectDescriptors(const detail::BinaryRecord& record)
{
    const Result<void*> symbol = detail::Loader::Symbol(record, "VasePlugin_Descriptors");
    if (!symbol.IsOk())
    {
        // 响亮且点名原因：老二进制照旧能**加载**（装载跳还在），只是工具读不了——不许返回空表。
        return Result<std::vector<const PluginDescriptor*>>::Err(
            Error{"binary has no VasePlugin_Descriptors entry (predates the M5 enumeration face)"});
    }

    // void* → 函数指针只有 reinterpret_cast 一条路（与 InspectBinary 同一处、同理由）。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto enumerate = reinterpret_cast<EnumerationEntryPoint>(symbol.Value());
    std::uint32_t count = 0;
    const PluginDescriptor* const* all = enumerate(&count);
    if (all == nullptr || count == 0U)
    {
        return Result<std::vector<const PluginDescriptor*>>::Err(
            Error{"VasePlugin_Descriptors returned no descriptor"});
    }

    std::vector<const PluginDescriptor*> out;
    out.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const PluginDescriptor* desc = *std::next(all, static_cast<std::ptrdiff_t>(index));
        if (desc == nullptr)
        {
            return Result<std::vector<const PluginDescriptor*>>::Err(
                Error{"VasePlugin_Descriptors returned a null descriptor"});
        }
        const Result<void> version = detail::CheckHeaderVersion(*desc);
        if (!version.IsOk())
        {
            return Result<std::vector<const PluginDescriptor*>>::Err(version.GetError());
        }
        out.push_back(desc);
    }
    return Result<std::vector<const PluginDescriptor*>>::Ok(std::move(out));
}

} // namespace vase
