#include "EnrichmentAction.h"
#include "src/GeneSurferPlugin.h"


using namespace mv::gui;

EnrichmentAction::EnrichmentAction(QObject* parent, const QString& title) :
    HorizontalGroupAction(parent, title),
    _enrichmentAPIPickerAction(this, "API"),
    _speciesPickerAction(this, "Species")
{
    //setIcon(mv::Application::getIconFont("FontAwesome").getIcon("ruler-combined"));
    setLabelSizingType(LabelSizingType::Auto);

    _enrichmentAPIPickerAction.setToolTip("Enrichment analysis API");
    _speciesPickerAction.setToolTip("Species");

    _enrichmentAPIPickerAction.initialize(QStringList({ "ToppGene", "gProfiler" }), "ToppGene");
    addAction(&_enrichmentAPIPickerAction);

    _speciesPickerAction.initialize(QStringList({ "None", "Mus musculus", "Homo sapiens" }), "None");
    addAction(&_speciesPickerAction);
    _speciesPickerAction.setEnabled(false);

    auto geneSurferPlugin = dynamic_cast<GeneSurferPlugin*>(parent->parent());
    if (geneSurferPlugin == nullptr)
        return;

    connect(&_enrichmentAPIPickerAction, &OptionAction::currentTextChanged, this, [this, geneSurferPlugin]{
        //qDebug() << "Enrichment API changed to: " << _enrichmentAPIPickerAction.getCurrentText();
        geneSurferPlugin->updateEnrichmentAPI();

        if (_enrichmentAPIPickerAction.getCurrentText() == "gProfiler")
        {
            _speciesPickerAction.setEnabled(true);
            _speciesPickerAction.setOptions(QStringList({ "Mus musculus", "Homo sapiens" }));
            _speciesPickerAction.setCurrentText("Mus musculus");
        }
        else
        {
            _speciesPickerAction.setEnabled(false);
            _speciesPickerAction.setOptions(QStringList({ "None", "Mus musculus", "Homo sapiens" }));
            _speciesPickerAction.setCurrentText("None");
        }
        });

    connect(&_speciesPickerAction, &OptionAction::currentTextChanged, this, [this, geneSurferPlugin] {
        geneSurferPlugin->updateEnrichmentSpecies();
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
