
#include "CorrelationModeAction.h"
#include "src/GeneSurferPlugin.h"


using namespace mv::gui;

CorrelationModeAction::CorrelationModeAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _geneSurferPlugin(dynamic_cast<GeneSurferPlugin*>(parent->parent())),
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

    if (_geneSurferPlugin == nullptr)
        return;

    corrFilter::CorrFilter& corrFilter = _geneSurferPlugin->getCorrFilter();

    connect(&_spatialCorrelationAction, &TriggerAction::triggered, [this, &corrFilter]() {
        corrFilter.setFilterType(corrFilter::CorrFilterType::SPATIAL);
        _geneSurferPlugin->updateFilterLabel();
        _geneSurferPlugin->updateSelection();
        });

    connect(&_hdCorrelationAction, &TriggerAction::triggered, [this, &corrFilter]() {
        corrFilter.setFilterType(corrFilter::CorrFilterType::HD);
        _geneSurferPlugin->updateFilterLabel();
        _geneSurferPlugin->updateSelection();
        });

    connect(&_diffAction, &TriggerAction::triggered, [this, &corrFilter]() {
        corrFilter.setFilterType(corrFilter::CorrFilterType::DIFF);
        _geneSurferPlugin->updateFilterLabel();
        _geneSurferPlugin->updateSelection();
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

    corrFilter::CorrFilter& corrFilter = _geneSurferPlugin->getCorrFilter();

    if (variantMap["FilterMode"] == "Spatial")
        corrFilter.setFilterType(corrFilter::CorrFilterType::SPATIAL);
    else if (variantMap["FilterMode"] == "HD")
        corrFilter.setFilterType(corrFilter::CorrFilterType::HD);
    else if (variantMap["FilterMode"] == "Diff")
        corrFilter.setFilterType(corrFilter::CorrFilterType::DIFF);
    _geneSurferPlugin->updateFilterLabel();
}

QVariantMap CorrelationModeAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    corrFilter::CorrFilter& corrFilter = _geneSurferPlugin->getCorrFilter();

    variantMap.insert("FilterMode", corrFilter.getCorrFilterTypeAsString());

    return variantMap;
}
