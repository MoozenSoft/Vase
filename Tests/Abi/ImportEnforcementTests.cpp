// T13 §8.7 的现场复现：账本与导入表是两套**互相补盲**的执法面。
//
// A 二进制直链 B 的文件时，一路没有任何服务解析发生——账本对这条边是**盲的**。没有导入表
// 这道执法，Eject(B) 会当场假成功（引用计数被 A 的导入表焊死，B 卸不掉），而假成功比失败
// 危险得多：它把「卸干净了」写进报告，实际留着一根指向已卸镜像的引用。这里验的就是补盲的
// 那一面：命中兄弟导入 → Adopt 拒，且 A 一根毛都没进来。
//
// 导入条目是**真的**（不是伪造字节）：A 真的引用了 B 的导出标记——只写 LINK_LIBRARIES
// 不产生条目，理由与实测证据写在 Tests/Abi/fixtures/ 那两个 .cpp 与本任务报告里。
#include "Vase/Host/PluginHost.h"

#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Pod/Pod.h"

#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace
{

// 文件名比对走小写（§8.7 本来就不做大小写敏感）：Windows 的导入表保留创建时大小写，
// 而 Linux 的 DT_NEEDED 是另一个拼法（`libBadLinkSiblingB.so`）。小写化之后两平台
// **同一条断言**都能用——不写 #ifdef，就没有「只在天平一侧被验过」的那一半。
std::string LowerAscii(std::string_view text)
{
    std::string lowered(text);
    for (char& letter : lowered)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter = static_cast<char>(letter - 'A' + 'a');
        }
    }
    return lowered;
}

TEST(Abi, AdoptRejectsBinaryThatImportsSiblingPlugin)
{
    vase::PluginHost host;
    // ① 先整局装一遍：登记路径（Adopt 的 KnownBinaries 兄弟表就建在这里）+ 证明两个
    //    二进制本身都能装载（A 的 DT_NEEDED 在运行期解得开）。销毁不卸货（§8.1）。
    vase::LoadPlan registerBoth;
    registerBoth.Ordered.push_back({.Id = "Vase.BadLinkSiblingB", .BinaryPath = VASE_FIXTURE_BADLINKB});
    registerBoth.Ordered.push_back({.Id = "Vase.BadLinkSiblingA", .BinaryPath = VASE_FIXTURE_BADLINKA});
    host.DestroyPod(host.CreatePod(registerBoth).Value());

    // ② 只留 B 的一局，再 Adopt A：A 的导入表里有 B，执法在装配之前就该拦下。
    vase::LoadPlan justB;
    justB.Ordered.push_back({.Id = "Vase.BadLinkSiblingB", .BinaryPath = VASE_FIXTURE_BADLINKB});
    const vase::PodHandle h = host.CreatePod(justB).Value();

    const vase::Result<vase::AdoptReport> refused = host.AdoptPlugin(h, "Vase.BadLinkSiblingA");
    ASSERT_FALSE(refused.IsOk()); // ASSERT_：下面立刻取 GetError()，Ok 上取会终止进程

    // 消息的两段各证明一件事：**是哪条检查**（不是别的原因早退）与**点名了谁**。
    // 后者是「红来自导入表执法」的凭据——只断「失败了」，A 因为任何别的理由被拒都算绿。
    const std::string lowered = LowerAscii(refused.GetError().Message());
    EXPECT_NE(lowered.find("imports sibling plugin"), std::string::npos);
    EXPECT_NE(lowered.find("badlinksiblingb"), std::string::npos);

    EXPECT_EQ(host.Resolve(h)->PluginCount(), 1U); // 拒绝 = A 一根毛没进来（规则②）
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
