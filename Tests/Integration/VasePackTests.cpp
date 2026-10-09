// VasePack 组合库的契约证人（spec D204）：六条一律**经 Loader 按路径装载**后取符号。
// 组合库不链进 VaseTests——链进去会让整个测试二进制从启动起就驻留它，且导入表直连
// 绕过了 Loader，恰好把「宿主零改动」这一跳挖空（spec §5.1）。
#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using GetPluginFn = const vase::PluginDescriptor* (*)(const char*);
using DescriptorsFn = const vase::PluginDescriptor* const* (*)(std::uint32_t*);

std::filesystem::path PackedLibrary() { return std::filesystem::path{VASE_FIXTURE_VASEPLUGINS}; }

// 六条共用：装载组合库，按字面契约名取一枚入口。失败时返回 nullptr，由调用方 ASSERT。
// Loader 是局部对象：EnsureResident 的记录随它析构而释放，但那**不卸货**（§8.1——
// 卸货只发生在 Eject / Shutdown），镜像仍在驻留期，故取到的函数指针与描述符可安全使用。
template <typename Fn>
Fn PackedEntry(const char* symbolName, vase::detail::Loader& loader, const vase::detail::BinaryRecord*& record)
{
    const vase::Result<vase::detail::BinaryRecord*> resident = loader.EnsureResident(PackedLibrary());
    if (!resident.IsOk())
    {
        return nullptr;
    }
    record = resident.Value();
    const vase::Result<void*> symbol = vase::detail::Loader::Symbol(*record, symbolName);
    if (!symbol.IsOk())
    {
        return nullptr;
    }
    // 与 PluginHost.cpp:70 同因：void* → 函数指针只有 reinterpret_cast 一条路。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<Fn>(symbol.Value());
}

std::set<std::string> PackedIds(vase::detail::Loader& loader, const vase::detail::BinaryRecord*& record)
{
    const auto all = PackedEntry<DescriptorsFn>("VasePlugin_Descriptors", loader, record);
    if (all == nullptr)
    {
        return {};
    }
    std::uint32_t count = 0;
    const vase::PluginDescriptor* const* descriptors = all(&count);
    std::set<std::string> ids;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const vase::PluginDescriptor* descriptor = *std::next(descriptors, static_cast<std::ptrdiff_t>(i));
        ids.insert(std::string{descriptor->Meta->Id});
    }
    return ids;
}

} // namespace

TEST(VasePack, GetPluginRoutesEveryPackedId)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const auto getPlugin = PackedEntry<GetPluginFn>("VasePlugin_GetPlugin", loader, record);
    ASSERT_NE(getPlugin, nullptr);

    for (const char* id : {"Vase.Hello", "Vase.Dependent", "Vase.Failing"})
    {
        const vase::PluginDescriptor* descriptor = getPlugin(id);
        ASSERT_NE(descriptor, nullptr) << id;
        EXPECT_EQ(descriptor->Meta->Id, id); // 逐字相等，不是「非空即可」
    }
    loader.Unload(*record);
}

TEST(VasePack, DescriptorsEnumerateAllPackedPlugins)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const std::set<std::string> ids = PackedIds(loader, record);
    ASSERT_EQ(ids.size(), 3U);
    EXPECT_EQ(ids, (std::set<std::string>{"Vase.Dependent", "Vase.Failing", "Vase.Hello"}));
    loader.Unload(*record);
}

TEST(VasePack, PackedDescriptorsKeepTheirOwnMetadata)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const auto getPlugin = PackedEntry<GetPluginFn>("VasePlugin_GetPlugin", loader, record);
    ASSERT_NE(getPlugin, nullptr);

    const vase::PluginDescriptor* hello = getPlugin("Vase.Hello");
    const vase::PluginDescriptor* dependent = getPlugin("Vase.Dependent");
    const vase::PluginDescriptor* failing = getPlugin("Vase.Failing");
    ASSERT_NE(hello, nullptr);
    ASSERT_NE(dependent, nullptr);
    ASSERT_NE(failing, nullptr);

    // 打包没把元数据搅平：三个成员各自的 Provides / Requires 仍是自己的。
    // MetaArray 的计数走 Size()（Count 是私有成员，公开面只有 Size()/Empty()）。
    EXPECT_EQ(hello->Meta->Provides.Size(), 1U); // Vase.Hello.Greeter
    EXPECT_EQ(hello->Meta->Requires.Size(), 0U);
    EXPECT_EQ(dependent->Meta->Requires.Size(), 1U); // requires Greeter
    EXPECT_EQ(dependent->Meta->Provides.Size(), 1U); // Vase.Dependent.Farewell
    EXPECT_EQ(failing->Meta->Provides.Size(), 0U);   // 空 Provides 那一员——证人判别力的对照点
    EXPECT_EQ(failing->Meta->Requires.Size(), 0U);
    loader.Unload(*record);
}

TEST(VasePack, UnknownIdAndNullReturnNull)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const auto getPlugin = PackedEntry<GetPluginFn>("VasePlugin_GetPlugin", loader, record);
    ASSERT_NE(getPlugin, nullptr);

    EXPECT_EQ(getPlugin("Vase.Nope"), nullptr);
    EXPECT_EQ(getPlugin(nullptr), nullptr); // 宏体的守卫语义（spec §5.1）
    loader.Unload(*record);
}

TEST(VasePack, HeaderVersionMatchesPackedBinary)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const auto getPlugin = PackedEntry<GetPluginFn>("VasePlugin_GetPlugin", loader, record);
    ASSERT_NE(getPlugin, nullptr);

    for (const char* id : {"Vase.Hello", "Vase.Dependent", "Vase.Failing"})
    {
        const vase::PluginDescriptor* descriptor = getPlugin(id);
        ASSERT_NE(descriptor, nullptr) << id;
        EXPECT_EQ(descriptor->HeaderVersion, vase::kHeaderVersion) << id;
    }
    loader.Unload(*record);
}

TEST(VasePack, LoaderFindsEntryByLiteralContractName)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> resident = loader.EnsureResident(PackedLibrary());
    ASSERT_TRUE(resident.IsOk()) << resident.GetError().Message();

    // 两枚字面契约名都必须解析得到——这是「宿主零改动」的可证伪点：
    // 谁把入口改名或漏发，这两行当场红。
    EXPECT_TRUE(vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_GetPlugin").IsOk());
    EXPECT_TRUE(vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_Descriptors").IsOk());

    // 错名负例：换个名字必须失败（否则上面两行可能只是在匹配某种通配）。
    EXPECT_FALSE(vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_GetPluginX").IsOk());
    EXPECT_FALSE(vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_Descriptor").IsOk());

    const vase::Result<void*> raw = vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_GetPlugin");
    ASSERT_TRUE(raw.IsOk());
    // 与 PluginHost.cpp:70 同因：void* → 函数指针只有 reinterpret_cast 一条路。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto getPlugin = reinterpret_cast<GetPluginFn>(raw.Value());
    for (const char* id : {"Vase.Hello", "Vase.Dependent", "Vase.Failing"})
    {
        const vase::PluginDescriptor* descriptor = getPlugin(id);
        ASSERT_NE(descriptor, nullptr) << id;
        EXPECT_EQ(descriptor->Meta->Id, id);
    }
    loader.Unload(*resident.Value());
}
