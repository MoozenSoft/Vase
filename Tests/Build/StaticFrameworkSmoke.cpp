#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"

int main()
{
    vase::PluginCatalog catalog;
    if (!catalog.RefreshResources({}).IsOk())
    {
        return 1;
    }
    vase::PluginHost host;
    const auto pod = host.CreatePod(vase::LoadPlan{});
    if (!pod.IsOk())
    {
        return 2;
    }
    return host.DestroyPod(pod.Value()).Clean() ? 0 : 3;
}
