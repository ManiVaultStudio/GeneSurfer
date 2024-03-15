#include "CorrelationModeAction.h"
#include "src/ExampleViewJSPlugin.h"

using namespace mv::gui;

CorrelationModeAction::CorrelationModeAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _spatialCorrelationAction(this, "Fliter by Spatial Correlation"),
    _hdCorrelationAction(this, "Filter by HD Correlation")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("bullseye"));
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_spatialCorrelationAction);
    addAction(&_hdCorrelationAction);

    _spatialCorrelationAction.setToolTip("Spatial correlation mode");
    _hdCorrelationAction.setToolTip("HD correlation mode");

    auto scatterplotPlugin = dynamic_cast<ExampleViewJSPlugin*>(parent->parent());
    if (scatterplotPlugin == nullptr)
        return;

    connect(&_spatialCorrelationAction, &TriggerAction::triggered, this, [this, scatterplotPlugin]() {
            scatterplotPlugin->setCorrelationMode(true);
        });

    connect(&_hdCorrelationAction, &TriggerAction::triggered, this, [this, scatterplotPlugin]() {
            scatterplotPlugin->setCorrelationMode(false);
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

void CorrelationModeAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    _spatialCorrelationAction.fromParentVariantMap(variantMap);
    _hdCorrelationAction.fromParentVariantMap(variantMap);
}

QVariantMap CorrelationModeAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    _spatialCorrelationAction.insertIntoVariantMap(variantMap);
    _hdCorrelationAction.insertIntoVariantMap(variantMap);

    return variantMap;
}