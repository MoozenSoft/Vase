#pragma once

// 平台差异的唯一收口（§13.1 第二条，v3 给它加了三档证据与导入表执法的分量）。
// Windows：LoadLibraryExW / FreeLibrary / GetProcAddress + PE 解析。
// Linux：dlopen(**RTLD_NOW | RTLD_LOCAL，不给宿主选项**——§8.7，POCO 的
//        RTLD_GLOBAL 默认在多插件反复进出的框架下不可接受) / dlclose / dlsym + ELF 解析。
//
// 「一文件一记录」：EnsureResident 对本实例的同一绝对路径只调一次平台加载——
// Unload 的「解除映射」语义因此与一次 FreeLibrary/dlclose 严格配对（§8.1：
// 卸货只发生在 Eject / Shutdown，DestroyPod 不卸货）。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vase::detail
{

struct BinaryRecord
{
    std::filesystem::path Path; // 绝对化后的路径（表内去重键）
    void* Raw = nullptr;        // 平台句柄（HMODULE / dlopen 返回值）
};

struct UnloadEvidence
{
    // §8.2 档二的平台分工写进结构：读报告的人不必背文档就知道哪个字段在本平台有判据力。
    bool ReopenWritable = false;             // Win 主判（**辅助**地位：改名替换骗得过它）
    bool MappingRemoved = false;             // Linux 主判：dl_iterate_phdr 条目消失
    bool ReopenWritableIsMeaningful = false; // Win: true；Linux: false（值仍记录，不作判据）
    bool MappingRemovalIsObservable = false; // Linux: true；Win: false
};

class VASE_HOST_API Loader
{
public:
    Loader() = default;
    ~Loader() = default;

    Loader(const Loader&) = delete;
    Loader& operator=(const Loader&) = delete;
    Loader(Loader&&) = delete;
    Loader& operator=(Loader&&) = delete;

    Result<BinaryRecord*> EnsureResident(const std::filesystem::path& path);
    UnloadEvidence Unload(const BinaryRecord& record);

    // —— 以下五项**与实例状态无关**，故声明为 static ——
    //
    // 计划原文把它们写成 `const` 实例成员（Symbol 更没带 const）。那样过不了
    // tidy：readability-convert-member-functions-to-static 对「不碰 this 的成员函数」
    // 逐个报。它们的 `const` 本来就只是「不改自己」的代理说法，而这里真正的事实是
    // **根本不看实例**——static 才是准确写法：行为一字不变、调用点不必改（写成
    // `loader.X(...)` 照样编过）、且不必为一个有代码级出路的检查花 NOLINT 额度。
    static Result<void*> Symbol(const BinaryRecord& record, std::string_view name);

    [[nodiscard]] static Result<ImageIdentity> MemoryIdentity(const BinaryRecord& record);      // 档三 · 内存侧
    [[nodiscard]] static Result<ImageIdentity> FileIdentity(const std::filesystem::path& path); // 档三 · 磁盘侧

    // §8.7 执法读的是**文件声明**（「这个二进制要谁」），从磁盘镜像解析——运行期
    // 已解析的导入表会替隐式兄弟链拉边，而账面看不见的正是这种「文件里写着」的关系。
    [[nodiscard]] static Result<std::vector<std::string>>
    ImportedLibraryNamesFromFile(const std::filesystem::path& path);

    [[nodiscard]] static std::string DescribeLoadFailure(const std::filesystem::path& path); // §8.2 缺依赖诊断

    [[nodiscard]] std::size_t ResidentBinaryCount() const { return Binaries.size(); }

    // 表的**查询键是字面比较**：只认传进来的路径与记录里的 Path 逐字相等。
    // EnsureResident 往表里存的是**绝对化后**的路径（§8.1「一文件一记录」的去重键），
    // 所以拿相对路径来查会查不到刚存的那条——调用方（T7/T12）请自己先 Absolute()。
    // 今天不是缺陷：计划里的调用方本就传 absolute(path)；写在这里免得日后重新发现一遍。
    [[nodiscard]] const BinaryRecord* FindResident(const std::filesystem::path& path) const;
    [[nodiscard]] std::vector<BinaryRecord> AllResident() const;

private:
    static Result<void*> PlatformLoad(const std::filesystem::path& path, std::string& outError);
    static void PlatformFree(void* raw);
    static void* PlatformSymbol(void* raw, const char* name);
    static bool PlatformReopenWritable(const std::filesystem::path& path);
    static bool PlatformMappingRemoved(const std::filesystem::path& path);

    // **每实例**一张表（不是进程级）：只存路径+句柄，不存任何实例级对象——**这一点由
    // BinaryRecord 的类型承载**（path + void* 句柄，装不下实例级对象），不是靠插入点断言
    // （§1.2）。「同一绝对路径只调一次平台加载」说的是**本实例**——两个 Loader 实例各持
    // 一个句柄，互不知情。
    std::vector<std::unique_ptr<BinaryRecord>> Binaries;
};

} // namespace vase::detail
