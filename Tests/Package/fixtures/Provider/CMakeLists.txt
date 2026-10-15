vase_add_plugin_fixture(PackageProviderPlugin
    SOURCES PackageProviderPlugin.cpp
    LINK_LIBRARIES VasePod PackageServices
    MANIFEST "plugin.json"
    PACK)
