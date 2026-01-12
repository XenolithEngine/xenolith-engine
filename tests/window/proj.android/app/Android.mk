LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := application
LOCAL_WHOLE_STATIC_LIBRARIES := stappler_application_generic

include $(BUILD_SHARED_LIBRARY)

$(call import-add-path,$(LOCAL_PATH)/../..)
$(call import-module,stappler-build/android)
