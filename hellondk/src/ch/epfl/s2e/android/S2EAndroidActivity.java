package ch.epfl.s2e.android;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.net.UnknownHostException;

import android.app.Activity;
import android.content.Context;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.Toast;

public class S2EAndroidActivity extends Activity {
    private static final String DEBUG_TAG = "S2EAndroidActivity";
    
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        LocationManager locmgr = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
    	locmgr.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1, 0, new LocationListener() {
			
			@Override
			public void onStatusChanged(String provider, int status, Bundle extras) {
				Log.i(DEBUG_TAG, "Status of provider "+provider+" changed to "+status);
			}
			
			@Override
			public void onProviderEnabled(String provider) {
				Log.i(DEBUG_TAG, "Provider "+provider+" enabled");
			}
			
			@Override
			public void onProviderDisabled(String provider) {
				Log.i(DEBUG_TAG, "Provider "+provider+" disabled");				
			}
			
			@Override
			public void onLocationChanged(Location location) {
				Log.i(DEBUG_TAG, "Location changed");
				sendToServer("lon: "+location.getLongitude()+"\n lat:"+location.getLatitude());
				
				
			}
		});
        
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
    private native String getS2EVersion();
    
    static {
        System.loadLibrary("android");
    }
    
    public void onClickSendLocation(View view) {
        Log.i(DEBUG_TAG, "Location will be transmitted when the next Location change arrives (use DDMS to invoke location change)");
    }
    
    private void sendToServer(String message) {
    	Socket echoSocket = null;
        PrintWriter out = null;
        BufferedReader in = null;

        try {
            echoSocket = new Socket("10.0.2.2", 6667);
            out = new PrintWriter(echoSocket.getOutputStream(), true);
            in = new BufferedReader(new InputStreamReader(
                                        echoSocket.getInputStream()));
            
            //send longitude and latitude
    	    out.println(message);

    	    //close connection
	    	out.close();
	    	in.close();
	    	echoSocket.close();     
            
        } catch (UnknownHostException e) {
        	e.printStackTrace();
        	Log.e(DEBUG_TAG, "Don't know about host: 10.0.2.2.");
        } catch (IOException e) {
        	e.printStackTrace();
        	Log.e(DEBUG_TAG, "Couldn't get I/O for "
                    + "the connection to: 10.0.2.2.");
//            System.exit(1);
        }

	}

	public void onClickSendUiid(View view) {
    		Log.d(DEBUG_TAG,"TODO UIID");
    		TelephonyManager tManager = (TelephonyManager)getSystemService(Context.TELEPHONY_SERVICE);
    		String uid = tManager.getDeviceId();
    		sendToServer(uid);
    }
    
}