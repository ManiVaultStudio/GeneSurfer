#include "SettingsAction.h"

#include "src/ExampleViewJSPlugin.h"

#include <QHBoxLayout>

using namespace mv::gui;

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _exampleViewJSPlugin(dynamic_cast<ExampleViewJSPlugin*>(parent)),

    _positionDatasetPickerAction(this, "PositionDataset"),
    _sliceDatasetPickerAction(this, "SliceDataset"),

    _avgExprDatasetPickerAction(this, "AvgExprDataset"),

    _positionAction(this, "Position"),

    _dimensionSelectionAction(this, "Dim"),

    _pointPlotAction(this, "Point Plot"),

    _singleCellModeAction(this, "Single Cell Mode"),

    _clusteringAction(this, "Cluster Settings"),

    _floodFillAction(this, "Flood Fill"),
    _sliceAction(this, "Slice"),
    _correlationModeAction(this, "Correlation Mode")
{
    setText("Settings");
    setSerializationName("SettingsAction");
    
    setShowLabels(true);
    setLabelSizingType(LabelSizingType::Auto);

    if (_exampleViewJSPlugin == nullptr)
        return;

    _singleCellModeAction.initialize(_exampleViewJSPlugin);

    _correlationModeAction.setToolTip("Correlation Mode");
    _positionAction.setToolTip("Position Dimension");
    _pointPlotAction.setToolTip("Point Plot");
    _clusteringAction.setToolTip("Cluster Settings");

    const auto updateEnabled = [this]() {
        bool hasDataset = _exampleViewJSPlugin->getPositionDataset().isValid();

        _clusteringAction.setEnabled(hasDataset);
        _positionAction.setEnabled(hasDataset);
        _correlationModeAction.setEnabled(hasDataset);
        _pointPlotAction.setEnabled(hasDataset);
        _singleCellModeAction.setEnabled(hasDataset);
        _dimensionSelectionAction.setEnabled(hasDataset);
    };

    connect(&_exampleViewJSPlugin->getPositionDataset(), &Dataset<Points>::changed, this, updateEnabled);

    updateEnabled();

    connect(&_positionDatasetPickerAction, &DatasetPickerAction::datasetPicked, [this](Dataset<DatasetImpl> pickedDataset) -> void {
        _exampleViewJSPlugin->getPositionDataset() = pickedDataset;
        });
    connect(&_exampleViewJSPlugin->getPositionDataset(), &Dataset<Points>::changed, this, [this](DatasetImpl* dataset) -> void {
        _positionDatasetPickerAction.setCurrentDataset(dataset);
        });
    connect(&_exampleViewJSPlugin->getSliceDataset(), &Dataset<Clusters>::changed, this, [this](DatasetImpl* dataset) -> void {
        _sliceDatasetPickerAction.setCurrentDataset(dataset);
        });
    connect(&_exampleViewJSPlugin->getAvgExprDataset(), &Dataset<Clusters>::changed, this, [this](DatasetImpl* dataset) -> void {
        _avgExprDatasetPickerAction.setCurrentDataset(dataset);
        });

 
    connect(&_floodFillAction, &VariantAction::variantChanged, _exampleViewJSPlugin, &ExampleViewJSPlugin::updateFloodFill);
    connect(&_sliceAction, &IntegralAction::valueChanged, _exampleViewJSPlugin, &ExampleViewJSPlugin::updateSlice);

}

void SettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);

    _positionDatasetPickerAction.fromParentVariantMap(variantMap);
    _sliceDatasetPickerAction.fromParentVariantMap(variantMap);
    _avgExprDatasetPickerAction.fromParentVariantMap(variantMap);

    // Load position dataset
    auto positionDataset = _positionDatasetPickerAction.getCurrentDataset();
    if (positionDataset.isValid())
    {
        qDebug() << ">>>>> Found position dataset " << positionDataset->getGuiName();
        Dataset pickedDataset = mv::data().getDataset(positionDataset.getDatasetId());
        _exampleViewJSPlugin->getPositionDataset() = pickedDataset;
    }

    // Load slice dataset
    auto sliceDataset = _sliceDatasetPickerAction.getCurrentDataset();
    if (sliceDataset.isValid())
    {
        qDebug() << ">>>>> Found a slice dataset " << sliceDataset->getGuiName();
        Dataset pickedDataset = mv::data().getDataset(sliceDataset.getDatasetId());
        _exampleViewJSPlugin->getSliceDataset() = pickedDataset;
    }

    // Load average expression dataset
    auto avgExprDataset = _avgExprDatasetPickerAction.getCurrentDataset();
    if (avgExprDataset.isValid())
    {
        qDebug() << ">>>>> Found Avg Expression dataset " << avgExprDataset->getGuiName();
        Dataset pickedDataset = mv::data().getDataset(avgExprDataset.getDatasetId());
        _exampleViewJSPlugin->getAvgExprDataset() = pickedDataset;
    }
    qDebug() << ">>>>> SettingsAction::fromVariantMap 1";
    _floodFillAction.fromParentVariantMap(variantMap);
    qDebug() << ">>>>> SettingsAction::fromVariantMap 2";
    //_correlationModeAction.fromParentVariantMap(variantMap);
    _singleCellModeAction.fromParentVariantMap(variantMap);
    qDebug() << ">>>>> SettingsAction::fromVariantMap 3";
    _clusteringAction.fromParentVariantMap(variantMap);
    qDebug() << ">>>>> SettingsAction::fromVariantMap 4";
    _positionAction.fromParentVariantMap(variantMap);  
    _pointPlotAction.fromParentVariantMap(variantMap); 
    _sliceAction.fromParentVariantMap(variantMap);
    //_dimensionSelectionAction.fromParentVariantMap(variantMap);
    
}

QVariantMap SettingsAction::toVariantMap() const
{
    auto variantMap = WidgetAction::toVariantMap();

    _positionDatasetPickerAction.insertIntoVariantMap(variantMap);
    _sliceDatasetPickerAction.insertIntoVariantMap(variantMap);

    _avgExprDatasetPickerAction.insertIntoVariantMap(variantMap);

    _positionAction.insertIntoVariantMap(variantMap);
    _pointPlotAction.insertIntoVariantMap(variantMap);
    _singleCellModeAction.insertIntoVariantMap(variantMap);
    _clusteringAction.insertIntoVariantMap(variantMap);
    _floodFillAction.insertIntoVariantMap(variantMap);
    _sliceAction.insertIntoVariantMap(variantMap);
    //_correlationModeAction.insertIntoVariantMap(variantMap);
    //_dimensionSelectionAction.insertIntoVariantMap(variantMap); // TO DO: not needed, done in plugin.cpp

    return variantMap;
}

