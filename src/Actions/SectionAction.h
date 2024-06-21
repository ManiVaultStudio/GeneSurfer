#pragma once
#include <actions/GroupAction.h>
#include <actions/IntegralAction.h>

using namespace mv::gui;

class GeneSurferPlugin;

/**
 * section action class
 *
 * Action class for section selection for 3D data
 */
class SectionAction : public GroupAction
{
    Q_OBJECT


public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE SectionAction(QObject* parent, const QString& title);


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

    IntegralAction& getSliceAction() { return _sliceAction; }

private:
    IntegralAction          _sliceAction;           /** slice or section action */

};

Q_DECLARE_METATYPE(SectionAction)

inline const auto sectionActionMetaTypeId = qRegisterMetaType<SectionAction*>("SectionAction");