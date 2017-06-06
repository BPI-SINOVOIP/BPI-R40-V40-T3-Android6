package com.allwinner.carvideo.videoplay;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.allwinner.carvideo.R;
import com.allwinner.carvideo.VideoStorage;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.MediaStore.Video;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

public class VideoPlayActivity extends Activity {
	private static final String TAG="VideoPlayActivity";
	private final static String PATH = "path";
	private final static String TITLE = "title";
	private final static String SIZE = "size";
	private final int KB = 1024;
	private final int MG = KB * KB;
	private final int GB = MG * KB;
	
	private ListView mListView; 
	private VideoListAdapter mVideoAdapter;
	private List<Map<String, Object>>  mVideoListArray;
	
	@Override
	protected void onCreate(Bundle arg0) {
		// TODO Auto-generated method stub
		super.onCreate(arg0);
		setContentView(R.layout.video_play);
		mListView = (ListView)findViewById(R.id.play_main_activity);
		mVideoListArray = new ArrayList<Map<String, Object>>();
		queryVideoFile();
		mVideoAdapter = new VideoListAdapter(this);
		mListView.setAdapter(mVideoAdapter);
		
		mListView.setCacheColorHint(0);
		mListView.setOnItemClickListener(new OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> parent, View view,
					int position, long id) {
				Map<String, Object> map = mVideoListArray.get(position);
				Intent movieIntent = new Intent();
				String path = (String)map.get(PATH);
	    		File file = new File(path);
//	    		movieIntent.putExtra(MediaStore.PLAYLIST_TYPE, MediaStore.PLAYLIST_TYPE_CUR_FOLDER);
	    		movieIntent.putExtra(MediaStore.EXTRA_FINISH_ON_COMPLETION, false);
	    		movieIntent.setAction(android.content.Intent.ACTION_VIEW);
	    		movieIntent.setDataAndType(Uri.fromFile(file), "video/*");
	    		startActivity(movieIntent);
		}});
	}
	
	private  void  queryVideoFile()
    {
		
    	ContentResolver resolver = getContentResolver();
    	String columns[] = new String[] {MediaStore.Video.Media.DATA,MediaStore.Video.Media.DISPLAY_NAME, Video.Media.SIZE};
    	Uri uri = Uri.parse(VideoStorage.VIDEO_BASE_URI);
    	String select = MediaStore.Video.Media.DATA + " like '" + VideoStorage.DIRECTORY + "%.mp4'";
    	Cursor cur = resolver.query( uri, columns, select,  null, null);
        Log.d(TAG,"cur=" + cur.getCount() + " column=" + cur.getColumnCount());
        if ((cur != null) && (cur.moveToFirst())) {
        		
        	String path = null;
            String title = null;
            String size = null;
            do {
            	path = cur.getString(cur.getColumnIndex(MediaStore.Video.Media.DATA));
                title = cur.getString(cur.getColumnIndex(MediaStore.Video.Media.DISPLAY_NAME));
                size = cur.getString(cur.getColumnIndex(Video.Media.SIZE));
                Map<String, Object> map = new HashMap<String, Object>();
        		map.put(PATH, path);
        		map.put(TITLE, title);
        		map.put(SIZE, size);
        		mVideoListArray.add(map);    
            } while (cur.moveToNext());
        }
        if(cur != null)
        {
        	cur.close();
        } 
    }
	
	public final class ViewHolder {
		ImageView icon;
		TextView title;
		TextView detail;
		
	}
	
	public class VideoListAdapter extends BaseAdapter {

		private LayoutInflater mInflater;

		public VideoListAdapter(Context context) {
			this.mInflater = LayoutInflater.from(context);
		}

		@Override
		public int getCount() {
			// TODO Auto-generated method stub
			return mVideoListArray.size();
		}

		@Override
		public Object getItem(int arg0) {
			// TODO Auto-generated method stub
			return null;
		}

		@Override
		public long getItemId(int arg0) {
			// TODO Auto-generated method stub
			return 0;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent) {

			ViewHolder holder = null;
			if (convertView == null) {

				holder = new ViewHolder();
				convertView = mInflater.inflate(
						R.layout.play_activity_item, null);
				holder.title = (TextView) convertView
						.findViewById(R.id.title);
				holder.icon = (ImageView)convertView.findViewById(R.id.image);
				holder.detail = (TextView)convertView.findViewById(R.id.detail);
				convertView.setTag(holder);

			} else {

				holder = (ViewHolder) convertView.getTag();
			}
			holder.title.setText((String) mVideoListArray.get(position).get(TITLE));
			String strLen = (String)mVideoListArray.get(position).get(SIZE);
			long length = Integer.parseInt(strLen);
			String display_size;
			if (length > GB)
				display_size = String.format("%.2f GB ", (double)length / GB);
			else if (length < GB && length > MG)
				display_size = String.format("%.2f MB ", (double)length / MG);
			else if (length < MG && length > KB)
				display_size = String.format("%.2f KB ", (double)length/ KB);
			else
				display_size = String.format("%.2f bytes ", (double)length);
			
			holder.detail.setText(display_size);
			return convertView;
		}

	}
}
