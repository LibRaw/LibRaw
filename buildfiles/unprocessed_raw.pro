
win32:CONFIG(debug,debug|release) {
	LIBS+=-Ldebug/
} else {
	LIBS+=-Lrelease/
}
win32:LIBS+=libraw.lib ws2_32.lib
unix:LIBS+=-lraw
INCLUDEPATH=../
CONFIG-=qt
win32:CONFIG+=console
SOURCES=../samples/unprocessed_raw.cpp
CONFIG+=warn_off
win32-g++:
{
    LIBS += -lws2_32
}
