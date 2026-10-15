#pragma once

#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/PluginDescriptor.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace testing_support
{

struct PackageProbeCounts
{
    std::size_t Lookup = 0;
    std::size_t Enumerate = 0;
    std::size_t Create = 0;
    std::size_t CreateConfig = 0;
    std::size_t OnLoad = 0;
    std::size_t Reset = 0;
};

vase::PackageEntryPoints ProbeEntries();
std::vector<vase::ManifestExpectation> ProbeExpectations();
// T6：关联完整的双成员包计划（进程内来源）。Id 借本 TU 的静态成员表，不借函数内临时串。
vase::LoadPlan MakeProbePackagePlan();
void ResetPackageProbe();
void SetProbeFailure(std::string_view failure);
PackageProbeCounts ReadPackageProbeCounts();

} // namespace testing_support
