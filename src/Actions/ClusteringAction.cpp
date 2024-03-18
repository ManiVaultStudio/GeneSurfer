#include "ClusteringAction.h"
#include "src/ExampleViewJSPlugin.h"

using namespace mv::gui;

ClusteringAction::ClusteringAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _numClusterAction(this, "numClusters", 1, 6, 3),
    //_corrThresholdAction(this, "CorrThreshold", 0.f, 1.0f, 0.15f, 2)
    _numGenesThresholdAction(this, "numFilteredGenes", 1, 100, 50)
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("image"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_numClusterAction);
    //addAction(&_corrThresholdAction);
    addAction(&_numGenesThresholdAction);

    _numClusterAction.setToolTip("Number of clusters");
    //_corrThresholdAction.setToolTip("Correlation Threshold");
    _numGenesThresholdAction.setToolTip("Number of filtered genes");


    auto exampleViewJSPlugin = dynamic_cast<ExampleViewJSPlugin*>(parent->parent());
    if (exampleViewJSPlugin == nullptr)
        return;

    connect(&_numClusterAction, &IntegralAction::valueChanged, [this, exampleViewJSPlugin](int32_t val) {
            exampleViewJSPlugin->updateNumCluster();
     });

   /* connect(&_corrThresholdAction, &DecimalAction::valueChanged, [this, exampleViewJSPlugin](float val) {
         exampleViewJSPlugin->updateCorrThreshold();
    });*/

    connect(&_numGenesThresholdAction, &IntegralAction::valueChanged, [this, exampleViewJSPlugin](float val) {
        exampleViewJSPlugin->updateCorrThreshold();
        });

    connect(&exampleViewJSPlugin->getPositionSourceDataset(), &Dataset<Points>::changed, this, [this, exampleViewJSPlugin]() {
        _numGenesThresholdAction.setMaximum(exampleViewJSPlugin->getPositionSourceDataset()->getDimensionsPickerAction().getEnabledDimensions().size());
        });

}

void ClusteringAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    _numClusterAction.fromParentVariantMap(variantMap);
    //_corrThresholdAction.fromParentVariantMap(variantMap);
    _numGenesThresholdAction.fromParentVariantMap(variantMap);
}

QVariantMap ClusteringAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    _numClusterAction.insertIntoVariantMap(variantMap);
    //_corrThresholdAction.insertIntoVariantMap(variantMap);
    _numGenesThresholdAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
