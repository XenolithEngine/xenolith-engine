package org.stappler.xenolith.core;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.net.ConnectivityManager;

public class DisplayConfigListener implements DisplayManager.DisplayListener {
	public static DisplayConfigListener create(Context context, long nativePointer) {
		DisplayManager manager = context.getSystemService(DisplayManager.class);
		if (manager != null) {
			return create(manager, nativePointer);
		}
		return null;
	}

	public static DisplayConfigListener create(DisplayManager manager, long nativePointer) {
		DisplayConfigListener ret = new DisplayConfigListener(manager, nativePointer);
		manager.registerDisplayListener(ret, null);
		return ret;
	}

	protected DisplayConfigListener(DisplayManager manager, long nativePointer) {
		_native = nativePointer;
		_manager = manager;
	}

	protected void finalize() {
		if (_manager != null) {
			_manager.unregisterDisplayListener(this);
			_manager = null;
			_native = 0;
		}
	}

	@Override
	public void onDisplayAdded(int displayId) {
		handleDisplayChanged(_native);
	}

	@Override
	public void onDisplayRemoved(int displayId) {
		handleDisplayChanged(_native);
	}

	@Override
	public void onDisplayChanged(int displayId) {
		handleDisplayChanged(_native);
	}

	protected native void handleDisplayChanged(long nativePointer);

	DisplayManager _manager = null;
	long _native = 0;
}
