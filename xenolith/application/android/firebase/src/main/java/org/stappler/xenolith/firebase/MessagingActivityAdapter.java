package org.stappler.xenolith.firebase;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.firebase.FirebaseApp;
import com.google.firebase.messaging.FirebaseMessaging;

public class MessagingActivityAdapter {
	Activity _activity = null;
	long _nativePointer = 0;
	int _requestId = 1;
	boolean _hasNotificationPermission = false;
	ActivityResultContracts.RequestPermission _activityResultContract = new ActivityResultContracts.RequestPermission();

	MessagingActivityAdapter(Activity activity, long nativePointer, int requestId) {
		_activity = activity;
		_nativePointer = nativePointer;
		_requestId = requestId;

		FirebaseApp.initializeApp(activity);
	}

	private void askNotificationPermission() {
		// This is only necessary for API level >= 33 (TIRAMISU)
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
			if (_activity.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) !=
					PackageManager.PERMISSION_GRANTED) {
				// Directly ask for the permission
				Intent intent = _activityResultContract.createIntent(_activity, Manifest.permission.POST_NOTIFICATIONS);
				_activity.startActivityForResult(intent, _requestId);
			} else {
				_hasNotificationPermission = true;
			}
		} else {
			_hasNotificationPermission = true;
		}
	}

	private void acquireToken() {
		FirebaseMessaging.getInstance().getToken()
				.addOnCompleteListener(new OnCompleteListener<String>() {
					@Override
					public void onComplete(@NonNull Task<String> task) {
						if (!task.isSuccessful()) {
							Log.w("AppSupportActivity", "Fetching FCM registration token failed", task.getException());
							return;
						}

						// Get new FCM registration token
						onTokenReceived(_nativePointer, task.getResult());
					}
				});
	}

	private void parseResult(int resultCode, Intent data) {
		_activityResultContract.parseResult(resultCode, data);
	}

	private native void onTokenReceived(long nativePointer, String value);
}
