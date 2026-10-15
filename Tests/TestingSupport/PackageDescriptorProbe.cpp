#include "PackageDescriptorProbe.h"

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Config/ConfigMacros.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

testing_support::PackageProbeCounts& Counts()
{
    static testing_support::PackageProbeCounts counts;
    return counts;
}

std::string_view& Failure()
{
    static std::string_view failure;
    return failure;
}

// 探针包计划的来源键：固定字面量，计划的 Id/PackageSourceId 都借它（静态存储期）。
constexpr const char* kProbeSourceId = "probe.package";

VASE_CONFIG(ProbeConfig, (std::int32_t, Amount, 1, vase::Meta{}));

void* CreateConfig()
{
    ++Counts().CreateConfig;
    return ProbeConfig::VaseConfigCreate();
}

constexpr vase::ConfigInfo ProbeConfigInfo()
{
    vase::ConfigInfo info = vase::FieldsOf<ProbeConfig>();
    info.CreateConfig = &CreateConfig;
    return info;
}

void ResetState() { ++Counts().Reset; }

class ProbePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        ++Counts().OnLoad;
        return vase::Result<void>::Ok();
    }
};

vase::Plugin* Create()
{
    ++Counts().Create;
    return std::make_unique<ProbePlugin>().release();
}

void Destroy(vase::Plugin* plugin) { const std::unique_ptr<vase::Plugin> owning{plugin}; }

constexpr vase::PluginMeta kMetaA{
    .Id = "Probe.A",
    .DisplayName = "Probe A",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {},
    .Config = ProbeConfigInfo(),
    .ProcessStates = {{.Name = "Probe.A.State", .Reset = &ResetState}},
};
constexpr vase::PluginMeta kMetaB{
    .Id = "Probe.B",
    .DisplayName = "Probe B",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {},
    .Config = ProbeConfigInfo(),
    .ProcessStates = {{.Name = "Probe.B.State", .Reset = &ResetState}},
};
constexpr vase::PluginMeta kMetaExtra{
    .Id = "Probe.Extra",
    .DisplayName = "Probe Extra",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {},
};
constexpr vase::PluginMeta kMetaEmpty{
    .Id = "",
    .DisplayName = "Empty Id",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {},
};
constexpr vase::PluginDescriptor kDescA{
    .HeaderVersion = vase::kHeaderVersion,
    .Meta = &kMetaA,
    .Create = &Create,
    .Destroy = &Destroy,
};
constexpr vase::PluginDescriptor kDescB{
    .HeaderVersion = vase::kHeaderVersion,
    .Meta = &kMetaB,
    .Create = &Create,
    .Destroy = &Destroy,
};
constexpr vase::PluginDescriptor kDescExtra{
    .HeaderVersion = vase::kHeaderVersion,
    .Meta = &kMetaExtra,
    .Create = &Create,
    .Destroy = &Destroy,
};
constexpr vase::PluginDescriptor kDescEmpty{
    .HeaderVersion = vase::kHeaderVersion,
    .Meta = &kMetaEmpty,
    .Create = &Create,
    .Destroy = &Destroy,
};
constexpr vase::PluginDescriptor kDescNullMeta{
    .HeaderVersion = vase::kHeaderVersion,
    .Meta = nullptr,
    .Create = &Create,
    .Destroy = &Destroy,
};
// Meta 为 null：版本闸若误挪到读取 Meta 之后，旧版本证人会直接崩溃。
constexpr vase::PluginDescriptor kDescStale{
    .HeaderVersion = vase::kHeaderVersion - 1U,
    .Meta = nullptr,
    .Create = &Create,
    .Destroy = &Destroy,
};

const vase::PluginDescriptor* Lookup(const char* id)
{
    ++Counts().Lookup;
    if (id == nullptr)
    {
        return nullptr;
    }
    const std::string_view requested{id};
    if (requested == "Probe.A")
    {
        return Failure() == "lookup-mismatch" ? &kDescB : &kDescA;
    }
    if (requested == "Probe.B")
    {
        return &kDescB;
    }
    return requested == "Probe.Extra" ? &kDescExtra : nullptr;
}

const vase::PluginDescriptor* const* Enumerate(std::uint32_t* count)
{
    ++Counts().Enumerate;
    static constexpr std::array kNormal{&kDescB, &kDescA};
    static constexpr std::array kDuplicate{&kDescA, &kDescA};
    static constexpr std::array kNullMeta{&kDescA, &kDescNullMeta};
    static constexpr std::array kStale{&kDescA, &kDescStale};
    static constexpr std::array kExtra{&kDescB, &kDescA, &kDescExtra};
    static constexpr std::array kNullDescriptor{&kDescA, static_cast<const vase::PluginDescriptor*>(nullptr)};
    static constexpr std::array kEmptyId{&kDescA, &kDescEmpty};
    *count = 2;
    if (Failure() == "empty-enumeration")
    {
        *count = 0;
        return kNormal.data();
    }
    if (Failure() == "extra-member")
    {
        *count = 3;
        return kExtra.data();
    }
    if (Failure() == "duplicate-id")
    {
        return kDuplicate.data();
    }
    if (Failure() == "null-meta")
    {
        return kNullMeta.data();
    }
    if (Failure() == "stale-header")
    {
        return kStale.data();
    }
    if (Failure() == "null-descriptor")
    {
        return kNullDescriptor.data();
    }
    return Failure() == "empty-id" ? kEmptyId.data() : kNormal.data();
}

} // namespace

namespace testing_support
{

vase::PackageEntryPoints ProbeEntries() { return {.Lookup = &Lookup, .Enumerate = &Enumerate}; }

std::vector<vase::ManifestExpectation> ProbeExpectations()
{
    return {
        {
            .Id = "Probe.A",
            .DisplayName = "Probe A",
            .Version = "1.0.0",
            .Config = {{.Key = "Amount", .Kind = vase::ValueKind::kInt32, .Default = std::int32_t{1}}},
            .ProcessStates = {"Probe.A.State"},
        },
        {
            .Id = "Probe.B",
            .DisplayName = "Probe B",
            .Version = "1.0.0",
            .Config = {{.Key = "Amount", .Kind = vase::ValueKind::kInt32, .Default = std::int32_t{1}}},
            .ProcessStates = {"Probe.B.State"},
        },
    };
}

vase::LoadPlan MakeProbePackagePlan()
{
    // 成员期望住函数内 static：计划条目的 Id 借它们（与 Solve 借快照同律，只借不拥有）。
    static const std::vector<vase::ManifestExpectation> kMembers = ProbeExpectations();
    vase::LoadPlan plan;
    vase::PackagePlanSource source;
    source.SourceId = kProbeSourceId;
    source.Kind = vase::PackageSourceKind::kInProcess;
    source.Entries = ProbeEntries();
    source.Members = kMembers;
    plan.PackageSources.push_back(std::move(source));
    for (const vase::ManifestExpectation& member : kMembers)
    {
        vase::LoadPlanEntry entry;
        entry.Id = member.Id;
        entry.InProcessEntry = ProbeEntries().Lookup;
        entry.Expected = member;
        entry.PackageSourceId = plan.PackageSources.front().SourceId;
        plan.Ordered.push_back(std::move(entry));
    }
    return plan;
}

void ResetPackageProbe()
{
    Counts() = {};
    Failure() = {};
}

void SetProbeFailure(std::string_view failure) { Failure() = failure; }
PackageProbeCounts ReadPackageProbeCounts() { return Counts(); }

} // namespace testing_support
