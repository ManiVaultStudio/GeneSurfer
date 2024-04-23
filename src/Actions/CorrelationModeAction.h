#pragma once

#include <actions/VerticalGroupAction.h>
#include <actions/TriggerAction.h>


using namespace mv::gui;

class GeneSurferPlugin;

/**
 * Correlation mode action class
 *
 * Action class for choosing correlation mode setting
 */
class CorrelationModeAction : public VerticalGroupAction
{
    Q_OBJECT


public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE CorrelationModeAction(QObject* parent, const QString& title);


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
    TriggerAction& getSpatialCorrelationAction() { return _spatialCorrelationAction; }
    TriggerAction& getHDCorrelationAction() { return _hdCorrelationAction; }
    TriggerAction& getDiffAction() { return _diffAction; }

private:
    GeneSurferPlugin*   _geneSurferPlugin;     /** Pointer to the GeneSurfer plugin */
    TriggerAction       _spatialCorrelationAction;     /** Trigger action for activating the spatial correlation mode */
    TriggerAction       _hdCorrelationAction;     /** Trigger action for activating the HD correlation mode */
    TriggerAction       _diffAction;     /** Trigger action for activating the diff mode */

    //friend class mv::AbstractActionsManager;
};

Q_DECLARE_METATYPE(CorrelationModeAction)

inline const auto correlationModeActionMetaTypeId = qRegisterMetaType<CorrelationModeAction*>("CorrelationModeAction");