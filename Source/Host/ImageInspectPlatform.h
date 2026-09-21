#pragma once

// Host 内部的镜像解析「桥」——公开头 Vase/Detail/ImageInspect.h 只有纯函数，
// 这里的几个是它们与平台 API（HMODULE / dl_iterate_phdr）的接合点。
//
// **为什么需要这么一份内部头**：ImageInspect{Windows,Posix}.cpp 与 Loader*.cpp
// 是两个 TU，而计划没有给这些跨 TU 的平台函数指定声明落点（它们不该进公开头）。
// 与其在每个调用方重复抄一遍声明，不如落一处。不进 Include/，故不是公开面。

#include "Vase/Detail/ImageInspect.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace vase::detail
{

// 磁盘镜像的原始字节。任何一步失败（不存在、不是普通文件、打不开、读失败）
// 都返回**空 vector**——调用方以空为「拿不到」，不区分成因（§9.2：filesystem
// 一律 error_code 形态，绝不抛）。
[[nodiscard]] std::vector<std::uint8_t> ReadImageFileBytes(const std::filesystem::path& path);

// 档三 · 内存侧身份特征。Windows：由 HMODULE 直接读 PE 调试目录。Linux：按路径
// 在 dl_iterate_phdr 的清单里全等匹配，收 PT_NOTE 字节（raw 不用）。
[[nodiscard]] Result<ImageIdentity> MemoryIdentityPlatform(const void* raw, const std::filesystem::path& path);

// 档三 · 磁盘侧身份特征。两平台都是「读文件字节 → 纯解析」。
[[nodiscard]] Result<ImageIdentity> FileIdentityPlatform(const std::filesystem::path& path);

// 档二 · 仅 Linux：该路径的镜像是否仍在进程的映射清单里（dl_iterate_phdr 条目
// 是否存在）。Windows 侧没有对应实现——那里这不是可观测的量（§8.2 档二）。
[[nodiscard]] bool IsImageMapped(const std::filesystem::path& path);

} // namespace vase::detail
