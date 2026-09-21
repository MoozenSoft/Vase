#pragma once

// 副作用的统一表示（§7.1）。回收语义（D8）：Recycle() 做逻辑撤销；
// 对象内存归 Scope/池所有，销毁路径 = Recycle + 显式析构调用，**不走 delete**。
// 移交式注册（Provide(unique_ptr)）的 delete 发生在外壳适配器的 Recycle 内部——
// 虚调用在持有该类型的镜像里执行，与 §3.1 的 VasePluginDestroy_ 同一性质。

namespace vase
{

class EffectScope;

class IEffect
{
public:
    virtual void Recycle() = 0;

    // 四个特殊成员全部显式处置：Effect 一律就地构造、由 Scope 回收，从不拷贝也从不移动。
    IEffect(const IEffect&) = delete;
    IEffect& operator=(const IEffect&) = delete;
    IEffect(IEffect&&) = delete;
    IEffect& operator=(IEffect&&) = delete;

protected:
    IEffect() = default;
    ~IEffect() = default; // 保护非虚析构：delete IEffect* 编不过，销毁权只属于 EffectScope

    friend class EffectScope;
};

} // namespace vase
