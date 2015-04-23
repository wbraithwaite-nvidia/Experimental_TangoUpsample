# NOT YET FINISHED AND COMPLETELY UNTESTED!

LOCAL_PATH:= $(call my-dir)/..

include $(CLEAR_VARS)
LOCAL_MODULE := libtango-prebuilt
LOCAL_SRC_FILES := ../third/lib/libtango_client_api.so
LOCAL_EXPORT_C_INCLUDES := ../third/inc
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libOddTangoUpsample
LOCAL_SHARED_LIBRARIES := libtango-prebuilt
LOCAL_CFLAGS    := -std=c++11
LOCAL_SRC_FILES := jni/TangoUpsampleNative.cpp \
                   jni/Tango.cpp \
				   jni/GlVideoOverlay.cpp \
                   ../tango-gl-renderer/ar_ruler.cpp \
                   ../tango-gl-renderer/axis.cpp \
                   ../tango-gl-renderer/cube.cpp \
                   ../tango-gl-renderer/camera.cpp \
                   ../tango-gl-renderer/frustum.cpp \
                   ../tango-gl-renderer/gl_util.cpp \
                   ../tango-gl-renderer/grid.cpp \
                   ../tango-gl-renderer/trace.cpp \
                   ../tango-gl-renderer/transform.cpp 
                   
LOCAL_C_INCLUDES := ../tango-gl-renderer/include \
					../third/inc \
                    ../third/inc/glm
					
LOCAL_LDLIBS    := -llog -lGLESv3 -L$(SYSROOT)/usr/lib
include $(BUILD_SHARED_LIBRARY)
