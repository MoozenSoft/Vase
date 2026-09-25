// VaseCatalog 的链接冒烟（T1）：能构造、默认态读侧三访问器可用。
// 真语义的测试从 T2 起按簇各立文件；这里只钉「target 立起来了、DLL 落位对了」。

#include "Vase/Catalog/PluginCatalog.h"

#include <gtest/gtest.h>

namespace
{

using vase::PluginCatalog;

TEST(CatalogWiring, DefaultCatalogHasEmptyReadSide)
{
    PluginCatalog catalog;
    EXPECT_TRUE(catalog.Ids().empty());
    EXPECT_TRUE(catalog.Warnings().empty());
    EXPECT_EQ(catalog.Find("Vase.Nope"), nullptr);
}

} // namespace
