/********************************************************************************
** Form generated from reading UI file 'Test_map_widget.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TEST_MAP_WIDGET_H
#define UI_TEST_MAP_WIDGET_H

#include <MapGraphicsView.h>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Test_map_widget
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout;
    Esri::ArcGISRuntime::MapGraphicsView *mapView;
    QWidget *sidePanel;
    QVBoxLayout *sideLayout;
    QLabel *coordinatesLabel;
    QLineEdit *coordinateInput;
    QPushButton *goToCoordinateButton;
    QSpacerItem *sideSpacer;
    QPushButton *exitButton;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *Test_map_widget)
    {
        if (Test_map_widget->objectName().isEmpty())
            Test_map_widget->setObjectName("Test_map_widget");
        Test_map_widget->resize(800, 600);
        centralwidget = new QWidget(Test_map_widget);
        centralwidget->setObjectName("centralwidget");
        horizontalLayout = new QHBoxLayout(centralwidget);
        horizontalLayout->setObjectName("horizontalLayout");
        mapView = new Esri::ArcGISRuntime::MapGraphicsView(centralwidget);
        mapView->setObjectName("mapView");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(1);
        sizePolicy.setHeightForWidth(mapView->sizePolicy().hasHeightForWidth());
        mapView->setSizePolicy(sizePolicy);

        horizontalLayout->addWidget(mapView);

        sidePanel = new QWidget(centralwidget);
        sidePanel->setObjectName("sidePanel");
        sideLayout = new QVBoxLayout(sidePanel);
        sideLayout->setObjectName("sideLayout");
        coordinatesLabel = new QLabel(sidePanel);
        coordinatesLabel->setObjectName("coordinatesLabel");

        sideLayout->addWidget(coordinatesLabel);

        coordinateInput = new QLineEdit(sidePanel);
        coordinateInput->setObjectName("coordinateInput");

        sideLayout->addWidget(coordinateInput);

        goToCoordinateButton = new QPushButton(sidePanel);
        goToCoordinateButton->setObjectName("goToCoordinateButton");

        sideLayout->addWidget(goToCoordinateButton);

        sideSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        sideLayout->addItem(sideSpacer);

        exitButton = new QPushButton(sidePanel);
        exitButton->setObjectName("exitButton");
        exitButton->setMinimumSize(QSize(120, 0));

        sideLayout->addWidget(exitButton);


        horizontalLayout->addWidget(sidePanel);

        Test_map_widget->setCentralWidget(centralwidget);
        menubar = new QMenuBar(Test_map_widget);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 22));
        Test_map_widget->setMenuBar(menubar);
        statusbar = new QStatusBar(Test_map_widget);
        statusbar->setObjectName("statusbar");
        Test_map_widget->setStatusBar(statusbar);

        retranslateUi(Test_map_widget);

        QMetaObject::connectSlotsByName(Test_map_widget);
    } // setupUi

    void retranslateUi(QMainWindow *Test_map_widget)
    {
        Test_map_widget->setWindowTitle(QCoreApplication::translate("Test_map_widget", "Test Map Widget", nullptr));
        coordinatesLabel->setText(QCoreApplication::translate("Test_map_widget", "Coordinates (lat, lon)", nullptr));
        coordinateInput->setPlaceholderText(QCoreApplication::translate("Test_map_widget", "34.056, -117.195", nullptr));
        goToCoordinateButton->setText(QCoreApplication::translate("Test_map_widget", "Go to location", nullptr));
        exitButton->setText(QCoreApplication::translate("Test_map_widget", "Exit Map", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Test_map_widget: public Ui_Test_map_widget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TEST_MAP_WIDGET_H
