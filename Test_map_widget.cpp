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
#include <QPushButton>

// C++ API headers
#include "Map.h"
#include "MapGraphicsView.h"
#include "MapTypes.h"

#include "ui_Test_map_widget.h"

using namespace Esri::ArcGISRuntime;

Test_map_widget::Test_map_widget(QWidget *parent /*=nullptr*/)
    : QMainWindow(parent)
    , m_ui(std::make_unique<Ui::Test_map_widget>())
{
    m_ui->setupUi(this);

    // Create a map using the ArcGISImagery BasemapStyle
    m_map = new Map(BasemapStyle::ArcGISImagery, this);

    // Create the map view widget
    m_mapView = m_ui->mapView;

    // Set map to map view
    m_mapView->setMap(m_map);

    // Connect the exit button created in the UI to close the window
    connect(m_ui->exitButton, &QPushButton::clicked, this, &QWidget::close);
}

Test_map_widget::~Test_map_widget() = default;
