#include "Vase/Detail/RegistryBus.h"

#include "Vase/Detail/Fail.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace vase::detail
{

std::uint64_t ServiceRegistry::Add(ServiceEntry entry)
{
    // §2.3-4「一服务一实现」的运行期兜底（求解期硬拒是 M2，§6.3）。重复注册是**用户可达的
    // 编程错误**，与解析失败同路：两个构建都终止。这里不能用裸 assert——它在 NDEBUG 下消失，
    // 于是 Debug 终止、Release 静默覆盖，同一个输入在两个构建里行为不同（CLAUDE.md 规矩 4）。
    if (std::ranges::any_of(Entries, [&entry](const ServiceEntry& existing)
                            { return existing.Alive && existing.Key == entry.Key; }))
    {
        detail::ProgrammerError("duplicate service registration: one identifier, one implementation");
    }

    const std::uint64_t id = NextId++;
    entry.Id = id;
    entry.Alive = true;
    Entries.push_back(entry);
    return id;
}

const ServiceEntry* ServiceRegistry::Find(ServiceKey key) const
{
    // Add 在两个构建里都拒重复，故一个键至多一条活项——扫描方向没有语义，用最直白的正向。
    for (const ServiceEntry& entry : Entries)
    {
        if (entry.Alive && entry.Key == key)
        {
            return &entry;
        }
    }
    return nullptr;
}

bool ServiceRegistry::Remove(ServiceKey key, std::uint64_t id)
{
    for (ServiceEntry& entry : Entries)
    {
        if (entry.Alive && entry.Id == id && entry.Key == key)
        {
            entry.Alive = false;
            if (entry.DestroyHolder != nullptr)
            {
                entry.DestroyHolder(entry.HeapHolder); // 移交式注册：实例随登记一起走
            }
            // 墓碑不再持有任何活引用，免得日后有人从 Find 之外的路径读到已死的实例。
            entry.Instance = nullptr;
            entry.HeapHolder = nullptr;
            entry.DestroyHolder = nullptr;
            return true;
        }
    }
    return false;
}

std::size_t ServiceRegistry::Count() const
{
    std::size_t alive = 0;
    for (const ServiceEntry& entry : Entries)
    {
        if (entry.Alive)
        {
            ++alive;
        }
    }
    return alive;
}

} // namespace vase::detail
