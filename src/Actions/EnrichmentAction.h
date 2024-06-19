#pragma once
#include <actions/HorizontalGroupAction.h>
#include <actions/OptionAction.h>

using namespace mv::gui;

class GeneSurferPlugin;

/**
 * Enrichment action class
 *
 * Action class for enrichment settings
 */
class EnrichmentAction : public HorizontalGroupAction
{
    Q_OBJECT

public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE EnrichmentAction(QObject* parent, const QString& title);


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

    OptionAction& getEnrichmentAPIPickerAction() { return _enrichmentAPIPickerAction; }
    OptionAction& getSpeciesPickerAction() { return _speciesPickerAction; }

private:
    OptionAction   _enrichmentAPIPickerAction;    /** Enrichment API picker action */
    OptionAction   _speciesPickerAction;          /** Species picker action */
};

Q_DECLARE_METATYPE(EnrichmentAction)

inline const auto enrichmentActionMetaTypeId = qRegisterMetaType<EnrichmentAction*>("EnrichmentAction");