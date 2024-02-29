#include "SettingsAction.h"

#include "src/ExampleViewJSPlugin.h"

#include <QHBoxLayout>

using namespace mv::gui;

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _exampleViewJSPlugin(dynamic_cast<ExampleViewJSPlugin*>(parent)),
    _positionAction(this, "Position"),

    _dimensionAction(this, "Dim"),

    _pointPlotAction(this, "Point Plot"),

    _singleCellModeAction(this, "Single Cell Mode"),

    _clusteringAction(this, "Cluster Settings"),

    _floodFillAction(this, "Flood Fill"),
    _sliceAction(this, "Slice"),
    _correlationModeAction(this, "Correlation Mode")
{
    setText("Settings");
    
    setShowLabels(true);
    setLabelSizingType(LabelSizingType::Auto);

    _singleCellModeAction.initialize(_exampleViewJSPlugin);

    addAction(&_dimensionAction);

    _dimensionAction.setToolTip("Dimension");

    _floodFillAction.setToolTip("Flood Fill");

    _correlationModeAction.setToolTip("Correlation Mode");
    _positionAction.setToolTip("Position Dimension");
    _pointPlotAction.setToolTip("Point Plot");
    _clusteringAction.setToolTip("Cluster Settings");

    if (_exampleViewJSPlugin == nullptr)
        return;

    connect(&_floodFillAction, &VariantAction::variantChanged, _exampleViewJSPlugin, &ExampleViewJSPlugin::updateFloodFill);
    connect(&_sliceAction, &IntegralAction::valueChanged, _exampleViewJSPlugin, &ExampleViewJSPlugin::updateSlice);

    connect(&_dimensionAction, &DimensionPickerAction::currentDimensionIndexChanged, [this](const std::uint32_t& currentDimensionIndex) {
        if (_exampleViewJSPlugin->isDataInitialized()) {
            _exampleViewJSPlugin->updateShowDimension();
        }
        });

    connect(&_exampleViewJSPlugin->getPositionSourceDataset(), &Dataset<Points>::changed, this, [this]() {
        _dimensionAction.setPointsDataset(_exampleViewJSPlugin->getPositionSourceDataset());
        //_dimensionAction.setCurrentDimensionIndex(0);
        });

}