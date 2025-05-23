#include "PointPlotAction.h"
#include "src/GeneSurferPlugin.h"

using namespace mv::gui;

PointPlotAction::PointPlotAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _pointSizeAction(this, "Point Size", 1, 50, 10),
    _pointOpacityAction(this, "Opacity", 0.f, 1.f, 0.1f, 2)
{
    setIcon(mv::util::StyledIcon("paint-brush"));
    setToolTip("Point plot settings");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_pointSizeAction);
    addAction(&_pointOpacityAction);

    _pointSizeAction.setToolTip("Size of individual points");
    _pointOpacityAction.setToolTip("Opacity of individual points");


    auto geneSurferPlugin = dynamic_cast<GeneSurferPlugin*>(parent->parent());
    if (geneSurferPlugin == nullptr)
        return;

 connect(&_pointSizeAction, &DecimalAction::valueChanged, [this, geneSurferPlugin](float val) {
         geneSurferPlugin->updateScatterPointSize();
     });

     connect(&_pointOpacityAction, &DecimalAction::valueChanged, [this, geneSurferPlugin](float val) {
         geneSurferPlugin->updateScatterOpacity();
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

void PointPlotAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    _pointSizeAction.fromParentVariantMap(variantMap);
    _pointOpacityAction.fromParentVariantMap(variantMap);
}

QVariantMap PointPlotAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    _pointSizeAction.insertIntoVariantMap(variantMap);
    _pointOpacityAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
