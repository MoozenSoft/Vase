package com.moozen.vase.mobile;

/** Immutable result returned from the synchronous JNI scenario. */
public final class NativeResult {
    public final boolean success;
    public final int completedRounds;
    public final String error;

    public NativeResult(boolean success, int completedRounds, String error) {
        this.success = success;
        this.completedRounds = completedRounds;
        this.error = error;
    }
}
