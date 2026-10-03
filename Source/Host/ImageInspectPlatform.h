#pragma once

// Host 内部的镜像解析「桥」——公开头 Vase/Detail/ImageInspect.h 只有纯函数，
// 这里的几个是它们与平台 API（HMODULE / dl_iterate_phdr / _dyld_*）的接合点。
//
// **为什么需要这么一份内部头**：ImageInspect{Windows,Linux,Darwin}.cpp 与 Loader*.cpp
// 是两个 TU，而计划没有给这些跨 TU 的平台函数指定声明落点（它们不该进公开头）。
// 与其在每个调用方重复抄一遍声明，不如落一处。不进 Include/，故不是公开面。

#include "Vase/Detail/ImageInspect.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace vase::detail
{

// 磁盘镜像的原始字节。任何一步失败（不存在、不是普通文件、打不开、读失败）
// 都返回**空 vector**——调用方以空为「拿不到」，不区分成因（§9.2：filesystem
// 一律 error_code 形态，绝不抛）。
[[nodiscard]] std::vector<std::uint8_t> ReadImageFileBytes(const std::filesystem::path& path);

// 档三 · 内存侧身份特征。Windows：由 HMODULE 直接读 PE 调试目录。Linux：按路径在
// dl_iterate_phdr 的清单里全等匹配，收 PT_NOTE 字节；macOS：_dyld_image_count 枚举、
// 路径先规范化（两者 raw 都不用）。
[[nodiscard]] Result<ImageIdentity> MemoryIdentityPlatform(const void* raw, const std::filesystem::path& path);

// 档三 · 磁盘侧身份特征。三平台都是「读文件字节 → 纯解析」（macOS 走 Mach-O）。
[[nodiscard]] Result<ImageIdentity> FileIdentityPlatform(const std::filesystem::path& path);

// 档二 · Linux 与 macOS，各自机制不同：该路径的镜像是否仍在进程的映射清单里
// （Linux：dl_iterate_phdr 条目是否存在；macOS：_dyld_image_count 枚举 + 规范化路径全等）。
// Windows 侧没有对应实现——那里这不是可观测的量（§8.2 档二）。
[[nodiscard]] bool IsImageMapped(const std::filesystem::path& path);

// 当前平台的镜像格式——格式知识只此一处（D174）。三个平台条件与
// Source/Host/CMakeLists.txt 的三分支一一对位。
[[nodiscard]] ImageFormat PlatformImageFormat();

// 按当前平台读「导入表 / 依赖名」。存在理由：让 Loader.cpp 不再需要平台分支（D174）。
[[nodiscard]] Result<std::vector<std::string>> ParseImportedLibraryNames(std::span<const std::uint8_t> fileBytes);

} // namespace vase::detail
