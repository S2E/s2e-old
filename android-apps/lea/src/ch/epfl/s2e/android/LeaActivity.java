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
 * - test case for privacy analysis with Leakalizer.
 * - test communication with S2E over custom opcodes by invoking native methods
 * 
 * The application sends sensitive data over a socket. 
 * It registers for Location updates and sends the Location to the host where the emulator runs.
 * Moreover, the application leaks unique device id when a button is pressed.
 * Moreover, there are some methods that test the auto-symbex feature of Leakalizer.
 * 
 * @author Andreas Kirchner <akalypse@gmail.com>
 *
 */

public class LeaActivity extends Activity {
    private static final String DEBUG_TAG = "LeaActivity";
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
			    startLeakingLocation(location);
			}
		});
        
    }
    
    protected void startLeakingLocation(Location location) {
      S2EAndroidWrapper.printMessage("geo fix detected");
      double lon = location.getLongitude();
      double lat = location.getLatitude();
      if(lon == 0) {
//          lonEx = S2EAndroidWrapper.getExampleDouble(lon);
          sendToServer(""+lon);
          S2EAndroidWrapper.killState(0, "fin! leaked.");
      } else {
          S2EAndroidWrapper.printMessage("else branch");
            S2EAndroidWrapper.killState(0, "fin! not leaked (else).");
      }
      S2EAndroidWrapper.killState(0, "fin! not leaked.");
    }

    public void onCallNative1(View view) {
        int test = S2EAndroidWrapper.getSymbolicInt("testvar");
        Log.d(DEBUG_TAG, "Back from journey to S2E. Nice trip.");
        test+=1;
        Log.d(DEBUG_TAG, "Back from test+=1");
        S2EAndroidWrapper.printExpressionInt(test, "what?");
    }
    
    public void onCallNative2(View view) {
        Location loc = new Location("FakeProvider");
        startLeakingLocation(loc);
    }
    
    public void onCallS2EVersion(View view) {
        String result = "S2E Version: " +S2EAndroidWrapper.getVersion();
        Log.v(DEBUG_TAG, "Result: "+result);
        Toast.makeText(this, result, Toast.LENGTH_LONG).show();
    }
    
    private native void helloLog(String logThis);
    private native String getString(int value1, int value2);
    
    static {
        System.loadLibrary("s2eandroid");
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

	   public void onClickTestMaze(View view) {
	       testMaze();
	   }
	
	
	public void onClickTestAutoSymbexInts(View view) {
	    Log.d(DEBUG_TAG, "We are starting the routine testAutoSymbex and cover all branches!");
	    testAutoSymbexInts(true, 1, 2);
	    Log.d(DEBUG_TAG, "Returned from the routine testAutoSymbex");
	    S2EAndroidWrapper.printMessage("State is back from autosymbex-int journey.");
	}
	
	public void onClickTestAutoSymbexDoubles(View view) {
	        Log.d(DEBUG_TAG, "We are starting the routine testAutoSymbex and cover all branches!");
	        testAutoSymbexDoubles(true, 1, 2);
	        Log.d(DEBUG_TAG, "Returned from the routine testAutoSymbex");
	        S2EAndroidWrapper.printMessage("State is back from autosymbex-double journey.");
	    }
	   
	public void onClickTestAutoSymbexChars(View view) {
	        Log.d(DEBUG_TAG, "We are starting the routine testAutoSymbex and cover all branches!");
	        testAutoSymbexChars(true, 'a', 'b');
	        Log.d(DEBUG_TAG, "Returned from the routine testAutoSymbex");
	        S2EAndroidWrapper.printMessage("State is back from autosymbex-char journey.");
	}
	
    public void onClickTestAutoSymbexFloats(View view) {
           Log.d(DEBUG_TAG, "We are starting the routine testAutoSymbex and cover all branches!");
           testAutoSymbexFloats(true, (float)1.5, (float)2.0);
           Log.d(DEBUG_TAG, "Returned from the routine testAutoSymbex");
           S2EAndroidWrapper.printMessage("State is back from autosymbex-char journey.");
    }
	
	private static void testAutoSymbexInts(boolean ok, int x, int y) {
	    if (ok) {
	        if (x == y) {
	            S2EAndroidWrapper.killState(0, "(int) z: x == y");
	        } else {
	            S2EAndroidWrapper.killState(1, "(int) z: x != y");
	        }
	    } else {
            if (x == y) {
                S2EAndroidWrapper.killState(2, "(int) !z: x == y");
            } else {
                S2EAndroidWrapper.killState(3, "(int) !z: x != y");
            }	        
	    }
	}
	
	   private static void testAutoSymbexDoubles(boolean ok, double x, double y) {
	        if (ok) {
	            if (x == y) {
	                S2EAndroidWrapper.killState(0, "(double) z: x == y");
	            } else {
	                S2EAndroidWrapper.killState(1, "(double) z: x != y");
	            }
	        } else {
	            if (x == y) {
	                S2EAndroidWrapper.killState(2, "(double) !z: x == y");
	            } else {
	                S2EAndroidWrapper.killState(3, "(double) !z: x != y");
	            }           
	        }
	    }
	   
       private static void testAutoSymbexFloats(boolean ok, float x, float y) {
           if (ok) {
               if (x == y) {
                   S2EAndroidWrapper.killState(0, "(float) z: x == y");
               } else {
                   S2EAndroidWrapper.killState(1, "(float) z: x != y");
               }
           } else {
               if (x == y) {
                   S2EAndroidWrapper.killState(2, "(float) !z: x == y");
               } else {
                   S2EAndroidWrapper.killState(3, "(float) !z: x != y");
               }           
           }
       }
	   
       private static void testAutoSymbexChars(boolean ok, char x, char y) {
           if (ok) {
               if (x == y) {
                   S2EAndroidWrapper.killState(0, "(char) z: x == y");
               } else {
                   S2EAndroidWrapper.killState(1, "(char) z: x != y");
               }
           } else {
               if (x == y) {
                   S2EAndroidWrapper.killState(2, "(char) !z: x == y");
               } else {
                   S2EAndroidWrapper.killState(3, "(char) !z: x != y");
               }           
           }
       }	    
	
	
	// #####################################################################
	// #####           SYMBOLIC MAZE ported to JAVA                   ######
	// #####################################################################
	
	public void testMaze() {
	    
	    final int ITERS = 32;
	    final int H = 7;
	    final int W = 11;
	    int playerpos_x = 1;
	    int playerpos_y = 1;
	    int oldpos_x = 0;
	    int oldpos_y = 0;
	    //index of array maze represents the vertial position (y)
	    String[] maze = {   "+-+---+---+",
	                        "| |     |#|",
	                        "| | --+ | |",
	                        "| |   | | |",
	                        "| +-- | | |",
	                        "|     |   |",
	                        "+-----+---+" };
	    
	    int numIterations = 0;    //Iteration number
	    
//	    int[] program = new int[ITERS];
//	    for(int i= 0; i < ITERS; i++) {
//	        program[i] = S2EAndroidWrapper.getSymbolicInt("input"+i);
//	        
//	    }
	    int[] program = S2EAndroidWrapper.getSymbolicIntArray(ITERS,"input");

	    maze[playerpos_y] = replaceCharAt(maze[playerpos_y], playerpos_x, 'X');

	    //Iterate and run 'program'
	    while(numIterations < ITERS)
	    {
	        //print maze
	        StringBuilder map = new StringBuilder();
	        map.append("MAP:\n");
	        for(String curLine : maze) {
	            map.append("\t\t"+curLine+"\n");
	        }
	        map.append("\n\n");
            S2EAndroidWrapper.printMessage(map.toString());  
	        
	        //store old player position
	        oldpos_x = playerpos_x;
	        oldpos_y = playerpos_y;
	        
	        //Move player position depending on the actual command
	        
	        int testval = program[numIterations];
	        
	        if (testval == 0) {
                playerpos_y--;
	            S2EAndroidWrapper.printMessage("Maze: UP to "+playerpos_x+"/ "+playerpos_y);
	        } else if (testval == 1) {
	            playerpos_y++;
	            S2EAndroidWrapper.printMessage("Maze: DOWN to "+playerpos_x+"/ "+playerpos_y);
	        } else if (testval == 2) {
                playerpos_x--;
	            S2EAndroidWrapper.printMessage("Maze: LEFT to "+playerpos_x+"/ "+playerpos_y);
	        } else if (testval == 3) {
                playerpos_x++;
	            S2EAndroidWrapper.printMessage("Maze: RIGHT to "+playerpos_x+"/ "+playerpos_y);
	        } else {
	            S2EAndroidWrapper.printMessage("Maze: UNDEF at "+playerpos_x+"/ "+playerpos_y);
	            S2EAndroidWrapper.killState(0, "Maze: loose (undef)");
	        }

//XXX:  switch statement causes troubles with the constraint solver	        
//	        switch (program[numIterations])
//	        {
//	            case 0: 
//	                playerpos_y--;
//	                break;
//	            case 1:    
//	                playerpos_y++;
//	                break;
//	            case 2:  
//	                playerpos_x--;
//	                break;
//	            case 3:
//	                playerpos_x++;
//	                break;
//	            default:
//	                S2EAndroidWrapper.killState(0, "loose");
//	        }

	        
            char block = maze[playerpos_y].charAt(playerpos_x);
            S2EAndroidWrapper.printMessage("Block is: "+block);  
	        
	        //If hit the price, You Win!!
	        
            if (block == '#')
            {
                S2EAndroidWrapper.printMessage("Maze: WIN in "+numIterations+" steps.\n");
                StringBuilder solution = new StringBuilder();
                for(int j = 0; j < ITERS; j++) {
                    int step = S2EAndroidWrapper.concretizeInt(program[j]);
                    solution.append(step+",");
                }
                S2EAndroidWrapper.printMessage("Maze: Solution "+solution.toString()+"\n");
                S2EAndroidWrapper.killState(0, "Maze: win");
            }
	        //If something is wrong do not advance

	        if ( block != ' ') {
	            if (playerpos_y == 2   && block == '|' 
                                       && playerpos_x > 0 
                                       && playerpos_x < W) {
	                S2EAndroidWrapper.printMessage("Maze: Secret path.");                      
                } else {
    	            S2EAndroidWrapper.printMessage("Maze: BAD. Don't advance to "+playerpos_x+"/ "+playerpos_y);
    	            playerpos_x = oldpos_x;
    	            playerpos_y = oldpos_y;
                }
	        }

	        //If crashed to a wall! Exit, you loose
	        if (oldpos_x==playerpos_x && oldpos_y==playerpos_y)
                S2EAndroidWrapper.killState(0, "Maze: loose (wallcrash at "+playerpos_x+"/ "+playerpos_y+")");

	        //put the player on the maze...
//            char[] lineold = maze[oldpos_y].toCharArray();
//	        lineold[oldpos_x]=' ';
	        
	        
	        maze[playerpos_y] = replaceCharAt(maze[playerpos_y], playerpos_x, 'X');
	        
	        //increment iteration
	        
	        numIterations++;
	        S2EAndroidWrapper.printMessage("Maze: OK. Step # "+numIterations+ " starts with "+playerpos_x+"/ "+playerpos_y);
	    }

	    //You couldn't make it! You loose!
	    //printf("You loose\n");
	    S2EAndroidWrapper.killState(0, "loose");
	
    }
	
	public static String replaceCharAt(String s, int pos, char c) {
	    StringBuffer buf = new StringBuffer( s );
	    buf.setCharAt( pos, c );
	    return buf.toString( );
	  }
	
}