
win32:CONFIG(debug,debug|release) {
	LIBS+=-Ldebug
} else {
	LIBS+=-Lrelease
}
unix:LIBS+=-lraw
win32:LIBS+=libraw.lib
INCLUDEPATH=../
CONFIG-=qt
win32:CONFIG+=console
SOURCES=../samples/multirender_test.cpp
CONFIG+=warn_off
