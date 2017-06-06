package com.allwinner.carvideo;

import java.util.ArrayList;
import java.util.List;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.Environment;
import android.os.IBinder;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.os.StatFs;
import android.util.Log;
import android.graphics.SurfaceTexture;
import android.media.MediaRecorder;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences.Editor;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.Size;

import android.location.Location;
import android.media.CamcorderProfile;
import android.media.CameraProfile;
import android.media.MediaRecorder;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.MediaStore.Video;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

import java.io.File;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Iterator;
import java.util.List;

import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.content.ComponentName;
import android.os.RemoteException;

import java.util.Timer;
import java.util.TimerTask;

import com.allwinner.carvideo.VideoStorage.OnMediaSavedListener;

public class VideoService extends Service implements 
	MediaRecorder.OnErrorListener,ServiceData,
    MediaRecorder.OnInfoListener{
	
	private static final String TAG = "VideoService";
	
	//t8 recorde(h264) A83RECORD=true; A83_T2RECORD=false;
	//t2 recorder(h264) A83RECORD=true; A83_T2RECORD=true;
	//t2 or t8 camera 0 recorder(not h264 recorder) A83RECORD=false; A83_T2RECORD=false;
	
	
	//t2 songhan camera 0 preview and recorder(640*480)
	/*
	 *private static final boolean A83RECORD = false;//true;//true;
	private static final boolean A83_T2RECORD = false;//true;//true;
	private static final boolean AUDIO_RECODER_H264 = false;//true;
	private static final boolean RecordSonghanPreview640x480 = true;//A83RECORD and A83_T2RECORDAUDIO_RECODER_H264 need set false
	 * */
	
	//t2 songhan camera 0 preview camera 1 recorder(720p) video 
		/*
		 * 
		 * private static final boolean A83RECORD = true;//true;//true;
		private static final boolean A83_T2RECORD = true;//true;//true;
		private static final boolean AUDIO_RECODER_H264 = false;//true;
		private static final boolean RecordSonghanPreview640x480 = false;//A83RECORD and A83_T2RECORDAUDIO_RECODER_H264 need set false
		 * */
	
	//t2 songhan camera 0 preview camera 1 recorder(720p) video and audio 
	/*
	 * 
	 * private static final boolean A83RECORD = true;//true;//true;
	private static final boolean A83_T2RECORD = true;//true;//true;
	private static final boolean AUDIO_RECODER_H264 = true;//true;
	private static final boolean RecordSonghanPreview640x480 = false;//A83RECORD and A83_T2RECORDAUDIO_RECODER_H264 need set false
	 * */
	 
	
	private static final boolean A83RECORD = false;//true;//true;
	private static final boolean A83_T2RECORD = false;//true;//true;
	private static final boolean AUDIO_RECODER_H264 = false;//true;
	private static final boolean RecordSonghanPreview640x480 = true;//A83RECORD and A83_T2RECORD AUDIO_RECODER_H264 need set false
	private static final boolean RecordSonghanPreview640x480_sound = true;//need RecordSonghanPreview640x480 set true.recorder sound
	private static final int SAVE_VIDEO = 0;
	private static final int UPDATE_RECORD_TIME = 1;
	private static final boolean outputformat = true;//false:mp4 true:ts
	private final IBinder mBinder = new LocalBinder();
	private final Handler mHandler = new MainHandler();
	private MediaRecorder mMediaRecorder;
	private boolean mPreviewing = false; // True if preview is started.
	private boolean mMediaRecorderRecording= false;
	private ContentValues mCurrentVideoValues;
	private String mVideoFilename;
	private String mCurrentVideoFilename;
	private Camera mCameraDevice;
	private Camera mCameraDeviceA83;
	private long mRecordingStartTime;
	private CamcorderProfile mProfile;
	private CamcorderProfile mProfileA83;
	private SurfaceTexture mSurfaceTexture;
	private final CameraErrorCallback mErrorCallback = new CameraErrorCallback();

	private RemoteCallbackList<IVideoCallback> mCallbackList = new RemoteCallbackList<IVideoCallback>();
	

	
	
	private BroadcastReceiver  mReceiver = new BroadcastReceiver(){

		private static final String TAG = "VideoService";
        @Override
        public void onReceive(Context context, Intent intent) {
			 String action = intent.getAction();
			 Log.d(TAG,"action#############=" + action);
			 if (action.equals("android.hardware.usb.action.USB_CAMERA_PLUG_IN_OUT")) {
				Log.i(TAG, "ACTION_USB_CAMERA_PLUG_IN_OUT");

    			Bundle bundle = intent.getExtras(); 
				if(bundle == null) {
    				Log.i(TAG,"bundle is null");
    				return; 
				}

    			final String name = bundle.getString("UsbCameraName");
    			final int state = bundle.getInt("UsbCameraState");
    			final int totalNum = bundle.getInt("UsbCameraTotalNumber");
    			final String msg = bundle.getString("extral_mng");
    			Log.i(TAG,"usb camera name = " + name + " totalNum = " + totalNum + " state = " + state);
    				
				if (state == 1) {
					Log.i(TAG, "uvc camera " + name + " plug in");
				} else {
					Log.i(TAG, "uvc camera " + name + " plug out");
					Log.d(TAG,"usb out stopPreview");
					if("video0".equals(name))
					{
						stopPreview();
					}
					else if("video1".equals(name))
					{
						UsbOutThread thread = new UsbOutThread();
						thread.start();
					}
					
				}

			}
			 else if (Intent.ACTION_MEDIA_EJECT.equalsIgnoreCase(action) )
			 {
				
		          Log.d(TAG,"action = " + action);  
		          UsbOutThread thread = new UsbOutThread();
					thread.start();     
			 }

		}
    };
	
	private class MainHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
           switch(msg.what) {
		   		case SAVE_VIDEO:
					Log.d(TAG,"MainHandler handleMessage SAVE_VIDEO");
					// open this will save video and then start record
					//stopVideoRecording();
					//startVideoRecording(null);
					setNextFileName();
					break;
				case UPDATE_RECORD_TIME:
					updateRecordingTime();
					break;
           	}
            
        }
    }
	
	private OnMediaSavedListener mMediaSavedListener = new OnMediaSavedListener() {
		
		@Override
		public void onMediaSaved(Uri uri) {
			// TODO Auto-generated method stub
			Log.d(TAG,"onMediaSaved uri=" + uri);
		}
	};
	
	class LocalBinder extends Binder {
        public VideoService getService() {
            return VideoService.this;
        }
    }
	
	
	 class CameraErrorCallback
			implements android.hardware.Camera.ErrorCallback {
		private static final String TAG = "CameraErrorCallback";
	
		@Override
		public void onError(int error, android.hardware.Camera camera) {
			Log.e(TAG, "Got camera error callback. error=" + error);
			if (error == android.hardware.Camera.CAMERA_ERROR_SERVER_DIED) {
				// We are not sure about the current state of the app (in preview or
				// snapshot or recording). Closing the app is better than creating a
				// new Camera object.
				throw new RuntimeException("Media server died.");
			}
		}
	}
	 
	private static String millisecondToTimeString(long milliSeconds, boolean displayCentiSeconds) {
        long seconds = milliSeconds / 1000; // round down to compute seconds
        long minutes = seconds / 60;
        long hours = minutes / 60;
        long remainderMinutes = minutes - (hours * 60);
        long remainderSeconds = seconds - (minutes * 60);

        StringBuilder timeStringBuilder = new StringBuilder();

        // Hours
        if (hours > 0) {
            if (hours < 10) {
                timeStringBuilder.append('0');
            }
            timeStringBuilder.append(hours);

            timeStringBuilder.append(':');
        }

        // Minutes
        if (remainderMinutes < 10) {
            timeStringBuilder.append('0');
        }
        timeStringBuilder.append(remainderMinutes);
        timeStringBuilder.append(':');

        // Seconds
        if (remainderSeconds < 10) {
            timeStringBuilder.append('0');
        }
        timeStringBuilder.append(remainderSeconds);

        // Centi seconds
        if (displayCentiSeconds) {
            timeStringBuilder.append('.');
            long remainderCentiSeconds = (milliSeconds - seconds * 1000) / 10;
            if (remainderCentiSeconds < 10) {
                timeStringBuilder.append('0');
            }
            timeStringBuilder.append(remainderCentiSeconds);
        }

        return timeStringBuilder.toString();
    }
	
	private void updateRecordingTime() {
		//Log.d(TAG,"updateRecordingTime mMediaRecorderRecording=" + mMediaRecorderRecording);
        if (!mMediaRecorderRecording) {
            return;
        }
        long now = SystemClock.uptimeMillis();
        long deltaAdjusted = now - mRecordingStartTime;

       
        String text;

        long targetNextUpdateDelay;
        text = millisecondToTimeString(deltaAdjusted, false);
        targetNextUpdateDelay = 1000;

		if(deltaAdjusted>= 1000*60*1) // 1 minute
		{
			mRecordingStartTime = now;
			Message message = new Message();      
           message.what = SAVE_VIDEO;
    	  // mHandler.sendMessage(message);
		}
        onUpdateTimes(text);

       
        long actualNextUpdateDelay = targetNextUpdateDelay - (deltaAdjusted % targetNextUpdateDelay);
        mHandler.sendEmptyMessageDelayed(
                UPDATE_RECORD_TIME, actualNextUpdateDelay);
    }
	
	// from MediaRecorder.OnErrorListener
    @Override
    public void onError(MediaRecorder mr, int what, int extra) {
        Log.e(TAG, "MediaRecorder error. what=" + what + ". extra=" + extra);
        
    }

    // from MediaRecorder.OnInfoListener
    @Override
    public void onInfo(MediaRecorder mr, int what, int extra) {
       Log.e(TAG, "MediaRecorder.OnInfoListener onInfo what=" + what );
       if((what == MediaRecorder.MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED) || (what == MediaRecorder.MEDIA_RECORDER_INFO_MAX_DURATION_REACHED) )
       {
    	   Message message = new Message();      
           message.what = SAVE_VIDEO;
    	   mHandler.sendMessage(message);
       }
    }
	
	private void openCamera() {
        if(mCameraDevice != null)
        {
        	return;
        }
        mCameraDevice = Camera.open(0);
        //mCameraDevice = Camera.open(1);
        //mCameraDevice.setAnalogInputState(true, Camera.INTERFACE_CVBS, Camera.SYSTEM_NTSC, Camera.CHANNEL_ALL_2X2, Camera.FORMAT_PL_420);
        //mCameraDevice.setAnalogInputState(true, Camera.INTERFACE_CVBS, Camera.SYSTEM_NTSC, Camera.CHANNEL_ONLY_1, Camera.FORMAT_PL_420);
        if(mCameraDevice == null)
        {
        	Log.e(TAG,"openCamera error!!!!!!!");
        }
        if(A83_T2RECORD)
		{
			Camera.Parameters parameters = mCameraDevice.getParameters(); 
            parameters.setPreviewFrameRate(20);
            mCameraDevice.setParameters(parameters);
        }
    }

	public void closeCamera() {
		Log.d(TAG, "closeCamera");
        if (mCameraDevice == null) {
            Log.d(TAG, "already stopped.");
            return;
        }
        mCameraDevice.setErrorCallback(null);
        mCameraDevice.release();
        mCameraDevice = null;
        mPreviewing = false;
		//mMediaRecorderRecording= false;
		Log.d(TAG,"closeCamera mMediaRecorderRecording=false");
    }
	
	

	public int startPreview(SurfaceTexture surfaceTexture) {
		Log.d(TAG,"startPreview " + surfaceTexture + " mPreviewing =" + mPreviewing);

		openCamera();
		if(mPreviewing)
		{
			try {
				mCameraDevice.setPreviewTexture(surfaceTexture);
			}
			catch (IOException ex) {
	            mPreviewing = false;
	            //closeCamera();
	            Log.e(TAG,"startPreview failed", ex);
	            return FAIL;
	        }
		}
		else
		{
			Log.d(TAG,"mCameraDevice " + mCameraDevice + " mErrorCallback =" + mErrorCallback);
			mCameraDevice.setErrorCallback(mErrorCallback);
        

	        try {
				//mCameraDevice.setDisplayOrientation(0);
	            mCameraDevice.setPreviewTexture(surfaceTexture);
				Parameters param = mCameraDevice.getParameters();
				//param.setPreviewSize(1920, 1080);
				param.setPreviewSize(640, 480);
				param.setPreviewFrameRate(20);
				mCameraDevice.setParameters(param);
			
	            mCameraDevice.startPreview();
	            mPreviewing = true;
	           
	        } catch (IOException ex) {
	            mPreviewing = false;
	            //closeCamera();
	            Log.e(TAG,"startPreview failed", ex);
	            return FAIL;
	        }
		}
		return OK;
	}

	
	public int stopPreview()  {
		if (!mPreviewing) return BAD_VALUE;
        mCameraDevice.stopPreview();
        mPreviewing = false;
        closeCamera();
        return OK;
	}

	public void stopService() {
		Log.d(TAG, "StopService()");
		stopSelf();
	}
	
	private void pauseAudioPlayback() {
        Intent i = new Intent("com.android.music.musicservicecommand");
        i.putExtra("command", "pause");
        sendBroadcast(i);
    }

	private String createName(long dateTaken) {
        Date date = new Date(dateTaken);
        SimpleDateFormat dateFormat = new SimpleDateFormat("'VID'_yyyyMMdd_HHmmss");
        return dateFormat.format(date);
    }

	private void deleteVideoFile(String fileName) {
        Log.d(TAG, "Deleting video " + fileName);
        File f = new File(fileName);
        if (!f.delete()) {
            Log.v(TAG, "Could not delete " + fileName);
        }
    }

	private void saveVideo()
	{
		long duration = SystemClock.uptimeMillis() - mRecordingStartTime;
		VideoStorage.addVideo(mVideoFilename, duration,
				mCurrentVideoValues, mMediaSavedListener, getContentResolver());
	}
	
	private String  generateVideoFilename(int outputFileFormat) {
        long dateTaken = System.currentTimeMillis();
        String title = createName(dateTaken);
        // Used when emailing.
        String filename = title + VideoStorage.convertOutputFormatToFileExt(outputFileFormat);
        String mime = VideoStorage.convertOutputFormatToMimeType(outputFileFormat);
        String path = VideoStorage.DIRECTORY + '/' + filename;
        String tmpPath = path ;//+ ".tmp";
        mCurrentVideoValues = new ContentValues(9);
        mCurrentVideoValues.put(Video.Media.TITLE, title);
        mCurrentVideoValues.put(Video.Media.DISPLAY_NAME, filename);
        mCurrentVideoValues.put(Video.Media.DATE_TAKEN, dateTaken);
        mCurrentVideoValues.put(MediaColumns.DATE_MODIFIED, dateTaken / 1000);
        mCurrentVideoValues.put(Video.Media.MIME_TYPE, mime);
        mCurrentVideoValues.put(Video.Media.DATA, path);
        mCurrentVideoValues.put(Video.Media.RESOLUTION,
                Integer.toString(mProfile.videoFrameWidth) + "x" +
                		Integer.toString(mProfile.videoFrameHeight));
       
        mVideoFilename = tmpPath;
        
        Log.v(TAG, "New video filename: " + mVideoFilename);
        return path;
    }
	
	public void setNextFileName()
	{
		if(mMediaRecorder != null)
		{
			saveVideo();
			//should add a new thread
			Log.d(TAG,"DeleteFileThread start");
			DeleteFileThread thread = new DeleteFileThread();
			thread.start();
			Log.d(TAG,"DeleteFileThread end");
			
			
		}
	}
	
	private void initializeRecorder() {
		
        if (mCameraDevice == null) return;
        mMediaRecorder = new MediaRecorder(1);
        //setupMediaRecorderPreviewDisplay();
        if(!A83RECORD)
        {
        	//stopPreview();
        	mCameraDevice.unlock();
            mMediaRecorder.setCamera(mCameraDevice);
            if((!RecordSonghanPreview640x480) || RecordSonghanPreview640x480_sound)
            {
            	//mMediaRecorder.setAudioSource(MediaRecorder.AudioSource.CAMCORDER);
            	mMediaRecorder.setAudioSource(MediaRecorder.AudioSource.MIC);
            }
            
            mMediaRecorder.setVideoSource(MediaRecorder.VideoSource.CAMERA);
            if(!RecordSonghanPreview640x480)
            {
            	mMediaRecorder.setProfile(mProfile);
            }
			if(RecordSonghanPreview640x480_sound)
			{
				if(outputformat)
            	{
            		mProfile.fileFormat = MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS;
            	}
				else
				{
					mProfile.fileFormat = MediaRecorder.OutputFormat.MPEG_4;
				}
				
				mMediaRecorder.setProfile(mProfile);
			}
            //
            if(RecordSonghanPreview640x480 && !RecordSonghanPreview640x480_sound)
            {
            	if(outputformat)
            	{
            		mMediaRecorder.setOutputFormat(MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS);
            	}
				else
				{
					mMediaRecorder.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4);
				}
            	
            }

            if(RecordSonghanPreview640x480 && !RecordSonghanPreview640x480_sound)
            {
            	mMediaRecorder.setVideoSize(640, 480);
                mMediaRecorder.setVideoEncodingBitRate(mProfile.videoBitRate);
                mMediaRecorder.setVideoEncoder(mProfile.videoCodec);
                mMediaRecorder.setVideoFrameRate(mProfile.videoFrameRate);
            }
            
            //mMediaRecorder.setMaxDuration(60 *1000*1);
            mMediaRecorder.setMaxDuration(20*1000*1); //2minute
    		//mMediaRecorder.setOrientationHint(180);
           // mMediaRecorder.setMaxFileSize(VideoStorage.VIDEO_FILE_MAX_SIZE);
            // Set output file.
            // Try Uri in the intent first. If it doesn't exist, use our own
            // instead.
            if(outputformat)
            {
            	generateVideoFilename(MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS);
            }
			else
			{
				generateVideoFilename(MediaRecorder.OutputFormat.MPEG_4);
			}
            
            
    		Log.d(TAG,"fengyun mVideoFilename= " + mVideoFilename);
            mMediaRecorder.setOutputFile(mVideoFilename);
        }
        else
        {
        	
        	
        	// video android audio
        	if(AUDIO_RECODER_H264)
        	{
        		mCameraDeviceA83.unlock();
                mMediaRecorder.setCamera(mCameraDeviceA83);
                //mMediaRecorder.setAudioSource(MediaRecorder.AudioSource.CAMCORDER);
                mMediaRecorder.setAudioSource(MediaRecorder.AudioSource.MIC);
                mMediaRecorder.setVideoSource(MediaRecorder.VideoSource.CAMERA);
                mMediaRecorder.setProfile(mProfileA83);
                //mMediaRecorder.setAudioEncoder(MediaRecorder.AudioEncoder.AAC);
                mMediaRecorder.setMaxDuration(60 *1000*1);//1 minute
        		//mMediaRecorder.setOrientationHint(180);
                mMediaRecorder.setAudioChannels(1);
                mMediaRecorder.setMaxFileSize(VideoStorage.VIDEO_FILE_MAX_SIZE);
        	}
        	else
        	{
        		mCameraDeviceA83.unlock();         
                mMediaRecorder.setCamera(mCameraDeviceA83);           
                mMediaRecorder.setVideoSource(MediaRecorder.VideoSource.CAMERA);
                if(outputformat)
            	{
            		mMediaRecorder.setOutputFormat(MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS);
            	}
				else
				{
					mMediaRecorder.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4);
				}
               // mMediaRecorder.setVideoFrameRate(mProfileA83.videoFrameRate);
                mMediaRecorder.setVideoFrameRate(25);
                mMediaRecorder.setVideoSize(mProfileA83.videoFrameWidth, mProfileA83.videoFrameHeight);
                Log.d(TAG,"videoFrameWidth=" + mProfileA83.videoFrameWidth + " mProfileA83.videoFrameHeight=" + mProfileA83.videoFrameHeight);
                mMediaRecorder.setVideoEncodingBitRate(mProfileA83.videoBitRate);
                mMediaRecorder.setVideoEncoder(mProfileA83.videoCodec);     
                mMediaRecorder.setOrientationHint(180);
                mMediaRecorder.setMaxDuration(60 *1000*1); //  1 minute
        	}
        	
            
        	//only video start 
        	
    		//mMediaRecorder.setOrientationHint(180);
           // mMediaRecorder.setMaxFileSize(VideoStorage.VIDEO_FILE_MAX_SIZE);
            
          //only video end
            // Set output file.
            // Try Uri in the intent first. If it doesn't exist, use our own
            // instead.
            if(outputformat)
            {
            	generateVideoFilename(MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS);
            }
			else
			{
            	generateVideoFilename(MediaRecorder.OutputFormat.MPEG_4);
			}
    		Log.d(TAG,"mVideoFilename= " + mVideoFilename);
            mMediaRecorder.setOutputFile(mVideoFilename);
           
            
        }
        
        // Unlock the camera object before passing it to media recorder.
        
        try {
            mMediaRecorder.prepare();
        } catch (IOException e) {
            Log.e(TAG, "prepare failed for " + mVideoFilename, e);
            releaseMediaRecorder();
            throw new RuntimeException(e);
        }

        mMediaRecorder.setOnErrorListener(this);
        mMediaRecorder.setOnInfoListener(this);
    }

	
	
	private void cleanupEmptyFile() {
        if (mVideoFilename != null) {
            File f = new File(mVideoFilename);
            if (f.length() == 0 && f.delete()) {
                Log.v(TAG, "Empty video file deleted: " + mVideoFilename);
                mVideoFilename = null;
            }
        }
    }
	private void releaseMediaRecorder() {
        Log.v(TAG, "Releasing media recorder.");
        if (mMediaRecorder != null) {
            cleanupEmptyFile();
            mMediaRecorder.reset();
            mMediaRecorder.release();
            mMediaRecorder = null;
        }
        mVideoFilename = null;
    }
	
	
	void openCameraForA83()
	{
		if(mCameraDeviceA83 != null)
        {
        	return;
        }
		Log.d(TAG,"mCameraDeviceA83 open");
		if(A83_T2RECORD)
		{
			mCameraDeviceA83 = Camera.open(2);
			Camera.Parameters parameters = mCameraDeviceA83.getParameters(); 
            parameters.setPreviewFrameRate(20);
            mCameraDeviceA83.setParameters(parameters);
        }
		else
		{
			mCameraDeviceA83 = Camera.open(1);
		}
		
        //mCameraDevice = Camera.open(1);
        //mCameraDevice.setAnalogInputState(true, Camera.INTERFACE_CVBS, Camera.SYSTEM_NTSC, Camera.CHANNEL_ALL_2X2, Camera.FORMAT_PL_420);
        //mCameraDevice.setAnalogInputState(true, Camera.INTERFACE_CVBS, Camera.SYSTEM_NTSC, Camera.CHANNEL_ONLY_1, Camera.FORMAT_PL_420);
		Log.d(TAG,"mCameraDeviceA83 =" + mCameraDeviceA83);
        if(mCameraDeviceA83 == null)
        {
        	Log.e(TAG,"openCamera error!!!!!!!");
        }

	}
	
	void closeA83Record()
	{
		Log.d(TAG, "closeA83Record mCameraDeviceA83=" + mCameraDeviceA83);
		if (mCameraDeviceA83 == null) {
			Log.d(TAG, "already stopped.");
			return;
		}
		mCameraDeviceA83.setErrorCallback(null);
		mCameraDeviceA83.release();
		mCameraDeviceA83 = null;
		
		//mMediaRecorderRecording = false;
		Log.d(TAG, "closeCamera mMediaRecorderRecording=false");

	}
	
	public int startVideoRecording(SurfaceTexture surfaceTexture)  {
		if(!VideoStorage.storageSpaceIsAvailable(getContentResolver()))
		{
			Log.e(TAG,"Not enough storage space!!!");
			Toast.makeText(this, "Not enough storage space!!!", Toast.LENGTH_LONG).show();
			return FAIL;
		}
		if(A83RECORD)
		{
			CameraOpenThread cameraOpenThread = new CameraOpenThread();
		    cameraOpenThread.start();
		    try {
	            cameraOpenThread.join();
	            if (mCameraDeviceA83 == null) {
	                return BAD_VALUE;
	            }
	        } catch (InterruptedException ex) {
	            // ignore
	        	Log.e(TAG,"open error " + ex);
	        }
		}
		else
		{
			openCamera();
		}
		
		if(surfaceTexture != null)
		{
			mSurfaceTexture = surfaceTexture;
		}
		else
		{
			surfaceTexture = mSurfaceTexture;
		}
		
		if(!A83RECORD)
		{
			if(mMediaRecorderRecording)
			{
				try{
					mCameraDevice.setPreviewTexture(surfaceTexture);
				}
				catch(IOException io)
				{
					Log.e(TAG,"startVideoRecording setPreviewTexture set error!!");
				}
				return BAD_VALUE;
			}
		}
		else
		{
			if(mMediaRecorderRecording)
			{
				/*try{
					mCameraDeviceA83.setPreviewTexture(surfaceTexture);
				}
				catch(IOException io)
				{
					Log.e(TAG,"startVideoRecording setPreviewTexture set error!!");
				}
				return BAD_VALUE;*/
			}
		}
		
        initializeRecorder();
        
        if (mMediaRecorder == null) {
            Log.e(TAG, "Fail to initialize media recorder");
            return FAIL;
        }
        pauseAudioPlayback();
		mRecordingStartTime = SystemClock.uptimeMillis();
        try {
            mMediaRecorder.start(); // Recording is now started
        } catch (RuntimeException e) {
            Log.e(TAG, "Could not start media recorder. ", e);
            releaseMediaRecorder();
            // If start fails, frameworks will not lock the camera for us.
            mCameraDevice.lock();
            return FAIL;
        }
        mMediaRecorderRecording = true;
        Log.d(TAG,"startRecording mMediaRecorderRecording = true");
		updateRecordingTime();
		return OK;
    }

	public int stopVideoRecording(){
		// TODO Auto-generated method stub
		Log.d(TAG,"stopVideoRecording mMediaRecorderRecording=" + mMediaRecorderRecording);
        boolean fail = false;
        
        if (mMediaRecorderRecording) {
            try {
                mMediaRecorder.setOnErrorListener(null);
                mMediaRecorder.setOnInfoListener(this);
                Log.d(TAG,"mMediaRecorder stop");
                mMediaRecorder.stop();
                mCurrentVideoFilename = mVideoFilename;
                Log.d(TAG, "stopVideoRecording: Setting current video filename: "
                        + mCurrentVideoFilename);
                
            } catch (RuntimeException e) {
                Log.e(TAG, "stop fail",  e);
                if (mVideoFilename != null) deleteVideoFile(mVideoFilename);
                fail = true;
            }
            mMediaRecorderRecording = false;
            Log.d(TAG,"stopRecording mMediaRecorderRecording=false");
            if (!fail) {
            	saveVideo();
            }
		// release media recorder
        releaseMediaRecorder();
        }
        if(A83RECORD)
        {
        	closeA83Record();
        }
        
        return OK;
  
	}

	public void setRecordBackground(boolean enabled)
	{
	
		if(mCameraDevice != null)
		{
			//mCameraDevice.setRecordBackground(enabled);	
		}
	}

	private void onUpdateTimes(String times)
	{
		int i = mCallbackList.beginBroadcast();
		while(i > 0)
		{
			i--;
			try
			{
				mCallbackList.getBroadcastItem(i).onUpdateTimes(times);
			}
			catch(RemoteException e)
			{
				e.printStackTrace();
			}
		}
		mCallbackList.finishBroadcast();
	}

	public void registerCallback(IVideoCallback callback)
	{
		// TODO Auto-generated method stub
		if(callback != null)
		{
			mCallbackList.register(callback);
		}
		
	}
	
	public void unregisterCallback(IVideoCallback callback)
			{
		// TODO Auto-generated method stub
		if(callback != null)
		{
			mCallbackList.unregister(callback);
		}
		
	}

	public boolean getPreviewState()
	{
		return mPreviewing;
	}

	public boolean getRecordingState()
	{
		Log.d(TAG,"getRecordingState = " + mMediaRecorderRecording);
		return mMediaRecorderRecording;
	}
	
	@Override
	public IBinder onBind(Intent arg0) {
		// TODO Auto-generated method stub
		return mBinder;
		
	}
	
	protected class CameraOpenThread extends Thread {
        @Override
        public void run() {
            openCameraForA83();
        }
    }
	
	class UsbOutThread extends Thread
	{
		@Override
		public void run() {
			if(mMediaRecorder != null)
			{
				Log.d(TAG,"stopVideoRecording thread start");
				stopVideoRecording();
				Log.d(TAG,"storpVideoRecording thread end");
			}
        }
	}
	
	class DeleteFileThread extends Thread
	{
		@Override
        public void run() {
        	String path;
        	if(outputformat)
        	{
        		path = generateVideoFilename(MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS);
        	}
			else
			{
				path = generateVideoFilename(MediaRecorder.OutputFormat.MPEG_4);
			}
			mRecordingStartTime = SystemClock.uptimeMillis();
			Log.d(TAG,"setNextFileName=" + path);
			try
			{
				mMediaRecorder.setNextSaveFile(path);
			}
			catch(IOException ex)
			{
				
			}
			Log.d(TAG,"setNextFileName=" + path + " end");
			if(!VideoStorage.storageSpaceIsAvailable(getContentResolver()))
			{
				Log.e(TAG,"Not enough storage space!!!");
				//Toast.makeText(this, "Not enough storage space!!!", Toast.LENGTH_LONG).show();
				stopVideoRecording();
				return;
			}
        }
	}
	
	@Override
	public void onCreate()
	{
		super.onCreate();
		//mProfile = CamcorderProfile.get(0, 5);//Back Camera 
		mProfile = CamcorderProfile.get(0, 4);//Back Camera for T2 camera 640*480 (720*480)
		if(A83_T2RECORD)
		{
			mProfileA83 = CamcorderProfile.get(0, 5);//
		}
		else
		{
			mProfileA83 = CamcorderProfile.get(0, 5);//Back Camera public static final int QUALITY_1080P = 6;
		}
		
		Log.d(TAG,"onCreate");
		//IntentFilter UsbCameraFilter = new IntentFilter();
		//UsbCameraFilter.addAction("android.hardware.usb.action.USB_CAMERA_PLUG_IN_OUT");
		
		registerReceiver(mReceiver, new IntentFilter("android.hardware.usb.action.USB_CAMERA_PLUG_IN_OUT"));
		
		IntentFilter mFilter = new IntentFilter();
		mFilter.addAction(Intent.ACTION_MEDIA_EJECT);
		mFilter.addDataScheme("file");
		registerReceiver(mReceiver,mFilter);
		
		
		//registerReceiver(mReceiver, UsbCameraFilter);
		Log.d(TAG,"onCreate registerReceiver finish");
		
	}
	
	@Override
	public void onDestroy() {
		// TODO Auto-generated method stub
		super.onDestroy();
		Log.d(TAG,"onDestroy");
	}

	@Override
	public void onRebind(Intent intent) {
		// TODO Auto-generated method stub
		super.onRebind(intent);
		Log.d(TAG,"onRebind");
	}

	@Override
	@Deprecated
	public void onStart(Intent intent, int startId) {
		// TODO Auto-generated method stub
		super.onStart(intent, startId);
		Log.d(TAG,"onStart");
	}

	@Override
	public boolean onUnbind(Intent intent) {
		// TODO Auto-generated method stub
		Log.d(TAG,"onUnbind");
		return super.onUnbind(intent);
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId)
	{
		Log.d(TAG,"onStartCommand");
		return super.onStartCommand(intent, flags, startId);
	}
	
}
