vase_add_plugin_fixture(PackageConsumerPlugin
    SOURCES PackageConsumerPlugin.cpp
    LINK_LIBRARIES VasePod PackageServices
    MANIFEST "plugin.json"
    PACK)
