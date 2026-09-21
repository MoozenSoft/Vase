#pragma once

// size-class 空闲链表（D8）。铁律 §1.2 的豁免边界，务必读懂再改：
// 池是进程级容器，但它**只持有空闲节点内存，不持有任何实例级对象**
// （spec 第 5 节原话）——这不违规，它是「反复 play/stop 稳态零分配」（D10）的实现。
//
// chunk 用 vector<std::uint8_t> 而非 unique_ptr<uint8_t[]>（后者是 C 数组类型）；
// vector 被移动时堆缓冲随指针转移，故 Chunks 扩容不影响已发出的块地址。

#include "Vase/Detail/Export.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace vase::detail
{

class VASE_POD_API ScopePool
{
public:
    // 块对齐**写死 16**，不用 alignof(std::max_align_t)：后者在 MSVC STL 是 8、libc++ 是 16
    // （实测：同一句 static_assert 在 Windows 失败、Linux 通过），会让 alignas(16) 的 Effect
    // 在 Linux 编得过、Windows 编不过——跨平台插件库不该有这种陷阱。
    static constexpr std::size_t kAlignment = 16;

    ScopePool() = default;
    ~ScopePool() = default;

    ScopePool(const ScopePool&) = delete;
    ScopePool& operator=(const ScopePool&) = delete;
    ScopePool(ScopePool&&) = delete;
    ScopePool& operator=(ScopePool&&) = delete;

    // 返回一块 Size 字节、kAlignment 对齐的内存（内部向上取整，最小 sizeof(FreeNode)）。
    // Size 无上界：超过 kChunkBytes 时按需开一块更大的 chunk（见 .cpp）。
    void* Acquire(std::size_t size);
    // Object 必须是先前 Acquire(Size) 的返回值且 Size 相同（两次的取整一致）。
    void Release(void* object, std::size_t size);

private:
    struct FreeNode
    {
        FreeNode* Next = nullptr;
    };

    static constexpr std::size_t kChunkBytes = std::size_t{64} * 1024;

    static std::size_t BlockBytes(std::size_t size)
    {
        const std::size_t rounded = (size + kAlignment - 1) & ~(kAlignment - 1);
        return rounded < sizeof(FreeNode) ? sizeof(FreeNode) : rounded;
    }

    std::vector<std::vector<std::uint8_t>> Chunks;
    std::uint8_t* ChunkCursor = nullptr;                      // 当前 chunk 的**已对齐**起点
    std::size_t BumpOffset = 0;                               // 相对 ChunkCursor
    std::vector<std::pair<std::size_t, FreeNode*>> FreeLists; // (块字节数, 链头)
};

} // namespace vase::detail
