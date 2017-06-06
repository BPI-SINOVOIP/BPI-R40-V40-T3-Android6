package com.example.twovideotest;

import java.io.File;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.media.MediaRecorder;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Environment;
import android.os.StatFs;
import android.os.storage.StorageManager;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.MediaStore.Video;
import android.util.Log;

public class VideoStorage {

	private static final String TAG = "VideoStorage";
	//public static final String VIDEO_BASE_URI = "content://media/internal/video/media";
	public static final String VIDEO_BASE_URI = "content://media/external/video/media";
   // public static final String DIRECTORY = "/mnt/usbhost";//"/mnt/sdcard/DCIM/Camera";
	 public static String DIRECTORY = "/storage/5924-0FE4";//"/mnt/sdcard/DCIM/Camera";
    public static final long UNAVAILABLE = -1L;
    public static final long PREPARING = -2L;
    public static final long UNKNOWN_SIZE = -3L;
    public static final int DELETE_MAX_TIMES = 5;
    public static final long LOW_STORAGE_THRESHOLD_BYTES = 1024*1024*1024;//200M;
    public static final long VIDEO_FILE_MAX_SIZE = 100*1024*1024;//10M; //最好小于LOW_STORAGE_THRESHOLD_BYTES的大小
    
    
    public  static void  setPath(String path)
    {
    	DIRECTORY = new String(path);
    	Log.d(TAG,"DIRECTORY=" + DIRECTORY);
    	 
    }
    
    public interface OnMediaSavedListener
    {
    	public void onMediaSaved(Uri uri);
    }
    
    public static long getStorageSpaceBytes()
	{
		String state = Environment.getExternalStorageState();
		String path = Environment.getExternalStorageDirectory().toString();
        Log.d(TAG, "path:" + path + " External storage state=" + state);
        File dir = new File(DIRECTORY);
        dir.mkdirs();
        if (!dir.isDirectory()) {
        	Log.e(TAG,"DIR is not exist ");
            return 0;
        }
        if(!dir.canWrite())
        {
        	Log.e(TAG,"DIR can not be write");
        	return 0;
        }

        try {
            StatFs stat = new StatFs(DIRECTORY);
            long size = stat.getAvailableBlocks() * (long) stat.getBlockSize();
			Log.d(TAG, "getAvailableSpace=" + size/1024/1024 + " M");
            return size;
        } catch (Exception e) {
            Log.e(TAG, "Fail to access external storage", e);
        }
        return 0;
	}
	
    private static int queryOldVideoFile(ContentResolver resolver,int format)
    {
    	int videoId = -1;
    	String columns[] = new String[] { MediaStore.Video.Media._ID,MediaStore.Video.Media.DATA,MediaColumns.DATE_MODIFIED };
    	Uri uri = Uri.parse(VIDEO_BASE_URI);
    	String select;
    	if(format == MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS)
    	{
    		select = MediaStore.Video.Media.DATA + " like '" + DIRECTORY + "%.ts'";
    	}
    	else
    	{
    		select = MediaStore.Video.Media.DATA + " like '" + DIRECTORY + "%.mp4'";
    	}
    	
    	Cursor cur = resolver.query( uri, columns, select,  null, MediaColumns.DATE_MODIFIED + " ASC");
        
        if ((cur != null) && (cur.moveToFirst())) {
                videoId = cur.getInt(cur.getColumnIndex(MediaStore.Video.Media._ID));   
                //String id = cur.getString(cur.getColumnIndex(MediaStore.Video.Media._ID));
                String path = cur.getString(cur.getColumnIndex(MediaStore.Video.Media.DATA));
                Log.d(TAG,"find path =" + path + "to delete");
                deleteVideoFile(path);
                //Log.d(TAG,"queryRecentVideoFile id=" + videoId);
                cur.close();
        }
        Log.d(TAG,"queryRecentVideoFile2222 id=" + videoId);
        return videoId;
    }
    
    public static void deleteVideoFile(String fileName) {
        Log.d(TAG, "Deleting video " + fileName);
        File f = new File(fileName);
        if (!f.delete()) {
            Log.v(TAG, "Could not delete " + fileName);
        }
    }
    private static void deleteVideoFile(ContentResolver resolver,int format)
    {
    	Uri uri = Uri.parse(VIDEO_BASE_URI);
    	int deleteId = -1;
    	
    	deleteId = queryOldVideoFile(resolver,format);
    	if(deleteId > 0)
    	{
    		resolver.delete(uri, MediaStore.Video.Media._ID + "=?", new String[] { Integer.toString(deleteId) });
    		Log.d(TAG,"delete " + deleteId + " succees");
    	}
    }
    
	public static boolean storageSpaceIsAvailable(ContentResolver resolver,int format)
	{
		int deleteTimes = 0;
		if(getStorageSpaceBytes() > LOW_STORAGE_THRESHOLD_BYTES)
		{
			return true;
		}
		
		while(deleteTimes < DELETE_MAX_TIMES)
		{
			deleteVideoFile(resolver,format);
			deleteTimes++;
		}
		
		return true;
	}
	
	public static String convertOutputFormatToFileExt(int outputFileFormat) {
        if (outputFileFormat == MediaRecorder.OutputFormat.MPEG_4) {
            return ".mp4";
        }
        else if(outputFileFormat == MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS)
        {
        	return ".ts";
        }
        return ".3gp";
    }
	
	public static String convertOutputFormatToMimeType(int outputFileFormat) {
        if (outputFileFormat == MediaRecorder.OutputFormat.MPEG_4) {
            return "video/mp4";
        }
        else if(outputFileFormat == MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS)
        {
        	return "video/ts";
        }
        return "video/3gpp";
    }
	
	public static void addVideo(String path,long duration,ContentValues values,OnMediaSavedListener listener,ContentResolver resolver)
	{
		VideoSaveTask videoSave = new VideoSaveTask(path, duration, values, listener, resolver);
		videoSave.execute();
	}
	
	private static class VideoSaveTask extends AsyncTask<Void, Void, Uri>
	{

		private String path;
		private long duration;
		private ContentValues values;
		private OnMediaSavedListener listener;
		private ContentResolver resolver;
		
		public VideoSaveTask(String path,long duration,ContentValues values,OnMediaSavedListener listener,ContentResolver resolver)
		{
			// TODO Auto-generated constructor stub
			this.path = path;
			this.duration = duration;
			this.values = new ContentValues(values);
			this.listener = listener;
			this.resolver = resolver;
		}
		
		public VideoSaveTask(String path)
		{
			// TODO Auto-generated constructor stub
			this.path = path;
		}
		
		@Override
		protected Uri doInBackground(Void... arg0) {
			// TODO Auto-generated method stub
			values.put(Video.Media.SIZE,new File(path).length());
			values.put(Video.Media.DURATION,duration);
			Uri uri = null;
			try
			{
				Uri videoTable = Uri.parse(VIDEO_BASE_URI);
				Log.d(TAG,"videoTable=" + videoTable);
				uri = resolver.insert(videoTable, values);
				//String finalName  = values.getAsString(Video.Media.DATA);
				//new File(path).renameTo(new File(finalName));
				//if()
				//{
				//	path = finalName;
				//}
				//resolver.update(uri, values, null, null);
			}
			catch(Exception e)
			{
				Log.e(TAG,"failed to add video to media storage:" + e);
			}
			return uri;
		}
		
		
		@Override
		protected void onPostExecute(Uri result) {
			// TODO Auto-generated method stub
			if(listener != null)
			{
				listener.onMediaSaved(result);
			}
		}
	}
}
