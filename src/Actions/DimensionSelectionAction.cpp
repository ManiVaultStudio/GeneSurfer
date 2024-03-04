#include "DimensionSelectionAction.h"
#include "src/ExampleViewJSPlugin.h"

using namespace mv::gui;

DimensionSelectionAction::DimensionSelectionAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _dimensionAction(this, "Gene")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("database"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_dimensionAction);

    _dimensionAction.setToolTip("Select the dimension to be shown");


    auto exampleViewJSPlugin = dynamic_cast<ExampleViewJSPlugin*>(parent->parent());
    if (exampleViewJSPlugin == nullptr)
        return;

    connect(&_dimensionAction, &DimensionPickerAction::currentDimensionIndexChanged, [this, exampleViewJSPlugin](const std::uint32_t& currentDimensionIndex) {
        if (exampleViewJSPlugin->isDataInitialized()) {
            exampleViewJSPlugin->updateShowDimension();
        }
        });

    connect(&exampleViewJSPlugin->getPositionSourceDataset(), &Dataset<Points>::changed, this, [this, exampleViewJSPlugin]() {
        _dimensionAction.setPointsDataset(exampleViewJSPlugin->getPositionSourceDataset());
        //_dimensionAction.setCurrentDimensionIndex(0);
        });

}


// void CorrelationModeAction::connectToPublicAction(WidgetAction* publicAction, bool recursive)
// {
    // auto publicCorrelationModeAction = dynamic_cast<CorrelationModeAction*>(publicAction);

    // Q_ASSERT(publicCorrelationModeAction != nullptr);

    // if (publicCorrelationModeAction == nullptr)
        // return;

    // if (recursive) {
        // actions().connectPrivateActionToPublicAction(&_scatterPlotAction, &publicCorrelationModeAction->getScatterPlotAction(), recursive);
        // actions().connectPrivateActionToPublicAction(&_densityPlotAction, &publicCorrelationModeAction->getDensityPlotAction(), recursive);
        // actions().connectPrivateActionToPublicAction(&_contourPlotAction, &publicCorrelationModeAction->getContourPlotAction(), recursive);
    // }

    // OptionAction::connectToPublicAction(publicAction, recursive);
// }

// void CorrelationModeAction::disconnectFromPublicAction(bool recursive)
// {
    // if (!isConnected())
        // return;

    // if (recursive) {
        // actions().disconnectPrivateActionFromPublicAction(&_scatterPlotAction, recursive);
        // actions().disconnectPrivateActionFromPublicAction(&_densityPlotAction, recursive);
        // actions().disconnectPrivateActionFromPublicAction(&_contourPlotAction, recursive);
    // }

    // OptionAction::disconnectFromPublicAction(recursive);
// }

void DimensionSelectionAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _dimensionAction.fromParentVariantMap(variantMap);
    
}

QVariantMap DimensionSelectionAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _dimensionAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
