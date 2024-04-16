#pragma once
#include <actions/DecimalAction.h>
#include <actions/VerticalGroupAction.h>
using namespace mv::gui;

class GeneSurferPlugin;

/**
 * Point plot action class
 *
 * Action class for point plot settings
 */
class PointPlotAction : public VerticalGroupAction
{
    Q_OBJECT


public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE PointPlotAction(QObject* parent, const QString& title);



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

    DecimalAction& getPointSizeAction() { return _pointSizeAction; }
    DecimalAction& getPointOpacityAction() { return _pointOpacityAction; }

private:
    DecimalAction           _pointSizeAction;           /** point size action */
    DecimalAction           _pointOpacityAction;        /** point opacity action */

    //friend class mv::AbstractActionsManager;
};

Q_DECLARE_METATYPE(PointPlotAction)

inline const auto pointPlotActionMetaTypeId = qRegisterMetaType<PointPlotAction*>("PointPlotAction");