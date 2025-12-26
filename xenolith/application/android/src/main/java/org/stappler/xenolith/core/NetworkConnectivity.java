package org.stappler.xenolith.core;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;

public class NetworkConnectivity extends ConnectivityManager.NetworkCallback {
	public static NetworkConnectivity create(Context context, long nativePointer) {
		ConnectivityManager manager = context.getSystemService(ConnectivityManager.class);
		if (manager != null) {
			return create(manager, nativePointer);
		}
		return null;
	}

	public static NetworkConnectivity create(ConnectivityManager manager, long nativePointer) {
		NetworkConnectivity ret = new NetworkConnectivity(manager, nativePointer);
		manager.registerDefaultNetworkCallback(ret);
		return ret;
	}

	protected NetworkConnectivity(ConnectivityManager manager, long nativePointer) {
		_native = nativePointer;
		_manager = manager;
		_network = _manager.getActiveNetwork();
		if (_network != null) {
			_capabilities = _manager.getNetworkCapabilities(_network);
			_properties = _manager.getLinkProperties(_network);
		}

		if (_capabilities != null) {
			nativeOnCreated(_native, _capabilities, _properties);
		} else {
			nativeOnCreated(_native, null, null);
		}
	}

	protected void finalize() {
		if (_manager != null) {
			nativeOnFinalized(_native);
			_manager.unregisterNetworkCallback(this);
			_manager = null;
			_network = null;
			_capabilities = null;
			_properties = null;
			_native = 0;
		}
	}

	@Override
	public void onAvailable(Network network) {
		_network = network;
		_capabilities = _manager.getNetworkCapabilities(_network);
		_properties = _manager.getLinkProperties(_network);
		nativeOnAvailable(_native, _capabilities, _properties);
	}

	@Override
	public void onLost(Network network) {
		nativeOnLost(_native);
		_network = null;
	}

	@Override
	public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {
		if (_network == network) {
			_capabilities = networkCapabilities;
			nativeOnCapabilitiesChanged(_native, networkCapabilities);
		}
	}

	@Override
	public void onLinkPropertiesChanged(Network network, LinkProperties linkProperties) {
		if (_network == network) {
			_properties = linkProperties;
			nativeOnLinkPropertiesChanged(_native, linkProperties);
		}
	}

	private native void nativeOnCreated(long nativePointer, NetworkCapabilities caps, LinkProperties props);
	private native void nativeOnFinalized(long nativePointer);
	private native void nativeOnAvailable(long nativePointer, NetworkCapabilities caps, LinkProperties props);
	private native void nativeOnLost(long nativePointer);
	private native void nativeOnCapabilitiesChanged(long nativePointer, NetworkCapabilities caps);
	private native void nativeOnLinkPropertiesChanged(long nativePointer, LinkProperties props);

	ConnectivityManager _manager;
	NetworkCapabilities _capabilities = null;
	LinkProperties _properties = null;
	Network _network;
	long _native;
}
