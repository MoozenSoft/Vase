package com.moozen.vase.mobile;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

/** Minimal launcher for visually inspecting the scenario used by instrumentation. */
public final class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        NativeResult result = NativeBridge.runScenario(getAssets());
        TextView output = new TextView(this);
        output.setPadding(24, 32, 24, 24);
        if (result.success) {
            output.setText("Vase Mobile: " + result.completedRounds + " clean Pod rounds");
        } else {
            output.setText("Vase Mobile failure: " + result.error);
        }
        setContentView(output);
    }
}
