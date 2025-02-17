# =============================================================================
#
# HAL implementation of Android Gatekeeper
#
# =============================================================================

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := gatekeeper.$(TARGET_SOC)
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_CFLAGS := -Wall -Werror -Wmissing-field-initializers -Wunused-parameter
# Software implementation is written in C++11
LOCAL_CPPFLAGS := -std=c++11
LOCAL_CPPFLAGS += -fpermissive -Wall -Werror -D__STDC_LIMIT_MACROS
LOCAL_CPPFLAGS += -D__FAILURE_RECORD_STORAGE_IN_SECURE_OBJECT__
LOCAL_LDLIBS := -llog

ALL_SRC_FILES := $(wildcard ${LOCAL_PATH}/src/*.c ${LOCAL_PATH}/src/*.cpp ${LOCAL_PATH}/src/gatekeeper_src/*.cpp)
LOCAL_SRC_FILES := $(ALL_SRC_FILES:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include

LOCAL_C_INCLUDES += external/libcxx/include
LOCAL_C_INCLUDES += hardware/libhardware/include/
LOCAL_C_INCLUDES += system/core/libsystem/include/
LOCAL_C_INCLUDES += system/core/libcutils/include_outside_system/

LOCAL_SHARED_LIBRARIES := libMcClient

LOCAL_SHARED_LIBRARIES += libgatekeeper

LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DUSE_VENDOR_PARTITION

include $(BUILD_SHARED_LIBRARY)
