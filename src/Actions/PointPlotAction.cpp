#include "PointPlotAction.h"
#include "src/ExampleViewJSPlugin.h"

using namespace mv::gui;

PointPlotAction::PointPlotAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _pointSizeAction(this, "Point Size", 1, 50, 10),
    _pointOpacityAction(this, "Opacity", 0.f, 1.f, 0.1f, 2)
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("paint-brush"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_pointSizeAction);
    addAction(&_pointOpacityAction);

    _pointSizeAction.setToolTip("Size of individual points");
    _pointOpacityAction.setToolTip("Opacity of individual points");


    auto exampleViewJSPlugin = dynamic_cast<ExampleViewJSPlugin*>(parent->parent());
    if (exampleViewJSPlugin == nullptr)
        return;

 connect(&_pointSizeAction, &DecimalAction::valueChanged, [this, exampleViewJSPlugin](float val) {
         exampleViewJSPlugin->updateScatterPointSize();
     });

     connect(&_pointOpacityAction, &DecimalAction::valueChanged, [this, exampleViewJSPlugin](float val) {
         exampleViewJSPlugin->updateScatterOpacity();
     });

}



// void CorrelationModeAction::connectToPublicAction(WidgetAction* publicAction, bool recursive)
// {
    // auto publicCorrelationModeAction = dynamic_cast<CorrelationModeAction*>(publicAction);

    // Q_ASSERT(publicCorrelationModeAction != nullptr);

    // if (publicCorrelationModeAction == nullptr)
        // return;

    // if (recursive) {
        // actions().connectPrivateActionToPublicAction(&_scatterPlotAction, &publicCorrelationModeAction->getScatterPlotAction(), recursive);
        // actions().connectPrivateActionToPublicAction(&_densityPlotAction, &publicCorrelationModeAction->getDensityPlotAction(), recursive);
        // actions().connectPrivateActionToPublicAction(&_contourPlotAction, &publicCorrelationModeAction->getContourPlotAction(), recursive);
    // }

    // OptionAction::connectToPublicAction(publicAction, recursive);
// }

// void CorrelationModeAction::disconnectFromPublicAction(bool recursive)
// {
    // if (!isConnected())
        // return;

    // if (recursive) {
        // actions().disconnectPrivateActionFromPublicAction(&_scatterPlotAction, recursive);
        // actions().disconnectPrivateActionFromPublicAction(&_densityPlotAction, recursive);
        // actions().disconnectPrivateActionFromPublicAction(&_contourPlotAction, recursive);
    // }

    // OptionAction::disconnectFromPublicAction(recursive);
// }

//void CorrelationModeAction::fromVariantMap(const QVariantMap& variantMap)
//{
//    OptionAction::fromVariantMap(variantMap);
//
//    _spatialCorrelationAction.fromParentVariantMap(variantMap);
//    _hdCorrelationAction.fromParentVariantMap(variantMap);
//}
//
//QVariantMap CorrelationModeAction::toVariantMap() const
//{
//    auto variantMap = OptionAction::toVariantMap();
//
//    _spatialCorrelationAction.insertIntoVariantMap(variantMap);
//    _hdCorrelationAction.insertIntoVariantMap(variantMap);
//
//    return variantMap;
//}
