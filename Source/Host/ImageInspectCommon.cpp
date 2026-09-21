#include "Vase/Detail/ImageInspect.h"

#include "ImageInspectPlatform.h"
#include "Vase/Detail/Result.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace vase::detail
{
namespace
{

// ——— 边界与读取 ———
//
// 纪律：每个取字节前都先 InBounds，查不过就 Err，绝不越界读。整数一律 memcpy 进
// 本地变量——非对齐安全（PE / ELF 的字段本来就不保证对齐），且两平台同码。
// 目标矩阵（x64 / arm64）全是小端，故「按本机序读」就是 LE 读法，不需要逐字节拼。

[[nodiscard]] bool InBounds(std::size_t offset, std::size_t length, std::size_t total)
{
    return offset <= total && length <= total - offset;
}

[[nodiscard]] std::uint8_t ReadU8(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint8_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] std::uint16_t ReadU16(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint16_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] std::uint32_t ReadU32(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] std::uint64_t ReadU64(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

// 读一条 NUL 结尾的字符串；扫到镜像末尾仍没有 NUL 就是 Err（不越界、不猜）。
[[nodiscard]] Result<std::string> ReadNulTerminated(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::string text;
    for (std::size_t index = at; index < bytes.size(); ++index)
    {
        const std::uint8_t value = ReadU8(bytes, index);
        if (value == 0U)
        {
            return Result<std::string>::Ok(std::move(text));
        }
        text.push_back(static_cast<char>(value));
    }
    return Result<std::string>::Err(Error{"unterminated string in image"});
}

[[nodiscard]] Error MissingCodeViewError()
{
    return Error{
        "no CodeView debug entry — 插件构建缺 /DEBUG:FULL（见 Cmake/Toolchains/windows-x64-*.cmake，§8.2 档三）"};
}

[[nodiscard]] Error MissingBuildIdError()
{
    return Error{"no NT_GNU_BUILD_ID note — 插件构建缺 -Wl,--build-id（见 "
                 "Cmake/Toolchains/linux-x64-clang-libcxx.cmake，§8.2 档三）"};
}

// ——— PE ———
//
// 字段偏移全部按 IMAGE_DOS_HEADER / IMAGE_FILE_HEADER / IMAGE_OPTIONAL_HEADER
// （PE32 与 PE32+ 两套）的公开布局；每一个数都在这里写死，不靠推导。

constexpr std::uint32_t kPeSignature = 0x00004550U;   // "PE\0\0"
constexpr std::uint32_t kRsdsSignature = 0x53445352U; // "RSDS"
constexpr std::uint16_t kPe32Magic = 0x010BU;
constexpr std::uint16_t kPe32PlusMagic = 0x020BU;

constexpr std::size_t kDosLfanewOffset = 0x3CU; // e_lfanew
constexpr std::size_t kCoffHeaderSize = 20U;
constexpr std::size_t kSectionHeaderSize = 40U;
constexpr std::size_t kDataDirectoryEntrySize = 8U;
constexpr std::size_t kDebugDirectoryEntrySize = 28U;
constexpr std::size_t kImportDescriptorSize = 20U;
constexpr std::size_t kCodeViewGuidAgeSize = 20U; // GUID(16) + Age(4)

// 调试目录与导入目录在 DataDirectory 里的下标（winnt.h 的 IMAGE_DIRECTORY_ENTRY_*）。
constexpr std::size_t kImportDirectoryIndex = 1U;
constexpr std::size_t kDebugDirectoryIndex = 6U;
constexpr std::uint32_t kDebugTypeCodeView = 2U;

// DataDirectory[0] 相对 OptionalHeader 起点的偏移：PE32 与 PE32+ 不同（标准布局）。
constexpr std::size_t kPe32DataDirectoryOffset = 96U;
constexpr std::size_t kPe32PlusDataDirectoryOffset = 112U;

// 节表项内的字段偏移。
constexpr std::size_t kSectionVirtualAddressOffset = 12U;
constexpr std::size_t kSectionRawSizeOffset = 16U;
constexpr std::size_t kSectionRawPointerOffset = 20U;

// 调试目录项内的字段偏移。
constexpr std::size_t kDebugEntryTypeOffset = 12U;
constexpr std::size_t kDebugEntryAddressOffset = 20U;
constexpr std::size_t kDebugEntryPointerOffset = 24U;

// 导入描述符内的字段偏移。
constexpr std::size_t kImportNameOffset = 12U;
constexpr std::size_t kImportFirstThunkOffset = 16U;

struct PeHeaderInfo
{
    std::size_t DataDirectory = 0;
    std::size_t SectionTable = 0;
    std::uint16_t SectionCount = 0;
};

struct DataDirectoryEntry
{
    std::uint32_t Rva = 0;
    std::uint32_t Size = 0;
};

[[nodiscard]] Result<PeHeaderInfo> ParsePeHeaders(std::span<const std::uint8_t> image)
{
    using Headers = Result<PeHeaderInfo>;

    if (!InBounds(kDosLfanewOffset, sizeof(std::uint32_t), image.size()))
    {
        return Headers::Err(Error{"not a PE image"});
    }
    const std::size_t peOffset = ReadU32(image, kDosLfanewOffset);
    if (!InBounds(peOffset, sizeof(std::uint32_t) + kCoffHeaderSize, image.size()) ||
        ReadU32(image, peOffset) != kPeSignature)
    {
        return Headers::Err(Error{"not a PE image"});
    }

    // COFF 头紧跟签名：NumberOfSections@+2、SizeOfOptionalHeader@+16。
    const std::size_t coffOffset = peOffset + sizeof(std::uint32_t);
    const std::size_t optionalHeader = coffOffset + kCoffHeaderSize;
    if (!InBounds(optionalHeader, 2U, image.size()))
    {
        return Headers::Err(Error{"not a PE image"});
    }
    const std::uint16_t magic = ReadU16(image, optionalHeader);
    const bool is64Bit = magic == kPe32PlusMagic;
    if (magic != kPe32Magic && !is64Bit)
    {
        return Headers::Err(Error{"not a PE image"});
    }

    PeHeaderInfo info;
    info.SectionCount = ReadU16(image, coffOffset + 2U);
    info.DataDirectory = optionalHeader + (is64Bit ? kPe32PlusDataDirectoryOffset : kPe32DataDirectoryOffset);
    info.SectionTable = optionalHeader + ReadU16(image, coffOffset + 16U);
    return Headers::Ok(info);
}

[[nodiscard]] DataDirectoryEntry ReadDataDirectoryEntry(std::span<const std::uint8_t> image, const PeHeaderInfo& info,
                                                        std::size_t index)
{
    const std::size_t at = info.DataDirectory + (index * kDataDirectoryEntrySize);
    if (!InBounds(at, kDataDirectoryEntrySize, image.size()))
    {
        return {};
    }
    return DataDirectoryEntry{.Rva = ReadU32(image, at), .Size = ReadU32(image, at + 4U)};
}

// RVA → 镜像内偏移。已映射时 RVA 就是偏移（span 起点即 HMODULE，重定位不改变
// 这一点）；磁盘字节则要经节表换算——只有落在某个节里的 RVA 才在文件中有字节。
[[nodiscard]] Result<std::size_t> PeRvaToOffset(std::span<const std::uint8_t> image, const PeHeaderInfo& info,
                                                std::uint32_t rva, bool loadedInMemory)
{
    using Offset = Result<std::size_t>;

    if (loadedInMemory)
    {
        if (!InBounds(rva, 1U, image.size()))
        {
            return Offset::Err(Error{"PE RVA outside the mapped image"});
        }
        return Offset::Ok(rva);
    }

    for (std::size_t index = 0; index < info.SectionCount; ++index)
    {
        const std::size_t section = info.SectionTable + (index * kSectionHeaderSize);
        if (!InBounds(section, kSectionHeaderSize, image.size()))
        {
            break;
        }
        const std::uint32_t virtualAddress = ReadU32(image, section + kSectionVirtualAddressOffset);
        const std::uint32_t rawSize = ReadU32(image, section + kSectionRawSizeOffset);
        if (rva < virtualAddress || rva - virtualAddress >= rawSize)
        {
            continue;
        }
        const std::size_t offset =
            static_cast<std::size_t>(ReadU32(image, section + kSectionRawPointerOffset)) + (rva - virtualAddress);
        if (!InBounds(offset, 1U, image.size()))
        {
            return Offset::Err(Error{"PE RVA outside the image"});
        }
        return Offset::Ok(offset);
    }
    return Offset::Err(Error{"PE RVA not inside any section"});
}

[[nodiscard]] Result<ImageIdentity> ReadCodeViewRecord(std::span<const std::uint8_t> image, std::size_t at)
{
    using Identity = Result<ImageIdentity>;

    if (!InBounds(at, sizeof(std::uint32_t) + kCodeViewGuidAgeSize, image.size()) ||
        ReadU32(image, at) != kRsdsSignature)
    {
        return Identity::Err(MissingCodeViewError());
    }
    const std::span<const std::uint8_t> guidAge = image.subspan(at + sizeof(std::uint32_t), kCodeViewGuidAgeSize);
    ImageIdentity identity;
    identity.Kind = IdentityKind::kPdbCodeView;
    identity.Bytes.assign(guidAge.begin(), guidAge.end());
    return Identity::Ok(std::move(identity));
}

// ——— ELF ———

constexpr std::uint32_t kElfMagic = 0x464C457FU; // 0x7F 'E' 'L' 'F'
constexpr std::size_t kElfClassOffset = 4U;
constexpr std::size_t kElfDataOffset = 5U;
constexpr std::uint8_t kElfClass64 = 2U;
constexpr std::uint8_t kElfDataLsb = 1U;

// ELF64 头字段偏移。**注意 e_phoff 在 0x20 而不是 0x1C**——0x1C 是 ELF32 的位置，
// 计划正文那处笔误按实测的 ELF64 布局写（测试构造字节用的也是 0x20）。
constexpr std::size_t kElfPhnumOffset = 0x38U;
constexpr std::size_t kElfPhentsizeOffset = 0x36U;
constexpr std::size_t kElfPhoffOffset = 0x20U;

// 本文件真正读到的 ELF64 头部字段的**最高端**：e_phoff@0x20(8) → 0x28、
// e_phentsize@0x36(2) → 0x38、e_phnum@0x38(2) → 0x3A。
//
// **别拿 kElfProgramHeaderSize（56）当头部大小**——那是**程序头条目**的大小，两者
// 数值恰好都在 0x36~0x3A 附近，很容易被当成同一个量。混用的后果是实的：56 或 57
// 字节的 span 能过那道守卫，却在 `ReadU16(image, 0x38)` 的 `subspan(56, 2)` 上违反
// 前置条件——Release 下越界读，MSVC debug 下 `_STL_VERIFY` 直接终止进程
// （`_HAS_EXCEPTIONS=0`，没有可接住的东西）。这条路径**从磁盘字节进入**
// （FileIdentity / ImportedLibraryNamesFromFile / DescribeLoadFailure），
// 一个被截断到恰好 56–57 字节、却带合法 `\x7FELF` 前缀的 .so 就能触发。
constexpr std::size_t kElfHeaderFieldsSize = 0x3AU;

constexpr std::size_t kElfProgramHeaderSize = 56U;
constexpr std::size_t kDynamicEntrySize = 16U;

// 程序头内的字段偏移。
constexpr std::size_t kPheaderOffsetOffset = 8U;
constexpr std::size_t kPheaderVaddrOffset = 16U;
constexpr std::size_t kPheaderFileSizeOffset = 32U;
constexpr std::size_t kPheaderMemSizeOffset = 40U;

constexpr std::uint32_t kPtLoad = 1U;
constexpr std::uint32_t kPtDynamic = 2U;
constexpr std::uint32_t kPtNote = 4U;

constexpr std::uint32_t kDtNull = 0U;
constexpr std::uint32_t kDtNeeded = 1U;
constexpr std::uint32_t kDtStrtab = 5U;

constexpr std::uint32_t kNtGnuBuildId = 3U;
constexpr std::size_t kElfNoteHeaderSize = 12U;

struct ElfProgramHeader
{
    std::uint32_t Type = 0;
    std::uint64_t Offset = 0;
    std::uint64_t Vaddr = 0;
    std::uint64_t FileSize = 0;
    std::uint64_t MemSize = 0;
};

// notes 里的 name / desc 都按 4 字节对齐（ELF 的 note 布局）。
[[nodiscard]] std::size_t AlignUp4(std::size_t value) { return ((value + 3U) / 4U) * 4U; }

[[nodiscard]] Result<std::vector<ElfProgramHeader>> ParseElfProgramHeaders(std::span<const std::uint8_t> image)
{
    using Headers = Result<std::vector<ElfProgramHeader>>;

    if (image.size() < kElfHeaderFieldsSize || ReadU32(image, 0) != kElfMagic)
    {
        return Headers::Err(Error{"not an ELF image"});
    }
    if (ReadU8(image, kElfClassOffset) != kElfClass64 || ReadU8(image, kElfDataOffset) != kElfDataLsb)
    {
        return Headers::Err(Error{"not a 64-bit little-endian ELF image"});
    }

    const auto phoff = static_cast<std::size_t>(ReadU64(image, kElfPhoffOffset));
    const std::uint16_t phentsize = ReadU16(image, kElfPhentsizeOffset);
    const std::uint16_t phnum = ReadU16(image, kElfPhnumOffset);
    if (phentsize < kElfProgramHeaderSize)
    {
        return Headers::Err(Error{"ELF program header entry too small"});
    }

    std::vector<ElfProgramHeader> headers;
    headers.reserve(phnum);
    for (std::uint16_t index = 0; index < phnum; ++index)
    {
        const std::size_t at = phoff + (static_cast<std::size_t>(index) * phentsize);
        if (!InBounds(at, kElfProgramHeaderSize, image.size()))
        {
            return Headers::Err(Error{"ELF program header outside the image"});
        }
        ElfProgramHeader header;
        header.Type = ReadU32(image, at);
        header.Offset = ReadU64(image, at + kPheaderOffsetOffset);
        header.Vaddr = ReadU64(image, at + kPheaderVaddrOffset);
        header.FileSize = ReadU64(image, at + kPheaderFileSizeOffset);
        header.MemSize = ReadU64(image, at + kPheaderMemSizeOffset);
        headers.push_back(header);
    }
    return Headers::Ok(std::move(headers));
}

// vaddr → 文件偏移：先按 PT_LOAD 的段平移表换算；没有段能解释时按 PIE 的恒等映射
// （p_vaddr == p_offset，链接器默认构建即如此），但必须仍落在镜像内。
[[nodiscard]] Result<std::size_t> ElfFileOffset(std::span<const std::uint8_t> image,
                                                const std::vector<ElfProgramHeader>& headers, std::uint64_t vaddr)
{
    using Offset = Result<std::size_t>;

    for (const ElfProgramHeader& header : headers)
    {
        if (header.Type != kPtLoad || header.FileSize == 0U)
        {
            continue;
        }
        if (vaddr >= header.Vaddr && vaddr - header.Vaddr < header.FileSize)
        {
            return Offset::Ok(static_cast<std::size_t>(header.Offset + (vaddr - header.Vaddr)));
        }
    }
    if (vaddr < image.size())
    {
        return Offset::Ok(static_cast<std::size_t>(vaddr));
    }
    return Offset::Err(Error{"ELF vaddr not mapped to any file offset"});
}

[[nodiscard]] bool IsGnuNoteName(std::span<const std::uint8_t> notes, std::size_t at)
{
    return ReadU8(notes, at) == static_cast<std::uint8_t>('G') &&
           ReadU8(notes, at + 1U) == static_cast<std::uint8_t>('N') &&
           ReadU8(notes, at + 2U) == static_cast<std::uint8_t>('U') && ReadU8(notes, at + 3U) == 0U;
}

} // namespace

std::vector<std::uint8_t> ReadImageFileBytes(const std::filesystem::path& path)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec)
    {
        return {};
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return {};
    }
    // 走 istreambuf_iterator 而不是 ifstream::read：后者要 char*，为取一个字节指针
    // 去 reinterpret_cast 不值当（那会招来 pro-type-reinterpret-cast）。镜像不大，
    // 逐字节抓的代价可以接受。
    std::vector<std::uint8_t> bytes;
    const std::istreambuf_iterator<char> begin{stream};
    const std::istreambuf_iterator<char> end;
    bytes.assign(begin, end);
    return bytes;
}

Result<ImageIdentity> ParsePeCodeView(std::span<const std::uint8_t> image, bool loadedInMemory)
{
    using Identity = Result<ImageIdentity>;

    const Result<PeHeaderInfo> headers = ParsePeHeaders(image);
    if (!headers.IsOk())
    {
        return Identity::Err(headers.GetError());
    }
    const PeHeaderInfo& info = headers.Value();

    const DataDirectoryEntry debug = ReadDataDirectoryEntry(image, info, kDebugDirectoryIndex);
    if (debug.Rva == 0U || debug.Size == 0U)
    {
        return Identity::Err(MissingCodeViewError());
    }
    const Result<std::size_t> directory = PeRvaToOffset(image, info, debug.Rva, loadedInMemory);
    if (!directory.IsOk())
    {
        return Identity::Err(MissingCodeViewError());
    }

    const std::size_t count = debug.Size / kDebugDirectoryEntrySize;
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::size_t entry = directory.Value() + (index * kDebugDirectoryEntrySize);
        if (!InBounds(entry, kDebugDirectoryEntrySize, image.size()))
        {
            break;
        }
        if (ReadU32(image, entry + kDebugEntryTypeOffset) != kDebugTypeCodeView)
        {
            continue;
        }
        // 已映射时读 AddressOfRawData（RVA），磁盘字节读 PointerToRawData（文件偏移）--
        // 后者免去一次节表换算，也让「文件里到底存了什么」更直白。
        const std::uint32_t inMemory = ReadU32(image, entry + kDebugEntryAddressOffset);
        const std::uint32_t onDisk = ReadU32(image, entry + kDebugEntryPointerOffset);
        const std::uint32_t where = loadedInMemory ? inMemory : onDisk;
        if (where == 0U)
        {
            continue;
        }
        return ReadCodeViewRecord(image, where);
    }
    return Identity::Err(MissingCodeViewError());
}

Result<std::vector<std::string>> ParsePeImports(std::span<const std::uint8_t> image, bool loadedInMemory)
{
    using Names = Result<std::vector<std::string>>;

    const Result<PeHeaderInfo> headers = ParsePeHeaders(image);
    if (!headers.IsOk())
    {
        return Names::Err(headers.GetError());
    }
    const PeHeaderInfo& info = headers.Value();

    std::vector<std::string> names;
    const DataDirectoryEntry imports = ReadDataDirectoryEntry(image, info, kImportDirectoryIndex);
    if (imports.Rva == 0U || imports.Size == 0U)
    {
        return Names::Ok(std::move(names)); // 没有导入表不是错误
    }
    const Result<std::size_t> descriptors = PeRvaToOffset(image, info, imports.Rva, loadedInMemory);
    if (!descriptors.IsOk())
    {
        return Names::Err(descriptors.GetError());
    }

    const std::size_t count = imports.Size / kImportDescriptorSize;
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::size_t entry = descriptors.Value() + (index * kImportDescriptorSize);
        if (!InBounds(entry, kImportDescriptorSize, image.size()))
        {
            break;
        }
        const std::uint32_t nameRva = ReadU32(image, entry + kImportNameOffset);
        if (nameRva == 0U)
        {
            break; // null 表尾
        }
        // OriginalFirstThunk / FirstThunk：非 0 就必须能被节表解释（0 允许）。
        // M1 只取名字，thunk 内容不参与判断——但要挡住「结构明显坏掉」的镜像。
        for (const std::size_t field : {std::size_t{0}, kImportFirstThunkOffset})
        {
            const std::uint32_t thunkRva = ReadU32(image, entry + field);
            if (thunkRva != 0U && !PeRvaToOffset(image, info, thunkRva, loadedInMemory).IsOk())
            {
                return Names::Err(Error{"PE import thunk RVA not inside any section"});
            }
        }
        const Result<std::size_t> nameAt = PeRvaToOffset(image, info, nameRva, loadedInMemory);
        if (!nameAt.IsOk())
        {
            return Names::Err(nameAt.GetError());
        }
        Result<std::string> name = ReadNulTerminated(image, nameAt.Value());
        if (!name.IsOk())
        {
            return Names::Err(name.GetError());
        }
        names.push_back(std::move(name.Value()));
    }
    return Names::Ok(std::move(names));
}

Result<ImageIdentity> ExtractBuildIdFromNotes(std::span<const std::uint8_t> notes)
{
    using Identity = Result<ImageIdentity>;

    std::size_t at = 0;
    while (InBounds(at, kElfNoteHeaderSize, notes.size()))
    {
        const std::uint32_t nameSize = ReadU32(notes, at);
        const std::uint32_t descSize = ReadU32(notes, at + 4U);
        const std::uint32_t type = ReadU32(notes, at + 8U);
        const std::size_t nameAt = at + kElfNoteHeaderSize;
        const std::size_t descAt = nameAt + AlignUp4(nameSize);
        if (!InBounds(descAt, descSize, notes.size()))
        {
            break; // 结构越界：当作「没有可用的建置 ID」，绝不越界读
        }
        if (type == kNtGnuBuildId && nameSize == 4U && IsGnuNoteName(notes, nameAt))
        {
            const std::span<const std::uint8_t> desc = notes.subspan(descAt, descSize);
            ImageIdentity identity;
            identity.Kind = IdentityKind::kElfBuildId;
            identity.Bytes.assign(desc.begin(), desc.end());
            return Identity::Ok(std::move(identity));
        }
        const std::size_t next = descAt + AlignUp4(descSize);
        if (next <= at)
        {
            // 防御性，不是「空 note 会让循环转不出去」——按上面的两处 InBounds 推，
            // next >= descAt >= at + 12，这一支**当前不可达**。留着是为了将来改
            // note 布局或 AlignUp4 时，别让不前进悄悄变成死循环。
            break;
        }
        at = next;
    }
    return Identity::Err(MissingBuildIdError());
}

Result<ImageIdentity> ParseElfBuildIdFile(std::span<const std::uint8_t> fileBytes)
{
    using Identity = Result<ImageIdentity>;

    const Result<std::vector<ElfProgramHeader>> headers = ParseElfProgramHeaders(fileBytes);
    if (!headers.IsOk())
    {
        return Identity::Err(headers.GetError());
    }

    std::vector<std::uint8_t> notes;
    for (const ElfProgramHeader& header : headers.Value())
    {
        if (header.Type != kPtNote)
        {
            continue;
        }
        const auto offset = static_cast<std::size_t>(header.Offset);
        const auto size = static_cast<std::size_t>(header.FileSize);
        if (!InBounds(offset, size, fileBytes.size()))
        {
            return Identity::Err(Error{"ELF PT_NOTE outside the image"});
        }
        const std::span<const std::uint8_t> region = fileBytes.subspan(offset, size);
        notes.insert(notes.end(), region.begin(), region.end());
    }
    return ExtractBuildIdFromNotes(notes);
}

Result<std::vector<std::string>> ParseElfNeededFile(std::span<const std::uint8_t> fileBytes)
{
    using Names = Result<std::vector<std::string>>;

    const Result<std::vector<ElfProgramHeader>> headers = ParseElfProgramHeaders(fileBytes);
    if (!headers.IsOk())
    {
        return Names::Err(headers.GetError());
    }

    std::vector<std::string> names;
    const auto dynamic = std::find_if(headers.Value().begin(), headers.Value().end(),
                                      [](const ElfProgramHeader& header) { return header.Type == kPtDynamic; });
    if (dynamic == headers.Value().end())
    {
        return Names::Ok(std::move(names)); // 没有 PT_DYNAMIC：没有导入声明可言
    }

    const auto dynamicAt = static_cast<std::size_t>(dynamic->Offset);
    const auto dynamicSize = static_cast<std::size_t>(dynamic->FileSize);
    if (!InBounds(dynamicAt, dynamicSize, fileBytes.size()))
    {
        return Names::Err(Error{"ELF PT_DYNAMIC outside the image"});
    }

    // DT_NEEDED 的 val 是 strtab 内的偏移，而 DT_STRTAB 可能排在它后面，
    // 故先收两遍信息再解析。
    std::uint64_t strtabVaddr = 0;
    bool hasStrtab = false;
    std::vector<std::uint64_t> neededOffsets;
    const std::size_t count = dynamicSize / kDynamicEntrySize;
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::size_t at = dynamicAt + (index * kDynamicEntrySize);
        const std::uint64_t tag = ReadU64(fileBytes, at);
        const std::uint64_t value = ReadU64(fileBytes, at + 8U);
        if (tag == kDtNull)
        {
            break;
        }
        if (tag == kDtStrtab)
        {
            strtabVaddr = value;
            hasStrtab = true;
        }
        else if (tag == kDtNeeded)
        {
            neededOffsets.push_back(value);
        }
    }
    if (neededOffsets.empty())
    {
        return Names::Ok(std::move(names));
    }
    if (!hasStrtab)
    {
        return Names::Err(Error{"ELF DT_NEEDED without DT_STRTAB"});
    }

    const Result<std::size_t> strtab = ElfFileOffset(fileBytes, headers.Value(), strtabVaddr);
    if (!strtab.IsOk())
    {
        return Names::Err(strtab.GetError());
    }
    for (const std::uint64_t needed : neededOffsets)
    {
        const std::size_t at = strtab.Value() + static_cast<std::size_t>(needed);
        if (!InBounds(at, 1U, fileBytes.size()))
        {
            return Names::Err(Error{"ELF DT_NEEDED name outside the image"});
        }
        Result<std::string> name = ReadNulTerminated(fileBytes, at);
        if (!name.IsOk())
        {
            return Names::Err(name.GetError());
        }
        names.push_back(std::move(name.Value()));
    }
    return Names::Ok(std::move(names));
}

std::string FirstUnresolvableImport(std::span<const std::uint8_t> fileBytes, bool isPe)
{
    const Result<std::vector<std::string>> names =
        isPe ? ParsePeImports(fileBytes, /*loadedInMemory=*/false) : ParseElfNeededFile(fileBytes);
    if (!names.IsOk() || names.Value().empty())
    {
        return {};
    }
    return names.Value().front();
}

} // namespace vase::detail
