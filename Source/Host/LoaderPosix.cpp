#include "Vase/Host/Loader.h"

#include "ImageInspectPlatform.h"
#include "Vase/Detail/Result.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <string>

namespace vase::detail
{

Result<void*> Loader::PlatformLoad(const std::filesystem::path& path, std::string& outError)
{
    // RTLD_NOW | RTLD_LOCAL（§8.7）：不给宿主选项——RTLD_GLOBAL 会把插件符号灌进
    // 全局命名空间，多插件反复进出时前后影响不可控。
    void* handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr)
    {
        // dlerror 的「非线程安全」在本项目不成立：§1.4 的绑定线程契约把全部装载/卸载
        // 串行在一根线程上，这里没有并发调用；而取 dlopen 的失败原文除此一家别无 API
        // （自己拼一句就是 §8.2 要的那种「看不出缺哪个依赖」的诊断）。
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        const char* reason = ::dlerror();
        outError = reason == nullptr ? "dlopen failed" : reason;
        return Result<void*>::Err(Error{outError});
    }
    return Result<void*>::Ok(handle);
}

void Loader::PlatformFree(void* raw)
{
    if (raw != nullptr)
    {
        static_cast<void>(::dlclose(raw));
    }
}

void* Loader::PlatformSymbol(void* raw, const char* name)
{
    if (raw == nullptr)
    {
        return nullptr;
    }
    return ::dlsym(raw, name); // dlsym 返回的就是 void*，不需要转换
}

bool Loader::PlatformReopenWritable(const std::filesystem::path& path)
{
    // §8.2 档二 · Linux 侧恒真的那一半：旧 inode 解除链接即可，与是否还映射着无关。
    //
    // open(2) 的**声明**本身就是可变参数（第三个 mode 可选），两参数形态照样落在
    // pro-type-vararg 里。没有等价替代：这里要的是「以写权限真开一次」，而
    // fopen("r+") 额外要求可读、access(W_OK) 只查权限位不真开——两者都改判据语义（§8.2）。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    const int descriptor = ::open(path.c_str(), O_WRONLY);
    if (descriptor < 0)
    {
        return false;
    }
    static_cast<void>(::close(descriptor));
    return true;
}

bool Loader::PlatformMappingRemoved(const std::filesystem::path& path)
{
    // §8.2 档二 · Linux 主判：dl_iterate_phdr 的清单里还有没有它。
    return !IsImageMapped(path);
}

} // namespace vase::detail
