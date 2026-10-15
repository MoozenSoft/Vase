#include "PackageServices.h"
#include "Vase/Plugin.h"

#include <cstdint>

namespace
{

VASE_CONFIG(ProviderConfig, (std::int32_t, Value, 3, vase::Meta{}));

class CounterImpl final : public package_fixture::ICounter
{
public:
    void SetValue(std::int32_t value) { Count = value; }
    [[nodiscard]] std::int32_t Value() const override { return Count; }

private:
    std::int32_t Count = 0;
};

class PackageProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        Counter.SetValue(ctx.Config<ProviderConfig>().Value);
        ctx.Provide<package_fixture::ICounter>(Counter);
        return vase::Result<void>::Ok();
    }

private:
    CounterImpl Counter;
};

} // namespace

VASE_PLUGIN(PackageProviderPlugin){
    .Id = "Vase.PackageProvider",
    .DisplayName = "包提供方",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.PackageProvider.Counter", .Version = 1}},
    .Config = vase::FieldsOf<ProviderConfig>(),
};
