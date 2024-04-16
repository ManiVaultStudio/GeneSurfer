#include "PositionAction.h"
#include "src/GeneSurferPlugin.h"

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


    auto geneSurferPlugin = dynamic_cast<GeneSurferPlugin*>(parent->parent());
    if (geneSurferPlugin == nullptr)
        return;

connect(&_xDimensionPickerAction, &DimensionPickerAction::currentDimensionIndexChanged, [this, geneSurferPlugin](const std::uint32_t& currentDimensionIndex) {
         if (geneSurferPlugin->isDataInitialized())
             geneSurferPlugin->updateSelectedDim();
     });

     connect(&_yDimensionPickerAction, &DimensionPickerAction::currentDimensionIndexChanged, [this, geneSurferPlugin](const std::uint32_t& currentDimensionIndex) {
         if (geneSurferPlugin->isDataInitialized())
             geneSurferPlugin->updateSelectedDim();
     });

     connect(&geneSurferPlugin->getPositionDataset(), &Dataset<Points>::changed, this, [this, geneSurferPlugin]() {
         _xDimensionPickerAction.setPointsDataset(geneSurferPlugin->getPositionDataset());
         _yDimensionPickerAction.setPointsDataset(geneSurferPlugin->getPositionDataset());

         _xDimensionPickerAction.setCurrentDimensionIndex(0);

         const auto yIndex = _xDimensionPickerAction.getNumberOfDimensions() >= 2 ? 1 : 0;

         _yDimensionPickerAction.setCurrentDimensionIndex(yIndex);
         });

     const auto updateReadOnly = [this, geneSurferPlugin]() -> void {
         setEnabled(geneSurferPlugin->getPositionDataset().isValid());
     };

     updateReadOnly();

     connect(&geneSurferPlugin->getPositionDataset(), &Dataset<Points>::changed, this, updateReadOnly);
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

void PositionAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    _xDimensionPickerAction.fromParentVariantMap(variantMap);
    _yDimensionPickerAction.fromParentVariantMap(variantMap);
}

QVariantMap PositionAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    _xDimensionPickerAction.insertIntoVariantMap(variantMap);
    _yDimensionPickerAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
