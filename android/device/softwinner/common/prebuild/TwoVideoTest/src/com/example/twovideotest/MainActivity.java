package com.example.twovideotest;

import java.io.IOException;
import android.media.CamcorderProfile;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.Camera.Parameters;
import android.util.Log;
import android.view.Menu;
import android.view.TextureView;
import android.view.TextureView.SurfaceTextureListener;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.view.Window;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;




//readme
/*for T3 Camera
 * 1:support 2 camera preview and recorder
 * 2:support recorder in background
 * {
 * 		startRender
 * 		stopRender
 * }
 * 3:support watemart turn on and turn off
 * {
 * 		mCameraDevice[index].startWaterMark();
 * }
 * 
 * 4:support get cvbs status
 * {
 * 		int status = Camera.getCVBSInStatus(index);
 * }
 * 5:support notification when cvbs out or in 
 * 
 * {
 * 		IntentFilter filter = new IntentFilter("android.hardware.tvd.state.change");
		mReceiver = new Receiver();
		registerReceiver(mReceiver, filter);
 * }
 * 6:support cvbs set brightness and so on  
 * {
 * 		mCameraDevice[index].setAnalogInputColor(67, 50, 100); //setting brightness and so on
 * }
 * 
 * 6:mIsSupport2Video true:2 camera false:1 camera
 * 
 * 
 * CSI or usb camera
 *video0
 *video1
 *video2
 *video3
 *
 *
 *CVBS Camera
 *video4
 *video5
 *video6
 *video7
 *
 *If you use sonix camera with two camera video ,please reference 
 *android\device\softwinner\common\prebuild\CarVideo\
 *
 *This apk support:
 *camera0 preview (yuv,mjpeg)
 *camera1 recorder (720P or 1080p)
 *
 * 
 * 
 * 
 * 
 * 
 * 
 * */
public class MainActivity extends Activity implements MediaRecorder.OnErrorListener,MediaRecorder.OnInfoListener{

	private static final String TAG = "TwoVideoMainActivity";
	private static final int MAX_NUM_OF_CAMERAS = 2;
	private static final int UPDATE_RECORD_TIME = 1;
	private static final int HIDDEN_CTL_MENU_BAR = 2;
	private static final int UPDATE_RECORD_TIME1 = 3;
	private VideoService mService = null;
	private LinearLayout mCtrlMenuBar;
	private ImageButton mRecordButton;
	private ImageButton mRecordButton1;
	private ImageButton mBackButton;
	private ImageButton mBackButton1;
	private SurfaceTexture mSurfaceTexture0;
	private SurfaceTexture mSurfaceTexture1;
	private TextView mRecordTime;
	private TextView mRecordTime1;
	private BroadcastReceiver mReceiver;
	private static final int VIDEO0 = 0;
	private static final int VIDEO1 = 1;
	private static final int VIDEO2 = 2;
	private static final int VIDEO3 = 3;
	private static final int VIDEO4 = 4;
	private static final int VIDEO5 = 5;
	private static final int VIDEO6 = 6;
	private static final int VIDEO7 = 7;
	
	private int cameraid0 = VIDEO2;
	private int cameraid1 = VIDEO4;
	
	private static final boolean mIsSupport2Video = true;
	
	private final Handler mHandler = new MainHandler();
	
	private class MainHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
        	//Log.d(TAG, "handleMessage message: " + msg.what);
            switch (msg.what) {
				case UPDATE_RECORD_TIME: {
					//updateRecordingTime((String)msg.obj);
					mRecordTime.setText((String)msg.obj);
                    break;
                }
				case UPDATE_RECORD_TIME1:
					mRecordTime1.setText((String)msg.obj);
					break;
				case HIDDEN_CTL_MENU_BAR:
				{
				}
				break;
                default:
                    
                   break;
            }
        }
    }
	
	private IVideoCallback.Stub mVideoCallback = new IVideoCallback.Stub() {
		@Override
		public void onUpdateTimes(int index,String times) throws RemoteException {
			// TODO Auto-generated method stub
			mHandler.removeMessages(UPDATE_RECORD_TIME);
        	Message message = new Message();
        	//Log.d(TAG,"onUpdateTimes index=" + index);
        	if(index != mService.isUVCCameraSonix(index))
        	{
        		message.what = UPDATE_RECORD_TIME;
        	}
        	else if(index == cameraid0)
        	{
        		message.what = UPDATE_RECORD_TIME;
        	}
        	else
        	{
        		message.what = UPDATE_RECORD_TIME1;
        	}
			
			message.obj = times;
       		mHandler.sendMessage(message);
		}
	};
	
	private ServiceConnection mVideoServiceConn = new ServiceConnection() {
		public void onServiceConnected(ComponentName classname, IBinder obj) {
			
			mService = ((VideoService.LocalBinder) obj).getService();
			Log.d(TAG,"mService=" + mService);
			mService.registerCallback(mVideoCallback);
			
		}

		public void onServiceDisconnected(ComponentName classname) {
			
			if(mService != null)
			{
				mService.unregisterCallback(mVideoCallback);
			}
			mService = null;
		}
	};
	
	private void bindVideoService() {
		Log.d(TAG,"bindVideoService###############");
        Intent intent = new Intent(this, VideoService.class);
        bindService(intent, mVideoServiceConn, Context.BIND_AUTO_CREATE);
    }

    private void unbindVideoService() {
        if (mVideoServiceConn != null) {
            unbindService(mVideoServiceConn);
        }
    }
    
    @Override
	protected void onResume() {
		// TODO Auto-generated method stub
		Log.d(TAG, "onResume ################");
		super.onResume();
		bindVideoService();
	
	}

	@Override
	protected void onPause() {
		// TODO Auto-generated method stub
		Log.d(TAG, "onPause ################");
		super.onResume();
		unbindVideoService();		
	}
	
	private class CameraErrorCallback implements android.hardware.Camera.ErrorCallback {
		private static final String TAG = "CameraErrorCallback";

		@Override
		public void onError(int error, android.hardware.Camera camera) {
			Log.e(TAG, "Got camera error callback. error=" + error);
			if (error == android.hardware.Camera.CAMERA_ERROR_SERVER_DIED) {
				
				throw new RuntimeException("Media server died.");
			}
		}
	}
	
	private OnLayoutChangeListener mLayoutListener = new OnLayoutChangeListener() {
        @Override
        public void onLayoutChange(View v, int left, int top, int right,
                int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
            
        }
    };
    
    private boolean getRecordingState(int index)
	{
		if(mService != null)
		return mService.getRecordingState(index);
		return false;
	}
    
	private void startPreview(int cameraId,SurfaceTexture surfaceTexture)
	{

		Log.d(TAG,"mService=" + mService + " surfaceTexture=" + surfaceTexture)	;
		if (mService != null && (surfaceTexture != null)) {
			mService.startPreview(cameraId,surfaceTexture);	
		} 
	}
	
	private  void  stopPreview(int cameraId)
	{
		if(mService != null)
		{
			mService.stopPreview(cameraId);	
		}		
	}
	
	public void onCtrlMenuBarClick(View view)
	{
		int id = view.getId();
		Log.d(TAG,"onCtrlMenuBarClick id=" + id);
		switch(id)
		{
			case R.id.recordbutton:
				int cameraid = mService.isUVCCameraSonix(cameraid0);
				
				if(getRecordingState(cameraid))
				{
					if(mService != null)
					{
						mService.stopVideoRecording(cameraid);
						mRecordTime.setVisibility(View.GONE);
						mRecordButton.setImageResource(R.drawable.record_select);
					}
				}
				else
				{
					if(mService != null)
					{
						mService.startVideoRecording(cameraid,mSurfaceTexture0);
						mRecordTime.setVisibility(View.VISIBLE);
						mRecordButton.setImageResource(R.drawable.pause_select);
					}
					
				}
				break;
			case R.id.recordbutton2:
				if(getRecordingState(cameraid1))
				{
					if(mService != null)
					{
						mService.stopVideoRecording(cameraid1);
						mRecordTime1.setVisibility(View.GONE);
						mRecordButton1.setImageResource(R.drawable.record_select);
					}
				}
				else
				{
					if(mService != null)
					{
						mService.startVideoRecording(cameraid1,mSurfaceTexture1);
						mRecordTime1.setVisibility(View.VISIBLE);
						mRecordButton1.setImageResource(R.drawable.pause_select);
					}
					
				}
				break;
			case R.id.settingbutton:
				finish();
				break;
			case R.id.settingbutton2:
				finish();
				break;
		
		}
	}
	
	private void startVideoService() {
		Log.d(TAG,"#############startVideoService####################");
        Intent intent = new Intent(MainActivity.this, VideoService.class);
		startService(intent);
	}
	
	private void stopVideoService()
	{
		Log.d(TAG,"###########stopVideoService##################");
		Intent intent = new Intent(this, VideoService.class);
		stopService(intent);
	}
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		Log.d(TAG,"onCreate start");
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		setContentView(R.layout.activity_main);
		startVideoService();
		mCtrlMenuBar = (LinearLayout)findViewById(R.id.ctrl_menu_bar);
		mRecordButton = (ImageButton)findViewById(R.id.recordbutton);
		mBackButton = (ImageButton)findViewById(R.id.settingbutton);
		mRecordTime = (TextView)findViewById(R.id.recording_time);
		mRecordTime1 = (TextView)findViewById(R.id.recording_time1);
		mRecordButton1 = (ImageButton)findViewById(R.id.recordbutton2);
		mBackButton1 = (ImageButton)findViewById(R.id.settingbutton2);
		RelativeLayout layout = (RelativeLayout)findViewById(R.id.surface2);
		if(!mIsSupport2Video)
			layout.setVisibility(View.GONE);
		mBackButton.setVisibility(View.GONE);
		mBackButton1.setVisibility(View.GONE);
		initVideoView();
		mReceiver = new BroadcastReceiver() {
			
			@Override
			public void onReceive(Context arg0, Intent arg1) {
				// TODO Auto-generated method stub
				Log.d(TAG,"Intent action=" + arg1.getAction());
				int startRecord = arg1.getIntExtra("start", -1);
				int stopRecord = arg1.getIntExtra("stop", -1);
				Log.d(TAG,"startRecord=" + startRecord + " stopRecord=" + stopRecord);
				if(startRecord == 0)
				{
					mService.startVideoRecording(cameraid0, mSurfaceTexture0);
					mRecordTime.setVisibility(View.VISIBLE);
					mRecordButton.setImageResource(R.drawable.pause_select);
				}
				else if(startRecord == 1)
				{
					mService.startVideoRecording(cameraid1, mSurfaceTexture1);
					mRecordTime1.setVisibility(View.VISIBLE);
					mRecordButton1.setImageResource(R.drawable.pause_select);
				}
				else if(startRecord == 2)
				{
					mService.startVideoRecording(cameraid0, mSurfaceTexture0);
					mRecordTime.setVisibility(View.VISIBLE);
					mRecordButton.setImageResource(R.drawable.pause_select);
					
					mService.startVideoRecording(cameraid1, mSurfaceTexture1);
					mRecordTime1.setVisibility(View.VISIBLE);
					mRecordButton1.setImageResource(R.drawable.pause_select);
				}
					
				
				if(stopRecord == 0)
				{
					mService.stopVideoRecording(cameraid0);
					mRecordTime.setVisibility(View.GONE);
					mRecordButton.setImageResource(R.drawable.record_select);
				}
				else if(stopRecord == 1)
				{
					mService.stopVideoRecording(cameraid1);
					mRecordTime1.setVisibility(View.GONE);
					mRecordButton1.setImageResource(R.drawable.record_select);
					
				}
				else if(stopRecord == 2)
				{
					mService.stopVideoRecording(cameraid0);
					mRecordTime.setVisibility(View.GONE);
					mRecordButton.setImageResource(R.drawable.record_select);
					
					mService.stopVideoRecording(cameraid1);
					mRecordTime1.setVisibility(View.GONE);
					mRecordButton1.setImageResource(R.drawable.record_select);
				}
				
			}
		};
		IntentFilter filter = new IntentFilter("com.android.twovideotest");
		registerReceiver(mReceiver, filter);
		Log.d(TAG,"onCreate finish");
	}
	@Override
	protected void onDestroy() {
		// TODO Auto-generated method stub
		Log.d(TAG,"##########onDestroy#############");
		int cameraid = mService.isUVCCameraSonix(cameraid0);
		if(cameraid == cameraid0)
		{
			if(!getRecordingState(cameraid0) && !getRecordingState(cameraid1))
			{
				stopVideoService();
			}
		}
		else
		{
			if(!getRecordingState(cameraid) && !getRecordingState(cameraid1))
			{
				stopVideoService();
			}
		}
		
		unregisterReceiver(mReceiver);
		super.onDestroy();
	}

	//
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	private void initVideoView()
	{
		TextureView textureView0 = (TextureView)findViewById(R.id.video0);
		textureView0.setSurfaceTextureListener(new SurfaceTextureListener() {
			@Override
			public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
				Log.d(TAG, "mVideo0 Available surface=" + surface);
				
				mSurfaceTexture0 = surface;
				int cameraid = mService.isUVCCameraSonix(cameraid0);
				if(cameraid == cameraid0)
				{
					if (getRecordingState(cameraid0)) {
						//startVideoRecording();
						mService.startRender(cameraid0, surface);
						mRecordButton.setImageResource(R.drawable.pause_select);
						mRecordTime.setVisibility(View.VISIBLE);
						
						
			        } else{
			        	startPreview(cameraid0,surface);
			        	mRecordButton.setImageResource(R.drawable.record_select);
			        	mRecordTime.setVisibility(View.GONE);
			        }
				}
				else
				{
					if (getRecordingState(cameraid)) {
						//startVideoRecording();
						startPreview(cameraid0, surface);
						//mService.startRender(cameraid0, surface);
						mRecordButton.setImageResource(R.drawable.pause_select);
						mRecordTime.setVisibility(View.VISIBLE);
						
						
			        } else{
			        	startPreview(cameraid0,surface);
			        	mRecordButton.setImageResource(R.drawable.record_select);
			        	mRecordTime.setVisibility(View.GONE);
			        }
				}
				
				
			}

			@Override
			public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
				mSurfaceTexture0 = null;
				Log.d(TAG,"onSurfaceTexture0  Destroyed ");
				if(mService.isUVCCameraSonix(cameraid0) == cameraid0)
				{
					if(getRecordingState(cameraid0))
					{
						mService.stopRender(cameraid0);
					}
					else
					{
						mService.stopPreview(cameraid0);
						mService.closeCamera(cameraid0);
					}
				}
				else
				{
					mService.stopPreview(cameraid0);
					mService.closeCamera(cameraid0);
				}
				
				return true;
			}

			@Override
			public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
			}

			@Override
			public void onSurfaceTextureUpdated(SurfaceTexture surface) {
			}
		});
		textureView0.addOnLayoutChangeListener(mLayoutListener);
		if(mIsSupport2Video)
		{
			TextureView textureView1 = (TextureView)findViewById(R.id.video1);
			textureView1.setSurfaceTextureListener(new SurfaceTextureListener() {
				@Override
				public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
					Log.d(TAG, "mVideo1 Available surface=" + surface);
					mSurfaceTexture1 = surface;
					
					
					if (getRecordingState(cameraid1)) {
						//startVideoRecording();
						mService.startRender(cameraid1, surface);
						mRecordButton1.setImageResource(R.drawable.pause_select);
						mRecordTime1.setVisibility(View.VISIBLE);
						
						
			        } else{
			        	startPreview(cameraid1,surface);
			        	mRecordButton1.setImageResource(R.drawable.record_select);
			        	mRecordTime1.setVisibility(View.GONE);
			        }
			        
			        
				}

				@Override
				public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
					Log.d(TAG, "onSurfaceTexture1 destroy");
					mSurfaceTexture1 = null;
					
					if(getRecordingState(cameraid1))
					{
						mService.stopRender(cameraid1);
						
					}
					else
					{
						mService.stopPreview(cameraid1);
						mService.closeCamera(cameraid1);
					}
					
					return true;
				}

				@Override
				public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
				}

				@Override
				public void onSurfaceTextureUpdated(SurfaceTexture surface) {
				}
			});
			textureView1.addOnLayoutChangeListener(mLayoutListener);
		}		
	}
	
	@Override
	public void onInfo(MediaRecorder arg0, int arg1, int arg2) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onError(MediaRecorder arg0, int arg1, int arg2) {
		// TODO Auto-generated method stub
		
	}

}
