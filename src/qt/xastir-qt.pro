#-------------------------------------------------
#
# Project created by QtCreator 2010-02-24T13:21:10
#
#-------------------------------------------------

QT       += network

TARGET = xastir-qt
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    packetinterface.cpp \
    netinterface.cpp

HEADERS  += \
    xastir.h \
    packetinterface.h \
    netinterface.h

FORMS    += mainwindow.ui
