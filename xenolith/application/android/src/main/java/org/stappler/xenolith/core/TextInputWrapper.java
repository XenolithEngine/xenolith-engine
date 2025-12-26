package org.stappler.xenolith.core;

import android.content.Context;
import android.text.InputType;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;

public class TextInputWrapper implements TextWatcher, TextView.OnEditorActionListener {

	private AppSupportActivity _activity = null;
	private EditText _editText = null;

	private boolean _inputProtected = false;
	private boolean _shouldUpdateString = false;
	private boolean _inputEnabled = false;

	// ===========================================================
	// Constructors
	// ===========================================================

	public TextInputWrapper(final AppSupportActivity activity, final EditText editText) {
		_activity = activity;
		_editText = editText;

		_editText.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);
		_editText.setOnEditorActionListener(this);

		_editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
			@Override
			public void onFocusChange(View v, boolean hasFocus) {
				if (hasFocus) {
					//got focus
				} else {
					TextInputWrapper.this.cancelInput();
				}
			}
		});
	}

	public boolean isFullScreenEdit() {
		final InputMethodManager imm = (InputMethodManager) _activity.getSystemService(Context.INPUT_METHOD_SERVICE);
		return imm.isFullscreenMode();
	}

	@Override
	public void afterTextChanged(final Editable s) {
		if (this.isFullScreenEdit()) {
			return;
		}

		handleTextChanged(s.toString());
	}

	@Override
	public void beforeTextChanged(final CharSequence pCharSequence, final int start, final int count, final int after) {

	}

	@Override
	public void onTextChanged(final CharSequence pCharSequence, final int start, final int before, final int count) {

	}

	@Override
	public boolean onEditorAction(final TextView pTextView, final int pActionID, final KeyEvent pKeyEvent) {
		if (_editText == pTextView && this.isFullScreenEdit()) {
			String text = pTextView.getText().toString();
			handleTextChanged(text);
		}

		if (pActionID == EditorInfo.IME_ACTION_DONE) {
			_editText.clearFocus();
			_activity.getContentView().requestFocus();
			finalizeInput();
		}

		return false;
	}

	public void runInput(String text, int cursorStart, int cursorLen, int type) {
		if (_editText != null) {
			_inputProtected = true;
			if (!isFullScreenEdit()) {
				_inputEnabled = true;
				nativeHandleInputEnabled(_activity.getNative(), true);
			}
			_editText.removeTextChangedListener(this);
			_editText.setText(text);
			_editText.setInputType(getInputTypeFromNative(type));
			if (cursorStart > text.length()) {
				cursorStart = text.length();
			}
			if (cursorStart < 0) {
				cursorStart = 0;
			}
			if (cursorLen == 0) {
				_editText.setSelection(cursorStart);
			} else {
				_editText.setSelection(cursorStart, cursorStart + cursorLen);
			}
			_editText.addTextChangedListener(this);
			_editText.requestFocus();

			final InputMethodManager imm = (InputMethodManager)_activity.getSystemService(Context.INPUT_METHOD_SERVICE);
			imm.showSoftInput(_editText, 0);
			_inputProtected = false;
			if (_shouldUpdateString) {
				handleTextChanged(_editText.getText().toString());
			}
		}
	}

	public void updateInput(String text, int cursorStart, int cursorLen, int type) {
		if (_editText != null) {
			_inputProtected = true;
			_editText.setInputType(getInputTypeFromNative(type));
			_editText.setText(text);
			if (text.isEmpty()) {
				_editText.setSelection(0);
			} else if (cursorLen == 0) {
				_editText.setSelection(cursorStart);
			} else {
				_editText.setSelection(cursorStart, cursorStart + cursorLen);
			}
			_inputProtected = false;
			if (_shouldUpdateString) {
				handleTextChanged(_editText.getText().toString());
			}
		}
	}

	public void updateCursor(int cursorStart, int cursorLen) {
		if (_editText != null) {
			_inputProtected = true;
			if (cursorLen == 0) {
				_editText.setSelection(cursorStart);
			} else {
				_editText.setSelection(cursorStart, cursorStart	+ cursorLen);
			}
			_inputProtected = false;
			if (_shouldUpdateString) {
				handleTextChanged(_editText.getText().toString());
			}
		}
	}

	public void cancelInput() {
		if (_editText != null && _inputEnabled) {
			_editText.removeTextChangedListener(this);
			final InputMethodManager imm = (InputMethodManager)_activity.getSystemService(Context.INPUT_METHOD_SERVICE);
			imm.hideSoftInputFromWindow(_editText.getWindowToken(), 0);
			_editText.clearFocus();
			_activity.getContentView().requestFocus();
			finalizeInput();
		}
	}

	public void finalizeInput() {
		if (_inputEnabled) {
			_inputEnabled = false;
			nativeHandleInputEnabled(_activity.getNative(), false);
		}
		nativeHandleCancelInput(_activity.getNative());
	}

	public void handleTextChanged(final String pText) {
		if (!_inputProtected) {
			boolean isFullscreen = isFullScreenEdit();
			if (isFullscreen) {
				nativeHandleInputEnabled(_activity.getNative(), true);
			}
			final int start = _editText.getSelectionStart();
			final int end = _editText.getSelectionEnd();
			nativeHandleTextChanged(_activity.getNative(), pText, start, end - start);
			if (isFullscreen) {
				nativeHandleInputEnabled(_activity.getNative(), false);
			}
			_shouldUpdateString = false;
		} else {
			_shouldUpdateString = true;
		}
	}

	private final static int InputType_Date_Date = 1;
	private final static int InputType_Date_DateTime = 2;
	private final static int InputType_Date_Time = 3;

	private final static int InputType_Number_Numbers = 4;
	private final static int InputType_Number_Decimial = 5;
	private final static int InputType_Number_Signed = 6;

	private final static int InputType_Phone = 7;

	private final static int InputType_Text_Text = 8;
	private final static int InputType_Text_Search = 9;
	private final static int InputType_Text_Punctuation = 10;
	private final static int InputType_Text_Email = 11;
	private final static int InputType_Text_Url = 12;

	private final static int InputType_ClassMask = 0x1F;
	private final static int InputType_PasswordBit = 0x20;
	private final static int InputType_MultiLineBit = 0x40;
	private final static int InputType_AutoCorrectionBit = 0x80;

	static public int getInputTypeFromNative(int nativeValue) {
		int cl = nativeValue & InputType_ClassMask;
		int ret = InputType.TYPE_CLASS_TEXT;
		if (cl == InputType_Date_Date || cl == InputType_Date_DateTime || cl == InputType_Date_Time) {
			ret = InputType.TYPE_CLASS_DATETIME;
			if (cl == InputType_Date_Date) {
				ret |= InputType.TYPE_DATETIME_VARIATION_DATE;
			} else if (cl == InputType_Date_DateTime) {
				ret |= InputType.TYPE_DATETIME_VARIATION_NORMAL;
			} else {
				ret |= InputType.TYPE_DATETIME_VARIATION_TIME;
			}
		} else if (cl == InputType_Number_Numbers || cl == InputType_Number_Decimial || cl == InputType_Number_Signed) {
			ret = InputType.TYPE_CLASS_NUMBER;
			if (cl == InputType_Number_Decimial) {
				ret |= InputType.TYPE_NUMBER_FLAG_DECIMAL;
			} else if (cl == InputType_Number_Signed) {
				ret |= InputType.TYPE_NUMBER_FLAG_SIGNED;
			}
			if ((nativeValue & InputType_PasswordBit) != 0) {
				ret |= InputType.TYPE_NUMBER_VARIATION_PASSWORD;
			} else {
				ret |= InputType.TYPE_NUMBER_VARIATION_NORMAL;
			}
		} else if (cl == InputType_Phone) {
			ret = InputType.TYPE_CLASS_PHONE;
		} else {
			ret = InputType.TYPE_CLASS_TEXT;
			if ((nativeValue & InputType_PasswordBit) != 0) {
				ret |= InputType.TYPE_TEXT_VARIATION_PASSWORD;
			} else if (cl == InputType_Text_Text || cl == InputType_Text_Punctuation) {
				ret |= InputType.TYPE_TEXT_VARIATION_NORMAL;
			} else if (cl == InputType_Text_Search) {
				ret |= InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;
			} else if (cl == InputType_Text_Email) {
				ret |= InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
			} else if (cl == InputType_Text_Url) {
				ret |= InputType.TYPE_TEXT_VARIATION_URI;
			}

			if ((nativeValue & InputType_PasswordBit) == 0) {
				if ((nativeValue & InputType_MultiLineBit) != 0) {
					ret |= InputType.TYPE_TEXT_FLAG_MULTI_LINE;
				}
				if ((nativeValue & InputType_AutoCorrectionBit) != 0) {
					ret |= InputType.TYPE_TEXT_FLAG_AUTO_CORRECT;
				} else {
					ret |= InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
				}
			}
		}
		return ret;
	}

	protected native void nativeHandleCancelInput(long nativePointer);
	protected native void nativeHandleTextChanged(long nativePointer, String text, int cursorStart, int cursorLen);
	protected native void nativeHandleInputEnabled(long nativePointer, boolean value);
}
