#include "EnrichmentAction.h"
#include "src/GeneSurferPlugin.h"

using namespace mv::gui;

EnrichmentAction::EnrichmentAction(QObject* parent, const QString& title) :
    HorizontalGroupAction(parent, title),
    _enrichmentAPIPickerAction(this, "API")
{
    //setIcon(mv::Application::getIconFont("FontAwesome").getIcon("ruler-combined"));
    setLabelSizingType(LabelSizingType::Auto);

    _enrichmentAPIPickerAction.setToolTip("Enrichment analysis API");

    _enrichmentAPIPickerAction.initialize(QStringList({ "ToppGene", "gProfiler" }), "ToppGene");
    addAction(&_enrichmentAPIPickerAction);

    auto geneSurferPlugin = dynamic_cast<GeneSurferPlugin*>(parent->parent());
    if (geneSurferPlugin == nullptr)
        return;

    connect(&_enrichmentAPIPickerAction, &OptionAction::currentTextChanged, this, [this, geneSurferPlugin]{
        qDebug() << "Enrichment API changed to: " << _enrichmentAPIPickerAction.getCurrentText();
        geneSurferPlugin->updateEnrichmentAPI();
        });
}

void EnrichmentAction::fromVariantMap(const QVariantMap& variantMap)
{
    HorizontalGroupAction::fromVariantMap(variantMap);
    _enrichmentAPIPickerAction.fromParentVariantMap(variantMap);
}

QVariantMap EnrichmentAction::toVariantMap() const
{
    auto variantMap = HorizontalGroupAction::toVariantMap();
    _enrichmentAPIPickerAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
