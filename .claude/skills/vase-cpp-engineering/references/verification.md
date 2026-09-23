# 验证制度

这份是"改完凭什么说完成"的判据。**两个静默陷阱让退出码单独用会假绿**——所有判据都要"退出码 + 正文/基数一起读"。

**命令、preset 名、工具链文件、脚本清单、按线基数，一律只在仓库根 `CLAUDE.md`「构建与测试」与「静态检查与格式」两节。** 本文件不抄——按 `CLAUDE.md` 规矩 7，可整段引用的块抄进第二处就变成没有链路的孤儿。这里只留**判据**与**为什么**。

---

## 1. 两个静默陷阱

| 陷阱 | 为什么会静默 | 正确判据 |
|---|---|---|
| **`run-clang-tidy` 退出 0 ≠ 没有 warning** | `.clang-tidy` 的 `WarningsAsErrors` 为空，tidy 永远不会因 warning 失败 | **退出 0 + 正文 `error:` 0 条 + 正文 `warning:` 0 条**，再连摘要行一起读 |
| **`ctest` 在一个测试都没发现时同样返回 0** | `gtest_discover_tests` 用 `DISCOVERY_MODE PRE_TEST`，枚举发生在 ctest **运行时**——测试被漏注册时它一声不吭地报成功 | 必须另跑 `ctest --preset <p> -N`，把 `Total Tests` 与按线基数表逐位对上 |

两处涉及的字面配置（`WarningsAsErrors` 的当前值、`DISCOVERY_MODE`）以根 `CLAUDE.md` 为准。

**读基数表时要分清两件容易混的事**：抑制量**全部落在第三方头里**（gtest 模板实例化是主要来源），我们自己的代码零正文 warning；摘要行里的 `N NOLINT` 是**本轮的抑制命中次数，不是仓库里的抑制处数**——同一个抑制会被每个包含它的 TU 各计一次，所以它与 `grep -c NOLINT` 天然对不上。

---

## 2. 各线基数，以及差值为什么是设计

preset 清单、各线 `Total Tests` 与按线差值那一列，读根 `CLAUDE.md`「构建与测试」的表——数字与差值都不进本文件（规矩 7：基数只住一处）。**要背的是差值的成因，不是差值本身**：

- **death test 一族受 `#ifndef NDEBUG` 门**（T3 的）：release 下它根本不注册，所以 release 的基数低于 debug。
- **Linux-only fixture 一族包在 `if(NOT WIN32)` 里**（T11 的 `NoBuildIdPlugin`）：所以 Linux 的基数高于 Windows。
- 两道门在 Linux release 上同时生效，各线最终数字以那张表为准。

**数字变了不一定是错，但要能说清变在哪一条**——说不出成因就去读那张表，别凭记忆报数。实测教训：把基数直接递到手上的回答，只会复述「差值是设计不是漏注册」；机制要自己去读那张表才拿得到。

---

## 3. 命令与脚本

全部命令块（Windows 三条、cl.exe 须经 `Scripts/msvc-env.cmd`、WSL 登录 shell 三条、tidy 三线、format 那一串 `git ls-files`）与四条 Windows 线 / 三条 tidy 线的脚本形态，见根 `CLAUDE.md` 对应两节。本文件只补三条**不在那里、或容易被漏掉**的：

- **`msvc-env.cmd` 会打一行 vswhere 的 "not recognized" 噪声**——无害，环境仍正确建立，别去追。
- **在 Git Bash 里拼 WSL 命令时别把 `$` 写进 `bash -lc "..."` 的双引号里**：`$VAR` 会被**外层 Git Bash 先展开**，命令照跑、结果是假的。涉及 `$` 的先落成脚本文件再调用。本项目已因此栽过不止一次（`$CXX` / `$FLAGS` 被吃空，命令照跑、结果假绿）。
- **`git ls-files` 那串不能省**：未 `git add` 的新文件会被**静默跳过**，门禁照样绿。

**「Windows 两条线绿」推不出「Linux 绿」**，而且成因有**两种，别混**：一条是 STL 不同（同一份共享文件、同版 tidy，MSVC STL 的某些路径就是不报），另一条是平台专属 TU 在 Windows 上**根本不编译**、天然只有 Linux 线看得见。推论：三条 debug 线各自跑、各自读正文；平台专属 TU 的告警与抑制数目本来就不对称。

---

## 4. `Tests/HotSwap` 按主干对待

**任何改动 `Loader`、依赖账本、`Eject` / `Adopt` 路径、描述符布局或 `HeaderVersion` 的提交，必须跑 `Tests/HotSwap/` 的全部用例，且 Windows 与 Linux 各自留证据。** 该目录全部用例的选择子见根 `CLAUDE.md` 规矩 6——**写成只匹配主循环那一类，会把守 Eject / Adopt 边界的用例整套漏掉**，照规则做的人会拿到几条绿灯然后从没跑过那一堆。这里也不写条数：写死的数会随用例增删漂移。

**「六线全绿」不等于「六线等价」**（T12 复评的判定，细节在 `CLAUDE.md` 规矩 6 末段）：主循环里那条覆盖断言在 Windows 上是**真的文件锁探针**（镜像还映射着时覆盖会失败，直接暴露"Eject 没真卸"），在 Linux 上是**空转的**（truncate-in-place，同一 inode，照样成功）。所以 Windows 比 Linux 多一层证据，两平台同跑得到的不是同一件事的两份拷贝——这也正是"各自留证据"不是形式主义的原因。

---

## 5. 改什么必须验什么

| 改动 | 必须做的 |
|---|---|
| 任何提交 | 六 preset 全绿；`ctest -N` 的 `Total Tests` 逐位对上基数表；三条 debug 线 tidy 正文干净；format 门全绿（命令与基数见 `CLAUDE.md`） |
| **`Loader`、依赖账本、`Eject` / `Adopt` 路径、描述符布局、`HeaderVersion`** | 另加 `Tests/HotSwap/` **全部**用例，**Windows 与 Linux 各自留证据**（选择子见上一节） |
| 两条承重工具链 flag（身份特征的构建要求，见 `CLAUDE.md`） | 同上。**症状是运行期 Adopt 全线拒绝，构建照常全绿**——所以摘它不会被编译期抓到 |
| 跨平台行为 / 平台专属代码 | 双平台各自跑。平台专属 TU 只在一条线上被 tidy 检查 |
| 构建树 / 工具链文件改动 | **删树重配**，不要用 `-D` 钉手工 override |
| 新增 target | 链了 `VaseBuildOptions`（规矩 1）；插件 target 经 `vase_add_plugin_fixture`（规矩 5） |
| 新增源码 / 测试 / 目录 | 引入了新的规矩或命令 → **同步更新根 `CLAUDE.md`** |
| 改本 skill 或 `CLAUDE.md` 本身 | 按 `CLAUDE.md` 规矩 7 判这一处属哪一类：阈值与可整段引用的块**只准有一处**；嵌在论述里的字面值必须带指针 |

---

## 6. 汇报纪律

- 说"测试过了"时，**把 `ctest` 的正文与 `ctest -N` 的基数一起给出**，不要只给一句"全绿"。
- **跳过某步验证要在汇报里明说是哪步、为什么**——静默跳过是本项目最贵的失败形态。
- 测试失败就贴输出，不要改述成"基本通过"。
- **验证前先确认自己在正确的树上**：`CMAKE_*_FLAGS_INIT` 只在首次 configure 进缓存，一棵陈旧的树会让新 flag 静默缺席。
- **没跑的那一步要说是没跑**：给出验证配方与实际跑过，是两件必须分得开的事——评审者只能从你的措辞里分辨，而"看起来像跑过"比"没跑"更难被发现。
