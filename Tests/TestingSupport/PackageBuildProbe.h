#pragma once

#include "PackageBuild.h"
#include "PackageDescriptorProbe.h"

namespace testing_support
{

inline const vase::PluginDescriptor* ProbeBuildLookup(const char* id) { return ProbeEntries().Lookup(id); }
inline const vase::PluginDescriptor* const* ProbeBuildEnumerate(std::uint32_t* count)
{
    return ProbeEntries().Enumerate(count);
}

// 两位成员的描述符各经一枚静态包装函数给出（brief 原话）：成员 Id 由描述符决定，不由调用者手写。
inline const vase::PluginDescriptor* ProbeAMemberDescriptor() { return ProbeEntries().Lookup("Probe.A"); }
inline const vase::PluginDescriptor* ProbeBMemberDescriptor() { return ProbeEntries().Lookup("Probe.B"); }

inline tools::cli::PackageBuildOptions MakeProbePackageBuildOptions()
{
    tools::cli::PackageBuildOptions options;
    options.SourceId = "Probe";
    options.Kind = vase::PackageSourceKind::kInProcess;
    options.Entries = {.Lookup = &ProbeBuildLookup, .Enumerate = &ProbeBuildEnumerate};
    options.Members = {{.Descriptor = &ProbeAMemberDescriptor}, {.Descriptor = &ProbeBMemberDescriptor}};
    return options;
}

} // namespace testing_support
