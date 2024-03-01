#pragma once
#include <actions/WidgetAction.h>
#include <actions/TriggerAction.h>
#include <actions/ToggleAction.h>
#include <actions/DatasetPickerAction.h>

using namespace mv::gui;

class ExampleViewJSPlugin;

/**
 * SingleCell mode action class
 *
 * Action class for choosing single cell mode setting
 */
class SingleCellModeAction : public WidgetAction
{
    Q_OBJECT
protected: // Widget
    class Widget : public WidgetActionWidget
    {
    public:
        Widget(QWidget* parent, SingleCellModeAction* overlayAction, const std::int32_t& widgetFlags);
    };

    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override
    {
        return new Widget(parent, this, widgetFlags);
    }

public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE SingleCellModeAction(QObject* parent, const QString& title);

    void initialize(ExampleViewJSPlugin* exampleViewJSPlugin);

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

    ToggleAction& getSingleCellOptionAction() { return _singleCellOptionAction; }

    TriggerAction& getComputeAvgExpressionAction() { return _computeAvgExpressionAction; }

    TriggerAction& getLoadAvgExpressionAction() { return _loadAvgExpressionAction; }

    DatasetPickerAction& getLabelDatasetPickerAction() { return _labelDatasetPickerAction; }

private:
    ExampleViewJSPlugin*    _exampleViewJSPlugin;     /** Pointer to scatterplot plugin */
    ToggleAction            _singleCellOptionAction;          /** single cell option action */

    TriggerAction           _computeAvgExpressionAction;
    TriggerAction           _loadAvgExpressionAction;

    DatasetPickerAction     _labelDatasetPickerAction;

    //friend class mv::AbstractActionsManager;
};

Q_DECLARE_METATYPE(SingleCellModeAction)

inline const auto singleCellModeActionMetaTypeId = qRegisterMetaType<SingleCellModeAction*>("SingleCellModeAction");