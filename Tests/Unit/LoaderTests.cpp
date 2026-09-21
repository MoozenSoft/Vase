#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cctype> // Windows 分支用 std::tolower
#else
#include <algorithm> // 其余平台分支用 std::ranges::find
#endif

namespace
{

// 构造字节用的写手。计划正文写的是 `b[at] = v` / `b.data() + at`，那两种写法会
// 分别招来 cppcoreguidelines 的「未检查容器访问」与「指针算术」两条（DetailTests
// 里已有一处同类 NOLINT）。这里换成 std::next + memcpy：构造出来的字节完全一致，
// 也不需要抑制——本文件因此一个 NOLINT 都没有。
void Blit(std::vector<std::uint8_t>& bytes, std::size_t at, const void* source, std::size_t length)
{
    std::memcpy(std::next(bytes.data(), static_cast<std::ptrdiff_t>(at)), source, length);
}

// DataDirectory 一项的宽度（Debug 目录是下标 6，导入目录是下标 1）。写成具名 size_t
// 常量：直接写 `6 * 8` 的话乘法在 int 里做、再隐式加宽到 size_t，过不了
// bugprone-implicit-widening-of-multiplication-result。
constexpr std::size_t kDataDirEntry = 8;

void Put8(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint8_t value)
{
    Blit(bytes, at, &value, sizeof(value));
}

void Put16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value)
{
    // 截断本身就是取低 8 位，不必再 & 0xFF——而 `value & 0xFFU` / `value >> 8U` 会让
    // uint16_t 先提升成 int，与无符号字面量做位运算，撞 bugprone-signed-bitwise。
    const std::array<std::uint8_t, 2> raw{
        static_cast<std::uint8_t>(value),
        static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> 8U),
    };
    Blit(bytes, at, raw.data(), raw.size());
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value)
{
    Put16(bytes, at, static_cast<std::uint16_t>(value));
    Put16(bytes, at + 2, static_cast<std::uint16_t>(value >> 16U));
}

void Put64(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint64_t value)
{
    Put32(bytes, at, static_cast<std::uint32_t>(value));
    Put32(bytes, at + 4, static_cast<std::uint32_t>(value >> 32U));
}

void PutStr(std::vector<std::uint8_t>& bytes, std::size_t at, const char* text)
{
    Blit(bytes, at, text, std::strlen(text) + 1);
}

std::vector<std::uint8_t> MakeMinimalPe()
{
    // 磁盘形态：DOS(64) + PE sig + COFF(20) + OptHdr(240, PE32+, Debug@dir[6]=RVA 0x2000,
    // Import@dir[1]=RVA 0x2100) + 1 节表 2 项；.debug→0x400，.rdata→0x500。
    std::vector<std::uint8_t> b(0x600, 0);
    PutStr(b, 0, "MZ");
    Put32(b, 0x3C, 0x40);    // e_lfanew
    PutStr(b, 0x40, "PE\0"); // 0x40: sig(4) + COFF(20) = 0x44..0x58
    Put16(b, 0x44, 0x8664);  // Machine
    Put16(b, 0x46, 2);       // NumberOfSections
    Put16(b, 0x54, 240);     // SizeOfOptionalHeader（COFF 起 0x44，字段 +16）
    Put16(b, 0x58, 0x20B);   // Magic PE32+（可选头起 0x58）
    Put32(b, 0x58 + 56, 0x3000);
    Put32(b, 0x58 + 108, 16); // NumberOfRvaAndSizes
    const std::size_t dirs = 0x58 + 112;
    Put32(b, dirs + (6 * kDataDirEntry), 0x2000);
    Put32(b, dirs + (6 * kDataDirEntry) + 4, 56); // Debug：1 项 28B + 1 null 28B
    Put32(b, dirs + (1 * kDataDirEntry), 0x2100);
    Put32(b, dirs + (1 * kDataDirEntry) + 4, 40); // Import：1 desc + null
    // 节表 @ 0x58+240 = 0x148：两项各 40B，故第二项起 0x148+40 = 0x170。
    // （计划正文这里写的是 0x168——0x148 起两项 40B 的第二项只能在 0x170；
    // 0x168 会让 .rdata 的头 8 字节落进第一项的尾部，解析器按节表换算就找不到
    // 导入表的 RVA 0x2100，实测报 "PE RVA not inside any section"。）
    PutStr(b, 0x148, ".debug"); // VA@+12, SizeOfRawData@+16, PointerToRawData@+20
    Put32(b, 0x148 + 12, 0x2000);
    Put32(b, 0x148 + 16, 0x100);
    Put32(b, 0x148 + 20, 0x400);
    PutStr(b, 0x170, ".rdata");
    Put32(b, 0x170 + 12, 0x2100);
    Put32(b, 0x170 + 16, 0x100);
    Put32(b, 0x170 + 20, 0x500);
    // Debug 目录项 @ 0x400：Type=2@+12, SizeOfData@+16, AddressOfRawData@+20=0x2040, PointerToRawData@+24=0x440
    Put32(b, 0x400 + 12, 2);
    Put32(b, 0x400 + 16, 0x28);
    Put32(b, 0x400 + 20, 0x2040);
    Put32(b, 0x400 + 24, 0x440);
    // RSDS @ 0x440：sig + GUID(0x01..0x10) + Age=2 + PdbPath
    PutStr(b, 0x440, "RSDS");
    for (std::size_t i = 0; i < 16; ++i)
    {
        Put8(b, 0x444 + i, static_cast<std::uint8_t>(i + 1));
    }
    Put32(b, 0x454, 2);
    PutStr(b, 0x458, "probe.pdb");
    // Import desc @ 0x500：Name RVA@+12 = 0x2140 → off 0x540；后跟 20B null 表尾
    Put32(b, 0x500 + 12, 0x2140);
    PutStr(b, 0x540, "sibling.dll");
    return b;
}

std::vector<std::uint8_t> MakeMinimalElf(std::uint64_t buildIdLen = 20)
{
    // e_ident(16) + e_type..(48) = 64；phnum=2（PT_NOTE@0x200, PT_DYNAMIC@0x300）；strtab 在 0x400。
    std::vector<std::uint8_t> b(0x500, 0);
    Put8(b, 0, 0x7F);
    PutStr(b, 1, "ELF"); // "ELF" 三字节 + NUL，正好落在 ident[1..4] 的位置
    Put8(b, 4, 2);
    Put8(b, 5, 1);      // ELFCLASS64, LSB
    Put8(b, 6, 1);      // version
    Put16(b, 16, 3);    // ET_DYN
    Put16(b, 18, 0x3E); // x86-64
    Put32(b, 20, 1);    // e_version
    Put64(b, 32, 64);   // e_phoff
    Put16(b, 52, 64);   // e_ehsize
    Put16(b, 54, 56);   // e_phentsize
    Put16(b, 56, 2);    // e_phnum
    // phdr[0] PT_NOTE @64：type@0=4, offset@8=0x200, vaddr@16, filesz@32, memsz@40
    Put32(b, 64 + 0, 4);
    Put64(b, 64 + 8, 0x200);
    Put64(b, 64 + 16, 0x200);
    const std::size_t noteBytes =
        12 + 4 + static_cast<std::size_t>(buildIdLen) + ((buildIdLen % 4) != 0 ? 4 - (buildIdLen % 4) : 0);
    Put64(b, 64 + 32, noteBytes);
    Put64(b, 64 + 40, noteBytes);
    // note @0x200：namesz=4, descsz, type=3, "GNU\0", desc=0xAA..
    Put32(b, 0x200, 4);
    Put32(b, 0x204, static_cast<std::uint32_t>(buildIdLen));
    Put32(b, 0x208, 3);
    PutStr(b, 0x20C, "GNU");
    for (std::uint64_t i = 0; i < buildIdLen; ++i)
    {
        Put8(b, 0x210 + static_cast<std::size_t>(i), static_cast<std::uint8_t>(0xAA + i));
    }
    // phdr[1] PT_DYNAMIC @120：offset 0x300，4 个 entry（STRTAB→0x400, NEEDED→10, NULL）
    Put32(b, 120 + 0, 2);
    Put64(b, 120 + 8, 0x300);
    Put64(b, 120 + 32, std::uint64_t{4} * 16);
    Put64(b, 0x300 + (0 * 16), 5);
    Put64(b, 0x300 + (0 * 16) + 8, 0x400); // DT_STRTAB
    Put64(b, 0x300 + (1 * 16), 1);
    Put64(b, 0x300 + (1 * 16) + 8, 10); // DT_NEEDED → strtab+10
    Put64(b, 0x300 + (2 * 16), 0);      // DT_NULL
    PutStr(b, 0x400 + 10, "libSibling.so");
    return b;
}

TEST(ImageInspect, PeFileCodeViewExtracted)
{
    auto b = MakeMinimalPe();
    vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParsePeCodeView(b, false);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_EQ(r.Value().Kind, vase::detail::IdentityKind::kPdbCodeView);
    ASSERT_EQ(r.Value().Bytes.size(), 20U); // GUID(16)+Age(4)
    EXPECT_EQ(r.Value().Bytes.front(), 0x01);
    EXPECT_EQ(*std::next(r.Value().Bytes.begin(), 16), 0x02); // Age=2 小端首字节
}

TEST(ImageInspect, PeFileImportsListed)
{
    auto b = MakeMinimalPe();
    vase::Result<std::vector<std::string>> r = vase::detail::ParsePeImports(b, false);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    ASSERT_EQ(r.Value().size(), 1U);
    EXPECT_EQ(r.Value().front(), "sibling.dll");
    // §8.2 缺依赖诊断的入口直接钉住：它只取**第一个**导入条目名，不查磁盘存在性。
    EXPECT_EQ(vase::detail::FirstUnresolvableImport(b, /*isPe=*/true), "sibling.dll");
}

TEST(ImageInspect, PeWithoutCodeViewFailsLouder)
{
    auto b = MakeMinimalPe();
    Put32(b, 0x58 + 112 + (6 * kDataDirEntry), 0);
    Put32(b, 0x58 + 112 + (6 * kDataDirEntry) + 4, 0); // 摘掉 Debug 目录
    const vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParsePeCodeView(b, false);
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("/DEBUG"), std::string::npos); // 指路（§8.2）
}

TEST(ImageInspect, ElfFileBuildIdExtracted)
{
    auto b = MakeMinimalElf();
    vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParseElfBuildIdFile(b);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_EQ(r.Value().Kind, vase::detail::IdentityKind::kElfBuildId);
    ASSERT_EQ(r.Value().Bytes.size(), 20U);
    EXPECT_EQ(*std::next(r.Value().Bytes.begin(), 19), static_cast<std::uint8_t>(0xAA + 19));
}

TEST(ImageInspect, ElfFileNeededListed)
{
    auto b = MakeMinimalElf();
    vase::Result<std::vector<std::string>> r = vase::detail::ParseElfNeededFile(b);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    ASSERT_EQ(r.Value().size(), 1U);
    EXPECT_EQ(r.Value().front(), "libSibling.so");
    // §8.2 缺依赖诊断的入口直接钉住：它只取**第一个** DT_NEEDED 名，不查磁盘存在性。
    EXPECT_EQ(vase::detail::FirstUnresolvableImport(b, /*isPe=*/false), "libSibling.so");
}

TEST(ImageInspect, ElfWithoutBuildIdNoteFailsLouder)
{
    auto b = MakeMinimalElf();
    Put32(b, 64 + 0, 6); // PT_NOTE → PT_PHDR：让解析器见不到 note
    const vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParseElfBuildIdFile(b);
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("--build-id"), std::string::npos); // 指路（§8.2）
}

std::filesystem::path FixturePath(const char* defineValue) { return std::filesystem::path{defineValue}; }

TEST(Loader, EnsureResidentDedupsAndRejectsMissing)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    vase::Result<vase::detail::BinaryRecord*> again = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(again.IsOk());
    EXPECT_EQ(again.Value(), r.Value());         // 复用同一记录（§8.1「确保驻留」）
    EXPECT_EQ(loader.ResidentBinaryCount(), 1U); // 且只触发一次平台加载

    const vase::Result<vase::detail::BinaryRecord*> missing = loader.EnsureResident("this-binary-does-not-exist.vase");
    ASSERT_FALSE(missing.IsOk());
    EXPECT_NE(missing.GetError().Message().find("does-not-exist"), std::string::npos);
    loader.Unload(*r.Value());
}

TEST(Loader, SymbolResolvesDescriptorEntry)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    // §8.1 的唯一加载路径第一跳：extern "C" 入口符号必须能按名取到。
    EXPECT_TRUE(vase::detail::Loader::Symbol(*r.Value(), "VasePlugin_GetPlugin").IsOk());
    EXPECT_FALSE(vase::detail::Loader::Symbol(*r.Value(), "VasePlugin_NoSuchEntry").IsOk());
    loader.Unload(*r.Value());
}

TEST(Loader, MemoryIdentityMatchesFileIdentity)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    // 档三原语的最小成立条件：同一文件的内存特征 == 磁盘特征（T11/T13 的
    // 「驻留复用也要比对」「换文件即变脸」全部建在这一条上）。
    vase::Result<vase::detail::ImageIdentity> m = vase::detail::Loader::MemoryIdentity(*r.Value());
    ASSERT_TRUE(m.IsOk()) << m.GetError().Message();
    vase::Result<vase::detail::ImageIdentity> f =
        vase::detail::Loader::FileIdentity(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(f.IsOk()) << f.GetError().Message();
    EXPECT_EQ(m.Value(), f.Value());
    loader.Unload(*r.Value());
}

TEST(Loader, ProbeImportsVasePod)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    vase::Result<std::vector<std::string>> names = vase::detail::Loader::ImportedLibraryNamesFromFile(r.Value()->Path);
    ASSERT_TRUE(names.IsOk()) << names.GetError().Message();
#ifdef _WIN32
    // 计划原文这里写的是 expected = "VasePod.dll"，却拿**已小写化**的 lowered 去比——
    // 那一比恒假（"vasepod.dll" != "VasePod.dll"），断言无从成立。expected 直接取小写形。
    const std::string expected = "vasepod.dll";
    const std::string found = [&]
    {
        for (const auto& n : names.Value())
        {
            if (n.size() == expected.size())
            {
                std::string lowered = n;
                for (auto& c : lowered)
                {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (lowered == expected)
                {
                    return lowered;
                }
            }
        }
        return std::string{};
    }(); // PE 导入表保留创建时大小写，但链接器来源不一——大小写不敏感匹配（§8.7 执法用文件名主干比对，同口径）
#else
    const std::string expected = "libVasePod.so";
    const std::string found =
        std::ranges::find(names.Value(), expected) != names.Value().end() ? expected : std::string{};
#endif
    EXPECT_FALSE(found.empty()); // 解析器在真实产物上工作，不只在构造字节上
    loader.Unload(*r.Value());
}

TEST(Loader, UnloadEvidencePerPlatform)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_UNLOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    const vase::detail::BinaryRecord copy = *r.Value(); // 先复制：Unload 会摘表（r.Value() 失效）
    const vase::detail::UnloadEvidence ev = loader.Unload(copy);
    EXPECT_EQ(loader.ResidentBinaryCount(), 0U);
#ifdef _WIN32
    EXPECT_TRUE(ev.ReopenWritableIsMeaningful);
    EXPECT_TRUE(ev.ReopenWritable);              // 档二 · Win：FreeLibrary 后文件必须可写开（辅助判据，§8.2）
    EXPECT_FALSE(ev.MappingRemovalIsObservable); // Win 上无判据力的那个字段照实为假
#else
    EXPECT_TRUE(ev.MappingRemovalIsObservable);
    EXPECT_TRUE(ev.MappingRemoved);              // 档二 · Linux：dl_iterate_phdr 条目消失（主判之一）
    EXPECT_FALSE(ev.ReopenWritableIsMeaningful); // Linux 上无判据力的那个字段照实为假
#endif
}

} // namespace
