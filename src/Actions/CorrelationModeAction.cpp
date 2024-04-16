#include "CorrelationModeAction.h"
#include "src/GeneSurferPlugin.h"

using namespace mv::gui;

CorrelationModeAction::CorrelationModeAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _spatialCorrelationAction(this, "Fliter by Spatial Correlation"),
    _hdCorrelationAction(this, "Filter by HD Correlation"),
    _diffAction(this, "Filter by diff")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("bullseye"));
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_spatialCorrelationAction);
    addAction(&_hdCorrelationAction);
    addAction(&_diffAction);

    _spatialCorrelationAction.setToolTip("Spatial correlation mode");
    _hdCorrelationAction.setToolTip("HD correlation mode");
    _diffAction.setToolTip("Diff mode");

    auto geneSurferPlugin = dynamic_cast<GeneSurferPlugin*>(parent->parent());
    if (geneSurferPlugin == nullptr)
        return;

    corrFilter::CorrFilter& filter = geneSurferPlugin->getCorrFilter();

   /* connect(&_spatialCorrelationAction, &TriggerAction::triggered, this, [scatterplotPlugin]() {
            scatterplotPlugin->setCorrelationMode(true);
        });

    connect(&_hdCorrelationAction, &TriggerAction::triggered, this, [scatterplotPlugin]() {
        scatterplotPlugin->setCorrelationMode(false);
        });*/

    connect(&_spatialCorrelationAction, &TriggerAction::triggered, this, [geneSurferPlugin, &filter]() {
        filter.setFilterType(corrFilter::CorrFilterType::SPATIAL);
        geneSurferPlugin->updateFilterLabel();
        geneSurferPlugin->updateSelection();
        });

    connect(&_hdCorrelationAction, &TriggerAction::triggered, this, [geneSurferPlugin, &filter]() {
        filter.setFilterType(corrFilter::CorrFilterType::HD);
        geneSurferPlugin->updateFilterLabel();
        geneSurferPlugin->updateSelection();
        });

    connect(&_diffAction, &TriggerAction::triggered, this, [geneSurferPlugin, &filter]() {
        filter.setFilterType(corrFilter::CorrFilterType::DIFF);
        geneSurferPlugin->updateFilterLabel();
        geneSurferPlugin->updateSelection();
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
