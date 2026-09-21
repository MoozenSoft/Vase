#pragma once

// T9 账本语义测试的两个服务标识（测试材料，不是公开 API）：两个插件 fixture 与
// LedgerSemanticsTests 共用这一份定义。解析靠 kName 字符串跨模块匹配（§6.1：不用
// type_index）——C++ 类型是否同名不是判据，标识一致才是。

#include <cstdint>
#include <string_view>

namespace samples_fixture
{

class ISharedService
{
public:
    static constexpr std::string_view kName = "Vase.Test.Shared";
    static constexpr std::uint32_t kVersion = 1;

    // 接口有身份：五个特殊成员全处置、不拷贝不移动（与 IGreeter / IEffect 同形）。
    ISharedService() = default;
    ISharedService(const ISharedService&) = delete;
    ISharedService& operator=(const ISharedService&) = delete;
    ISharedService(ISharedService&&) = delete;
    ISharedService& operator=(ISharedService&&) = delete;
    virtual ~ISharedService() = default;

    [[nodiscard]] virtual int Value() const = 0;
};

// 只由宿主提供（Stage0 在根 Context 注册）：消费者取它成功，但账本**不记**这条边——
// 宿主提供方的 ProviderInstance 为空（§5.6「宿主的解析不落边」）。
class IHostOnlyService
{
public:
    static constexpr std::string_view kName = "Vase.Test.HostOnly";
    static constexpr std::uint32_t kVersion = 1;

    IHostOnlyService() = default;
    IHostOnlyService(const IHostOnlyService&) = delete;
    IHostOnlyService& operator=(const IHostOnlyService&) = delete;
    IHostOnlyService(IHostOnlyService&&) = delete;
    IHostOnlyService& operator=(IHostOnlyService&&) = delete;
    virtual ~IHostOnlyService() = default;

    [[nodiscard]] virtual int Marker() const = 0;
};

} // namespace samples_fixture
