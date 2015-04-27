
LOCAL_PATH	:= $(call my-dir)/..

include $(CLEAR_VARS)
LOCAL_MODULE := libtango-prebuilt
LOCAL_SRC_FILES := third/lib/libtango_client_api.so
LOCAL_EXPORT_C_INCLUDES := third/inc
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libOddTangoUpsample
LOCAL_SHARED_LIBRARIES := libtango-prebuilt
LOCAL_CFLAGS    := -std=c++11
LOCAL_SRC_FILES := jni/TangoUpsampleNative.cpp \
                   jni/Tango.cpp \
				   jni/GlVideoOverlay.cpp \
				   jni/GlBilateralGrid.cpp \
				   jni/GlPlaneMesh.cpp \
				   jni/GlPointcloud.cpp \
				   jni/GlQuad.cpp \
				   jni/GlDepthUpsampler.cpp \
				   jni/MaterialShaders.cpp \
				   jni/GlMaterial.cpp \
                   modules/tango-gl-renderer/ar_ruler.cpp \
                   modules/tango-gl-renderer/axis.cpp \
                   modules/tango-gl-renderer/cube.cpp \
                   modules/tango-gl-renderer/camera.cpp \
                   modules/tango-gl-renderer/frustum.cpp \
                   modules/tango-gl-renderer/gl_util.cpp \
                   modules/tango-gl-renderer/grid.cpp \
                   modules/tango-gl-renderer/trace.cpp \
                   modules/tango-gl-renderer/transform.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/jni \
					$(LOCAL_PATH)/modules \
					$(LOCAL_PATH)/third/inc \
                    $(LOCAL_PATH)/third/inc/glm
LOCAL_LDLIBS    := -llog -lGLESv3 -L$(SYSROOT)/usr/lib
include $(BUILD_SHARED_LIBRARY)
