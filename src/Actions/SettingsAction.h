#pragma once

#include <actions/WidgetAction.h>
#include <actions/DecimalAction.h>
#include <actions/IntegralAction.h>
#include <actions/VariantAction.h>
#include <actions/DatasetPickerAction.h>
#include <PointData/DimensionPickerAction.h>
#include <PointData/DimensionsPickerAction.h>
#include <actions/ToggleAction.h>

using namespace mv::gui;

class ExampleViewJSPlugin;
/**
 * Settings action class
 *
 * Action class for configuring settings
 */
class SettingsAction : public WidgetAction
{
public:
    /**
     * Construct with \p parent object and \p title
     * @param parent Pointer to parent object
     * @param title Title
     */
    Q_INVOKABLE SettingsAction(QObject* parent, const QString& title);

public: // Action getters
    
    DimensionPickerAction& getXDimensionPickerAction() { return _xDimensionPickerAction; }
    DimensionPickerAction& getYDimensionPickerAction() { return _yDimensionPickerAction; }
    DimensionPickerAction& getDimensionAction() { return _dimensionAction; }
    DecimalAction& getPointSizeAction() { return _pointSizeAction; }
    DecimalAction& getPointOpacityAction() { return _pointOpacityAction; }
    IntegralAction& getNumClusterAction() { return _numClusterAction; }
    DecimalAction& getCorrThresholdAction() { return _corrThresholdAction; }
    VariantAction& getFloodFillAction() { return _floodFillAction; }
    IntegralAction& getSliceAction() { return _sliceAction; }
    DatasetPickerAction& getDatasetPickerAction() { return _datasetPickerAction; }
    ToggleAction& getCorrSpatialAction() { return _corrSpatialAction; }
    ToggleAction& getSingleCellAction() { return _singleCellAction; }

protected:

    class Widget : public WidgetActionWidget {
    public:
        Widget(QWidget* parent, SettingsAction* settingsAction);
    };

    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override {
        return new SettingsAction::Widget(parent, this);
    };

private:
    ExampleViewJSPlugin*    _exampleViewJSPlugin;       /** Pointer to Example OpenGL Viewer Plugin */
    DimensionPickerAction   _xDimensionPickerAction;    /** X-dimension picker action */
    DimensionPickerAction   _yDimensionPickerAction;    /** Y-dimension picker action */
    DimensionPickerAction   _dimensionAction;           /** dimension picker action */
    DecimalAction           _pointSizeAction;           /** point size action */
    DecimalAction           _pointOpacityAction;        /** point opacity action */
    IntegralAction           _numClusterAction;        /** number of cluster action */
    DecimalAction           _corrThresholdAction;        /** correlation threshold action */
    VariantAction           _floodFillAction;           /** flood fill action */
    IntegralAction           _sliceAction;           /** slice action */
    DatasetPickerAction     _datasetPickerAction;       /** dataset picker action */
    ToggleAction            _corrSpatialAction;          /** correlation option action */
    ToggleAction            _singleCellAction;          /** single cell option action */
};
