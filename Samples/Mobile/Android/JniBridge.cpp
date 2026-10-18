// 同步 JNI：只读 APK 资产，Host 只存在于本调用线程。
#include "MobileScenario.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"
#include "VaseMobileResources.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr std::size_t kChunkSize = 8192;
constexpr const char* kNativeResultClass = "com/moozen/vase/mobile/NativeResult";

vase::Result<std::string> ReadAsset(AAssetManager* manager, std::string_view assetName)
{
    const std::string name{assetName};
    using AssetPtr = std::unique_ptr<AAsset, decltype(&AAsset_close)>;
    const AssetPtr asset{AAssetManager_open(manager, name.c_str(), AASSET_MODE_STREAMING), &AAsset_close};
    if (!asset)
    {
        return vase::Result<std::string>::Err(vase::Error{"missing APK asset " + name});
    }
    const auto length = AAsset_getLength64(asset.get());
    if (length < 0 || static_cast<std::uint64_t>(length) > std::string{}.max_size())
    {
        return vase::Result<std::string>::Err(vase::Error{"invalid APK asset size: " + name});
    }
    std::string contents(static_cast<std::size_t>(length), '\0');
    std::size_t position = 0;
    while (position < contents.size())
    {
        const auto count = static_cast<int>(std::min(kChunkSize, contents.size() - position));
        const auto writable = std::span<char>{contents.data(), contents.size()}.subspan(position);
        const int read = AAsset_read(asset.get(), writable.data(), count);
        if (read <= 0)
        {
            return vase::Result<std::string>::Err(vase::Error{"truncated APK asset: " + name});
        }
        position += static_cast<std::size_t>(read);
    }
    return vase::Result<std::string>::Ok(std::move(contents));
}

jobject MakeResult(JNIEnv* env, bool success, std::uint32_t completed, const std::string& message)
{
    jclass resultClass = env->FindClass(kNativeResultClass);
    if (resultClass == nullptr)
    {
        return nullptr; // 失败由 JVM 挂起的异常表达，不吞掉。
    }
    jmethodID constructor = env->GetMethodID(resultClass, "<init>", "(ZILjava/lang/String;)V");
    if (constructor == nullptr)
    {
        env->DeleteLocalRef(resultClass);
        return nullptr;
    }
    jstring detail = env->NewStringUTF(message.c_str());
    if (detail == nullptr)
    {
        env->DeleteLocalRef(resultClass);
        return nullptr;
    }
    std::array<jvalue, 3> args{};
    args.at(0).z = static_cast<jboolean>(success);
    args.at(1).i = static_cast<jint>(completed);
    args.at(2).l = detail;
    jobject result = env->NewObjectA(resultClass, constructor, args.data());
    env->DeleteLocalRef(detail);
    env->DeleteLocalRef(resultClass);
    return result;
}
// Optional replacement is used only by instrumentation, while both paths
// continue through the exact same Catalog / Solve / Host implementation.
jobject RunScenarioFromAssets(JNIEnv* env, jobject assets, std::size_t selectedIndex, const std::string* replacement)
{
    if (assets == nullptr)
    {
        return MakeResult(env, false, 0, "AssetManager is null");
    }
    AAssetManager* manager = AAssetManager_fromJava(env, assets);
    if (manager == nullptr)
    {
        return MakeResult(env, false, 0, "AssetManager conversion failed");
    }

    std::vector<vase_mobile::OwnedPackageResource> owned;
    owned.reserve(vase_mobile::kPackageResources.size());
    for (std::size_t index = 0; index < vase_mobile::kPackageResources.size(); ++index)
    {
        const auto& resource = vase_mobile::kPackageResources.at(index);
        std::string contents;
        if (replacement != nullptr && index == selectedIndex)
        {
            contents = *replacement;
        }
        else
        {
            auto asset = ReadAsset(manager, resource.AssetName);
            if (!asset.IsOk())
            {
                return MakeResult(env, false, 0, asset.GetError().Message());
            }
            contents = std::move(asset.Value());
        }
        owned.push_back(vase_mobile::OwnedPackageResource{
            .Location = std::string{resource.AssetName},
            .Contents = std::move(contents),
        });
    }
    std::vector<vase::PackageResource> views;
    views.reserve(owned.size());
    for (const auto& resource : owned)
    {
        views.push_back(vase::PackageResource{.Location = resource.Location, .Contents = resource.Contents});
    }
    const auto scenario = vase_mobile::RunPackageScenario(views);
    if (!scenario.IsOk())
    {
        return MakeResult(env, false, 0, scenario.GetError().Message());
    }
    return MakeResult(env, scenario.Value().Clean, scenario.Value().CompletedRounds, "");
}

// Instrumentation-only composite entry: the two *real* static descriptors
// become members of one synthetic in-memory participating package. This
// exercises disabled-member drift without changing the shipping two packages.
const vase::PluginDescriptor* CompositeLookup(const char* id)
{
    const auto& first = vase_mobile::kPackageBindings.at(0).Entries;
    const auto& second = vase_mobile::kPackageBindings.at(1).Entries;
    if (const auto* found = first.Lookup(id); found != nullptr)
    {
        return found;
    }
    return second.Lookup(id);
}

const vase::PluginDescriptor* const* CompositeEnumerate(std::uint32_t* count)
{
    const auto& first = vase_mobile::kPackageBindings.at(0).Entries;
    const auto& second = vase_mobile::kPackageBindings.at(1).Entries;
    std::uint32_t firstCount = 0;
    std::uint32_t secondCount = 0;
    const auto* firstMembers = first.Enumerate(&firstCount);
    const auto* secondMembers = second.Enumerate(&secondCount);
    if (firstCount != 1 || secondCount != 1 || firstMembers == nullptr || secondMembers == nullptr)
    {
        *count = 0;
        return nullptr;
    }
    static const std::array<const vase::PluginDescriptor*, 2> kMembers{
        std::span{firstMembers, static_cast<std::size_t>(firstCount)}.front(),
        std::span{secondMembers, static_cast<std::size_t>(secondCount)}.front(),
    };
    *count = static_cast<std::uint32_t>(kMembers.size());
    return kMembers.data();
}

jobject RunDisabledMemberProbe(JNIEnv* env, std::string_view syntheticJson)
{
    const std::array resources{
        vase::PackageResource{
            .Location = "instrumentation/participating/pack.json",
            .Contents = syntheticJson,
        },
    };
    vase::PluginCatalog catalog;
    const auto refreshed = catalog.RefreshResources(resources);
    if (!refreshed.IsOk())
    {
        return MakeResult(env, false, 0, "resource import: " + refreshed.GetError().Message());
    }
    const std::array bindings{
        vase::InProcessBinding{
            .SourceId = "Provider",
            .Entries =
                {
                    .Lookup = &CompositeLookup,
                    .Enumerate = &CompositeEnumerate,
                },
        },
    };
    vase::PluginOverride enabled;
    enabled.Id = "Vase.PackageProvider";
    enabled.Enabled = true;
    vase::PluginOverride disabled;
    disabled.Id = "Vase.PackageConsumer";
    disabled.Enabled = false;
    const std::array overrides{enabled, disabled};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = catalog.Solve(request, bindings);
    if (!solved.IsOk())
    {
        return MakeResult(env, false, 0, "Solve: " + solved.GetError().Message());
    }
    vase::PluginHost host;
    std::uint32_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    const auto created = host.CreatePod(solved.Value().Plan, options);
    if (!created.IsOk())
    {
        const bool beforePod = stage0Calls == 0 && host.ForTestCounters().PluginInstances == 0 &&
                               host.ForTestCounters().Scopes == 0 && host.ForTestLedgerEdgeCount() == 0;
        return MakeResult(env, false, 0,
                          created.GetError().Message() + (beforePod ? "; before-pod=true" : "; before-pod=false"));
    }
    const bool clean = host.DestroyPod(created.Value()).Clean();
    return MakeResult(env, clean && stage0Calls == 1, 1,
                      clean && stage0Calls == 1 ? "" : "synthetic stage0 or cleanup mismatch");
}

} // namespace

// NOLINTNEXTLINE(readability-identifier-naming) JNI names must match the Java declarations exactly.
extern "C" JNIEXPORT jobject JNICALL Java_com_moozen_vase_mobile_NativeBridge_runScenario(JNIEnv* env,
                                                                                          jclass /*unused*/,
                                                                                          jobject assets)
{
    return RunScenarioFromAssets(env, assets, 0, nullptr);
}

// Test-only entrypoint: substitute one manifest's bytes in memory without writing
// into the target APK, and exercise the same package import / Pod lifecycle.
// NOLINTNEXTLINE(readability-identifier-naming) JNI names must match the Java declarations exactly.
extern "C" JNIEXPORT jobject JNICALL Java_com_moozen_vase_mobile_NativeBridge_runScenarioWithResourceForTest(
    JNIEnv* env, jclass /*unused*/, jobject assets, jstring sourceId, jbyteArray replacementBytes)
{
    if (sourceId == nullptr || replacementBytes == nullptr)
    {
        return MakeResult(env, false, 0, "test resource sourceId and bytes are required");
    }
    const char* rawId = env->GetStringUTFChars(sourceId, nullptr);
    if (rawId == nullptr)
    {
        return nullptr; // A JVM exception (typically OOM) is pending.
    }
    const std::string id{rawId};
    env->ReleaseStringUTFChars(sourceId, rawId);

    std::size_t selectedIndex = vase_mobile::kPackageResources.size();
    for (std::size_t i = 0; i < vase_mobile::kPackageResources.size(); ++i)
    {
        if (vase_mobile::kPackageResources.at(i).SourceId == id)
        {
            selectedIndex = i;
            break;
        }
    }
    if (selectedIndex == vase_mobile::kPackageResources.size())
    {
        return MakeResult(env, false, 0, "unknown test resource sourceId: " + id);
    }

    const jsize length = env->GetArrayLength(replacementBytes);
    // Read as JNI's actual byte type, then preserve every byte in owned storage.
    // In particular, malformed JSON fixtures may contain embedded NUL bytes.
    std::vector<jbyte> bytes(static_cast<std::size_t>(length));
    if (length > 0)
    {
        env->GetByteArrayRegion(replacementBytes, 0, length, bytes.data());
        if (env->ExceptionCheck() == JNI_TRUE)
        {
            return nullptr;
        }
    }
    std::string replacement;
    replacement.reserve(bytes.size());
    for (const jbyte byte : bytes)
    {
        replacement.push_back(static_cast<char>(byte));
    }
    return RunScenarioFromAssets(env, assets, selectedIndex, &replacement);
}

// Test-only composite: the payload contains real Provider and Consumer
// metadata in one participating in-memory package. Consumer is disabled but
// MUST still match its statically linked descriptor before Stage0 runs.
// NOLINTNEXTLINE(readability-identifier-naming) JNI names match Java declarations.
extern "C" JNIEXPORT jobject JNICALL Java_com_moozen_vase_mobile_NativeBridge_runDisabledMemberProbeForTest(
    JNIEnv* env, jclass /*unused*/, jbyteArray syntheticBytes)
{
    if (syntheticBytes == nullptr)
    {
        return MakeResult(env, false, 0, "synthetic package bytes are required");
    }
    const jsize length = env->GetArrayLength(syntheticBytes);
    std::vector<jbyte> bytes(static_cast<std::size_t>(length));
    if (length > 0)
    {
        env->GetByteArrayRegion(syntheticBytes, 0, length, bytes.data());
        if (env->ExceptionCheck() == JNI_TRUE)
        {
            return nullptr;
        }
    }
    std::string contents;
    contents.reserve(bytes.size());
    for (const jbyte byte : bytes)
    {
        contents.push_back(static_cast<char>(byte));
    }
    return RunDisabledMemberProbe(env, contents);
}
