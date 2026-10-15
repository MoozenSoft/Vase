#include "PackageServices.h"
#include "Vase/Plugin.h"

#include <cstdint>

namespace
{

class ResultImpl final : public package_fixture::IResult
{
public:
    void SetObserved(std::int32_t value) { Observation = value; }
    [[nodiscard]] std::int32_t Observed() const override { return Observation; }

private:
    std::int32_t Observation = 0;
};

class PackageConsumerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<package_fixture::IResult>(Result);
        return vase::Result<void>::Ok();
    }

    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        Result.SetObserved(ctx.Get<package_fixture::ICounter>().Value());
        return vase::Result<void>::Ok();
    }

private:
    ResultImpl Result;
};

} // namespace

VASE_PLUGIN(PackageConsumerPlugin){
    .Id = "Vase.PackageConsumer",
    .DisplayName = "包消费方",
    .Version = "1.0.0",
    .Requires = {{.Name = "Vase.PackageProvider.Counter", .Version = 1}},
    .Provides = {{.Name = "Vase.PackageConsumer.Result", .Version = 1}},
};
