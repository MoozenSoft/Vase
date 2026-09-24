#pragma once

// 描述符——插件二进制与宿主之间的静态契约（v3 §3.1）。POD 纪律：固定布局、
// 数组+长度、const char*/string_view 指向只读字面量——VaseCli scan（M5）将只读
// 数据段拿到全部元信息，不执行镜像里的任何代码。
//
// 拆分 PluginMeta / PluginDescriptor 的理由（spec 3.3(1)，已过编译探针）：
// VASE_PLUGIN 后面那块用户花括号必须整体初始化一个变量，而宏展开到 descriptor
// 聚合里时花括号放不进去——所以宏的最后一行恰好是 `const PluginMeta kVaseMeta_X = PluginMeta`，
// 用户写的 `{...}` 直接落在它身上。作者仍只写一处（§3.1「只有第一项需要手写」）。

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace vase
{

class Context; // 前向声明即可：Plugin 的虚函数只接收引用（T5 定义）
class Plugin;  // 同上：PluginDescriptor 的两个函数指针按名引用它

// §8.3：「插件与宿主包含同一份 Vase 头文件」这条前提唯一的执行点（§3.1）。
// 每次不兼容改动递增；插件作者不需要知道它的存在。
// 1 → 2（M2a-T3）：PluginMeta 布局变更——新增 OptionalRequires 与 Config 两槽（D26）。
inline constexpr std::uint32_t kHeaderVersion = 2U;

struct ServiceRef
{
    std::string_view Name;
    std::uint32_t Version = 0;
};

// 作者写的部分：纯数据、可平凡拷贝。三个服务数组各 16 条上限是编译错误，
// 不静默截断（spec 3.3(2)；上限值随真实插件调整时改这一个常量）。
struct PluginMeta
{
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view Version;
    MetaArray<ServiceRef, 16> Requires;
    // NSDMI `{}` 是刻意的：clang 的 -Wmissing-designated-field-initializers（默认开）会把
    // 省略中间字段判成 error，带 NSDMI 才豁免——bump 的「既有站点零改动」靠它成立。
    // NOLINTNEXTLINE(readability-redundant-member-init) NSDMI 为刻意，理由见上条注释。
    MetaArray<ServiceRef, 16> OptionalRequires{}; // 缺失不跳过（§3.3）；凭声明执法覆盖它（spec 3.4）
    MetaArray<ServiceRef, 16> Provides;
    ConfigInfo Config{}; // .Config = vase::FieldsOf<T>() 显式引用；忘写的静默点照旧（§13.3）
};

// 二进制契约：HeaderVersion 必须是**第一个**字段——加载路径先读它再读其余（§3.1）。
// 四个字段各给零值初值：聚合体也得每个字段都有初值（VASE_PLUGIN 的聚合初始化照样覆盖）。
struct PluginDescriptor
{
    std::uint32_t HeaderVersion = 0;
    const PluginMeta* Meta = nullptr;
    Plugin* (*Create)() = nullptr;
    void (*Destroy)(Plugin*) = nullptr;
};

// 上面那条「HeaderVersion 是第一个字段」由布局断言承重：PluginHost 的加载路径就按这个
// 偏移先读它（§3.1/§8.3），改动字段顺序会在这里编译期失败，而不是运行期读到垃圾。
static_assert(offsetof(PluginDescriptor, HeaderVersion) == 0,
              "HeaderVersion 必须是 PluginDescriptor 的第一个字段（加载路径先读它再读其余）");

class Plugin
{
public:
    // 虚析构必须存在：VasePluginDestroy_ 在插件镜像内 delete，这条路径的正确性以它为前提。
    virtual ~Plugin() = default;

    // 默认构造**必须显式 default**：删拷贝/移动会一并抑制隐式默认构造，
    // 少了这一行 VASE_PLUGIN 的 `std::make_unique<Type>()` 当场编不过。
    Plugin() = default;
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;
    Plugin(Plugin&&) = delete;
    Plugin& operator=(Plugin&&) = delete;

    virtual Result<void> OnLoad(Context& ctx) = 0;
    virtual Result<void> OnStart(Context& ctx)
    {
        static_cast<void>(ctx); // 不是所有插件都需要启动动作（§3.1）
        return Result<void>::Ok();
    }

    // 构造函数只做初始化，不注册副作用、不启动活动（§5.4 的前提，明文写出防复发）。
};

} // namespace vase

// 生成物全集（spec 3.3(1) 探针定稿）：工厂 + 唯一命名描述符 + 统一入口。
// 符号名由类名派生不是插件 Id（Id 含点号不是合法标识符；类名在镜像内天然唯一，§3.1）。
// 一个库装一个插件才有 GetPlugin；组合库的分发表由 VasePack 生成（M5，§8.6），
// 与本宏无关。
//
// 作者侧写法：VASE_PLUGIN(MyPlugin){ ... }; —— 花括号紧贴宏。它落在宏实参内，
// 不受 Allman 管辖，换行写会被格式门判红。
// 宏体里**一个 NOLINT 都不需要**——三条会被报的检查各有代码级出路：
//   · Create / Destroy 只在本 TU 内被取地址 → 放进匿名命名空间，同时避开
//     misc-use-internal-linkage 与 misc-use-anonymous-namespace（`static` 只满足前者，
//     会立刻招来后者；两者都是内部链接，语义等价）；
//   · 创建走 make_unique、销毁端用 unique_ptr 接住再析构：既消掉裸 new/delete 表达式，
//     也消掉 misc-const-correctness——后者对 raw 的建议是给**指针所指**加 const
//     （`::vase::Plugin const* raw`），那是另一个函数类型、赋不进描述符的 Destroy 槽，
//     所以正确的出路是把裸指针整个去掉，而不是照它的 fix-it 改。
// 与 spec 3.3(1) 的探针定稿相比，宏体的外围写法有**四处**不同：上面两处、匿名命名空间的
// 包裹、以及 GetPlugin 的 nullptr 守卫（探针片段没有）。探针证明的**机制**——用户花括号
// 落在宏末行的变量声明上——原样保留。spec 是历史记录，不改史；其宏体写法以此为最新。
#define VASE_PLUGIN(Type)                                                                                              \
    extern const ::vase::PluginMeta kVaseMeta_##Type;                                                                  \
    namespace                                                                                                          \
    {                                                                                                                  \
    ::vase::Plugin* VasePluginCreate_##Type() { return std::make_unique<Type>().release(); }                           \
    void VasePluginDestroy_##Type(::vase::Plugin* raw) { const std::unique_ptr<::vase::Plugin> owning{raw}; }          \
    }                                                                                                                  \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* VasePluginDesc_##Type()                                     \
    {                                                                                                                  \
        static const ::vase::PluginDescriptor kDesc{::vase::kHeaderVersion, &kVaseMeta_##Type,                         \
                                                    &VasePluginCreate_##Type, &VasePluginDestroy_##Type};              \
        return &kDesc;                                                                                                 \
    }                                                                                                                  \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)                        \
    {                                                                                                                  \
        return id != nullptr && std::string_view{id} == kVaseMeta_##Type.Id ? VasePluginDesc_##Type() : nullptr;       \
    }                                                                                                                  \
    const ::vase::PluginMeta kVaseMeta_##Type = ::vase::PluginMeta
