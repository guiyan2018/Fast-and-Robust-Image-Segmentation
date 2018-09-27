#-------------------------------------------------
#
# Project created by QtCreator 2017-11-23T18:31:57
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = imgSegmentation
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        src\main.cpp \
        src\mainwindow.cpp \
    src\Bilateral.cpp \
    src\GMM.cpp \
    src\mylabel.cpp

HEADERS += \
    src\Bilateral.h \
    src\gcgraph.hpp \
    src\GMM.h \
    src\mainwindow.h \
    src\mylabel.h

FORMS += \
        src\mainwindow.ui

INCLUDEPATH += D:\opencv\build\include \
               D:\opencv\build\include\opencv \
               D:\opencv\build\include\opencv2 \


LIBS += D:\opencv\build\x64\vc14\lib\opencv_world341.lib

