#include "DimensionSelectionAction.h"
#include "src/GeneSurferPlugin.h"

using namespace mv::gui;

DimensionSelectionAction::DimensionSelectionAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _dimensionAction(this, "Gene")
{
    setIcon(mv::util::StyledIcon("search"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_dimensionAction);

    _dimensionAction.setToolTip("Select the dimension to be shown");

    auto geneSurferPlugin = dynamic_cast<GeneSurferPlugin*>(parent->parent());
    if (geneSurferPlugin == nullptr)
        return;

    connect(&_dimensionAction, &GenePickerAction::currentDimensionIndexChanged, [this, geneSurferPlugin](const std::uint32_t& currentDimensionIndex) {
        if (geneSurferPlugin->isDataInitialized() && _dimensionAction.getCurrentDimensionIndex() != -1) {
            geneSurferPlugin->updateShowDimension();
        }
        });

    connect(&geneSurferPlugin->getPositionSourceDataset(), &Dataset<Points>::changed, this, [this, geneSurferPlugin]() {
        auto sortedGeneNames = geneSurferPlugin->getPositionSourceDataset()->getDimensionNames();

        std::sort(sortedGeneNames.begin(), sortedGeneNames.end(), [](const QString& a, const QString& b) {
            // Compare alphabetically first
            int minLength = std::min(a.length(), b.length());
            for (int i = 0; i < minLength; i++) {
                if (a[i] != b[i]) {
                    return a[i] < b[i];
                }
            }
            // If one is a prefix of the other, or they are identical up to the minLength, sort by length
            return a.length() < b.length();
         });

        QStringList sortedGeneNamesList;
        for (const auto& str : sortedGeneNames) {
            sortedGeneNamesList.append(str);
        }

        _dimensionAction.setDimensionNames(sortedGeneNamesList);
        _dimensionAction.setCurrentDimensionIndex(-1);
        });

}

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
