package org.stappler.xenolith.firebase;

import android.Manifest;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.util.DisplayMetrics;
import android.util.Log;

import androidx.annotation.NonNull;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.firebase.messaging.FirebaseMessaging;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

import org.stappler.xenolith.core.AppSupportActivity;

import java.util.Map;

public class MessagingService extends FirebaseMessagingService {
	public static final String TAG = "Stappler-GCM";
	public static final String NOTIGICATION_CHANNEL = "general-channel";
	public static final int NOTIFICATION_ID = 1;

	long _nativePointer = 0;

	String _channelName = "Оповещения";
	int _iconDrawable = 0;

	@Override
	public void onCreate() {
		super.onCreate();

		_nativePointer = onCreated();

		String channelId = "general-channel";
		if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
			NotificationChannel channel = new NotificationChannel(
					NOTIGICATION_CHANNEL,
					_channelName,
					NotificationManager.IMPORTANCE_HIGH);

			NotificationManager notificationManager =
					(NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
			notificationManager.createNotificationChannel(channel);
		}

		acquireToken();
	}

	@Override
	public void onDestroy() {
		onDestroyed(_nativePointer);
		super.onDestroy();
	}

	public void setDefaultIcon(int value) {
		_iconDrawable = value;
	}

	@Override
	public void onNewToken(@NonNull String s) {
		super.onNewToken(s);
		onTokenReceived(_nativePointer, s);
	}

	@Override
	public void onMessageReceived(RemoteMessage remoteMessage) {
		// Not getting messages here? See why this may be: https://goo.gl/39bRNJ
		Log.d(TAG, "From: " + remoteMessage.getFrom());

		RemoteMessage.Notification notification = remoteMessage.getNotification();

		try {
			Uri link = notification.getLink();
			// Check if message contains a notification payload.
			if (remoteMessage.getNotification() != null) {
				sendNotification(notification.getTitle(), notification.getBody(), link);
			} else {
				Map<String, String> data = remoteMessage.getData();
				String title = data.get("title");
				String alert = data.get("alert");
				String strLink = data.get("link");

				if (strLink != null && !strLink.isEmpty()) {
					sendNotification(title, alert, Uri.parse(strLink));
				} else {
					sendNotification(title, alert, link);
				}
			}
		} catch (Exception e) {
			Log.d(TAG, "Library is not loaded");
		}
	}

	private int dpToPx(Context context) {
		DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
		return Math.round(64 * (displayMetrics.xdpi / DisplayMetrics.DENSITY_DEFAULT));
	}

	private Bitmap getResizedBitmap(Bitmap bm, int newHeight, int newWidth) {
		int width = bm.getWidth();
		int height = bm.getHeight();
		float scaleWidth = ((float) newWidth) / width;
		float scaleHeight = ((float) newHeight) / height;

		Matrix matrix = new Matrix();
		matrix.postScale(scaleWidth, scaleHeight);

		return Bitmap.createBitmap(bm, 0, 0, width, height, matrix, false);
	}

	private Bitmap getIconBitmap(Context context) {
		int id = _iconDrawable;
		int iconSize = dpToPx(context);

		Bitmap bm = BitmapFactory.decodeResource(context.getResources(), id);
		return getResizedBitmap(bm , iconSize, iconSize);
	}

	private void sendNotification(String header, String message, Uri link) {
		if (header == null || message == null || header.isEmpty() || message.isEmpty()) {
			return;
		}

		Context context = getApplicationContext();

		Intent intent = new Intent(this, AppSupportActivity.class);
		intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
		if (link != null) {
			intent.putExtra(Intent.EXTRA_ORIGINATING_URI, link);
		}

		PendingIntent contentIntent = PendingIntent.getActivity(this, 0, intent,
				PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_IMMUTABLE);

		Notification.Builder mBuilder =
				new Notification.Builder(this)
						//.setLargeIcon(getIconBitmap(context))
						.setSmallIcon(_iconDrawable)
						.setContentTitle(header)
						.setContentText(message)
						.setAutoCancel(true)
						.setContentIntent(contentIntent)
						.setPriority(Notification.PRIORITY_HIGH);

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
			mBuilder.setChannelId(NOTIGICATION_CHANNEL).setColorized(false);
		}

		NotificationManager notificationManager =
				(NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

		notificationManager.notify(0, mBuilder.build());

		try {
			onRemoteNotification(_nativePointer, header, message, link.toString());
		} catch (UnsatisfiedLinkError e) {
			Log.d(TAG, "Library is not loaded (UnsatisfiedLinkError)");
		} catch (Exception e) {
			Log.d(TAG, "Library is not loaded");
		}
	}

	protected void acquireToken() {
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

	protected native long onCreated();
	protected native void onDestroyed(long nativePointer);
	protected native void onRemoteNotification(long nativePointer, String header, String message, String link);
	protected native void onTokenReceived(long nativePointer, String value);

	static {
		System.loadLibrary("application");
	}
}
