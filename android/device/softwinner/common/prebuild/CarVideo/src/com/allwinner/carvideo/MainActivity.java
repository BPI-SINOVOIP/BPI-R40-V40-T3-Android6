package com.allwinner.carvideo;

import java.io.File;


import com.allwinner.carvideo.videoplay.VideoPlayActivity;

import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.os.StatFs;
import android.provider.MediaStore;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.graphics.SurfaceTexture;
import android.text.method.HideReturnsTransformationMethod;
import android.util.Log;
import android.view.Menu;
import android.view.TextureView;
import android.view.TextureView.SurfaceTextureListener;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity extends Activity implements SurfaceTextureListener,ServiceData{
	
	private static final String TAG="CarVideo_MainActivity";
	private static final int UPDATE_RECORD_TIME = 1;
	private static final int HIDDEN_CTL_MENU_BAR = 2;
	private static final int START_RECORDER = 3;

	private TextureView mTextureView;
	private SurfaceTexture mSurfaceTexture;
	private VideoService mService = null;
	private Context mContext;
	private TextView mRecordTime;
	private LinearLayout mCtrlMenuBar;
	private ImageButton mRecordButton;
	private final Handler mHandler = new MainHandler();
	
	private OnLayoutChangeListener mLayoutListener = new OnLayoutChangeListener() {
        @Override
        public void onLayoutChange(View v, int left, int top, int right,
                int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
            
        }
    };
    
    private ServiceConnection mVideoServiceConn = new ServiceConnection() {
		public void onServiceConnected(ComponentName classname, IBinder obj) {
			
			mService = ((VideoService.LocalBinder) obj).getService();
			mService.setRecordBackground(false);
			Log.d(TAG,"mService=" + mService);
			mService.registerCallback(mVideoCallback);
		
		
			if(getRecordingState())
			{
				Log.d(TAG,"onStart############# RecordingState = true");	
				
				mRecordTime.setVisibility(View.VISIBLE);
				mRecordButton.setImageResource(R.drawable.pause_select);
			}
		}

		public void onServiceDisconnected(ComponentName classname) {
			mService = null;
		}
	};
	
	private IVideoCallback.Stub mVideoCallback = new IVideoCallback.Stub() {
		@Override
		public void onUpdateTimes(String times) throws RemoteException {
			// TODO Auto-generated method stub
			mHandler.removeMessages(UPDATE_RECORD_TIME);
        	Message message = new Message();
			message.what = UPDATE_RECORD_TIME;
			message.obj = times;
       		mHandler.sendMessage(message);
		}
	};
	
	private OnClickListener mVideoSurfaceClicked = new OnClickListener() {
		@Override
		public void onClick(View v) {
			mHandler.removeMessages(HIDDEN_CTL_MENU_BAR);
			showCtrlMenuBar();
		}
	};
	
	
	private class MainHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
        	//Log.v(TAG, "handleMessage message: " + msg.what);
            switch (msg.what) {
				case UPDATE_RECORD_TIME: {
                    updateRecordingTime((String)msg.obj);
                    break;
                }
				case HIDDEN_CTL_MENU_BAR:
				{
					hiddeCtrlMenuBar();
					break;
				}
				
				case START_RECORDER:
					startVideoRecording_l();
					break;
                default:
                    
                    break;
            }
        }
    }
	
	private void showCtrlMenuBar()
	{
		mCtrlMenuBar.setVisibility(View.VISIBLE);
		mHandler.sendEmptyMessageDelayed(HIDDEN_CTL_MENU_BAR, TIMES_OUT);
	}
	
	private void hiddeCtrlMenuBar()
	{
		mCtrlMenuBar.setVisibility(View.GONE);
	}
	
	public void onCtrlMenuBarClick(View view)
	{
		int id = view.getId();
		mHandler.removeMessages(HIDDEN_CTL_MENU_BAR);
		switch(id)
		{
			case R.id.recordbutton:
				if(getRecordingState())
				{
					stopVideoRecording();
				}
				else
				{
					startVideoRecording();
				}
				break;
			case R.id.settingbutton:
				//Intent settingIntent = new Intent(MainActivity.this, VideoSettingActivity.class);
				//startActivity(settingIntent);
				mService.setNextFileName();
				//VideoStorage.deleteVideoFile(getContentResolver());
				break;
			case R.id.playbutton:
				Intent playIntent = new Intent(MainActivity.this, VideoPlayActivity.class);
				startActivity(playIntent);			
				break;
		}
		mHandler.sendEmptyMessageDelayed(HIDDEN_CTL_MENU_BAR, TIMES_OUT);
	}
	
	
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mContext = this;
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        this.requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.activity_main);
        mContext.startService(new Intent(MainActivity.this,VideoService.class));
        mTextureView = (TextureView)findViewById(R.id.preview_content);
        mTextureView.setSurfaceTextureListener(this);
        mTextureView.setOnClickListener(mVideoSurfaceClicked);
        mTextureView.addOnLayoutChangeListener(mLayoutListener);
        mRecordTime = (TextView)findViewById(R.id.recording_time);
        mCtrlMenuBar = (LinearLayout)findViewById(R.id.ctrl_menu_bar);
		Log.d(TAG,"onCreate##################");
        mRecordButton = (ImageButton)findViewById(R.id.recordbutton);
        
 
    }

	@Override
	public void onSurfaceTextureAvailable(SurfaceTexture surface, int width,
			int height) {
		// TODO Auto-generated method stub
		mSurfaceTexture = surface;
		
		onPreviewUIReady();
	}

	@Override
	public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
		// TODO Auto-generated method stub
		mSurfaceTexture = null;
		
		Log.d(TAG, "surfaceTexture is destroyed");
        onPreviewUIDestroyed();
        return true;
	}

	@Override
	protected void onResume() {
		// TODO Auto-generated method stub
		super.onResume();
		showCtrlMenuBar();
		Log.d(TAG, "onResume ################");	
	}

	@Override
	public void onStart() {
		Log.d(TAG,"onStart#############");
		super.onStart();
		mRecordButton.setImageResource(R.drawable.record_select);
		if (null == mService) {
			Intent intent = new Intent(this, VideoService.class);
			if (!mContext.bindService(intent, mVideoServiceConn, Context.BIND_AUTO_CREATE)) {
				Log.e(TAG, "Failed to bind to VideoService###################");
			}
		}
		else
		{
			mService.setRecordBackground(false);
			mService.registerCallback(mVideoCallback);
			if(getRecordingState())
			{
				Log.d(TAG,"onStart############# RecordingState = true");	
				mService.setRecordBackground(false);
				mRecordTime.setVisibility(View.VISIBLE);
				mRecordButton.setImageResource(R.drawable.pause_select);
			}
		}	
	}
	
	@Override
	public void onStop() {
		Log.d(TAG,"onStop#############");
		if(mService == null)
		{
			super.onStop();
			return;
		}	
		mService.unregisterCallback(mVideoCallback);
		if(getPreviewState())
		{
			mService.stopPreview();
			mService.closeCamera();
		}
		else if(getRecordingState())
		{
			mService.setRecordBackground(true);
		}
		else
		{
			mService.closeCamera();
		}
		super.onStop();
	}
	
	@Override
	protected void onDestroy() {
		// TODO Auto-generated method stub
		Log.d(TAG,"onDestroy#############");
		if(getRecordingState())
		{
			mService.setRecordBackground(true);
		}
		else if(getPreviewState())
		{
			mService.stopPreview();
			mService.closeCamera();
		}
		if (mService != null) 
		{
			mContext.unbindService(mVideoServiceConn);
			mService = null;
		}
		super.onDestroy();
	}

	@Override
	public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width,
			int height) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onSurfaceTextureUpdated(SurfaceTexture surface) {
		// TODO Auto-generated method stub
		
	}
    
	private void onPreviewUIReady()
	{
		if (getRecordingState()) {
			//startVideoRecording();
        } else{
           // startPreview();
        }
		startPreview();
	}
	
	private boolean getPreviewState()
	{
		if(mService != null)
		return mService.getPreviewState();
		return false;
	}

	private boolean getRecordingState()
	{
		if(mService != null)
		return mService.getRecordingState();
		return false;
	}
	
	private void onPreviewUIDestroyed()
	{
		stopPreview();
	}
	
	private void startPreview()
	{

		if((mService != null) && (mSurfaceTexture != null))
		{		
			mService.startPreview(mSurfaceTexture);
		}
		else
		{
			Log.e(TAG,"startPreview error!! mService=" + mService + " or mSurfaceTexture=" + mSurfaceTexture);
		}
	}
	
	private void stopPreview()
	{
		if(getPreviewState())
		{
			if(mService != null)
			{
				mService.stopPreview();	
			}
		}
	}
	
	private void startVideoRecording_l()
	{
		if(mService.startVideoRecording(mSurfaceTexture) != FAIL)
		{
			Log.d(TAG,"startVideoRecording ok");
			mRecordTime.setText("");
			mRecordTime.setVisibility(View.VISIBLE);
			mRecordButton.setImageResource(R.drawable.pause_select);
		}
	}
	
	private void startVideoRecording()
	{
		Log.d(TAG,"startVideoRecording");
		if((mService == null) || (mSurfaceTexture == null))
			return;
		mHandler.sendEmptyMessage(START_RECORDER);
	}
	
	private void stopVideoRecording()
	{
		if(mService == null)
			return;
		if(mService.stopVideoRecording() == OK)
		{
			Log.d(TAG,"stopVideoRecording = ok");
			mRecordTime.setVisibility(View.GONE);
			mRecordButton.setImageResource(R.drawable.record_select);
			stopPreview();
			startPreview();
		}
		
	}
	
	private void updateRecordingTime(String text)
	{
		//Log.d(TAG,"updateRecordingTime text:" + text);
		mRecordTime.setText(text);
	}
	
	@Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }
}
