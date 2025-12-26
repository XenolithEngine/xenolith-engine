package org.stappler.xenolith.core;

import android.content.ClipboardManager;
import android.content.Context;

public class ClipboardListener implements ClipboardManager.OnPrimaryClipChangedListener {
	public static ClipboardListener create(Context context, long nativePointer) {
		ClipboardManager manager = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
		if (manager != null) {
			return create(manager, nativePointer);
		}
		return null;
	}

	public static ClipboardListener create(ClipboardManager manager, long nativePointer) {
		ClipboardListener ret = new ClipboardListener(manager, nativePointer);
		manager.addPrimaryClipChangedListener(ret);
		return ret;
	}

	protected ClipboardListener(ClipboardManager manager, long nativePointer) {
		_native = nativePointer;
		_manager = manager;
	}

	protected void finalize() {
		if (_manager != null) {
			_manager.removePrimaryClipChangedListener(this);
			_manager = null;
			_native = 0;
		}
	}

	@Override
	public void onPrimaryClipChanged() {
		handleClipChanged(_native);
	}

	protected native void handleClipChanged(long nativePtr);

	ClipboardManager _manager;
	long _native;
}
