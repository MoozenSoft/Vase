#include "MobileScenario.h"

#include "PackageServices.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"
#include "VaseMobileResources.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace vase_mobile
{
namespace
{
constexpr std::uint32_t kRoundCount = 50;
constexpr std::int32_t kFirstValue = 7;
constexpr std::int32_t kMiddleValue = 3;
constexpr std::uint32_t kCycleLength = 3;

vase::Result<ScenarioReport> Fail(std::string reason)
{
    return vase::Result<ScenarioReport>::Err(vase::Error{std::move(reason)});
}
} // namespace

vase::Result<ScenarioReport> RunPackageScenario(std::span<const vase::PackageResource> resources)
{
    vase::PluginCatalog catalog;
    const auto imported = catalog.RefreshResources(resources);
    if (!imported.IsOk())
    {
        return Fail("mobile scenario resource import: " + imported.GetError().Message());
    }

    vase::PluginHost host;
    for (std::uint32_t round = 0; round < kRoundCount; ++round)
    {
        const std::int32_t expected = round % kCycleLength == 1 ? kMiddleValue : kFirstValue;
        vase::PluginOverride provider;
        provider.Id = "Vase.PackageProvider";
        provider.Enabled = true;
        provider.Config.Set("Value", vase::Value::From(expected));
        const std::array overrides{provider};

        vase::LoadRequest request;
        request.Overrides = overrides;
        const auto solved = catalog.Solve(request, kPackageBindings);
        if (!solved.IsOk())
        {
            return Fail("mobile scenario Solve round " + std::to_string(round) + ": " + solved.GetError().Message());
        }
        const auto created = host.CreatePod(solved.Value().Plan);
        if (!created.IsOk())
        {
            return Fail("mobile scenario CreatePod round " + std::to_string(round) + ": " +
                        created.GetError().Message());
        }

        vase::Pod* pod = host.Resolve(created.Value());
        const auto* observed = pod == nullptr ? nullptr : pod->Root().TryGet<package_fixture::IResult>();
        const bool serviceMatches = observed != nullptr && observed->Observed() == expected;
        const bool expectedLiveState =
            host.ForTestLedgerEdgeCount() == 1 && host.ForTestCounters().PluginInstances == 2;
        const auto destroyed = host.DestroyPod(created.Value());
        if (!serviceMatches || !expectedLiveState || !destroyed.Clean() || host.ForTestLedgerEdgeCount() != 0 ||
            host.ForTestCounters().PluginInstances != 0 || host.ForTestCounters().Scopes != 0)
        {
            return Fail("mobile scenario not clean or service mismatch at round " + std::to_string(round));
        }
    }
    return vase::Result<ScenarioReport>::Ok(ScenarioReport{.CompletedRounds = kRoundCount, .Clean = true});
}
} // namespace vase_mobile
