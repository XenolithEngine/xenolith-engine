/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLAndroidNetworkConnectivity.h"
#include "XLAndroidContextController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static void NetworkConnectivity_nativeOnCreated(JNIEnv *env, jobject thiz, jlong nativePointer,
		jobject caps, jobject props) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleCreated(env, caps, props);
}

static void NetworkConnectivity_nativeOnFinalized(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleFinalized(env);
}

static void NetworkConnectivity_nativeOnAvailable(JNIEnv *env, jobject thiz, jlong nativePointer,
		jobject caps, jobject props) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleAvailable(env, caps, props);
}

static void NetworkConnectivity_nativeOnLost(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleLost(env);
}

static void NetworkConnectivity_nativeOnCapabilitiesChanged(JNIEnv *env, jobject thiz,
		jlong nativePointer, jobject caps) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleCapabilitiesChanged(env, caps);
}

static void NetworkConnectivity_nativeOnLinkPropertiesChanged(JNIEnv *env, jobject thiz,
		jlong nativePointer, jobject props) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleLinkPropertiesChanged(env, props);
}

static JNINativeMethod s_networkMethods[] = {
	{"nativeOnCreated", "(JLandroid/net/NetworkCapabilities;Landroid/net/LinkProperties;)V",
		reinterpret_cast<void *>(&NetworkConnectivity_nativeOnCreated)},
	{"nativeOnFinalized", "(J)V", reinterpret_cast<void *>(&NetworkConnectivity_nativeOnFinalized)},
	{"nativeOnAvailable", "(JLandroid/net/NetworkCapabilities;Landroid/net/LinkProperties;)V",
		reinterpret_cast<void *>(&NetworkConnectivity_nativeOnAvailable)},
	{"nativeOnLost", "(J)V", reinterpret_cast<void *>(&NetworkConnectivity_nativeOnLost)},
	{"nativeOnCapabilitiesChanged", "(JLandroid/net/NetworkCapabilities;)V",
		reinterpret_cast<void *>(&NetworkConnectivity_nativeOnCapabilitiesChanged)},
	{"nativeOnLinkPropertiesChanged", "(JLandroid/net/LinkProperties;)V",
		reinterpret_cast<void *>(&NetworkConnectivity_nativeOnLinkPropertiesChanged)},
};

static void registerNetowrkMethods(const jni::RefClass &cl) {
	static std::atomic<int> s_counter = 0;

	if (s_counter.fetch_add(1) == 0) {
		cl.registerNatives(s_networkMethods);
	}
}

static NetworkFlags readCapabilities(const NetworkConnectivity::NetworkConnectivityProxy &proxy,
		const jni::Ref &caps) {
	auto app = jni::Env::getApp();
	NetworkFlags ret = NetworkFlags::None;

	const auto &Caps = app->NetworkCapabilities;

	if (Caps.NET_CAPABILITY_INTERNET()
			&& Caps.hasCapability(caps, Caps.NET_CAPABILITY_INTERNET())) {
		ret |= NetworkFlags::Internet;
	}

	if (Caps.NET_CAPABILITY_NOT_CONGESTED()
			&& !Caps.hasCapability(caps, Caps.NET_CAPABILITY_NOT_CONGESTED())) {
		ret |= NetworkFlags::Congested;
	}

	if (Caps.NET_CAPABILITY_NOT_METERED()
			&& !Caps.hasCapability(caps, Caps.NET_CAPABILITY_NOT_METERED())) {
		ret |= NetworkFlags::Metered;
	}

	if (Caps.NET_CAPABILITY_NOT_RESTRICTED()
			&& !Caps.hasCapability(caps, Caps.NET_CAPABILITY_NOT_RESTRICTED())) {
		ret |= NetworkFlags::Restricted;
	}

	if (Caps.NET_CAPABILITY_NOT_ROAMING()
			&& !Caps.hasCapability(caps, Caps.NET_CAPABILITY_NOT_ROAMING())) {
		ret |= NetworkFlags::Roaming;
	}

	if (Caps.NET_CAPABILITY_NOT_SUSPENDED()
			&& !Caps.hasCapability(caps, Caps.NET_CAPABILITY_NOT_SUSPENDED())) {
		ret |= NetworkFlags::Suspended;
	}

	if (Caps.NET_CAPABILITY_NOT_VPN() && !Caps.hasCapability(caps, Caps.NET_CAPABILITY_NOT_VPN())) {
		ret |= NetworkFlags::Vpn;
	}

	if (Caps.NET_CAPABILITY_PRIORITIZE_BANDWIDTH()
			&& Caps.hasCapability(caps, Caps.NET_CAPABILITY_PRIORITIZE_BANDWIDTH())) {
		ret |= NetworkFlags::PrioritizeBandwidth;
	}

	if (Caps.NET_CAPABILITY_PRIORITIZE_LATENCY()
			&& Caps.hasCapability(caps, Caps.NET_CAPABILITY_PRIORITIZE_LATENCY())) {
		ret |= NetworkFlags::PrioritizeLatency;
	}

	if (Caps.NET_CAPABILITY_TEMPORARILY_NOT_METERED()
			&& Caps.hasCapability(caps, Caps.NET_CAPABILITY_TEMPORARILY_NOT_METERED())) {
		ret |= NetworkFlags::TemporarilyNotMetered;
	}

	if (Caps.NET_CAPABILITY_TRUSTED() && Caps.hasCapability(caps, Caps.NET_CAPABILITY_TRUSTED())) {
		ret |= NetworkFlags::Trusted;
	}

	if (Caps.NET_CAPABILITY_VALIDATED()
			&& Caps.hasCapability(caps, Caps.NET_CAPABILITY_VALIDATED())) {
		ret |= NetworkFlags::Validated;
	}

	if (Caps.NET_CAPABILITY_WIFI_P2P()
			&& Caps.hasCapability(caps, Caps.NET_CAPABILITY_WIFI_P2P())) {
		ret |= NetworkFlags::WifiP2P;
	}

	/*if (Caps.getTransportInfo) {
		if (auto ti = Caps.getTransportInfo(caps)) {
			slog().debug("NetworkConnectivity", ti.getClassName().getString());
		}
	}*/

	if (Caps.hasTransport) {
		if (Caps.hasTransport(caps, Caps.TRANSPORT_ETHERNET())) {
			ret |= NetworkFlags::Wired;
		}
		if (Caps.hasTransport(caps, Caps.TRANSPORT_WIFI())) {
			ret |= NetworkFlags::WLAN;
		}
		if (Caps.hasTransport(caps, Caps.TRANSPORT_CELLULAR())) {
			ret |= NetworkFlags::WLAN;
		}
		if (Caps.hasTransport(caps, Caps.TRANSPORT_VPN())) {
			ret |= NetworkFlags::Vpn;
		}
	}

	return ret;
}

void NetworkConnectivity::finalize() {
	if (proxy && thiz) {
		proxy.finalize(thiz.ref(jni::Env::getEnv()));
	}

	thiz = nullptr;
}

void NetworkConnectivity::handleCreated(JNIEnv *env, jobject caps, jobject props) {
	if (proxy && caps) {
		flags = readCapabilities(proxy, jni::Ref(caps, env));
		if (callback) {
			callback(flags);
		}
	} else if (flags != NetworkFlags::None) {
		flags = NetworkFlags::None;
		if (callback) {
			callback(flags);
		}
	}
}

void NetworkConnectivity::handleFinalized(JNIEnv *env) {
	flags = NetworkFlags::None;
	callback = nullptr;
}

void NetworkConnectivity::handleAvailable(JNIEnv *env, jobject caps, jobject props) {
	if (proxy && caps) {
		flags = readCapabilities(proxy, jni::Ref(caps, env));
		if (callback) {
			callback(flags);
		}
	} else if (flags != NetworkFlags::None) {
		flags = NetworkFlags::None;
		if (callback) {
			callback(flags);
		}
	}
}

void NetworkConnectivity::handleLost(JNIEnv *env) {
	//log::source().verbose("NetworkConnectivity", "onLost");
	flags = NetworkFlags::None;
	if (callback) {
		callback(flags);
	}
}

void NetworkConnectivity::handleCapabilitiesChanged(JNIEnv *env, jobject caps) {
	if (proxy && caps) {
		flags = readCapabilities(proxy, jni::Ref(caps, env));
		if (callback) {
			callback(flags);
		}
	} else if (flags != NetworkFlags::None) {
		flags = NetworkFlags::None;
		if (callback) {
			callback(flags);
		}
	}
}

void NetworkConnectivity::handleLinkPropertiesChanged(JNIEnv *env, jobject props) {
	//log::source().verbose("NetworkConnectivity", "onLinkPropertiesChanged");
}

NetworkConnectivity::~NetworkConnectivity() { finalize(); }

NetworkConnectivity::NetworkConnectivity(const jni::Ref &context, Function<void(NetworkFlags)> &&cb)
: proxy(jni::Env::getClassLoader()->findClass(context.getEnv(), NetworkConnectivityClassName)) {

	auto clazz = proxy.getClass().ref(context.getEnv());
	if (proxy) {
		registerNetowrkMethods(clazz);

		thiz = proxy.create(clazz, context, reinterpret_cast<jlong>(this));
		callback = sp::move(cb);
		if (callback) {
			callback(flags);
		}
	}
}

} // namespace stappler::xenolith::platform
