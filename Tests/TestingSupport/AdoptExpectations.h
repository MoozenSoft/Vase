#pragma once

// 测试用清单期望工厂（T8 起，多文件共用）：逐字对位 Samples/ 插件描述符的期望表。
// 改插件描述符/VASE_CONFIG 必来这里对一眼（D87 校准同法）；今后家族的期望随各任务加在本头。

#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/ManifestExpectation.h"

#include <cstdint>
#include <string>

namespace testing_support
{

// HelloPlugin.cpp 的 VASE_PLUGIN/VASE_CONFIG 逐字抄：Repeats（kInt32 默认 1，label 问候次数）
// + MoodValue（kEnum 默认 kQuiet=0，label 情绪，choices 按序 安静/响亮——Mood.h kMoodChoices）。
inline vase::ManifestExpectation MakeHelloExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.Hello";
    expected.DisplayName = "示例插件";
    expected.Version = "0.1.0";
    expected.Provides = {vase::ExpectedService{.Name = "Vase.Hello.Greeter", .Version = 1}};
    expected.Config = {
        vase::ExpectedConfigField{
            .Key = "Repeats",
            .Kind = vase::ValueKind::kInt32,
            .Default = vase::ConfigBlob::Storage{std::int32_t{1}},
            .Label = "问候次数",
        },
        vase::ExpectedConfigField{
            .Key = "MoodValue",
            .Kind = vase::ValueKind::kEnum,
            .Default = vase::ConfigBlob::Storage{vase::ConfigBlob::EnumStored{.Value = 0}},
            .Label = "情绪",
            .Choices =
                {
                    vase::ExpectedChoice{.Value = 0, .Label = "安静"},
                    vase::ExpectedChoice{.Value = 1, .Label = "响亮"},
                },
        },
    };
    return expected;
}

// VersionedA.cpp / VersionedAPrime.cpp 的 VASE_PLUGIN 逐字抄——两版描述符**逐字节相同**
// （T5 评审确认），换件位共用这一份期望。
inline vase::ManifestExpectation MakeVersionedAExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.VersionedA";
    expected.DisplayName = "双版本探针";
    expected.Version = "1.0.0";
    expected.Provides = {vase::ExpectedService{.Name = "Vase.Test.Counter", .Version = 1}};
    return expected;
}

// EdgeConsumerPlugin.cpp 逐字抄。
inline vase::ManifestExpectation MakeEdgeConsumerExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.EdgeConsumer";
    expected.DisplayName = "账本消费者探针";
    expected.Version = "0.0.1";
    expected.Requires = {
        vase::ExpectedService{.Name = "Vase.Test.Shared", .Version = 1},
        vase::ExpectedService{.Name = "Vase.Test.HostOnly", .Version = 1},
    };
    return expected;
}

// CollisionProviderPlugin.cpp 逐字抄。
inline vase::ManifestExpectation MakeCollisionProviderExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.CollisionProvider";
    expected.DisplayName = "重复提供者探针";
    expected.Version = "0.0.1";
    expected.Provides = {vase::ExpectedService{.Name = "Vase.Test.Shared", .Version = 1}};
    return expected;
}

// ConfigConsumerPlugin.cpp 逐字抄：Echo（kInt32 默认 1，min 0 / max 10，label 回声）、
// Banner（kString 默认 "default"，Meta{} → label 空串）、Mood（kEnum 默认 kQuiet=0，
// label 情绪，choices 按序 安静/响亮——kMoodChoices）。
inline vase::ManifestExpectation MakeConfigConsumerExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.ConfigConsumer";
    expected.DisplayName = "配置应用探针";
    expected.Version = "0.0.1";
    expected.Provides = {vase::ExpectedService{.Name = "Vase.Test.ConfigEcho", .Version = 1}};
    expected.Config = {
        vase::ExpectedConfigField{
            .Key = "Echo",
            .Kind = vase::ValueKind::kInt32,
            .Default = vase::ConfigBlob::Storage{std::int32_t{1}},
            .Min = vase::ConfigBlob::Storage{std::int32_t{0}},
            .Max = vase::ConfigBlob::Storage{std::int32_t{10}},
            .Label = "回声",
        },
        vase::ExpectedConfigField{
            .Key = "Banner",
            .Kind = vase::ValueKind::kString,
            .Default = vase::ConfigBlob::Storage{std::string{"default"}},
        },
        vase::ExpectedConfigField{
            .Key = "Mood",
            .Kind = vase::ValueKind::kEnum,
            .Default = vase::ConfigBlob::Storage{vase::ConfigBlob::EnumStored{.Value = 0}},
            .Label = "情绪",
            .Choices =
                {
                    vase::ExpectedChoice{.Value = 0, .Label = "安静"},
                    vase::ExpectedChoice{.Value = 1, .Label = "响亮"},
                },
        },
    };
    return expected;
}

// NoBuildIdPlugin.cpp 逐字抄（Linux-only fixture）。
inline vase::ManifestExpectation MakeNoBuildIdExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.NoBuildId";
    expected.DisplayName = "无建置 ID 探针";
    expected.Version = "0.0.1";
    return expected;
}

// BehindStrictUnusedPlugin.cpp 逐字抄。
inline vase::ManifestExpectation MakeBehindStrictUnusedExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.BehindStrictUnused";
    expected.DisplayName = "strict 声明但没用";
    expected.Version = "0.0.1";
    expected.Requires = {vase::ExpectedService{.Name = "Vase.Test.Behind", .Version = 1}};
    return expected;
}

// BadLinkSiblingA.cpp 逐字抄。
inline vase::ManifestExpectation MakeBadLinkSiblingAExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.BadLinkSiblingA";
    expected.DisplayName = "互链探针 A";
    expected.Version = "1.0.0";
    return expected;
}

// LoadProbe.cpp 逐字抄（UnloadProbe 同源码同描述符，期望同份）。
inline vase::ManifestExpectation MakeLoadProbeExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.LoadProbe";
    expected.DisplayName = "装载探针";
    expected.Version = "0.0.1";
    return expected;
}

// DeadProviderPlugin.cpp 逐字抄。
inline vase::ManifestExpectation MakeDeadProviderExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.DeadProvider";
    expected.DisplayName = "失败提供者探针";
    expected.Version = "0.0.1";
    expected.Provides = {vase::ExpectedService{.Name = "Vase.Test.Dead", .Version = 1}};
    return expected;
}

} // namespace testing_support
