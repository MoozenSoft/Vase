// Greeter.h —— 提供方与消费方共用同一个头（§3.1 的服务接口约定）；
// 它同时是 Embedding（宿主）能「认识」这个服务的载体——宿主不需要插件的实现，
// 只需要标识。Samples 不进发布物，Samples 内部包含用引号，路径相对仓库根。
#pragma once

#include <cstdint>
#include <string_view>

namespace samples
{

class IGreeter
{
public:
    static constexpr std::string_view kName = "Vase.Hello.Greeter";
    static constexpr std::uint32_t kVersion = 1;

    // 多态接口的五个特殊成员全处置（cppcoreguidelines-special-member-functions）：
    // 接口有身份，不拷贝不移动——与 IEffect / Plugin / Context 同形。
    IGreeter() = default;
    IGreeter(const IGreeter&) = delete;
    IGreeter& operator=(const IGreeter&) = delete;
    IGreeter(IGreeter&&) = delete;
    IGreeter& operator=(IGreeter&&) = delete;
    virtual ~IGreeter() = default;

    [[nodiscard]] virtual std::string_view Greet() const = 0;
};

struct GreetEvent
{
    static constexpr std::string_view kName = "Vase.Hello.GreetEvent";
    static constexpr std::uint32_t kVersion = 1;
    int Seq;
};

} // namespace samples
