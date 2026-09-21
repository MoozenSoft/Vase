// 互链 fixture 对之 A（T13 §8.7）：**链接期**直链 B 的文件，而不是经服务解析碰它。
// 这正是账本的盲区——账本记的是运行期解析出的边，A 与 B 之间一条都没有；于是若没有
// 导入表这道执法，Eject(B) 会当场假成功（引用计数被 A 的导入表焊死，B 卸不掉）。
#include "Vase/Plugin.h"

// B 的标记：只在链接期被解析（Windows 走 BadLinkSiblingB.lib 的导入跳板；
// Linux 走 libBadLinkSiblingB.so）。
extern "C" int VaseBadLinkSiblingBMarker();

namespace
{

// 命名空间作用域的初始化式 = 一条**不能被优化掉**的引用：函数体在别的镜像里，
// 编译器无从证明它没有副作用，调用必须发出（只写 CMake 的 LINK_LIBRARIES、
// 代码里不引用，产不出任何导入条目——见 BadLinkSiblingB.cpp 顶部那段）。
// [[maybe_unused]]：这个值本身没用途，要的只是链接期那条边。
[[maybe_unused]] const int kLinkedMarker = VaseBadLinkSiblingBMarker();

class BadLinkSiblingAPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(BadLinkSiblingAPlugin){
    .Id = "Vase.BadLinkSiblingA",
    .DisplayName = "互链探针 A",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {},
};
