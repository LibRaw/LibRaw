
win32:CONFIG(debug,debug|release) {
	LIBS+=-Ldebug/
} else {
	LIBS+=-Lrelease/
}
win32:LIBS+=libraw.lib
unix:LIBS+=-lraw
INCLUDEPATH=../
CONFIG-=qt
win32 {
	CONFIG+=console
	SOURCES=../samples/half_mt_win32.c
} else {
	SOURCES=../samples/half_mt.c
}
CONFIG+=warn_off
win32-g++:
{
    LIBS += -lws2_32
}
