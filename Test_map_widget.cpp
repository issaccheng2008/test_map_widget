// Copyright 2025 ESRI
//
// All rights reserved under the copyright laws of the United States
// and applicable international laws, treaties, and conventions.
//
// You may freely redistribute and use this sample code, with or
// without modification, provided you include the original copyright
// notice and use restrictions.
//
// See the Sample code usage restrictions document for further information.
//

// Other headers
#include "Test_map_widget.h"

// Qt headers
#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>

// C++ API headers
#include "Map.h"
#include "MapGraphicsView.h"
#include "MapTypes.h"

using namespace Esri::ArcGISRuntime;

Test_map_widget::Test_map_widget(QWidget *parent /*=nullptr*/)
    : QMainWindow(parent)
{
    // Create a map using the ArcGISImagery BasemapStyle
    m_map = new Map(BasemapStyle::ArcGISImagery, this);

    // Create the map view widget
    m_mapView = new MapGraphicsView(this);

    // Set map to map view
    m_mapView->setMap(m_map);

    // Create an exit button allowing the user to close the map window
    m_exitButton = new QPushButton(tr("Exit Map"), this);
    connect(m_exitButton, &QPushButton::clicked, this, &QWidget::close);

    // Arrange the map view and button side by side
    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_mapView, /*stretch*/ 1);
    layout->addWidget(m_exitButton);

    // Create a central widget to host the layout
    m_centralWidget = new QWidget(this);
    m_centralWidget->setLayout(layout);

    // Set the composed widget as the main window's central widget
    setCentralWidget(m_centralWidget);
}

Test_map_widget::~Test_map_widget() = default;
