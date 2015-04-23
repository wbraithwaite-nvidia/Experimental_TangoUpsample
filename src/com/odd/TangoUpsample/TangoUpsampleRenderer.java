
package com.odd.TangoUpsample;

//----------------------------------------------------------------------------------------
import android.opengl.GLSurfaceView;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;
import android.util.Log;
import java.nio.ByteBuffer;

//----------------------------------------------------------------------------------------
public class TangoUpsampleRenderer 
	implements GLSurfaceView.Renderer
{
	private static String			TAG = "TangoUpsample";

    public TangoUpsampleActivity	activity;
	public int						width_ = 0, height_ = 0;
    public boolean					isAutoRecovery = true;
	public boolean					useDepth = true;


    public void onDrawFrame(GL10 gl) 
	{
        TangoUpsampleNative.render(width_, height_);
    }

	// Called after the surface is created and whenever the OpenGL ES surface size changes. 
    public void onSurfaceChanged(GL10 gl, int width, int height) 
	{
        TangoUpsampleNative.connectTexture();
        TangoUpsampleNative.connectService();
		TangoUpsampleNative.connectCallbacks();
		width_ = width;
		height_ = height;
    }

	// Called when the rendering thread starts and whenever the EGL context is lost. 
	// The EGL context will typically be lost when the Android device awakes after going to sleep. 
	// Or on a resume from pause.
    public void onSurfaceCreated(GL10 gl, EGLConfig config) 
	{
		// All resources are destroyed when the EGL-context is recreated. So we recreate.
        TangoUpsampleNative.setupGlResources();

		// this has to be here, because the initialization requires GL resources to exist.
        TangoUpsampleNative.initialize(activity);
        TangoUpsampleNative.setupConfig(isAutoRecovery, useDepth);
    }

	public void notifyPausing() 
	{
		/*
		TangoUpsampleNative.disconnectService();
		TangoUpsampleNative.destroyGlResources();
		width_ = height_ = 0;
		*/
	}



	public static class ContextFactory implements GLSurfaceView.EGLContextFactory 
	{
		private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
		private int glVersion_;

		private static void checkEglError(String prompt, EGL10 egl) 
		{
			int error;
			while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) 
				Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
		}

		public ContextFactory(double version)
		{
			glVersion_ = (int)version;
		}

		public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) 
		{
			int glVersion = 3;
			Log.w(TAG, "creating OpenGL ES " + glVersion_ + " context");
			int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, glVersion_, EGL10.EGL_NONE };
			// attempt to create a OpenGL ES 3.0 context
			EGLContext context = egl.eglCreateContext( display, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
			checkEglError("After eglCreateContext", egl);
			return context; // returns null if 3.0 is not supported;
		}

		public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) 
		{
            egl.eglDestroyContext(display, context);
        }
	}

}
