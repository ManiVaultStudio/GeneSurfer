#include "SettingsAction.h"

#include "src/GeneSurferPlugin.h"

#include <QHBoxLayout>

using namespace mv::gui;

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _geneSurferPlugin(dynamic_cast<GeneSurferPlugin*>(parent)),
    _positionDatasetPickerAction(this, "PositionDataset"),
    _sliceDatasetPickerAction(this, "SliceDataset"),
    _avgExprDatasetPickerAction(this, "AvgExprDataset"),
    _positionAction(this, "Position"),
    _dimensionSelectionAction(this, "Gene searching"),
    _pointPlotAction(this, "Point Plot"),
    _singleCellModeAction(this, "Single Cell Mode"),
    _clusteringAction(this, "Cluster Settings"),
    _sectionAction(this, "Section selection"),
    _correlationModeAction(this, "Gene filtering"),
    _enrichmentAction(this, "Enrichment settings")
{
    setText("Settings");
    setSerializationName("SettingsAction");
    
    setShowLabels(true);
    setLabelSizingType(LabelSizingType::Auto);

    if (_geneSurferPlugin == nullptr)
        return;

    _singleCellModeAction.initialize(_geneSurferPlugin);

    const auto updateEnabled = [this]() {
        bool hasDataset = _geneSurferPlugin->getPositionDataset().isValid();

        _clusteringAction.setEnabled(hasDataset);
        _positionAction.setEnabled(hasDataset);
        _correlationModeAction.setEnabled(hasDataset);
        _pointPlotAction.setEnabled(hasDataset);
        _singleCellModeAction.setEnabled(hasDataset);
        _dimensionSelectionAction.setEnabled(hasDataset);
        _enrichmentAction.setEnabled(hasDataset);
    };

    connect(&_geneSurferPlugin->getPositionDataset(), &Dataset<Points>::changed, this, updateEnabled);

    updateEnabled();

    connect(&_positionDatasetPickerAction, &DatasetPickerAction::datasetPicked, [this](Dataset<DatasetImpl> pickedDataset) -> void {
        _geneSurferPlugin->getPositionDataset() = pickedDataset;
        });
    connect(&_geneSurferPlugin->getPositionDataset(), &Dataset<Points>::changed, this, [this](DatasetImpl* dataset) -> void {
        _positionDatasetPickerAction.setCurrentDataset(dataset);
        });
    connect(&_geneSurferPlugin->getSliceDataset(), &Dataset<Clusters>::changed, this, [this](DatasetImpl* dataset) -> void {
        _sliceDatasetPickerAction.setCurrentDataset(dataset);
        });
    connect(&_geneSurferPlugin->getAvgExprDataset(), &Dataset<Clusters>::changed, this, [this](DatasetImpl* dataset) -> void {
        _avgExprDatasetPickerAction.setCurrentDataset(dataset);
        });
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
        _geneSurferPlugin->getPositionDataset() = pickedDataset;
    }

    // Load slice dataset
    auto sliceDataset = _sliceDatasetPickerAction.getCurrentDataset();
    if (sliceDataset.isValid())
    {
        qDebug() << ">>>>> Found slice dataset " << sliceDataset->getGuiName();
        Dataset pickedDataset = mv::data().getDataset(sliceDataset.getDatasetId());
        _geneSurferPlugin->getSliceDataset() = pickedDataset;
    }

    // Load average expression dataset
    auto avgExprDataset = _avgExprDatasetPickerAction.getCurrentDataset();
    if (avgExprDataset.isValid())
    {
        qDebug() << ">>>>> Found Avg Expression dataset " << avgExprDataset->getGuiName();
        Dataset pickedDataset = mv::data().getDataset(avgExprDataset.getDatasetId());
        _geneSurferPlugin->getAvgExprDataset() = pickedDataset;
    }
    //qDebug() << "SettingsAction::fromVariantMap 1";
    _correlationModeAction.fromParentVariantMap(variantMap);
    //qDebug() << "SettingsAction::fromVariantMap 2";
    _singleCellModeAction.fromParentVariantMap(variantMap);
    //qDebug() << "SettingsAction::fromVariantMap 3";
    _clusteringAction.fromParentVariantMap(variantMap);
    //qDebug() << "SettingsAction::fromVariantMap 4";
    _enrichmentAction.fromParentVariantMap(variantMap);
    //qDebug() << "SettingsAction::fromVariantMap 5";
    _positionAction.fromParentVariantMap(variantMap);  
    _pointPlotAction.fromParentVariantMap(variantMap); 
    _sectionAction.fromParentVariantMap(variantMap);
    
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
    _sectionAction.insertIntoVariantMap(variantMap);
    _correlationModeAction.insertIntoVariantMap(variantMap);
    _enrichmentAction.insertIntoVariantMap(variantMap);

    return variantMap;
}

