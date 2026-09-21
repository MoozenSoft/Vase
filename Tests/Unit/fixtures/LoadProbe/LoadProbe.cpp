#include "Vase/Plugin.h"

#include <cstdint>
#include <string_view>

// Loader 端到端探针：一个最小真实插件二进制。两个 CMake target
// （LoadProbe / UnloadProbe）同源码不同输出名，各供一组用例独立装载。
//
// 拆两个文件是为了让**档二证据**干净：Linux 的 dl_iterate_phdr「条目消失」与
// Windows 的「可写开」判的都是**这个镜像还有没有引用**。LoadProbe 被四条用例
// 装载（去重/取符号/身份比对/导入表），它们各自返回前都调了 Unload；
// UnloadProbe 则只被 UnloadEvidencePerPlatform 一条装载、随即卸载，
// 避免同一镜像的 OS 引用计数把那条断言污染成假红。

namespace
{

// 探针事件：让 OnLoad 做一件**真实的 Pod 动作**，从而在导入表里留下 VasePod。
//
// 这不是装饰。计划原文的 OnLoad 是空的（只 return Ok），那样这个「真实插件二进制」
// 一个 VasePod 符号都不引用——链接器只从导入库拉被引用的成员，于是产物里**没有**
// VasePod 的导入条目（实测 llvm-readobj --coff-imports：只有 MSVCP140D / KERNEL32 /
// VCRUNTIME140D）。而 ProbeImportsVasePod 断言的正是「解析器在真实产物上看得见
// VasePod」——空 OnLoad 让它无从成立。Context::Emit<E> 会调到 Context::EmitRaw，
// 那是 VasePod 的 out-of-line 成员，导入条目由此而来（D11 的链接期事实也就真的成立）。
// 事件没有订阅者，派发是空转（§2.4 同步派发）。
struct LoadProbeEvent
{
    static constexpr std::string_view kName = "Vase.LoadProbe.Loaded";
    static constexpr std::uint32_t kVersion = 1;
};

class LoadProbePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Emit(LoadProbeEvent{});
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(LoadProbePlugin){
    .Id = "Vase.LoadProbe",
    .DisplayName = "装载探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
};
