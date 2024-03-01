#pragma once

#include <actions/GroupAction.h>
#include <actions/WidgetAction.h>
#include <actions/IntegralAction.h>
#include <actions/VariantAction.h>
#include <actions/DatasetPickerAction.h>
#include <PointData/DimensionPickerAction.h>
#include <PointData/DimensionsPickerAction.h>

#include "CorrelationModeAction.h"
#include "PositionAction.h"
#include "PointPlotAction.h"
#include "SingleCellModeAction.h"
#include "ClusteringAction.h"

using namespace mv::gui;

class ExampleViewJSPlugin;
/**
 * Settings action class
 *
 * Action class for configuring settings
 */
class SettingsAction : public GroupAction
{
public:
    /**
     * Construct with \p parent object and \p title
     * @param parent Pointer to parent object
     * @param title Title
     */
    Q_INVOKABLE SettingsAction(QObject* parent, const QString& title);

    ///**
    // * Get action context menu
    // * @return Pointer to menu
    // */
    //QMenu* getContextMenu();

public: // Serialization

    /**
     * Load widget action from variant map
     * @param Variant map representation of the widget action
     */
    void fromVariantMap(const QVariantMap& variantMap) override;

    /**
     * Save widget action to variant map
     * @return Variant map representation of the widget action
     */

    QVariantMap toVariantMap() const override;

public: // Action getters

    PositionAction& getPositionAction() { return _positionAction; }

    DimensionPickerAction& getDimensionAction() { return _dimensionAction; }

    PointPlotAction& getPointPlotAction() { return _pointPlotAction; }

    SingleCellModeAction& getSingleCellModeAction() { return _singleCellModeAction; }

    ClusteringAction& getClusteringAction() { return _clusteringAction; }

    CorrelationModeAction& getCorrelationModeAction() { return _correlationModeAction; }

    VariantAction& getFloodFillAction() { return _floodFillAction; }
    IntegralAction& getSliceAction() { return _sliceAction; }   

private:
    ExampleViewJSPlugin*    _exampleViewJSPlugin;       /** Pointer to Example OpenGL Viewer Plugin */

    DatasetPickerAction     _positionDatasetPickerAction;
    DatasetPickerAction     _sliceDatasetPickerAction;

    DatasetPickerAction     _avgExprDatasetPickerAction;

    PositionAction          _positionAction;            /** position action */

    DimensionPickerAction   _dimensionAction;           /** dimension picker action */

    PointPlotAction         _pointPlotAction;           /** point plot action */

    CorrelationModeAction   _correlationModeAction;     /** correlation mode action */

    SingleCellModeAction    _singleCellModeAction;      /** single cell mode action */
    
    ClusteringAction        _clusteringAction;          /** clustering action */

    VariantAction           _floodFillAction;           /** flood fill action */
    IntegralAction          _sliceAction;           /** slice action */
};
