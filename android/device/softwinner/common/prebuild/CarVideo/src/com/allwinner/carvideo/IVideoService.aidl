package com.allwinner.carvideo;
import com.allwinner.carvideo.IVideoCallback;
/**
 * IVideoService.aidl
 * 作者：gufengyun123@126.com
 * 时间：2014-4-16
 * 功能：
 **/
 
interface IVideoService {
	void startPreview();
	void stopPreview();
	void startVideoRecording();
	void stopVideoRecording();
	void registerCallback(IVideoCallback callback);
	void unregisterCallback(IVideoCallback callback);
}
