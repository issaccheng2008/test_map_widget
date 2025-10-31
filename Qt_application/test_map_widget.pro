#-------------------------------------------------
#  Copyright 2025 ESRI
#
#  All rights reserved under the copyright laws of the United States
#  and applicable international laws, treaties, and conventions.
#
#  You may freely redistribute and use this sample code, with or
#  without modification, provided you include the original copyright
#  notice and use restrictions.
#
#  See the Sample code usage restrictions document for further information.
#-------------------------------------------------

TARGET = test_map_widget
TEMPLATE = app

CONFIG += c++17

# additional modules are pulled in via arcgisruntime.pri
QT += widgets concurrent network

lessThan(QT_MAJOR_VERSION, 6) {
    error("$$TARGET requires Qt 6.8.2")
}

equals(QT_MAJOR_VERSION, 6) {
    lessThan(QT_MINOR_VERSION, 8) {
        error("$$TARGET requires Qt 6.8.2")
    }
	equals(QT_MINOR_VERSION, 8) : lessThan(QT_PATCH_VERSION, 2) {
		error("$$TARGET requires Qt 6.8.2")
	}
}

ARCGIS_RUNTIME_VERSION = 200.8.0
include($$PWD/arcgisruntime.pri)

win32:CONFIG += \
    embed_manifest_exe

SOURCES += \
    main.cpp \
    Test_map_widget.cpp \
    OverlayImageWidget.cpp \
    GridPreviewWindow.cpp \
    generate_path.cpp \
    network.cpp

HEADERS += \
    Test_map_widget.h \
    OverlayImageWidget.h \
    GridPreviewWindow.h \
    GridState.h \
    generate_path.h \
    ImageScalingConstants.h \
    network.h

FORMS += \
    Test_map_widget.ui

RESOURCES += \
    resources.qrc

#-------------------------------------------------------------------------------
