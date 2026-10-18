package com.moozen.vase.mobile;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.res.AssetManager;
import androidx.test.platform.app.InstrumentationRegistry;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;

public final class MobilePackageTest {
    private static final String PROVIDER_ASSET = "vase/Provider/pack.json";

    private static AssetManager targetAssets() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext().getAssets();
    }

    private static byte[] readAsset(AssetManager assets, String name) throws IOException {
        try (InputStream input = assets.open(name);
                ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[8192];
            for (int count; (count = input.read(buffer)) != -1; ) {
                output.write(buffer, 0, count);
            }
            return output.toByteArray();
        }
    }

    private static void assertRejected(NativeResult result) {
        assertFalse(result.success);
        assertEquals(0, result.completedRounds);
        assertTrue("Expected a rejection reason", result.error != null && !result.error.isEmpty());
    }

    @Test
    public void targetApkRunsFiftyCleanPods() {
        NativeResult result = NativeBridge.runScenario(targetAssets());
        assertTrue(result.error, result.success);
        assertEquals(50, result.completedRounds);
        assertEquals("", result.error);
    }

    @Test
    public void instrumentationApkCannotBorrowTargetAssets() {
        NativeResult result = NativeBridge.runScenario(
                InstrumentationRegistry.getInstrumentation().getContext().getAssets());
        assertRejected(result);
        assertTrue(result.error, result.error.contains("missing APK asset"));
    }

    @Test
    public void unmodifiedInMemoryResourceRunsFiftyCleanPods() throws IOException {
        byte[] original = readAsset(targetAssets(), PROVIDER_ASSET);
        NativeResult result = NativeBridge.runScenarioWithResourceForTest(
                targetAssets(), "Provider", original);
        assertTrue(result.error, result.success);
        assertEquals(50, result.completedRounds);
    }

    @Test
    public void malformedResourceIsRejectedBeforePodCreation() {
        NativeResult result = NativeBridge.runScenarioWithResourceForTest(
                targetAssets(), "Provider", "{not JSON".getBytes(StandardCharsets.UTF_8));
        assertRejected(result);
    }

    @Test
    public void changedMemberMetadataIsRejectedBeforePodCreation() throws IOException, JSONException {
        JSONObject provider = new JSONObject(
                new String(readAsset(targetAssets(), PROVIDER_ASSET), StandardCharsets.UTF_8));
        provider.getJSONArray("plugins").getJSONObject(0).put("version", "99.0.0");
        NativeResult result = NativeBridge.runScenarioWithResourceForTest(
                targetAssets(), "Provider", provider.toString().getBytes(StandardCharsets.UTF_8));
        assertRejected(result);
    }

    private static byte[] compositeParticipatingPackage(boolean drift)
            throws IOException, JSONException {
        JSONObject provider = new JSONObject(
                new String(readAsset(targetAssets(), PROVIDER_ASSET), StandardCharsets.UTF_8));
        JSONObject consumer = new JSONObject(
                new String(readAsset(targetAssets(), "vase/Consumer/pack.json"), StandardCharsets.UTF_8));
        JSONObject disabledMember = consumer.getJSONArray("plugins").getJSONObject(0);
        disabledMember.put("enabledByDefault", false);
        if (drift) {
            disabledMember.put("displayName", "deliberately-mutated-disabled-member");
        }
        provider.getJSONArray("plugins").put(disabledMember);
        return provider.toString().getBytes(StandardCharsets.UTF_8);
    }

    @Test
    public void participatingPackageWithDisabledMemberPassesWhenMetadataMatches()
            throws IOException, JSONException {
        NativeResult result = NativeBridge.runDisabledMemberProbeForTest(
                compositeParticipatingPackage(false));
        assertTrue(result.error, result.success);
        assertEquals(1, result.completedRounds);
        assertEquals("", result.error);
    }

    @Test
    public void participatingPackageRejectsDisabledMemberDriftBeforeStage0()
            throws IOException, JSONException {
        NativeResult result = NativeBridge.runDisabledMemberProbeForTest(
                compositeParticipatingPackage(true));
        assertRejected(result);
        assertTrue(result.error, result.error.contains("Vase.PackageConsumer"));
        assertTrue(result.error, result.error.contains("displayName"));
        assertTrue(result.error, result.error.contains("before-pod=true"));
    }

    @Test
    public void unknownReplacementSourceIsRejected() {
        NativeResult result = NativeBridge.runScenarioWithResourceForTest(
                targetAssets(), "Unknown", "{}".getBytes(StandardCharsets.UTF_8));
        assertRejected(result);
        assertTrue(result.error, result.error.contains("unknown test resource sourceId"));
    }
}
