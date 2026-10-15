#pragma once

#include "Vase/Catalog/Analysis.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PackagePlan.h"

#include <string>

namespace tools
{

// 只维护来源到文字的规则；命令行布局由各调用者维护，跳过静态条目不要求绑定。
inline std::string SourceText(const vase::PluginCatalog& catalog, const vase::AnalyzedPlugin& plugin)
{
    std::string text = "source " + plugin.SourceId + ", ";
    switch (plugin.Kind)
    {
    case vase::PackageSourceKind::kDynamic:
        text += "dynamic";
        break;
    case vase::PackageSourceKind::kInProcess:
        text += "inProcess";
        break;
    }
    const vase::CatalogSource* source = catalog.SourceFor(plugin.Id);
    if (source != nullptr && !source->BinaryPath.empty())
    {
        text += ", binary \"" + source->BinaryPath.string() + "\"";
    }
    if (plugin.Kind == vase::PackageSourceKind::kInProcess && plugin.Decision == vase::LoadDecision::kLoad)
    {
        text += ", needs host binding at execution time";
    }
    return text;
}

} // namespace tools
