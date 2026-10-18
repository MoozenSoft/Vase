# VasePack 移动端发布波设计

状态：用户于 2026-10-16 确认 grilling 最终共识；本文已据此修订，尚未进入实现。
环境盘点日期：2026-10-15；环境事实不视为项目构建或目标运行验证。

## 1. 已确认的目标

- Android 采用 A 方案：Vase 框架与插件包均编为静态归档，最终链入应用 JNI `.so`。
  交付包 `.a`、完整链接依赖、入口头，以及 JNI `.so` 集成示例；参考 CppDemo 工具链配置。
- iOS 交付静态包及框架 `.a`、入口头和应用链接验证；Vase 与插件代码最终进入 App 可执行文件。
- 先完成一个平台的真实 Provider/Consumer 闭环，再推进另一个平台。
- 服务消费结果为 `Value=7`，反复建局、销局均 Clean。
- 复用 N:1 包清单、双入口绑定、Catalog 求解及 Host 整包预检。
- Android 独立插件包 `.so`（B 方案）留待有实际需求时另立波次。
- 桌面验证不代替移动验证；不增加移动端热插拔、CI/CD 或 Vase 对外分发。
- 用户明确不安装 Xcode。独立 SDK 与现有工具链完成 iOS 静态编译/链接验证，设备运行另记证据。
- 本波仅服务仓库内产品集成，不建立独立 SDK 交付目录或通用 install/export 体系。
- 最低目标为 Android API 29、iOS 15.0；编译、链接、生成候选清单和打包不依赖连接手机。
- 包清单随 APK/App 作为内置只读资源发布，正常启动不生成清单、不复制清单到磁盘。
- C++ 描述符是元信息唯一真值；本波要求宿主伴生构建与移动目标公开元信息一致。
  插件实现代码允许平台差异；有意的平台元信息差异不自动适配，实际不一致时拒绝运行。

本文件是设计材料；构建命令与测试基数的唯一真值仍为根 `CLAUDE.md`。

## 2. 环境盘点结果

| 项 | 已读配置或实际结果 |
|---|---|
| CppDemo | `D:/AllProject/AndroidStudio/CppDemo`，只读参考，不修改该项目 |
| Android SDK | 用户指定的本机 SDK 目录存在 |
| 已安装 NDK | `28.2.13676358`、`30.0.16248370` |
| 参考项目选用 NDK | `30.0.16248370` |
| 参考项目 CMake | Gradle 指定 `4.1.2`；CMakeLists 要求至少 `4.0` |
| 参考项目 Android API | `minSdk=29`、`compileSdk=37`、`targetSdk=37` |
| 参考项目 C++ | C++20；一个 JNI SHARED 目标链接 Android 的 `android`、`log` |
| NDK 编译器 | 实际执行 `clang++ --version` 得 Android Clang `21.0.0` |
| Android 运行条件 | `adb devices -l` 当前无设备；SDK 下未发现 `system-images` 目录 |
| Mac | 实际查询为 Intel x86_64，当前 developer directory 是 CommandLineTools |
| Apple 编译器 | 当前工具链报告 Apple Clang `16.0.0` |
| iOS SDK | 已按用户要求从 Xcode 16.2 压缩包单独提取 iPhoneOS 18.2 SDK，安装到 `~/Library/Developer/SDKs/iPhoneOS18.2.sdk`；未安装 Xcode，现有 developer directory 不变 |
| iOS 工具链探针 | Apple Clang 16 与 MacPorts Clang 23 均通过 C++20、无异常的 arm64 编译、`.a` 归档及链接；Mach-O 标记为 iOS、SDK 18.2。探针不是 Vase 工程验证；iOS 15.0 最低目标另于 2026-10-16 获用户确认 |

SDK 的机器路径由环境或本地配置提供，不写入可移植 preset。Android API 29、iOS 15.0 是已确认
的最低目标，不代表所有更高系统版本均已实测；更低版本不纳入本波承诺。

## 3. 现有实现需要解决的接缝

1. 根 CMake 只接受 Clang 23 或 Windows MSVC，不接受当前 NDK Clang 21 / AppleClang。
   移动配置必须独立约束所选官方工具链，不能放宽桌面八线版本门。
2. VasePod、VaseHost、VaseCatalog 当前明确建立为 SHARED。iOS 静态交付需扩出真实静态框架构建，
   保持 Host → Pod、Catalog → Host、插件只依赖 Pod 的链接方向。
3. 根 CMake 无条件引入桌面 Samples、Console、CLI 和 Tests。移动构建需明确选择发布目标及移动证人，
   不能让桌面 discovery、death test 或桌面工具进入移动默认构建。
4. `vase_stage_pack` 当前生成目标平台可执行文件，并在构建机直接运行。交叉编译后这一步不成立。
   不能将桌面描述符替代目标描述符，却宣称已完成移动清单验证。
5. 当前 Host 按 WIN32 / APPLE / 其他选择平台件。移动平台需要明确能力边界，不能因为宏匹配便宣称
   Android 等同 Linux、iOS 等同 macOS；尤其不可照搬桌面卸载证据。
6. Catalog 当前仅有目录 Refresh，包解析入口仅接受文件。Android APK assets 不能通过该入口直接读取，
   需新增只读资源输入接缝，不能以启动时复制临时文件规避“不依赖写盘”的决定。

## 4. 已选交付形态与构建结构

### 4.1 最终链接归属

Android 将插件包 `.a` 与静态框架链入一个 JNI `.so`；仓库构建保留包归档、生成入口头和传递链接依赖。
`pack.json` 按第 5 节在发布阶段预生成，再作为只读资源打包。它在目标实际校验前只有候选清单证据。
iOS 同样将包与静态框架链入应用。两平台使用现有进程内来源入口绑定，不引入移动动态装卸路径。
该方案同时覆盖 Android 的 `.so` / `.a`，并让双 STATIC 包闭环可复用。

Android 的 JNI 桥只负责资源读取、调用边界及结果返回；Provider/Consumer 保持纯 Vase C++ 插件，
不为每个插件增加 JNI。iOS 的应用边界采用 Objective-C++ 或 C++，不引入完整 Swift 集成工具链。
包 `.a` 不作为独立运行文件放进 APK / App；包内的逻辑插件仍各有 Id、配置、服务依赖与实例生命周期。

### 4.2 平台构建选择

- 新增移动平台构建路径；桌面默认目标、框架 SHARED 形态及八条 preset 行为保持不变。
- 移动端 VasePod / VaseHost / VaseCatalog 建为 STATIC，保持原链接方向及同一份框架状态。
  Android 所有进入 JNI `.so` 的静态目标启用 PIC。
- 插件仍经 `vase_add_plugin_fixture` 定义。移动发布形只建立 `.Pack` 静态目标及必要的目标属性载体，
  不编译对应开发形 SHARED 插件。未带 PACK 却进入移动发布包的目标在配置期带名拒绝。
  移动分支须保留现有包函数消费的 class / manifest / members / entry 属性，不另立插件定义入口。
- 包仍经 `vase_pack_plugins(... STATIC)` 生成；保持包专属查询/枚举入口及现有生成头。
- 移动默认构建不引入桌面 Console、CLI 可执行、动态 Samples 或全套 GoogleTest。
  只构建框架、发布包与移动集成证人；候选清单生成器在独立的宿主原生构建中运行。
- vcpkg 继续管理依赖：移动目标只需要 nlohmann-json；GoogleTest 转为桌面测试选择的依赖，
  不在设备目标里为跳过的桌面测试构建 gtest。JSON 解析/序列化仍由 Catalog 唯一承担。

### 4.3 工具链与运行库

- Android 使用 NDK 官方 `android.toolchain.cmake`，版本取 CppDemo 的 NDK r30 / Clang 21，
  ABI 固定 arm64-v8a。工具链文件由环境中的 NDK 根定位，目标编译器不能回退到 Windows clang。
- iOS 使用已验证的 MacPorts Clang 23 + 独立 iPhoneOS 18.2 SDK，显式设 arm64、iOS 目标及 sysroot。
  采用 Ninja，不依赖 Xcode generator 或 `xcodebuild`；不能把所有 try-compile 都改为 STATIC 来遮盖链接失败。
- 桌面 Clang 23 门禁保留。Android 是有平台/NDK 身份检查的特定版本例外，不接受任意低版 Clang。
  Apple Clang 16 的环境探针仅作为已知可行证据，本波不额外引入第二套 iOS 项目验证线。
- Android 示例明确选择共享 libc++，最终 APK 包含同一 NDK 的 `libc++_shared.so`，便于应用以后接入
  其他原生库时维持一个运行库。Vase 与插件代码仍全部静态合入 JNI 库；共享 libc++ 不代表选了 B。
- iOS 按 SDK 的系统 libc++ 契约链接；不把 MacPorts 的 macOS libc++ 动态库作为 iOS 运行依赖交付。
- 所有自有目标继续链接 VaseBuildOptions，保持 C++20、无异常、可见性及警告约束。

## 5. 发布期候选清单与运行期只读资源

### 5.1 元信息真值与支持范围

C++ 描述符决定 Id、展示字段、版本、服务依赖、配置定义/默认值及进程级状态。
现有成员 MANIFEST 只提供 enabledByDefault：未声明时默认启用，声明后缺失、解析失败或 Id 不符
则生成失败。源策略文件是宿主生成器的显式构建依赖，不从已有发布清单反向取策略。

本波要求上述公开元信息在宿主伴生构建与移动构建间保持一致。平台不同可以使用不同实现代码，
但不能据此保证两份描述符相同。若目标条件编译改变成员、服务、配置默认值等元信息，运行期校验
必须拒绝；本波不自动修正或支持这类有意差异，也不手抄完整 JSON 作为第二份真值。

### 5.2 发布期流程

1. 为选定静态包建立独立宿主原生构建，生成器在构建机执行。宿主与移动构建共用插件源码、包成员
   定义、类名/入口命名规则、策略输入及可共享的编译定义；不得把宿主编译器伪装成目标编译器，
   也不强行定义 __ANDROID__ 等内置平台宏来声称执行了目标代码。
2. 宿主生成器显式链接本次选定包，复用 GeneratePackageManifest 的进程内分支：调用包双入口及
   成员描述符函数，核对版本、成员集合和入口一致性，按既有规则物化清单并合入启用策略。
   包成员与符号表由 CMake 包属性机械生成，调用者不重复维护 Id 列表。
3. 调用 Catalog 现有写出 API，得到 `source.kind=inProcess`、不含 binary 的候选 pack.json。
   可复用既有单包 staging 的写入、重读验证及失败保留原目录规则；不增加多包联合提交承诺。
4. 发布编排显式依赖宿主生成器及其输入、各移动静态包和框架。生成失败或任一输入缺失即使本次
   打包失败，不能静默复用上一轮的候选清单。宿主/目标构建树及输出必须分开，不能混入错误架构归档。
5. 候选清单随产品打包为 APK assets / App bundle 资源。源策略和宿主生成器不是正常运行依赖，
   应用中不链接 PackageBuild / PackageStage 或新的包准备库。

宿主生成器只产生候选清单。宿主编译或链接失败时报告真实错误，不增加假实现掩盖插件的平台依赖。
共享源码不等于共享预处理结果；只有实际移动产物执行比对通过，才能报告对应目标的一致性验证通过。
不新增编译器 AST 导出、离线 ELF/Mach-O 指针重定位扫描器或设备参与每次构建的生成步骤。

桌面原有显式 vase_stage_pack 继续工作。移动发布编排使用宿主生成器，不能直接执行目标 ARM
生成器；误走桌面执行路径时应提前带名拒绝，而非等到构建机报无法运行二进制。

### 5.3 只读资源导入 Catalog

新增目录无关的 Catalog 输入接缝：平台接入层批量提供“逻辑资源位置 + JSON 内容”，只接受
inProcess 包。Android 从 APK assets 直接读取字节，iOS 从 App 内置资源读取；不提取为临时文件，
不创建可写包根，不提供外置包清单覆盖或运行中修改功能。启停和参数变化继续使用 Preset/Overrides。

资源名表由选定包定义生成，接入层逐项读取；声明资源缺失或读取失败时整批失败，不能略过缺包。
Catalog 内的内容解析和文件解析共用 PackageJson 的同一解析实现，JSON 类型不进入公开头。
资源输入与目录输入共用快照构建中的重复 sourceId / 插件 Id 检查、成员关联、排序及提交规则，
不增加第二套注册表或求解算法。空资源集合保持库级空快照语义，移动示例必须读取其已声明的全部包。

一次导入必须成功才整体替换快照；失败保留旧快照且向调用者返回 Error，应用不自动用旧快照继续
这次启动。成功后输入缓冲区即可释放，Catalog 自持清单值。既有 Plan/Find 的快照借用窗保持不变：
下次成功导入（目录或资源）使旧借用失效，失败不使其失效。

资源采用逻辑位置归因，如 `assets/vase/Provider/pack.json`，不得伪造绝对磁盘路径。
建议 CatalogSource 新增自持 ResourceLocation：目录来源保持 ManifestPath 为真实绝对路径、
ResourceLocation 为空；资源来源相反。资源输入的 Directory() 为空，HasScanned 仍表示已成功导入。
资源位置必须非空且在本批内唯一，资源来源按逻辑位置确定排序；错误点名资源位置、来源及可得的成员。
文件来源和资源来源的定位信息只由对应导入入口填充，不再从 Binary / Subdirectory 猜测。

Analyze、Solve 及 Host 整包预检继续消费同形快照和 InProcessBinding。
本波资源入口不接受 dynamic 包，也不把资源内容构造成文件路径加载请求。

### 5.4 目标校验与公开面

应用只读内置清单并导入 Catalog，然后用 SourceId 与包双入口建立绑定，执行 Solve、CreatePod。
Host 每次建局都按实际链接的目标描述符检查完整参与包，含禁用成员；实际包与清单漂移时，
在创建 Pod 和调用任何插件生命周期代码前拒绝。不重新生成或改写清单来让比对通过。

新增资源输入方法/小值类型属于 Catalog 宿主公开 API 增量。若增加 ResourceLocation 字段，需明确
记录 CatalogSource 的宿主 ABI 变化；不把这类改动声称为零公开面增量。
不改 PluginMeta / PluginDescriptor / PackageEntryPoints 布局，不 bump kHeaderVersion。

## 6. 移动端加载与生命周期边界

- 移动发布构建仅支持进程内来源。提供静态发布平台实现，文件路径装载入口一律以 Result 带名拒绝，
  不因 ANDROID / APPLE 宏匹配而接入桌面 dlopen 路径。
- 不编译桌面内存映像枚举、文件锁探针、动态替换通道作为移动能力；需要满足共享层链接的入口
  必须明确报不支持，不返回假句柄或声称已卸载。
- Eject 已有“linked-in binary cannot be ejected”早闸继续适用；Adopt 不增加进程内入口。
  不以强制卸载或跳过身份检查换取可编译。
- 插件间仍经 Context 服务解析落账本，构造函数不注册副作用；应用不缓存服务借用跨过 DestroyPod。
- JNI 发起 Host 创建、建局和销局的调用必须位于同一绑定线程，不能因 Java 调用来源可变就放宽单线程契约。
- DestroyPod 回收实例、Effect、服务、订阅与账本；代码及进程级状态随最终应用驻留。
  不把“反复建局 Clean”写成“进程级状态已重置”或“代码已卸载”。

## 7. 仓库内链接依赖与接入示例

静态组合库本身只含分发表，并不自动把依赖归档的成员合并进同一个 `.a`。仓库内最终链接必须包含：

- 包分发表归档、每个被选择的成员 `.Pack` 归档及 VasePod / Host / Catalog 归档。
- 对应生成入口头、所需公共头；最终应用资源中包含预生成的候选 pack.json。
- 明确描述上述依赖的现有 CMake 目标，由其表达传递链接关系；不要求应用手猜归档顺序。
- Android 示例最终 JNI `.so` 和同 NDK libc++ 运行依赖；iOS 应用链接证人的产物与检查记录。

最终链接必须依赖正常硬引用链提取包成员，不使用 whole-archive / force_load 把保活问题掩盖掉。
不建立脱离原构建树的独立 SDK 目录、通用 install/export 或树外消费测试；这不降低仓库内最终链接
必须包含完整归档依赖的要求。
复用现有 Provider/Consumer fixture 源及服务契约，避免维护第二套测试插件逻辑。
移动示例中的库名是仓库示例名，不把产品专属 JNI 名称写进 Vase 的公开接口。

Android 先提供可构建的 JNI 集成与最小 APK 验证壳。参考 CppDemo 的 Gradle/NDK/CMake 版本，
但不修改 CppDemo，也不复制其业务界面或整个生成工程。测试读取打包资源、建局并返回结构化成功/失败，
不能只打印 Value=7 让构建机 grep 成功文本。

iOS 使用 C++/Objective-C++ 应用入口做干净最终链接验证，不安装 Xcode、不在本波要求签名、
安装或 App Store 打包。未在设备执行的初始化和销局路径如实列为未验证。

## 8. 验收矩阵与完成边界

| 验收项 | 判据 |
|---|---|
| Android 干净构建 | arm64-v8a 的包/框架/成员归档及 JNI `.so` 均生成；文件头为 AArch64；自有代码零构建警告 |
| iOS 干净构建 | 包、成员、框架真实归档；最终链接证人为 Mach-O arm64，平台 iOS、SDK 与 deployment target 符合构建配置 |
| 仓库内链接完整 | 通过 CMake 传递依赖实际链接包、成员及框架归档；最终链接成功，无人工追加遗漏归档 |
| 保活 | 干净最终链接包含 Provider、Consumer 与包专属入口；作为链接输入但未选入包的负对照成员不进入产物 |
| 离线生成与打包 | 未连接手机也能生成候选清单并打包；策略变化会重生成，生成失败不会偷偷消费旧资源 |
| Android 目标运行 | 在实际 arm64 执行环境中加载 JNI 库，直接读取 APK 内置清单、资源导入/Solve/CreatePod，服务消费 Value=7 |
| 重复建局销局 | 同一 Host 内按 7/3/7 配置循环多轮；每轮消费新值，DestroyPod Clean，账本/实例/作用域归零 |
| 目标元信息边界 | 目标条件编译改变公开元信息时实际比对拒绝；不自动重写清单，也不声称此类差异已获支持 |
| 资源与文件解析同源 | 相同 JSON 在文件/内容入口得到同形结果，坏格式/unknown 字段/重复项等规则不分叉 |
| 只读启动 | 应用建局路径不调用生成器或清单写出，无需复制清单或创建可写缓存目录 |
| 资源失败 | 声明资源缺失、读取失败、坏格式或重复来源/插件 Id 均失败；保留旧快照，不继续本次建局 |
| 资源所有权与归因 | 导入后释放输入仍可 Solve，失败保留快照；错误保留逻辑资源位置，不伪造绝对磁盘路径 |
| 预检独立性 | 篡改内置候选清单或传错包入口，含禁用成员的漂移也在创建 Pod / 插件 OnLoad 前被拒绝 |
| 生命周期边界 | 进程内 Eject 拒绝；移动文件来源装载拒绝；销局后的服务借用不被宿主保存 |
| 桌面回归 | 根 CLAUDE 要求的八线、-N/名字集合、四线 tidy、format；触发规矩 6 时三平台专项证据 |

Android 设备暂未连接，不因这点停止可独立进行的构建工作；但不能将交叉编译或桌面执行替代 Android
运行证据。没有 arm64 执行环境时，报告“构建/链接完成，目标运行未验证”，不宣布完整闭环完成。
iOS 本波承诺静态构建与最终链接验证；设备运行、签名与安装另行安排，未验证项明确保留。

先以 Release 验证移动发布产物；桌面 Debug/Release 原矩阵保持。新增移动配置、命令、实际
用例基数及验证证据在实现后统一回填根 CLAUDE，不从桌面数推导移动测试数。

## 9. 决策状态与文档义务

- 已确认：Android A；iOS 静态；Android B 后续；先 Android 再 iOS；不安装 Xcode。
- grilling Q1–Q3：预生成清单且正常启动不依赖写盘；仅仓库内产品集成；最低 Android API 29 / iOS 15.0。
- grilling Q4–Q6：C++ 描述符唯一元信息真值；构建打包不依赖手机；包清单仅为产品内置只读资源。
- grilling Q7：宿主与移动目标公开元信息必须一致；平台实现可不同，元信息漂移拒绝，本波不自动适配。
- 用户已确认最终共识。宿主候选清单、只读资源导入及证据分层据此落文，不能当作已经实现。
- 旧草案中的启动生成/写盘、多包目录替换、应用包准备库、独立 SDK 交付目录均已撤回。
- 现实验证条件：Android arm64 设备尚未连接；iOS 独立 SDK 探针通过，项目构建/运行尚未发生。
- 实现后更新 CLAUDE 项目状态/命令/门禁表、README 接入说明、wiki 发布形态与移动边界，
  以及 C++ 技能中“移动实现不存在”“发布必须静态”等状态句；按规矩 7 保持字面真值单点。
- 本文只记录设计与已有环境事实，不把建议中的目标、函数或目录写成仓库已支持的接口。

## 10. 外部依据

- [Android NDK CMake](https://developer.android.com/ndk/guides/cmake)：官方 NDK 工具链及 Android 构建参数。
- [Android C++ runtime](https://developer.android.com/ndk/guides/cpp-support)：多共享库的运行库一致性要求；
  交付 `.a` 不等于必须选择 `c++_static`，需按最终进程的 DSO 布局决定。
- [Apple Command Line Tools](https://developer.apple.com/documentation/xcode/installing-the-command-line-tools/)：
  独立 Command Line Tools 提供 macOS SDK；iPhoneOS SDK 已从用户提供的压缩包额外提取，未安装 Xcode。
- [CMAKE_OSX_SYSROOT](https://cmake.org/cmake/help/v4.0/variable/CMAKE_OSX_SYSROOT.html)：
  iOS 可显式指定独立 SDK 的路径；sysroot 应在 project / enable_language 前配置。
