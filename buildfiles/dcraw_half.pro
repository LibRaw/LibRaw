
win32:CONFIG(debug,debug|release) {
	LIBS+=-Ldebug/
} else {
	LIBS+=-Lrelease/
}
win32:LIBS+=libraw.lib
unix:LIBS+=-lraw
INCLUDEPATH=../
CONFIG-=qt
win32:CONFIG+=console
SOURCES=../samples/dcraw_half.c
CONFIG+=warn_off
