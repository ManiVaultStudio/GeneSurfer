#pragma once
#include <actions/VerticalGroupAction.h>
#include <PointData/DimensionPickerAction.h>

using namespace mv::gui;

class GeneSurferPlugin;

/**
 * position action class
 *
 * Action class for position dimensions
 */
class PositionAction : public VerticalGroupAction
{
    Q_OBJECT


public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE PositionAction(QObject* parent, const QString& title);


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

    DimensionPickerAction& getXDimensionPickerAction() { return _xDimensionPickerAction; }
    DimensionPickerAction& getYDimensionPickerAction() { return _yDimensionPickerAction; }

private:
    //GeneSurferPlugin*  _scatterplotPlugin;     /** Pointer to scatterplot plugin */
    DimensionPickerAction   _xDimensionPickerAction;    /** X-dimension picker action */
    DimensionPickerAction   _yDimensionPickerAction;    /** Y-dimension picker action */

    //friend class mv::AbstractActionsManager;
};

Q_DECLARE_METATYPE(PositionAction)

inline const auto positionActionMetaTypeId = qRegisterMetaType<PositionAction*>("PositionAction");