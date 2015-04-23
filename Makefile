ifeq (CYGWIN, $(findstring CYGWIN, $(shell uname)))
	ANDROID=android.bat
else
	ANDROID=android
endif

default: install

prebuild:
	$(ANDROID) update project --path .

all: prebuild
	ndk-build V=1
	ant debug

install: all
	adb install -r bin/OddTangoUpsample-debug.apk

uninstall:
	adb uninstall com.odd.TangoUpsample

clean: prebuild
	ant clean
	ndk-build clean
	rm -rf libs obj
