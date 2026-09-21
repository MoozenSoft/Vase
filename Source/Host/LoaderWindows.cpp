#include "Vase/Host/Loader.h"

#include "Vase/Detail/Result.h"

#include <filesystem>
#include <string>

// misc-include-cleaner 在这个 TU 里没有代码级出路，故整段抑制（理由如下，不是偷懒）：
// 本文件的每个实体都来自 Win32 API，而 windows.h 是**系统侧的伞形头**——符号实体在
// winnt.h / libloaderapi.h / fileapi.h / handleapi.h / errhandlingapi.h 里。检查器
// 因此同时报两件自相矛盾的事：「包含了 windows.h 却没用它」+「HMODULE 等十几个符号
// 没有直接包含的提供者」。**实测过两条代码级出路，都不成立**：
//   · 自己在 Source/Host 下包一层带 `IWYU pragma: begin_exports/end_exports` 的头
//     （同一手法对 Include/Vase/Plugin.h 那种**用户侧**伞形头有效）——对系统头无效，
//     符号照样认不出提供者；
//   · 直接包含那些真正声明的子头（libloaderapi.h 等）——那是把编译器矩阵的内部布局
//     钉进仓库，比抑制更脆。
// 故就地抑制；范围限于本 TU（另一个平台文件 LoaderPosix.cpp 不含 Win32，无需抑制）。
// NOLINTBEGIN(misc-include-cleaner)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// Unicode 未决项（§13.1「平台差异的收口方式」里那条）：M1 不做 wchar/char 的收口层。
// 路径只经 std::filesystem::path 自己取用——Windows 上 path::c_str() 本就是宽字符，
// 这里唯一一次窄/宽转换发生在符号名（Symbol 传进来的 std::string → GetProcAddress
// 要的 const char*），那一处不涉路径，故不引 Unicode 问题。

namespace vase::detail
{

Result<void*> Loader::PlatformLoad(const std::filesystem::path& path, std::string& outError)
{
    HMODULE module = ::LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (module == nullptr)
    {
        outError = "LoadLibraryExW failed (GetLastError=" + std::to_string(::GetLastError()) + ")";
        return Result<void*>::Err(Error{outError});
    }
    return Result<void*>::Ok(static_cast<void*>(module));
}

void Loader::PlatformFree(void* raw)
{
    if (raw != nullptr)
    {
        static_cast<void>(::FreeLibrary(static_cast<HMODULE>(raw)));
    }
}

void* Loader::PlatformSymbol(void* raw, const char* name)
{
    if (raw == nullptr)
    {
        return nullptr;
    }
    FARPROC entry = ::GetProcAddress(static_cast<HMODULE>(raw), name);
    // GetProcAddress 给的是函数指针，Loader 的接口是 void*——这对类型上 static_cast
    // 不合法，只有 reinterpret_cast 一条路。写法必须留在**同一行**：clang-format 一旦
    // 把它折行，行尾的 NOLINT 就盖不到诊断所在的那一行（实测：诊断报在 reinterpret_cast
    // 那一行，注释落在下一行 → 抑制失效）。
    return reinterpret_cast<void*>(entry); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

bool Loader::PlatformReopenWritable(const std::filesystem::path& path)
{
    // §8.2 档二 · Windows 主判（辅助地位）：能写开说明没有残留的映射挡着。
    // 0 共享模式 = 独占地要写权限，任何别的持有者都会让它失败。
    // 用 auto* const 而非 const HANDLE：HANDLE 是指针 typedef，const 加在 typedef 上
    // 得到的是 `void* const`，misc-misplaced-const 会报（语义没错，写法有歧义）；
    // 写 auto 又会被 readability-qualified-auto 要求补上那个 `*`。
    auto* const opened =
        ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (opened == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    static_cast<void>(::CloseHandle(opened));
    return true;
}

bool Loader::PlatformMappingRemoved(const std::filesystem::path& path)
{
    // Windows 侧不可观测（§8.2 档二）：FreeLibrary 的返回值不保证卸载干净，
    // 也没有 dl_iterate_phdr 那样的清单可查。故这里恒 false，且不构成判据。
    static_cast<void>(path);
    return false;
}

// NOLINTEND(misc-include-cleaner)

} // namespace vase::detail
