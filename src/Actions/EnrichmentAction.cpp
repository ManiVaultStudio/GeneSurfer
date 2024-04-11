#include "EnrichmentAction.h"
#include "src/ExampleViewJSPlugin.h"

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

    auto exampleViewJSPlugin = dynamic_cast<ExampleViewJSPlugin*>(parent->parent());
    if (exampleViewJSPlugin == nullptr)
        return;

    connect(&_enrichmentAPIPickerAction, &OptionAction::currentTextChanged, this, [this, exampleViewJSPlugin]{
        qDebug() << "Enrichment API changed to: " << _enrichmentAPIPickerAction.getCurrentText();
        exampleViewJSPlugin->setEnrichmentAPI();
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
