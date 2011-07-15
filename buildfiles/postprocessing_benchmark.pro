
CONFIG(debug,debug|release) {
	LIBS+=debug/libraw.lib
} else {
	LIBS+=release/libraw.lib
}
INCLUDEPATH=../
CONFIG-=qt
win32:CONFIG+=console
SOURCES=../samples/postprocessing_benchmark.cpp
CONFIG+=warn_off
