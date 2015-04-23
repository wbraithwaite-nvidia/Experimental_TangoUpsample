
package com.odd.TangoUpsample;

public class TangoUpsampleNative 
{
    static {
        System.loadLibrary("OddTangoUpsample");
    }

    public static native int initialize(TangoUpsampleActivity activity);
    public static native void setupConfig(boolean isAutoRecovery, boolean useDepth);

    public static native void connectTexture();
    public static native int connectCallbacks();

    public static native int connectService();
    public static native void disconnectService();
	public static native void resetMotionTracking();
	public static native boolean getIsLocalized();

    public static native void setupGlResources();
	public static native void destroyGlResources();

    public static native void render(int portWidth, int portHeight);
    public static native void setCamera(int cameraIndex);

    public static native byte updateStatus();

    public static native String getPoseString();
    public static native String getVersionNumber();
}

