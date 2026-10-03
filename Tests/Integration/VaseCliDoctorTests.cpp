// doctor 收窄四项（M6/D143–D155）：①磁盘读身份、②装载读版号（D152 置后）、③写探针、④平台锁。
// doctor 只判环境事实，不比对清单↔描述符（D155）——manifest 的 Id 与描述符不同不报错，是特性不是漏。
#include "Doctor.h"

#include "Cli.h"
#include "Vase/Catalog/LibraryFileName.h"

#include "CatalogSandbox.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <iterator>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <cstring>

// NOLINTBEGIN(misc-include-cleaner) windows.h 是系统侧伞形头、符号实体散在各子头——
// 与 Tools/VaseCli/Doctor.cpp 头部同机制；末行的 #undef 是在拆它带进的 CopyFile→CopyFileA
// 宏（不拆会吃掉 CatalogSandbox::CopyFile）。
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef CopyFile
// NOLINTEND(misc-include-cleaner)
#else
#include <system_error> // error_code overload：③ 的 chmod 证人支两处
#include <unistd.h>     // geteuid：root 跑测试时 DAC 探针不咬，如实跳过（风险 2 处置）
#endif

namespace
{

using testing_support::CatalogSandbox;

void Stage(const CatalogSandbox& sandbox, const std::string& subdirectory, const std::string& stem,
           const char* fixturePath, const std::string& id)
{
    sandbox.CopyFile(subdirectory + "/" + vase::catalog_detail::LibraryFileName(stem), fixturePath);
    sandbox.WriteFile(subdirectory + "/plugin.json", std::string{
                                                         "{\n    \"schemaVersion\": 1, \"id\": \"" + id +
                                                             "\", \"displayName\": \"诊断材料\", \"version\": "
                                                             "\"0.0.1\", \"binary\": \"" +
                                                             stem + "\"\n}",
                                                     });
}

#ifdef _WIN32
// 目标平台（x64 / arm64）都是小端：整段搬字节，避开移位带符号形状。
std::uint32_t ReadLeU32(const std::string& bytes, std::size_t index)
{
    std::uint32_t value = 0;
    std::memcpy(&value, &*std::next(bytes.begin(), static_cast<std::ptrdiff_t>(index)), sizeof(value));
    return value;
}
#endif

// 翻掉 machine 字段后写回；三平台头部 sanity 不符即响亮失败（staging 缺料当场响，CopyFile 同形）。
void FlipMachineField(const std::filesystem::path& binary)
{
    std::string bytes;
    {
        std::ifstream in(binary, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    if (bytes.size() < 64U)
    {
        ADD_FAILURE() << "staged binary too small: " << binary;
        return;
    }
    const auto at = [&bytes](std::size_t index)
    { return static_cast<std::uint8_t>(*std::next(bytes.begin(), static_cast<std::ptrdiff_t>(index))); };
#ifdef _WIN32
    const std::uint32_t lfanew = ReadLeU32(bytes, 60U); // e_lfanew：指向 "PE\0\0" 签名
    if (at(0U) != 'M' || at(1U) != 'Z' || lfanew >= bytes.size() || lfanew + 6U > bytes.size() ||
        at(static_cast<std::size_t>(lfanew)) != 'P')
    {
        ADD_FAILURE() << "not a parsable PE header in " << binary;
        return;
    }
    const std::ptrdiff_t machine = static_cast<std::ptrdiff_t>(lfanew) + 4; // COFF 头前两字节
    *std::next(bytes.begin(), machine) = static_cast<char>(0x4E);           // IMAGE_FILE_MACHINE_I386
    *std::next(bytes.begin(), machine + 1) = static_cast<char>(0x01);
#elif defined(__APPLE__)
    if (at(0U) != 0xCFU || at(1U) != 0xFAU || at(2U) != 0xEDU || at(3U) != 0xFEU)
    {
        ADD_FAILURE() << "not a parsable Mach-O header in " << binary;
        return;
    }
    // 翻的是**装载器认得的字段**（cputype@4），不是身份特征——本函数服务于
    // UnloadableBinaryIsNamedByCheckTwo：要让 ② 红、① 照常绿（identity ok）。
    // 翻成 CPU_TYPE_I386(7)：macOS 14 已无 32 位，dyld 以「不支持的架构」拒收。
    *std::next(bytes.begin(), 4) = static_cast<char>(0x07);
    *std::next(bytes.begin(), 5) = static_cast<char>(0x00);
    *std::next(bytes.begin(), 6) = static_cast<char>(0x00);
    *std::next(bytes.begin(), 7) = static_cast<char>(0x00);
#else
    if (at(0U) != 0x7FU || at(1U) != 'E' || at(2U) != 'L' || at(3U) != 'F')
    {
        ADD_FAILURE() << "not a parsable ELF header in " << binary;
        return;
    }
    *std::next(bytes.begin(), 18) = static_cast<char>(0x03); // e_machine = EM_386（ELF64 偏移固定）
    *std::next(bytes.begin(), 19) = static_cast<char>(0x00);
#endif
    std::ofstream out(binary, std::ios::binary); // 默认即 trunc，不带默认 openmode 相
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out.good())
    {
        ADD_FAILURE() << "cannot write tampered binary " << binary;
    }
}

TEST(VaseCliDoctor, HealthyTreePassesAllChecks)
{
    const CatalogSandbox sandbox("doctor-ok");
    Stage(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE, "Vase.Probe");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Probe: identity ok"), std::string::npos);
    EXPECT_NE(out.str().find("host header version"), std::string::npos); // D153 的节头 info
    EXPECT_NE(out.str().find("HeaderVersion matches (1 descriptor(s))"), std::string::npos);
    EXPECT_NE(out.str().find("doctor: ok"), std::string::npos);
}

TEST(VaseCliDoctor, MissingIdentityFeatureFailsCheckOne)
{
    const CatalogSandbox sandbox("doctor-noident");
    // NoIdentityPlugin 三平台各摘各的身份特征（M4/D108，macOS 支 D167）——①的负例自此三侧同构成立。
    Stage(sandbox, "NoId", "NoIdentityPlugin", VASE_FIXTURE_NOIDENTITY, "Vase.NoId");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("check 1"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.NoId"), std::string::npos);
    EXPECT_NE(out.str().find("tier-3 adopt would reject"), std::string::npos);        // 后果指针（D143①）
    EXPECT_NE(out.str().find("Vase.NoId: HeaderVersion matches"), std::string::npos); // ②照常——两项独立
}

TEST(VaseCliDoctor, MissingEnumerationEntryIsNamedByCheckTwo)
{
    const CatalogSandbox sandbox("doctor-noenum");
    Stage(sandbox, "NoEnum", "NoEnumeration", VASE_FIXTURE_NOENUMERATION, "Vase.NoEnum");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("no VasePlugin_Descriptors entry"), std::string::npos); // D118 文本零新造
}

TEST(VaseCliDoctor, NewerHeaderVersionIsNamedByCheckTwo)
{
    const CatalogSandbox sandbox("doctor-stale");
    Stage(sandbox, "Stale", "StaleEnum", VASE_FIXTURE_STALEENUM, "Vase.Stale"); // 唯一变量 k+1（D123 探针）

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("HeaderVersion mismatch: binary 5, host 4"), std::string::npos);
}

TEST(VaseCliDoctor, UnloadableBinaryIsNamedByCheckTwo)
{
    const CatalogSandbox sandbox("doctor-badload");
    // 计划素材换（plan 偏离登记）：FailingLoadPlugin 坏在 OnLoad——工具层装载读（EnsureResident+
    // InspectDescriptors）走不到它，其 DLL 平台装载正常。②「可装载性」的真证人改翻 machine 字段。
    Stage(sandbox, "BadLoad", "LoadProbe", VASE_FIXTURE_LOADPROBE, "Vase.BadLoad");
    FlipMachineField(sandbox.Root / "BadLoad" / vase::catalog_detail::LibraryFileName("LoadProbe"));

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("failed to load binary"), std::string::npos);     // D153：可装载性归 ②
    EXPECT_NE(out.str().find("Vase.BadLoad: identity ok"), std::string::npos); // ①独立：文件在、特征在
}

TEST(VaseCliDoctor, TwoFailuresAreReportedIndependently)
{
    const CatalogSandbox sandbox("doctor-two");
    Stage(sandbox, "NoId", "NoIdentityPlugin", VASE_FIXTURE_NOIDENTITY, "Vase.NoId");
    Stage(sandbox, "Stale", "StaleEnum", VASE_FIXTURE_STALEENUM, "Vase.Stale");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("doctor: FAIL (checks 1 and 2 reported failures)"), std::string::npos); // 不遇错即停
}

TEST(VaseCliDoctor, ResidentMappingProbeIsPlatformAsymmetric)
{
    const CatalogSandbox sandbox("doctor-lock");
    Stage(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE, "Vase.Probe");
#ifdef _WIN32
    // ④ 的契约语义=「换件链下一步能否落新字节」，是写问题（M6-T4b 裁定：探针改写开，此支复活）。
    // LoadLibraryW 是对位现象：映射只挡写不挡读（首测实测 gle=0/32）——①②照常、单点归因只剩 ④。
    const std::filesystem::path binary = sandbox.Root / "Probe" / vase::catalog_detail::LibraryFileName("LoadProbe");
    // NOLINTBEGIN(misc-include-cleaner) HMODULE/LoadLibraryW/FreeLibrary 伞形头之外无直接提供者——同 Doctor.cpp 探针区。
    const HMODULE held = ::LoadLibraryW(binary.c_str());
    ASSERT_NE(held, nullptr) << "LoadLibrary setup failed — environment surprise, not a verdict";
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("doctor: FAIL (checks 4 reported failures)"), std::string::npos); // 单点归因：只有 ④
    EXPECT_NE(out.str().find("locked — sharing violation"), std::string::npos);
    ::FreeLibrary(held); // 尽力还原；Windows 下 kept-resident 时 temp 残留归 sandbox 的尽力清理
    // NOLINTEND(misc-include-cleaner)
#else
    // 正半句：POSIX（Linux/macOS）上锁不可探测——info 行逐字钉、且**不拖退出码**（D144④）。
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("lock probe: not observable on this platform"), std::string::npos);
#endif
}

TEST(VaseCliDoctor, UnwritableSubdirectoryFailsCheckThree)
{
    const CatalogSandbox sandbox("doctor-writable");
    Stage(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE, "Vase.Probe");
#ifdef _WIN32
    // 首测（spec §6 风险 2）在本机证伪：DELETE_ON_CLOSE + BACKUP_SEMANTICS 目录句柄持有期间（测得句柄有效、
    // 关闭后目录真删），③ 的建文件与删文件照过——delete-pending 不挡目录内新建，非实现缺陷。详 plan 偏离登记 T4。
    GTEST_SKIP() << "platform probe negative: D143③ witness degraded to contract-only";
#else
    const std::filesystem::path sub = sandbox.Root / "Probe";
    if (geteuid() == 0)
    {
        GTEST_SKIP() << "running as root: DAC permissions would not bind the probe";
    }
    std::error_code ec;
    std::filesystem::permissions(sub,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::replace, ec);
    ASSERT_FALSE(ec) << "chmod setup failed: " << ec.message();
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("check 3"), std::string::npos);
    EXPECT_NE(out.str().find("Probe: FAIL"), std::string::npos);
    std::filesystem::permissions(sub, std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ec);
    ASSERT_FALSE(ec) << "permission restore failed: " << ec.message(); // 不恢复会绊住沙箱析构与后续跑
#endif
}

} // namespace
