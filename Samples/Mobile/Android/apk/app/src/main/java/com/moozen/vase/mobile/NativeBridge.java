package com.moozen.vase.mobile;

import android.content.res.AssetManager;

/** Never caches native Host, Pod or service references between calls. */
public final class NativeBridge {
    static {
        System.loadLibrary("VaseMobileJni");
    }

    private NativeBridge() {
    }

    public static native NativeResult runScenario(AssetManager assets);

    /** Test hook: changes one manifest in memory only, never the APK assets. */
    public static native NativeResult runScenarioWithResourceForTest(
            AssetManager assets, String sourceId, byte[] replacementBytes);

    /** Test-only: preflight two real descriptors in one participating package. */
    public static native NativeResult runDisabledMemberProbeForTest(byte[] syntheticPackage);
}
