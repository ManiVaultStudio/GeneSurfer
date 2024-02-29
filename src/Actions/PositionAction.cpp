#include "PositionAction.h"
#include "src/ExampleViewJSPlugin.h"

using namespace mv::gui;

PositionAction::PositionAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _xDimensionPickerAction(this, "X"),
    _yDimensionPickerAction(this, "Y")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("ruler-combined"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_xDimensionPickerAction);
    addAction(&_yDimensionPickerAction);

    _xDimensionPickerAction.setToolTip("X dimension");
    _yDimensionPickerAction.setToolTip("Y dimension");


    auto exampleViewJSPlugin = dynamic_cast<ExampleViewJSPlugin*>(parent->parent());
    if (exampleViewJSPlugin == nullptr)
        return;

connect(&_xDimensionPickerAction, &DimensionPickerAction::currentDimensionIndexChanged, [this, exampleViewJSPlugin](const std::uint32_t& currentDimensionIndex) {
         if (exampleViewJSPlugin->isDataInitialized())
             exampleViewJSPlugin->updateSelectedDim();
     });

     connect(&_yDimensionPickerAction, &DimensionPickerAction::currentDimensionIndexChanged, [this, exampleViewJSPlugin](const std::uint32_t& currentDimensionIndex) {
         if (exampleViewJSPlugin->isDataInitialized())
             exampleViewJSPlugin->updateSelectedDim();
     });

     connect(&exampleViewJSPlugin->getPositionDataset(), &Dataset<Points>::changed, this, [this, exampleViewJSPlugin]() {
         _xDimensionPickerAction.setPointsDataset(exampleViewJSPlugin->getPositionDataset());
         _yDimensionPickerAction.setPointsDataset(exampleViewJSPlugin->getPositionDataset());

         _xDimensionPickerAction.setCurrentDimensionIndex(0);

         const auto yIndex = _xDimensionPickerAction.getNumberOfDimensions() >= 2 ? 1 : 0;

         _yDimensionPickerAction.setCurrentDimensionIndex(yIndex);
         });

     const auto updateReadOnly = [this, exampleViewJSPlugin]() -> void {
         setEnabled(exampleViewJSPlugin->getPositionDataset().isValid());
     };

     updateReadOnly();

     connect(&exampleViewJSPlugin->getPositionDataset(), &Dataset<Points>::changed, this, updateReadOnly);
     // TO DO: how do the following work? - end

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

//void CorrelationModeAction::fromVariantMap(const QVariantMap& variantMap)
//{
//    OptionAction::fromVariantMap(variantMap);
//
//    _spatialCorrelationAction.fromParentVariantMap(variantMap);
//    _hdCorrelationAction.fromParentVariantMap(variantMap);
//}
//
//QVariantMap CorrelationModeAction::toVariantMap() const
//{
//    auto variantMap = OptionAction::toVariantMap();
//
//    _spatialCorrelationAction.insertIntoVariantMap(variantMap);
//    _hdCorrelationAction.insertIntoVariantMap(variantMap);
//
//    return variantMap;
//}
