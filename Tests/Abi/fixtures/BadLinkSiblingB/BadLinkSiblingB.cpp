// 互链 fixture 对之 B（T13 §8.7）：一个最小插件 + **一个独立的导出标记**。
//
// 标记不是装饰：`vase_add_plugin_fixture` 给每个插件 target 都设了 CXX_VISIBILITY_PRESET
// hidden，隐藏之后插件唯一导出的符号是 VASE_PLUGIN 生成的 `VasePlugin_GetPlugin`——
// 而 A 与 B **各有一份同名的**，A 调它自己那份即可，链接器不会为 A 记下指向 B 的边。
// 「A 链了 B」于是只写在 CMake 里、什么导入条目也不产生（Windows 无引用导出的导入库
// 是空的）。Linux 侧无引用的 .so 留不留取决于 `--as-needed`，**本机 clang 驱动默认不开**，
// 所以那一侧的 NEEDED 只是偶然活着——不是「链接器在替我们兜着」。
// 3e 要拦的是**真实的**导入条目，所以 B 必须多露一个 A 无法自给的名字，A 真的调它。
#include "Vase/Plugin.h"

namespace
{

class BadLinkSiblingBPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};

} // namespace

// 名字带 Vase 前缀（不进任何命名空间，避免把 extern "C" 的 C 链接弄丢）。
extern "C" VASE_EXPORT int VaseBadLinkSiblingBMarker() { return 1307; }

VASE_PLUGIN(BadLinkSiblingBPlugin){
    .Id = "Vase.BadLinkSiblingB",
    .DisplayName = "互链探针 B",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {},
};
