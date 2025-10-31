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

// Qt headers
#include <QApplication>
#include <QMessageBox>

#include "ArcGISRuntimeEnvironment.h"

#include "Test_map_widget.h"

using namespace Esri::ArcGISRuntime;

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ArcGISRuntimeEnvironment::setUseLegacyAuthentication(false);
    // Use of ArcGIS location services, such as basemap styles, geocoding, and routing services,
    // requires an access token. For more information see
    // https://links.esri.com/arcgis-runtime-security-auth.

    // The following methods grant an access token:

    // 1. User authentication: Grants a temporary access token associated with a user's ArcGIS account.
    // To generate a token, a user logs in to the app with an ArcGIS account that is part of an
    // organization in ArcGIS Online or ArcGIS Enterprise.

    // 2. API key authentication: Get a long-lived access token that gives your application access to
    // ArcGIS location services. Go to the tutorial at https://links.esri.com/create-an-api-key.
    // Copy the API Key access token.

    const QString accessToken = QString("AAPTxy8BH1VEsoebNVZXo8HurGlhe_M6UHXSvqoW1-L9IDEgOik3UmWL-0aAt7GWTOSEbXrRR-Ksi8PIgv-p3v6iR_A9u4hvhx9FL6RmkAQY1vNLHyFdBvTv8nVVgVidU1-fE-i94_mH6fHglRx9OoJOSR4VD67Jbw0omuLqowyqfDiMaX4elLl5guFZsS6lRcDnbWjDU-k4tqgQFOjA3ObbOICM5bPm1VsJAYLYVchME-E.AT1_x2k92Iwv");

    if (accessToken.isEmpty()) {
        qWarning()
            << "Use of ArcGIS location services, such as the basemap styles service, requires"
            << "you to authenticate with an ArcGIS account or set the API Key property.";
    } else {
        ArcGISRuntimeEnvironment::setApiKey(accessToken);
    }

    // Production deployment of applications built with ArcGIS Maps SDK requires you to
    // license ArcGIS Maps SDK functionality. For more information see
    // https://links.esri.com/arcgis-runtime-license-and-deploy.

    // ArcGISRuntimeEnvironment::setLicense("Place license string in here");

    //  use this code to check for initialization errors
    //  QObject::connect(ArcGISRuntimeEnvironment::instance(), &ArcGISRuntimeEnvironment::errorOccurred, [](const Error& error){
    //    QMessageBox msgBox;
    //    msgBox.setText(error.message);
    //    msgBox.exec();
    //  });

    //  if (ArcGISRuntimeEnvironment::initialize() == false)
    //  {
    //    application.quit();
    //    return 1;
    //  }

    Test_map_widget applicationWindow;
    applicationWindow.setMinimumWidth(800);
    applicationWindow.setMinimumHeight(600);
    applicationWindow.show();

    return application.exec();
}
