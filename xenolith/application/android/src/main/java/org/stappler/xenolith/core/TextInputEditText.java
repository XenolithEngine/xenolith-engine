package org.stappler.xenolith.core;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.widget.EditText;

public class TextInputEditText extends EditText {

	public TextInputEditText(final Context context) {
		super(context);
	}

	public TextInputEditText(final Context context, final AttributeSet attrs) {
		super(context, attrs);
	}

	public TextInputEditText(final Context context, final AttributeSet attrs, final int defStyle) {
		super(context, attrs, defStyle);
	}

	@Override
	public boolean onKeyPreIme(final int pKeyCode, final KeyEvent pKeyEvent) {
		/* Let GlSurfaceView get focus if back key is input. */
		if (pKeyCode == KeyEvent.KEYCODE_BACK && pKeyEvent.getAction() == KeyEvent.ACTION_UP)  {
			clearFocus();
			//_wrapper.cancelInput();
		}

		return super.onKeyPreIme(pKeyCode, pKeyEvent);
	}
}
