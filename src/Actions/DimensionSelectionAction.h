#pragma once
#include <actions/GroupAction.h>
#include <PointData/DimensionPickerAction.h>

using namespace mv::gui;

class ExampleViewJSPlugin;

/**
 * position action class
 *
 * Action class for position dimensions
 */
class DimensionSelectionAction : public GroupAction
{
    Q_OBJECT


public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE DimensionSelectionAction(QObject* parent, const QString& title);


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

    DimensionPickerAction& getDimensionAction() { return _dimensionAction; }

private:
    DimensionPickerAction   _dimensionAction;           /** dimension picker action */

    //friend class mv::AbstractActionsManager;
};

Q_DECLARE_METATYPE(DimensionSelectionAction)

inline const auto dimensionSelectionActionMetaTypeId = qRegisterMetaType<DimensionSelectionAction*>("DimensionSelectionAction");