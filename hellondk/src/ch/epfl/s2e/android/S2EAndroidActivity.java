package ch.epfl.s2e.android;

import android.app.Activity;
import android.content.Context;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.View;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.net.UnknownHostException;

/**
 * This is a basic example written for two purposes:
 * - test case for privacy analysis with S2EAndroid.
 * - test communication with S2E over custom opcodes by invoking native methods
 * 
 * The application sends sensitive data over a socket. 
 * It registers for Location updates and sends the Location to the host where the emulator runs.
 * Moreover, the application leaks unique device id when a button is pressed.
 * 
 * 
 * @author Andreas Kirchner <akalypse@gmail.com>
 *
 */

public class S2EAndroidActivity extends Activity {
    private static final String DEBUG_TAG = "S2EAndroidActivity";
    private static final String HOST_IP = "10.0.2.2";
    private static final int HOST_PORT = 6667;
    
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
		        int test = S2EAndroidWrapper.getSymbolicInt("testvar");
				double lat = S2EAndroidWrapper.getSymbolicDouble("latitude");
				double lon = location.getLongitude();
				double latEx = 0;
				double lonEx = 0;
                if(lat <= 0) {
                    latEx = S2EAndroidWrapper.getExampleDouble(lat);
//                    lonEx = S2EAndroidWrapper.getExampleDouble(lon);
                } else {
                    latEx = S2EAndroidWrapper.getExampleDouble(lat);
//                    lonEx = S2EAndroidWrapper.getExampleDouble(lon);
                }
				
				String concat = new String("lon: "+lonEx+"\n lat:"+latEx);
				sendToServer(concat);	
			}
		});
        
    }
    
    public void onCallNative1(View view) {
        int test = S2EAndroidWrapper.getSymbolicInt("testvar");
        Log.d(DEBUG_TAG, "Back from journey to S2E. Nice trip.");
        test+=1;
        Log.d(DEBUG_TAG, "Back from test+=1.");  
    }
    
    public void onCallNative2(View view) {
        String result = getString(5,2);
        Log.v(DEBUG_TAG, "Result: "+result);
        
        result = getString(105, 1232);
        Log.v(DEBUG_TAG, "Result2: "+result);
    }
    
    public void onCallS2EVersion(View view) {
        String result = "S2E Version: " +S2EAndroidWrapper.getVersion();
        Log.v(DEBUG_TAG, "Result: "+result);
        Toast.makeText(this, "S2E Version:", Toast.LENGTH_LONG).show();
    }
    
    private native void helloLog(String logThis);
    private native String getString(int value1, int value2);
    
    static {
        System.loadLibrary("s2etest");
    }
    
    public void onClickSendLocation(View view) {
        LocationManager locmgr = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
        Location lastPos = locmgr.getLastKnownLocation(LocationManager.GPS_PROVIDER);
        if (lastPos == null) {
            Toast.makeText(this, "Location will be transmitted when the next Location change arrives (use DDMS to invoke location change)", Toast.LENGTH_LONG).show();
            Log.i(DEBUG_TAG, "Location will be transmitted when the next Location change arrives (use DDMS to invoke location change)");
        } else {
            sendToServer("LastKnownPosition: \t lon: "+lastPos.getLongitude()+"\n\t\t\t lat:"+lastPos.getLatitude());
        }
    }
    
    private void sendToServer(String message) {
    	Socket echoSocket = null;
        PrintWriter out = null;
        BufferedReader in = null;

        try {
            echoSocket = new Socket(HOST_IP, HOST_PORT);
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
        	Log.e(DEBUG_TAG, "Don't know about host "+HOST_IP);
        	Toast.makeText(this, "Don't know about host:"+HOST_IP+". Create a socket on port "+HOST_PORT+" with 'socket -sl 6667' on the host.", Toast.LENGTH_LONG).show();
        } catch (IOException e) {
        	e.printStackTrace();
        	Log.e(DEBUG_TAG, "Couldn't get I/O for "
                    + "the connection to:"+HOST_IP);
//            System.exit(1);
        }

	}

	public void onClickSendUiid(View view) {
    		TelephonyManager tManager = (TelephonyManager)getSystemService(Context.TELEPHONY_SERVICE);
    		String uid = tManager.getDeviceId();
    		sendToServer(uid);
    }
    
}