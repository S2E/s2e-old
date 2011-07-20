package ch.epfl.s2e.android;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class S2EAndroidActivity extends Activity {
    private static final String DEBUG_TAG = "S2EAndroidActivity";
    
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
    }
    
    public void onCallNative1(View view) {
        // call to the native method
    	helloLog("This will log to LogCat via the native call.");
    }
    
    public void onCallNative2(View view) {
        String result = getString(5,2);
        Log.v(DEBUG_TAG, "Result: "+result);
        
        result = getString(105, 1232);
        Log.v(DEBUG_TAG, "Result2: "+result);
    }
    
    public void onCallS2EVersion(View view) {
        String result = getS2EVersion();
        Log.v(DEBUG_TAG, "Result: "+result);
    }
    
    private native void helloLog(String logThis);
    private native String getString(int value1, int value2);
    public native String getS2EVersion();
    
    static {
        System.loadLibrary("android");
    }
    
}