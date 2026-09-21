# 导入表执法 fixture（T13 §8.7）：一对真互链的插件——A 链 B 的文件，A 因此拿到一条
# 指向 B 的**真实**导入条目（不是伪造字节：12 节 3e 写「伪造导入表」，真互链是它的
# 超集，且零工具依赖）。形态定义只走 vase_add_plugin_fixture。
#
# **只写 LINK_LIBRARIES 不产生条目**——两端代码必须真有一条引用，理由写在
# BadLinkSiblingA.cpp / BadLinkSiblingB.cpp 的顶部（只链不引 = Windows 的空导入库，
# 3e 会为错的原因绿）。Linux 侧无引用的 .so 留不留取决于 `--as-needed`，**本机 clang
# 驱动默认不开**，于是那一侧的 NEEDED 只是偶然活着——别把它读成「链接器已经在保护我们」。
vase_add_plugin_fixture(BadLinkSiblingB
    SOURCES BadLinkSiblingB/BadLinkSiblingB.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(BadLinkSiblingA
    SOURCES BadLinkSiblingA/BadLinkSiblingA.cpp
    LINK_LIBRARIES VasePod BadLinkSiblingB)
