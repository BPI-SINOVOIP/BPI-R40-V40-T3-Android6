package com.allwinner.carvideo;

import java.io.File;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.media.MediaRecorder;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Environment;
import android.os.StatFs;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.MediaStore.Video;
import android.util.Log;

public class VideoStorage {

	private static final String TAG = "VideoStorage";
	public static final String VIDEO_BASE_URI = "content://media/internal/video/media";
    public static final String DIRECTORY = "/mnt/sdcard";//"/mnt/sdcard/DCIM/Camera";
	//public static final String DIRECTORY = "/mnt/udiskh";//"/mnt/sdcard/DCIM/Camera";
	 //public static final String DIRECTORY = "/mnt/mapcard";//"/mnt/sdcard/DCIM/Camera";
	//public static final String DIRECTORY = "/mnt/extsd";//"/mnt/sdcard/DCIM/Camera";
	 //public static final String DIRECTORY = "/storage/extsd";//"/mnt/sdcard/DCIM/Camera";
    public static final long UNAVAILABLE = -1L;
    public static final long PREPARING = -2L;
    public static final long UNKNOWN_SIZE = -3L;
    public static final int DELETE_MAX_TIMES = 10;
    public static final long LOW_STORAGE_THRESHOLD_BYTES = 200*1024*1024;//200M;
    public static final long VIDEO_FILE_MAX_SIZE = 50*1024*1024;//10M; //鏈�ソ灏忎簬LOW_STORAGE_THRESHOLD_BYTES鐨勫ぇ灏�
    
    public interface OnMediaSavedListener
    {
    	public void onMediaSaved(Uri uri);
    }
    
    public static long getStorageSpaceBytes()
	{
		String state = Environment.getExternalStorageState();
        Log.d(TAG, "External storage state=" + state);
        File dir = new File(DIRECTORY);
        dir.mkdirs();
        if (!dir.isDirectory() || !dir.canWrite()) {
            return UNAVAILABLE;
        }

        try {
            StatFs stat = new StatFs(DIRECTORY);
			Log.d(TAG, "getAvailableSpace=" + stat.getAvailableBlocks() * (long) stat.getBlockSize());
            return stat.getAvailableBlocks() * (long) stat.getBlockSize();
        } catch (Exception e) {
            Log.i(TAG, "Fail to access external storage", e);
        }
        return UNKNOWN_SIZE;
	}
	
    private static int queryOldVideoFile(ContentResolver resolver)
    {
    	int videoId = -1;
    	String columns[] = new String[] { MediaStore.Video.Media._ID,MediaStore.Video.Media.DATA,MediaColumns.DATE_MODIFIED };
    	Uri uri = Uri.parse(VIDEO_BASE_URI);
    	String select = MediaStore.Video.Media.DATA + " like '" + DIRECTORY + "%.mp4'";
    	Cursor cur = resolver.query( uri, columns, select,  null, MediaColumns.DATE_MODIFIED + " ASC");
        
        if ((cur != null) && (cur.moveToFirst())) {
                videoId = cur.getInt(cur.getColumnIndex(MediaStore.Video.Media._ID));   
                //String id = cur.getString(cur.getColumnIndex(MediaStore.Video.Media._ID));
                //String path = cur.getString(cur.getColumnIndex(MediaStore.Video.Media.DATA));
                //Log.d(TAG,"id=" + id + " path =" + path);
                //Log.d(TAG,"queryRecentVideoFile id=" + videoId);
                cur.close();
        }
        Log.d(TAG,"queryRecentVideoFile2222 id=" + videoId);
        return videoId;
    }
    
    public static void deleteVideoFile(ContentResolver resolver)
    {
    	Uri uri = Uri.parse(VIDEO_BASE_URI);
    	int deleteId = -1;
    	
    	for(int i=0;i<DELETE_MAX_TIMES;i++)
    	{
    		deleteId = queryOldVideoFile(resolver);
        	if(deleteId > 0)
        	{
        		resolver.delete(uri, MediaStore.Video.Media._ID + "=?", new String[] { Integer.toString(deleteId) });
        		Log.d(TAG,"delete " + deleteId + " succees");
        	}
    	}
    	
    }
    
	public static boolean storageSpaceIsAvailable(ContentResolver resolver)
	{
		int deleteTimes = 0;
		while(deleteTimes < DELETE_MAX_TIMES)
		{
			if(getStorageSpaceBytes() > LOW_STORAGE_THRESHOLD_BYTES)
			{
				return true;
			}
			else
			{
				deleteVideoFile(resolver);
			}
			deleteTimes++;
		}
		
		return false;
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
