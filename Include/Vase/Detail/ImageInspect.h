#pragma once

// 自写 PE / ELF / Mach-O 镜像解析（§8.2 末段 / §8.7：三档证据、导入表执法、缺依赖诊断
// **共用同一条解析路径**）。纯字节进、纯数据出：同一份解析代码读内存镜像与
// 磁盘文件——档三的「身份」必须来自同一种读法才可比。
// 三解析器的名字契约（返回文件原文、不做路径归一）钉在下方 Mach-O 段声明注释（D176）。
// 无异常纪律（§9.2 清单）：不碰任何会抛的容器 API；越界读一律先查再取。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vase::detail
{

enum class IdentityKind : std::uint8_t
{
    kElfBuildId,  // Linux：.note.gnu.build-id 的 desc 字节（sha1 形态 20B）
    kPdbCodeView, // Windows：RSDS 的 GUID(16B) + Age(4B)
    kMachOUuid,   // macOS：LC_UUID 的 16 字节
};

struct ImageIdentity
{
    // 无默认值：Kind 必须由生产者显式给。留个 kElfBuildId 当默认，等于给「新解析路径
    // 忘了赋 Kind」埋一个静默错——贴错标签后档三的 Kind+Bytes 全等比对恒不等，
    // 表现为「Adopt 总是拒绝」，看不出根因（D175）。
    IdentityKind Kind;
    std::vector<std::uint8_t> Bytes;

    bool operator==(const ImageIdentity&) const = default;
};

// —— PE（Windows）。loadedInMemory：镜像已映射（HMODULE 基址 = span 起点，
//    RVA 直接可用）；false：磁盘字节（RVA 先经节表映射到文件偏移）。
VASE_HOST_API Result<ImageIdentity> ParsePeCodeView(std::span<const std::uint8_t> image, bool loadedInMemory);
VASE_HOST_API Result<std::vector<std::string>> ParsePeImports(std::span<const std::uint8_t> image, bool loadedInMemory);

// —— ELF（Linux）。notes：一个或多个 PT_NOTE 区域拼接后的字节（内存侧由
//    dl_iterate_phdr 收集；文件侧由 ParseElfBuildIdFile 内部收集后复用同一函数）。
VASE_HOST_API Result<ImageIdentity> ExtractBuildIdFromNotes(std::span<const std::uint8_t> notes);
VASE_HOST_API Result<ImageIdentity> ParseElfBuildIdFile(std::span<const std::uint8_t> fileBytes);
VASE_HOST_API Result<std::vector<std::string>> ParseElfNeededFile(std::span<const std::uint8_t> fileBytes);

// —— Mach-O（macOS）。纯字节进、纯数据出，**跨平台编译**（八线的合成字节用例都调它）。
//    契约：名字一律返回**文件里的原文**，不做路径归一——`@rpath/` 的处置在比对点（D176）。
VASE_HOST_API Result<ImageIdentity> ParseMachOUuidFile(std::span<const std::uint8_t> fileBytes);
VASE_HOST_API Result<std::vector<std::string>> ParseMachODylibNamesFile(std::span<const std::uint8_t> fileBytes);

// 镜像格式。三值而非 bool：`isPe` 那个布尔是「只有两种格式」时代的形状（D174）。
enum class ImageFormat : std::uint8_t
{
    kPe,
    kElf,
    kMachO,
};

// 供 §8.2「加载失败时补一句缺哪个依赖」：在**文件字节**上找导入表里磁盘上
// 不存在的条目名；找不到任何解释则返回空串（诊断尽力而为，不假装有把握）。
//
// 契约边界（纯函数拿不到的东西，就别假装拿到）：本函数只做「从文件字节里取出
// **第一个**导入条目名」这一步——它没有路径，也就无从查磁盘。名字里的
// Unresolvable 说的是**调用语境**：调用方（EnsureResident 的失败路径）已经把
// 加载失败握在手里，拿这个名字当「最可能的缺项」写进报告。存在性判定若要更准，
// 是 §8.7 执法（读同一份导入表 + 文件名主干比对）而不是这里的事。
VASE_HOST_API std::string FirstUnresolvableImport(std::span<const std::uint8_t> fileBytes, ImageFormat format);

} // namespace vase::detail
