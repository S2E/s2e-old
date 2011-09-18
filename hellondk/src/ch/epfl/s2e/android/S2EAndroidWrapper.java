package ch.epfl.s2e.android;

public class S2EAndroidWrapper {

	public static native int  getVersion();
	public static native void printMessage(String message);
	public static native void printWarning(String warning);
	public static native void printExpressionInt(int symbexpr, String name);
	public static native void enableForking();
	public static native void disableForking();
    public static native void enableInterrupts();
    public static native void disableInterrupts();
	public static native void killState(int status, String message);
	
	public static native int       getSymbolicInt(String name);
	public static native double    getSymbolicDouble(String name);
    public static native long      getSymbolicLong(String name);
    public static native float     getSymbolicFloat(String name);
    public static native boolean   getSymbolicBoolean(String name);
	
	public static native int       getExampleInt(int symbvar);
	public static native double    getExampleDouble(double symbvar);
	public static native long      getExampleLong(long symbvar);
	public static native float     getExampleFloat(float symbvar);
	public static native boolean   getExampleBoolean(boolean symbvar);
	
	public static native int       concretizeInt(int var);
	public static native double    concretizeDouble(double var);
	public static native long      concretizeLong(long var);
	public static native float     concretizeFloat(float var);
	public static native boolean   concretizeBoolean(boolean var);
	
	public static native void assertThat(boolean condition, String failMessage);
	
	public static native void traceAndroidLocation(String message);
	public static native void traceAndroidUID(String message);
	
	//Load the library
	  static {
	    System.loadLibrary("s2etest");
	  }
	
}
