CONFIG(debug,debug|release) {
	win32:OUTD=debug-$$QMAKE_TARGET.arch
	macx:OUTD=debug
} else {
	win32:OUTD=release-$$QMAKE_TARGET.arch
	macx:OUTD=release
}
INCLUDEPATH=../
OBJECTS_DIR = $$OUTD/
MOC_DIR = $$OUTD/
RCC_DIR = $$OUTD/
UI_DIR = $$OUTD/
DESTDIR = $$OUTD/
LIBS+=-L$$OUTD
CONFIG+=warn_off

win32-g++:
{
    LIBS += -lws2_32
}
