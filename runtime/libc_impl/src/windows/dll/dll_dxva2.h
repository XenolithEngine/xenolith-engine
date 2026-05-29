#include "dlltable.h"

namespace sprt {

struct Dxva2Table : DllTable {
	Dxva2Table() : DllTable(L"dxva2.dll", &__dxva2_begin, &__dxva2_end) { }

	DllTableRecord __dxva2_begin;
	SPWIN_DEFINE_PROTO(CapabilitiesRequestAndCapabilitiesReply)
	SPWIN_DEFINE_PROTO(DXVA2CreateDirect3DDeviceManager9)
	SPWIN_DEFINE_PROTO(DXVA2CreateVideoService)
	SPWIN_DEFINE_PROTO(DXVAHD_CreateDevice)
	SPWIN_DEFINE_PROTO(DegaussMonitor)
	SPWIN_DEFINE_PROTO(DestroyPhysicalMonitor)
	SPWIN_DEFINE_PROTO(DestroyPhysicalMonitors)
	SPWIN_DEFINE_PROTO(GetCapabilitiesStringLength)
	SPWIN_DEFINE_PROTO(GetMonitorBrightness)
	SPWIN_DEFINE_PROTO(GetMonitorCapabilities)
	SPWIN_DEFINE_PROTO(GetMonitorColorTemperature)
	SPWIN_DEFINE_PROTO(GetMonitorContrast)
	SPWIN_DEFINE_PROTO(GetMonitorDisplayAreaPosition)
	SPWIN_DEFINE_PROTO(GetMonitorDisplayAreaSize)
	SPWIN_DEFINE_PROTO(GetMonitorRedGreenOrBlueDrive)
	SPWIN_DEFINE_PROTO(GetMonitorRedGreenOrBlueGain)
	SPWIN_DEFINE_PROTO(GetMonitorTechnologyType)
	SPWIN_DEFINE_PROTO(GetNumberOfPhysicalMonitorsFromHMONITOR)
	SPWIN_DEFINE_PROTO(GetNumberOfPhysicalMonitorsFromIDirect3DDevice9)
	SPWIN_DEFINE_PROTO(GetPhysicalMonitorsFromHMONITOR)
	SPWIN_DEFINE_PROTO(GetPhysicalMonitorsFromIDirect3DDevice9)
	SPWIN_DEFINE_PROTO(GetTimingReport)
	SPWIN_DEFINE_PROTO(GetVCPFeatureAndVCPFeatureReply)
	SPWIN_DEFINE_PROTO(OPMGetVideoOutputsFromHMONITOR)
	SPWIN_DEFINE_PROTO(OPMGetVideoOutputsFromIDirect3DDevice9Object)
	SPWIN_DEFINE_PROTO(RestoreMonitorFactoryColorDefaults)
	SPWIN_DEFINE_PROTO(RestoreMonitorFactoryDefaults)
	SPWIN_DEFINE_PROTO(SaveCurrentMonitorSettings)
	SPWIN_DEFINE_PROTO(SaveCurrentSettings)
	SPWIN_DEFINE_PROTO(SetMonitorBrightness)
	SPWIN_DEFINE_PROTO(SetMonitorColorTemperature)
	SPWIN_DEFINE_PROTO(SetMonitorContrast)
	SPWIN_DEFINE_PROTO(SetMonitorDisplayAreaPosition)
	SPWIN_DEFINE_PROTO(SetMonitorDisplayAreaSize)
	SPWIN_DEFINE_PROTO(SetMonitorRedGreenOrBlueDrive)
	SPWIN_DEFINE_PROTO(SetMonitorRedGreenOrBlueGain)
	SPWIN_DEFINE_PROTO(SetVCPFeature)
	SPWIN_DEFINE_PROTO(UABGetCertificate)
	DllTableRecord __dxva2_end;
};

} // namespace sprt
