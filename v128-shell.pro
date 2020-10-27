requires(linux)
requires(qtHaveModule(gui))

QT += gui qml

SOURCES += \
    main.cpp

OTHER_FILES = \
    main.qml

RESOURCES += v128-shell.qrc

target.path = /usr/bin
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS v128-shell.pro
sources.path = /usr/bin
INSTALLS += target sources
