
package com.odd.TangoUpsample;

//----------------------------------------------------------------------------------------
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Point;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.Display;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;
import android.util.Log;

//----------------------------------------------------------------------------------------
public class TangoUpsampleActivity 
	extends Activity 
	implements View.OnClickListener
{
	private static final String		TAG = "TangoUpsample";

	private TangoUpsampleRenderer	renderer_;
    private GLSurfaceView			view_;
    private TextView				tangoPoseStatusText_;
    private String					appVersionString_;

    public static final String		EXTRA_KEY_PERMISSIONTYPE = "PERMISSIONTYPE";
    public static final String		EXTRA_VALUE_VIO = "MOTION_TRACKING_PERMISSION";
    public static final String		EXTRA_VALUE_VIOADF = "ADF_LOAD_SAVE_PERMISSION";

	//----------------------------------------------------------------------------------------
    @Override
    protected void onCreate(Bundle savedInstanceState) 
	{
        super.onCreate(savedInstanceState);

        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);

        Intent intent1 = new Intent();
        intent1.setAction("android.intent.action.REQUEST_TANGO_PERMISSION");
        intent1.putExtra(EXTRA_KEY_PERMISSIONTYPE, EXTRA_VALUE_VIO);
        startActivityForResult(intent1, 0);

        Intent intent2 = new Intent();
        intent2.setAction("android.intent.action.REQUEST_TANGO_PERMISSION");
        intent2.putExtra(EXTRA_KEY_PERMISSIONTYPE, EXTRA_VALUE_VIOADF);
        startActivityForResult(intent2, 0);

        setTitle(R.string.app_name);        
		setContentView(R.layout.tango_upsample);

		// create the view and set the renderer...
        view_ = (GLSurfaceView)findViewById(R.id.surfaceview);
		view_.setEGLContextFactory(new TangoUpsampleRenderer.ContextFactory(3.0));
        renderer_ = new TangoUpsampleRenderer();
        renderer_.activity = TangoUpsampleActivity.this;
        renderer_.isAutoRecovery = true;
		renderer_.useDepth = true;
        view_.setRenderer(renderer_);

        tangoPoseStatusText_ = (TextView)findViewById(R.id.debug_info);
		tangoPoseStatusText_.setTextColor(0xffff00ff);
		//tangoPoseStatusText_.setBackgroundColor(0xff000000);

        try {
            PackageInfo pInfo = this.getPackageManager().getPackageInfo(this.getPackageName(), 0);
            appVersionString_ = pInfo.versionName;
        } catch (NameNotFoundException e) {
            e.printStackTrace();
            appVersionString_ = " ";
        }

        findViewById(R.id.reset).setOnClickListener(this);
        findViewById(R.id.first).setOnClickListener(this);
        findViewById(R.id.third).setOnClickListener(this);
        findViewById(R.id.first_color).setOnClickListener(this);
        findViewById(R.id.first_fisheye).setOnClickListener(this);
        findViewById(R.id.first_pointcloud).setOnClickListener(this);
		/*
        new Thread(new Runnable() 
		{
            @Override
            public void run() {
                while (true) {
                    try {
                        Thread.sleep(10);	// every 10ms

                        runOnUiThread(new Runnable() {
                            public void run() {
                                boolean isLocalized = TangoUpsampleNative.getIsLocalized();
                                if(isLocalized) {
                                    findViewById(R.id.reset).setVisibility(View.GONE);
                                } else {
                                    findViewById(R.id.reset).setVisibility(View.VISIBLE);
                                }
                                tangoPoseStatusText_.setText(TangoUpsampleNative.getPoseString());
                            }
                        });

                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
        }).start();
		*/
    }

	//----------------------------------------------------------------------------------------
	@Override
    protected void onDestroy() 
	{
        super.onDestroy();
    }

	//----------------------------------------------------------------------------------------
    @Override
    protected void onResume() 
	{
        super.onResume();
        view_.onResume(); // GLSurfaceView clients must pass this along.

		View decorView = getWindow().getDecorView();
		int uiOptions = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                      | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                      | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                      | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                      | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                      | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
		decorView.setSystemUiVisibility(uiOptions);
    }

	//----------------------------------------------------------------------------------------
    @Override
    protected void onPause() 
	{
        super.onPause();

		view_.queueEvent(new Runnable() {
			@Override public void run() {
				renderer_.notifyPausing(); // notify renderer of pause for resource cleanup.
			}
		});

        view_.onPause(); // GLSurfaceView clients must pass this along.

		//TangoUpsampleNative.disconnectService();
    }

	//----------------------------------------------------------------------------------------
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) 
	{
        if (requestCode == 0) {
            if (resultCode == RESULT_CANCELED) {
                Toast.makeText(this,
                "Motion Tracking Permission Needed!", Toast.LENGTH_SHORT).show();
                finish();
            }
        }
        if (requestCode == 1) {
            if (resultCode == RESULT_CANCELED) {
                Toast.makeText(this,
                "ADF Permission Needed!", Toast.LENGTH_SHORT).show();
                finish();
            }
        }
    }

	//----------------------------------------------------------------------------------------
    @Override
    public void onClick(View v) 
	{
        switch (v.getId()) {
        case R.id.reset:
            TangoUpsampleNative.resetMotionTracking();
            break;
        case R.id.first:
            TangoUpsampleNative.setCamera(1);
            break;
        case R.id.third:
            TangoUpsampleNative.setCamera(0);
            break;
        case R.id.first_color:
            TangoUpsampleNative.setCamera(2);
            break;
        case R.id.first_fisheye:
            TangoUpsampleNative.setCamera(3);
            break;
        case R.id.first_pointcloud:
            TangoUpsampleNative.setCamera(4);
            break;
        }
    }

	//----------------------------------------------------------------------------------------
}

//----------------------------------------------------------------------------------------
