#include "SectionAction.h"
#include "src/GeneSurferPlugin.h"

using namespace mv::gui;

SectionAction::SectionAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _sliceAction(this, "Section", 0, 52) //initialize with 52 slices, will adjust after sliceDataset is loaded
{
    setToolTip("Section selection for 3D data");
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("mouse-pointer"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_sliceAction);

    _sliceAction.setToolTip("Section selection for 3D data");

    auto geneSurferPlugin = dynamic_cast<GeneSurferPlugin*>(parent->parent());
    if (geneSurferPlugin == nullptr)
        return;

    connect(&geneSurferPlugin->getSliceDataset(), &Dataset<Points>::changed, this, [this, geneSurferPlugin]() {
        _sliceAction.setMaximum(geneSurferPlugin->getSliceDataset()->getClusters().size() - 1);// Start from 0
        });

    connect(&_sliceAction, &IntegralAction::valueChanged, this, [this, geneSurferPlugin]() {
        geneSurferPlugin->updateSlice(_sliceAction.getValue());
        });
}

void SectionAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    
    _sliceAction.fromParentVariantMap(variantMap);
}

QVariantMap SectionAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _sliceAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
