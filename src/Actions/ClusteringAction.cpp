#include "ClusteringAction.h"
#include "src/ExampleViewJSPlugin.h"

using namespace mv::gui;

ClusteringAction::ClusteringAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _numClusterAction(this, "numClusters", 1, 6, 3),
    _corrThresholdAction(this, "CorrThreshold", 0.f, 1.0f, 0.15f, 2)
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("image"));
    //setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceExpandedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_numClusterAction);
    addAction(&_corrThresholdAction);

    _numClusterAction.setToolTip("Number of clusters");
    _corrThresholdAction.setToolTip("Correlation Threshold");


    auto exampleViewJSPlugin = dynamic_cast<ExampleViewJSPlugin*>(parent->parent());
    if (exampleViewJSPlugin == nullptr)
        return;

    connect(&_numClusterAction, &IntegralAction::valueChanged, [this, exampleViewJSPlugin](int32_t val) {
            exampleViewJSPlugin->updateNumCluster();
     });

    connect(&_corrThresholdAction, &DecimalAction::valueChanged, [this, exampleViewJSPlugin](float val) {
         exampleViewJSPlugin->updateCorrThreshold();
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

void ClusteringAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    _numClusterAction.fromParentVariantMap(variantMap);
    _corrThresholdAction.fromParentVariantMap(variantMap);
}

QVariantMap ClusteringAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    _numClusterAction.insertIntoVariantMap(variantMap);
    _corrThresholdAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
