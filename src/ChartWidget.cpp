#include "ChartWidget.h"
#include "GeneSurferPlugin.h"

#include <QDebug>
#include <QString>

using namespace mv;
using namespace mv::gui;

// =============================================================================
// ChartCommObject
// =============================================================================

ChartCommObject::ChartCommObject() :
    _selectedIDsFromJS()
{
}

void ChartCommObject::js_qt_passSelectionToQt(const QString& data){
    _selectedIDsFromJS.clear();

    if (!data.isEmpty())
    {
        qDebug() << "ChartCommObject::js_qt_passSelectionToQt: Selected item:" << data;
    }    
    
    // Notify ManiVault core and thereby other plugins about new selection
    emit passSelectionToCore(data);
}



// =============================================================================
// ChartWidget
// =============================================================================

ChartWidget::ChartWidget(GeneSurferPlugin* geneSurferPlugin):
    _geneSurferPlugin(geneSurferPlugin),
    _comObject()
{
    // For more info on drag&drop behavior, see the ExampleViewPlugin project
    setAcceptDrops(true);

    // Ensure linking to the resources defined in res/chart.qrc
    Q_INIT_RESOURCE(genesurfer_resources);

    // ManiVault and Qt create a "QtBridge" object on the js side which represents _comObject
    // there, we can connect the signals qt_js_* and call the slots js_qt_* from our communication object
    init(&_comObject);

    layout()->setContentsMargins(0, 0, 0, 0);

    connect(this, &mv::gui::WebWidget::webPageFullyLoaded, this, &ChartWidget::onWebPageFullyLoaded);
}

void ChartWidget::initWebPage()
{
    qDebug() << "ChartWidget::initWebPage: WebChannel bridge is available.";
    // This call ensures data chart setup when this view plugin is opened via the context menu of a data set
    //_geneSurferPlugin->convertDataAndUpdateChart();
}

void ChartWidget::onWebPageFullyLoaded()
{
    qDebug() << "ChartWidget::onWebPageLoaded: Web page loaded.";
    _geneSurferPlugin->convertDataAndUpdateChart();
}


