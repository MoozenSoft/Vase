// T11 构建级证人的校验器（VasePackageStageVerifier）：从盘上的包清单里读一个成员的启用策略，
// 用进程退出码回答「是不是期望值」。构建脚本不自己解析 JSON（spec §3.1 的交付检查复用同一份解析件）。
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"

#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>

namespace
{

int Report(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main(int argc, const char* const* argv)
{
    if (argc != 4)
    {
        return Report("usage: VasePackageStageVerifier <pack.json> <plugin-id> <true|false>");
    }
    const std::filesystem::path manifestPath{*std::next(argv, 1)};
    const std::string pluginId{*std::next(argv, 2)};
    const std::string expected{*std::next(argv, 3)};
    const auto manifest = vase::ParsePackageManifestFile(manifestPath);
    if (!manifest.IsOk())
    {
        return Report("cannot read " + manifestPath.string() + ": " + manifest.GetError().Message());
    }
    for (const vase::ManifestEntry& entry : manifest.Value().Plugins)
    {
        if (entry.Id != pluginId)
        {
            continue;
        }
        const std::string actual = entry.EnabledByDefault ? "true" : "false";
        if (actual != expected)
        {
            std::string message{pluginId};
            message += ": enabledByDefault=";
            message += actual;
            message += ", expected ";
            message += expected;
            return Report(message);
        }
        return 0;
    }
    return Report("member not present in package manifest: " + pluginId);
}
