package org.stappler.xenolith.core;

import static java.util.Arrays.copyOf;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.content.pm.ProviderInfo;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

// Based on AndroidX source code
public class ClipboardContentProvider extends ContentProvider {
	public static ClipboardContentProvider Self = null;

	private static final String[] COLUMNS = {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
	private static final String DISPLAYNAME_FIELD = "displayName";

	private String _authority;
	private final Object _lock = new Object();
	private long _native = 0;

	@Override
	public boolean onCreate() {
		Self = this;
		return true;
	}

	@Override
	public void attachInfo(Context context, ProviderInfo info) {
		super.attachInfo(context, info);

		// Check our security attributes.
		if (info.exported) {
			// Our intent here is to help application developers to avoid *accidentally* opening up
			// this provider to the "world" which may lead to vulnerabilities in their applications.
			throw new SecurityException("Provider must not be exported");
		}
		if (!info.grantUriPermissions) {
			throw new SecurityException("Provider must grant uri permissions");
		}
		if (info.authority == null || info.authority.trim().isEmpty()) {
			throw new SecurityException("Provider must have a non-empty authority");
		}

		final String authority = info.authority.split(";")[0];
		synchronized (_lock) {
			_authority = authority;
		}
	}

	@Override
	public Cursor query(Uri uri, String [] projection, String selection, String [] selectionArgs,
			String sortOrder) {
		if (_native == 0) {
			return null;
		}

		String filePath = getPathForUri(_native, uri.toString());
		if (filePath == null || filePath.isEmpty()) {
			return null;
		}

		String displayName = uri.getQueryParameter(DISPLAYNAME_FIELD);
		final File file = new File(filePath);

		if (projection == null) {
			projection = COLUMNS;
		}

		String[] cols = new String[projection.length];
		Object[] values = new Object[projection.length];
		int i = 0;
		for (String col : projection) {
			if (OpenableColumns.DISPLAY_NAME.equals(col)) {
				cols[i] = OpenableColumns.DISPLAY_NAME;
				values[i++] = (displayName == null) ? file.getName() : displayName;
			} else if (OpenableColumns.SIZE.equals(col)) {
				cols[i] = OpenableColumns.SIZE;
				values[i++] = file.length();
			}
		}

		cols = copyOf(cols, i);
		values = copyOf(values, i);

		final MatrixCursor cursor = new MatrixCursor(cols, 1);
		cursor.addRow(values);
		return cursor;
	}

	@Override
	public String getType(Uri uri) {
		if (_native == 0) {
			return "application/octet-stream";
		}
		String type = getTypeForUri(_native, uri.toString());
		if (type == null || type.isEmpty()) {
			return "application/octet-stream";
		}
		return type;
	}

	@Override
	public String getTypeAnonymous(Uri uri) {
		return "application/octet-stream";
	}

	@Override
	public Uri insert(Uri uri, ContentValues contentValues) {
		throw new UnsupportedOperationException("No external inserts");
	}

	@Override
	public int delete(Uri uri, String s, String[] strings) {
		throw new UnsupportedOperationException("No external delete");
	}

	@Override
	public int update(Uri uri, ContentValues contentValues, String s, String[] strings) {
		throw new UnsupportedOperationException("No external updates");
	}

	@Override
	public ParcelFileDescriptor openFile(Uri uri, String mode)
			throws FileNotFoundException {
		if (_native == 0) {
			return null;
		}

		String path = getPathForUri(_native, uri.toString());
		if (path == null || path.isEmpty()) {
			throw new FileNotFoundException();
		}
		// ContentProvider has already checked granted permissions
		final File file = new File(path);
		if (mode.equals("r")) {
			return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
		}
		return null;
	}

	public long getNative() {
		return _native;
	}

	protected void setNative(long nativePointer) {
		_native = nativePointer;
	}

	protected String getAuthority() {
		return _authority;
	}

	protected native String getTypeForUri(long nativePtr, String uri);
	protected native String getPathForUri(long nativePtr, String uri);
}
