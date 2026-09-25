#pragma once

// Catalog 测试沙箱：temp 目录下的一次性插件树，析构 remove_all。
// 机制与 Tests/HotSwap/HotSwapLoopTests.cpp:38-43 同型（声明序契约见 T7）。

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Fail.h"

namespace testing_support
{

class CatalogSandbox
{
public:
    explicit CatalogSandbox(std::string_view name)
    {
        std::error_code ec;
        const std::filesystem::path temp = std::filesystem::temp_directory_path(ec);
        if (ec)
        {
            vase::detail::ProgrammerError("CatalogSandbox: no temp directory");
        }
        const auto ns = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        Root = temp / ("vase-catalog-" + std::string(name) + "-" + std::to_string(ns));
    }

    CatalogSandbox(const CatalogSandbox&) = delete;
    CatalogSandbox& operator=(const CatalogSandbox&) = delete;
    CatalogSandbox(CatalogSandbox&&) = delete;
    CatalogSandbox& operator=(CatalogSandbox&&) = delete;

    ~CatalogSandbox()
    {
        std::error_code ec;
        std::filesystem::remove_all(Root, ec); // 尽力清理；残留只是 temp 垃圾，不判据
    }

    void CreateDir(std::string_view relPath) const
    {
        std::error_code ec;
        std::filesystem::create_directories(Root / std::filesystem::path(relPath), ec);
    }

    void WriteFile(std::string_view relPath, std::string_view content) const
    {
        const std::filesystem::path full = Root / std::filesystem::path(relPath);
        std::error_code ec;
        std::filesystem::create_directories(full.parent_path(), ec);
        std::ofstream out(full, std::ios::binary); // ofstream 默认即 trunc；不带默认写 openmode 相或是 signed bitwise
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void CopyFile(std::string_view relPath, const std::filesystem::path& source) const
    {
        const std::filesystem::path full = Root / std::filesystem::path(relPath);
        std::error_code ec;
        std::filesystem::create_directories(full.parent_path(), ec);
        std::filesystem::copy_file(source, full, std::filesystem::copy_options::overwrite_existing, ec);
        // staging 缺料当场响，别拖到 Refresh/Solve 的「没有这个插件」再猜根因（T7-b）。
        if (ec || !std::filesystem::exists(full, ec))
        {
            ADD_FAILURE() << "CopyFile " << source << " -> " << full << " failed: " << ec.message();
            return;
        }
    }

    std::filesystem::path Root;
};

inline void RefreshOrFail(vase::PluginCatalog& catalog, const CatalogSandbox& sandbox)
{
    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
}

inline void StageManifests(const CatalogSandbox& sandbox,
                           const std::vector<std::pair<std::string, std::string>>& dirToJson)
{
    for (const auto& [dir, json] : dirToJson)
    {
        sandbox.WriteFile(dir + "/plugin.json", json);
    }
}

} // namespace testing_support
