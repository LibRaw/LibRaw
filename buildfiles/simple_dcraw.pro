
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
SOURCES=../samples/simple_dcraw.cpp
CONFIG+=warn_off
