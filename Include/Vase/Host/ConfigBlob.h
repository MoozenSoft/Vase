#pragma once

// 配置的拥有面（spec 3.3 / D24）：只在宿主侧存在，插件作者不可见（作者只见类型化结构体）。
// 无 JSON、无插件代码——§4.2「合并可被单元测试穷举」以这个形状为前提。
// 导出类带 STL 成员走全局 /wd4251（CLAUDE.md 规矩 1；前提由 D13 + §8.5 矩阵钉着）。
// kString 的借用视图指向 Entry::Stored 里 std::string 的数据——窗口 = 本 blob 任何后续变更之前：
// 任意键的 Set/MergeShallow 插入都可能重分配挪走短串（Set 已把拷入上提到重分配点之前挡住自借用）。
// 借出的指针随来源 blob 存续——装配供给源已收编为 slot->Replays 单源，覆盖全部实例存活期（R-F1，spec §3.3 勘误）。

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Detail/Export.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace vase
{

class VASE_HOST_API ConfigBlob
{
public:
    using Storage = std::variant<bool, std::int32_t, std::int64_t, float, double, std::string>;

    struct Entry
    {
        std::string Key;
        Storage Stored;
    };

    void Set(std::string_view key, const Value& view); // upsert；kString 深拷入拥有存储

    // 借用语义见头注释。禁 .value()——optional 在 kNone 上的 value() 抛 bad_optional_access。
    [[nodiscard]] std::optional<Value> Find(std::string_view key) const;

    void MergeShallow(const ConfigBlob& over); // §4.2：逐 key upsert，嵌套=不可分割整体（本层无嵌套可言）

    [[nodiscard]] std::size_t Size() const { return Fields.size(); }
    [[nodiscard]] const std::vector<Entry>& Entries() const { return Fields; }

    [[nodiscard]] static ConfigBlob FromDefaults(const ConfigInfo& info);

private:
    std::vector<Entry> Fields; // 插入序；线性查找（字段数量级 = 十）
};

} // namespace vase
