#include "Vase/Detail/RegistryBus.h"
#include "Vase/Event/Event.h"
#include "Vase/Service/Service.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace
{

struct IFooService
{
    static constexpr std::string_view kName = "Vase.Test.Foo";
    static constexpr std::uint32_t kVersion = 1;

    // 多态接口的五个特殊成员全处置（cppcoreguidelines-special-member-functions；
    // 与 IEffect / Plugin 同形：接口有身份，不拷贝不移动）。
    IFooService() = default;
    IFooService(const IFooService&) = delete;
    IFooService& operator=(const IFooService&) = delete;
    IFooService(IFooService&&) = delete;
    IFooService& operator=(IFooService&&) = delete;
    virtual ~IFooService() = default;

    [[nodiscard]] virtual int Value() const = 0;
};
static_assert(vase::HasServiceIdentity<IFooService>);
struct NotAService
{
};
static_assert(
    !vase::HasServiceIdentity<NotAService>); // 缺标识 → concept 为假（编译错误文案由 static_assert 在 Get 处给出）

struct PingEvent
{
    static constexpr std::string_view kName = "Vase.Test.Ping";
    static constexpr std::uint32_t kVersion = 1;
    int Seq = 0; // 给下一个实例化它的人去掉「未初始化就读」这个坑
};
static_assert(vase::HasEventIdentity<PingEvent>);

class StubFoo : public IFooService
{
public:
    [[nodiscard]] int Value() const override { return 3; }
};

std::vector<int>& EmitLog()
{
    static std::vector<int> log;
    return log;
}

vase::detail::ServiceEntry MakeEntry(std::string_view name, std::uint32_t version, void* instance)
{
    vase::detail::ServiceEntry e;
    e.Key = vase::ServiceKey{.Name = name, .Version = version};
    e.Instance = instance;
    e.Origin = vase::ServiceOrigin::kHost;
    e.ProviderId = "";
    e.ProviderInstance = nullptr;
    e.HeapHolder = nullptr;
    e.DestroyHolder = nullptr;
    e.Id = 0;
    e.Alive = false;
    return e;
}

TEST(ServiceRegistry, AddFindRemoveAndCount)
{
    vase::detail::ServiceRegistry reg;
    StubFoo foo;
    const std::uint64_t id = reg.Add(MakeEntry("Vase.Test.Foo", 1, &foo));
    ASSERT_NE(reg.Find({.Name = "Vase.Test.Foo", .Version = 1}), nullptr);
    EXPECT_EQ(reg.Find({.Name = "Vase.Test.Foo", .Version = 1})->Instance, &foo);
    EXPECT_EQ(reg.Count(), 1U);
    EXPECT_TRUE(reg.Remove({.Name = "Vase.Test.Foo", .Version = 1}, id));
    EXPECT_EQ(reg.Find({.Name = "Vase.Test.Foo", .Version = 1}), nullptr);
    EXPECT_EQ(reg.Count(), 0U);
}

TEST(ServiceRegistry, MajorVersionMismatchIsMiss)
{
    vase::detail::ServiceRegistry reg;
    StubFoo foo;
    reg.Add(MakeEntry("Vase.Test.Foo", 1, &foo));
    EXPECT_EQ(reg.Find({.Name = "Vase.Test.Foo", .Version = 2}), nullptr); // §6.1：主版本不同即不同服务
}

TEST(EventBus, EmitDispatchesSynchronouslyInOrder)
{
    vase::detail::EventBus bus;
    EmitLog().clear();
    const auto one = [](const void* event, void* tag)
    { EmitLog().push_back(*static_cast<const int*>(event) + *static_cast<int*>(tag)); };
    const auto destroy = [](void*) {};
    int tagA = 100;
    int tagB = 200;
    bus.Add({.Name = "Vase.Test.Ping", .Version = 1}, &tagA, one, destroy);
    bus.Add({.Name = "Vase.Test.Ping", .Version = 1}, &tagB, one, destroy);
    const int seq = 7;
    bus.Emit({.Name = "Vase.Test.Ping", .Version = 1}, &seq);
    EXPECT_EQ(EmitLog(), (std::vector<int>{107, 207})); // 订阅序；同步派发（§2.4）
}

TEST(EventBus, SelfUnsubscribeDuringEmitIsSafe)
{
    vase::detail::EventBus bus;
    EmitLog().clear();
    static std::uint64_t firstId = 0;
    const auto selfRemove = [](const void*, void* tag)
    {
        EmitLog().push_back(1);
        static_cast<vase::detail::EventBus*>(tag)->Remove({.Name = "Vase.Test.Ping", .Version = 1}, firstId);
    };
    const auto noop = [](void*) {};
    // Add 签名：(key, handler, invoke, destroy)。handler=&bus、invoke=selfRemove、destroy=noop
    // （tag 指向 bus 本身而非堆对象，无销毁义务）。
    firstId = bus.Add({.Name = "Vase.Test.Ping", .Version = 1}, &bus, selfRemove, noop);
    bus.Emit({.Name = "Vase.Test.Ping", .Version = 1}, nullptr);
    EXPECT_EQ(EmitLog().size(), 1U);
    EXPECT_EQ(bus.Count(), 0U); // 派发结束后真删（DeferredDestroy）
}

TEST(EventBus, CountTracksAliveSubscriptions)
{
    vase::detail::EventBus bus;
    const auto invoke = [](const void*, void*) {};
    const auto destroy = [](void*) {};
    const std::uint64_t id = bus.Add({.Name = "Vase.Test.Pong", .Version = 1}, nullptr, invoke, destroy);
    EXPECT_EQ(bus.Count(), 1U);
    bus.Remove({.Name = "Vase.Test.Pong", .Version = 1}, id);
    EXPECT_EQ(bus.Count(), 0U);
}

TEST(ServiceRegistryDeath, DuplicateRegistrationTerminatesBothConfigs)
{
    // 重复注册是**编程错误**，两个构建都终止。本用例**特意不加 NDEBUG 门**：
    // 若实现退回裸 assert，debug 线仍绿而 release 线必红——它守的是「两态一致」。
    vase::detail::ServiceRegistry reg;
    StubFoo foo;
    reg.Add(MakeEntry("Vase.Test.Foo", 1, &foo));
    EXPECT_DEATH(reg.Add(MakeEntry("Vase.Test.Foo", 1, &foo)), "duplicate service registration");
}

} // namespace
