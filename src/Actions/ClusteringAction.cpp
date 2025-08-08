#include "ClusteringAction.h"
#include "src/GeneSurferPlugin.h"

using namespace mv::gui;

ClusteringAction::ClusteringAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _numClusterAction(this, "numClusters", 1, 6, 3),
    _numGenesThresholdAction(this, "numFilteredGenes", 1, 100, 50)
{
    setToolTip("Clustering settings");
    setIcon(mv::util::StyledIcon("th-large"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_numClusterAction);
    addAction(&_numGenesThresholdAction);

    _numClusterAction.setToolTip("Number of clusters");
    _numGenesThresholdAction.setToolTip("Number of filtered genes");

    // hide the slider, only show the spinbox
    _numClusterAction.setDefaultWidgetFlags(IntegralAction::SpinBox);
    _numGenesThresholdAction.setDefaultWidgetFlags(IntegralAction::SpinBox);


    auto geneSurferPlugin = dynamic_cast<GeneSurferPlugin*>(parent->parent());
    if (geneSurferPlugin == nullptr)
        return;

    connect(&_numClusterAction, &IntegralAction::valueChanged, [this, geneSurferPlugin](int32_t val) {
            geneSurferPlugin->updateNumCluster();
     });


    // Debounce timer for the number of genes threshold action
    _NRangeDebounceTimer.setSingleShot(true);

    connect(&_numGenesThresholdAction,
        &IntegralAction::valueChanged,
        this,
        [this]() { _NRangeDebounceTimer.start(300); });

    connect(&_NRangeDebounceTimer, &QTimer::timeout, this, [this, geneSurferPlugin]() {
        geneSurferPlugin->updateCorrThreshold(); 
        });

    //connect(&_numGenesThresholdAction, &IntegralAction::valueChanged, [this, geneSurferPlugin](float val) {
    //    geneSurferPlugin->updateCorrThreshold();
    //    });

    connect(&geneSurferPlugin->getPositionSourceDataset(), &Dataset<Points>::changed, this, [this, geneSurferPlugin]() {
        _numGenesThresholdAction.setMaximum(geneSurferPlugin->getPositionSourceDataset()->getDimensionsPickerAction().getEnabledDimensions().size());
        });

}

void ClusteringAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    _numClusterAction.fromParentVariantMap(variantMap);
    _numGenesThresholdAction.fromParentVariantMap(variantMap);
}

QVariantMap ClusteringAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    _numClusterAction.insertIntoVariantMap(variantMap);
    _numGenesThresholdAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
