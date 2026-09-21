#pragma once

// 跨动态库的符号可见性。
//
// Windows：符号默认不导出，必须显式 dllexport。
// Linux/macOS：符号默认全部可见，因此要靠 -fvisibility=hidden 收紧（见 CMake 侧
//   的 CXX_VISIBILITY_PRESET），再由 VASE_EXPORT 把该露的放出来。

#ifdef _WIN32
#define VASE_EXPORT __declspec(dllexport)
#define VASE_IMPORT __declspec(dllimport)
#define VASE_HIDDEN
#else
#define VASE_EXPORT __attribute__((visibility("default")))
#define VASE_IMPORT __attribute__((visibility("default")))
#define VASE_HIDDEN __attribute__((visibility("hidden")))
#endif

// 各动态库自己的公开 API：构建该库时导出，其它地方导入。
// 构建宏由 CMake 侧 target_compile_definitions(<target> PRIVATE VASE_<LIB>_BUILD) 提供。
#ifdef VASE_POD_BUILD
#define VASE_POD_API VASE_EXPORT
#else
#define VASE_POD_API VASE_IMPORT
#endif

#ifdef VASE_HOST_BUILD
#define VASE_HOST_API VASE_EXPORT
#else
#define VASE_HOST_API VASE_IMPORT
#endif
