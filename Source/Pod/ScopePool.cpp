#include "Vase/Detail/ScopePool.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>

namespace vase::detail
{

void* ScopePool::Acquire(std::size_t size)
{
    assert(size > 0);
    const std::size_t bytes = BlockBytes(size);

    // 迭代器而非下标：非常量下标的 operator[] 过不了 cppcoreguidelines-pro-bounds-*。
    for (auto entry = FreeLists.begin(); entry != FreeLists.end(); ++entry)
    {
        if (entry->first == bytes)
        {
            FreeNode* node = entry->second;
            if (node->Next == nullptr)
            {
                FreeLists.erase(entry);
            }
            else
            {
                entry->second = node->Next;
            }
            return node; // 复用的块本就是 kAlignment 对齐的
        }
    }

    if (ChunkCursor == nullptr || BumpOffset + bytes > kChunkBytes)
    {
        // 多要 kAlignment 字节做对齐余量，再把可用起点向上取整到 kAlignment——
        // 不赌分配器给多少对齐（kAlignment 写死 16，而 max_align_t 在 MSVC STL 只有 8）。
        // 同时按需开大：bytes > kChunkBytes 时若只开 kChunkBytes，返回区会越过 chunk 末尾。
        const std::size_t chunkBytes = (bytes > kChunkBytes ? bytes : kChunkBytes) + kAlignment;
        Chunks.emplace_back(chunkBytes, std::uint8_t{0});
        std::size_t space = chunkBytes;
        void* cursor = Chunks.back().data();
        ChunkCursor = static_cast<std::uint8_t*>(std::align(kAlignment, 1, cursor, space));
        assert(ChunkCursor != nullptr); // 留了 kAlignment 余量，必然成功
        BumpOffset = 0;
    }
    std::uint8_t* block = std::next(ChunkCursor, static_cast<std::ptrdiff_t>(BumpOffset));
    BumpOffset += bytes;
    return block;
}

void ScopePool::Release(void* object, std::size_t size)
{
    assert(object != nullptr);
    assert(size > 0);
    const std::size_t bytes = BlockBytes(size);
    auto* node = static_cast<FreeNode*>(object);
    for (auto& entry : FreeLists)
    {
        if (entry.first == bytes)
        {
            node->Next = entry.second;
            entry.second = node;
            return;
        }
    }
    node->Next = nullptr;
    FreeLists.emplace_back(bytes, node);
}

} // namespace vase::detail
