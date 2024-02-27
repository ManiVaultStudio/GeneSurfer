#include "SettingsAction.h"

#include "src/ExampleViewJSPlugin.h"

#include <QHBoxLayout>

using namespace mv::gui;

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    WidgetAction(parent, title),
    _exampleViewJSPlugin(dynamic_cast<ExampleViewJSPlugin*>(parent)),
    _xDimensionPickerAction(this, "X"),
    _yDimensionPickerAction(this, "Y"),
    _dimensionAction(this, "Dim"),
    _pointSizeAction(this, "Point Size", 1, 50, 10),
    _pointOpacityAction(this, "Opacity", 0.f, 1.f, 0.1f, 2),
    _numClusterAction(this, "numClusters", 1, 6, 3),
    _corrThresholdAction(this, "CorrT", 0.f, 1.0f, 0.15f, 2),
    _floodFillAction(this, "Flood Fill"),
    _sliceAction(this, "Slice"),
    _corrSpatialAction(this, "Spatial", false),
    _singleCellAction(this, "Single Cell", false)
{
    setText("Settings");

    _xDimensionPickerAction.setToolTip("X dimension");
    _yDimensionPickerAction.setToolTip("Y dimension");
    _dimensionAction.setToolTip("Dimension");
    _pointSizeAction.setToolTip("Size of individual points");
    _pointOpacityAction.setToolTip("Opacity of individual points");
    _numClusterAction.setToolTip("Number of clusters");
    _corrThresholdAction.setToolTip("Correlation Threshold");
    _floodFillAction.setToolTip("Flood Fill");
    _corrSpatialAction.setToolTip("Spatial or HD");
    _singleCellAction.setToolTip("Single Cell or ST");

    connect(&_floodFillAction, &VariantAction::variantChanged, _exampleViewJSPlugin, &ExampleViewJSPlugin::updateFloodFill);
    connect(&_sliceAction, &IntegralAction::valueChanged, _exampleViewJSPlugin, &ExampleViewJSPlugin::updateSlice);
    connect(&_corrSpatialAction, &ToggleAction::toggled, _exampleViewJSPlugin, &ExampleViewJSPlugin::updateCorrOption);
    connect(&_singleCellAction, &ToggleAction::toggled, _exampleViewJSPlugin, &ExampleViewJSPlugin::updateSingleCellOption);

    connect(&_dimensionAction, &DimensionPickerAction::currentDimensionIndexChanged, [this](const std::uint32_t& currentDimensionIndex) {
        if (_exampleViewJSPlugin->isDataInitialized()) {
            _exampleViewJSPlugin->updateShowDimension();
        }
        });

    connect(&_exampleViewJSPlugin->getPositionSourceDataset(), &Dataset<Points>::changed, this, [this]() {
        _dimensionAction.setPointsDataset(_exampleViewJSPlugin->getPositionSourceDataset());
        //_dimensionAction.setCurrentDimensionIndex(0);
        });

    connect(&_singleCellAction, &ToggleAction::toggled, this, [this]() {
        if (_exampleViewJSPlugin->isUsingSingleCell()) {
            qDebug() << "Using Single Cell";
            _dimensionAction.setPointsDataset(_exampleViewJSPlugin->getAvgExprDataset());
            //_dimensionAction.setCurrentDimensionIndex(0);
            
        }
        else {
            qDebug() << "Using Spatial";
            _dimensionAction.setPointsDataset(_exampleViewJSPlugin->getPositionSourceDataset());
            //_dimensionAction.setCurrentDimensionIndex(0);
            
        }
        });


    if (_exampleViewJSPlugin == nullptr)
        return;

    // TO DO: how do the following work? - start
     connect(&_xDimensionPickerAction, &DimensionPickerAction::currentDimensionIndexChanged, [this](const std::uint32_t& currentDimensionIndex) {
         if (_exampleViewJSPlugin->isDataInitialized())
             _exampleViewJSPlugin->updateSelectedDim();
     });

     connect(&_yDimensionPickerAction, &DimensionPickerAction::currentDimensionIndexChanged, [this](const std::uint32_t& currentDimensionIndex) {
         if (_exampleViewJSPlugin->isDataInitialized())
             _exampleViewJSPlugin->updateSelectedDim();
     });

     connect(&_exampleViewJSPlugin->getPositionDataset(), &Dataset<Points>::changed, this, [this]() {
         _xDimensionPickerAction.setPointsDataset(_exampleViewJSPlugin->getPositionDataset());
         _yDimensionPickerAction.setPointsDataset(_exampleViewJSPlugin->getPositionDataset());

         _xDimensionPickerAction.setCurrentDimensionIndex(0);

         const auto yIndex = _xDimensionPickerAction.getNumberOfDimensions() >= 2 ? 1 : 0;

         _yDimensionPickerAction.setCurrentDimensionIndex(yIndex);
         });

     const auto updateReadOnly = [this]() -> void {
         setEnabled(_exampleViewJSPlugin->getPositionDataset().isValid());
     };

     updateReadOnly();

     connect(&_exampleViewJSPlugin->getPositionDataset(), &Dataset<Points>::changed, this, updateReadOnly);
     // TO DO: how do the following work? - end

     connect(&_pointSizeAction, &DecimalAction::valueChanged, [this](float val) {
         _exampleViewJSPlugin->updateScatterPointSize();
     });

     connect(&_pointOpacityAction, &DecimalAction::valueChanged, [this](float val) {
         _exampleViewJSPlugin->updateScatterOpacity();
     });

     connect(&_numClusterAction, &IntegralAction::valueChanged, [this](int32_t val) {
            _exampleViewJSPlugin->updateNumCluster();
     });

     connect(&_corrThresholdAction, &DecimalAction::valueChanged, [this](float val) {
         _exampleViewJSPlugin->updateCorrThreshold();
         });

}

SettingsAction::Widget::Widget(QWidget* parent, SettingsAction* settingsAction) :
    WidgetActionWidget(parent, settingsAction)
{
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    auto layout = new QHBoxLayout();

    const int margin = 5;
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(2);

    layout->addWidget(settingsAction->getXDimensionPickerAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getXDimensionPickerAction().createWidget(this));

    layout->addWidget(settingsAction->getYDimensionPickerAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getYDimensionPickerAction().createWidget(this));

    layout->addWidget(settingsAction->getDimensionAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getDimensionAction().createWidget(this));

    layout->addWidget(settingsAction->getPointSizeAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getPointSizeAction().createWidget(this));

    layout->addWidget(settingsAction->getPointOpacityAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getPointOpacityAction().createWidget(this));

    layout->addWidget(settingsAction->getNumClusterAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getNumClusterAction().createWidget(this));

    layout->addWidget(settingsAction->getCorrThresholdAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getCorrThresholdAction().createWidget(this));

    layout->addWidget(settingsAction->getCorrSpatialAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getCorrSpatialAction().createWidget(this));

    layout->addWidget(settingsAction->getSingleCellAction().createLabelWidget(this));
    layout->addWidget(settingsAction->getSingleCellAction().createWidget(this));

    setLayout(layout);
}