package org.stappler.xenolith.core;

import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Insets;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.widget.FrameLayout;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

public class AppSupportActivity extends NativeActivity {
	private long _nativePointer = 0;
	private View _contentView = null;
	private TextInputWrapper _input = null;
	private DisplayManager displayManager;
	private DisplayManager.DisplayListener displayListener;
	private OnBackInvokedCallback backCallback;
	private boolean backCallbackEnabled = false;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		boolean decorFitsSystemWindows = false;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && Build.VERSION.SDK_INT < 35) {
			final int stableFlag = View.SYSTEM_UI_FLAG_LAYOUT_STABLE;

			final View decorView = getWindow().getDecorView();
			final int sysUiVis = decorView.getSystemUiVisibility();
			decorView.setSystemUiVisibility(decorFitsSystemWindows
					? sysUiVis & ~stableFlag
					: sysUiVis | stableFlag);
		}
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
			getWindow().setDecorFitsSystemWindows(decorFitsSystemWindows);
		}
	}

	@Override
	protected void onStart() {
		super.onStart();
	}

	@Override
	protected void onResume() {
		super.onResume();
	}

	@Override
	protected void onDestroy() {
		if (displayManager != null && displayListener != null) {
			displayManager.unregisterDisplayListener(displayListener);
		}

		super.onDestroy();

		_nativePointer = 0;
	}

	@Override
	public View onCreateView(View parent, String name, Context context, AttributeSet attrs) {
		return super.onCreateView(parent, name, context, attrs);
	}

	@Override
	public View onCreateView(String name, Context context, AttributeSet attrs) {
		return super.onCreateView(name, context, attrs);
	}

	@Override
	public void setContentView(View view) {
		view.setOnApplyWindowInsetsListener((v, insets) -> {
			// Get the system window insets (e.g., status bar, navigation bar)
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
				boolean statusBarVisible = insets.isVisible(WindowInsets.Type.statusBars());
				boolean navigationVisible = insets.isVisible(WindowInsets.Type.navigationBars());

				handleInsetsVisible(_nativePointer, statusBarVisible, navigationVisible);

				Insets i = insets.getInsets(WindowInsets.Type.systemBars());
				this.handleContentInsets(_nativePointer, i.top, i.right, i.bottom, i.left);

				Insets ime = insets.getInsets(WindowInsets.Type.ime());
				this.handleImeInsets(_nativePointer, ime.top, ime.right, ime.bottom, ime.left);

			} else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
				Insets i = insets.getStableInsets();
				this.handleContentInsets(_nativePointer, i.top, i.right, i.bottom, i.left);
			} else {
				this.handleContentInsets(_nativePointer,
						insets.getStableInsetTop(), insets.getStableInsetRight(),
						insets.getStableInsetBottom(), insets.getStableInsetLeft());
			}

			return insets;
		});

		ViewGroup.LayoutParams framelayout_params =
				new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
						ViewGroup.LayoutParams.MATCH_PARENT);
		FrameLayout _frameLayout = new FrameLayout(this);
		_frameLayout.setLayoutParams(framelayout_params);

		ViewGroup.LayoutParams edittext_layout_params =
				new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
						ViewGroup.LayoutParams.WRAP_CONTENT);
		TextInputEditText editText = new TextInputEditText(this);
		editText.setLayoutParams(edittext_layout_params);

		_frameLayout.addView(editText);
		_contentView = view;
		// ...add to FrameLayout
		_frameLayout.addView(view);

		_input = new TextInputWrapper(this, editText);

		super.setContentView(_frameLayout);
	}

	public View getContentView() {
		return _contentView;
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);

		handleActivityResult(_nativePointer, requestCode, resultCode, data);
	}
	public void runInput(String text, int cursorStart, int cursorLen, int type) {
		_input.runInput(text, cursorStart, cursorLen, type);
	}

	public void updateInput(String text, int cursorStart, int cursorLen, int type) {
		_input.updateInput(text, cursorStart, cursorLen, type);
	}

	public void updateCursor(int cursorStart, int cursorLen) {
		_input.updateCursor(cursorStart, cursorLen);
	}

	public void cancelInput() {
		_input.cancelInput();
	}

	/*private void askNotificationPermission() {
		// This is only necessary for API level >= 33 (TIRAMISU)
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
			if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) !=
					PackageManager.PERMISSION_GRANTED) {
				// Directly ask for the permission
				Intent intent = _activityResultContract.createIntent(this, Manifest.permission.POST_NOTIFICATIONS);
				startActivityForResult(intent, PERMISSION_REQUEST_ID);
			} else {
				_hasNotificationPermission = true;
			}
		} else {
			_hasNotificationPermission = true;
		}
	}*/

	protected void openURL( String inURL ) {
		Intent browse = new Intent( Intent.ACTION_VIEW , Uri.parse( inURL ) );
		browse.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		startActivity( browse );
	}

	public long getNative() {
		return _nativePointer;
	}

	protected void setNative(long nativePointer) {
		_nativePointer = nativePointer;
	}

	protected void setBackButtonHandlerEnabled(boolean enabled) {
		if (backCallbackEnabled == enabled) {
			return;
		}

		backCallbackEnabled = enabled;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
			if (backCallbackEnabled) {
				backCallback = new OnBackInvokedCallback() {
					@Override
					public void onBackInvoked() {
						handleBackInvoked(_nativePointer);
					}
				};

				getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
					OnBackInvokedDispatcher.PRIORITY_DEFAULT,
					backCallback
				);
			} else {
				getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(
					backCallback
				);
				backCallback = null;
			}
		}
	}

	protected native void handleActivityResult(long nativePtr, int requestCode, int resultCode, Intent data);
	protected native void handleInsetsVisible(long nativePtr, boolean statusBarVisible, boolean navigationVisible);
	protected native void handleContentInsets(long nativePtr, int top, int right, int bottom, int left);
	protected native void handleImeInsets(long nativePtr, int top, int right, int bottom, int left);
	protected native void handleBackInvoked(long nativePtr);

	static {
		System.loadLibrary("application");
	}
}
