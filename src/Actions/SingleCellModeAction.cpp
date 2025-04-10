#include "SingleCellModeAction.h"
#include "src/GeneSurferPlugin.h"

#include <QMenu>
#include <QGroupBox>

using namespace mv::gui;

SingleCellModeAction::SingleCellModeAction(QObject* parent, const QString& title) :
    WidgetAction(parent, title),
    _singleCellOptionAction(this, "Use scRNA-seq genes", false),
    _computeAvgExpressionAction(this, "Compute average expression"),
    _loadAvgExpressionAction(this, "Load average expression"),
    _labelDatasetPickerAction(this, "Label for mapping")
{
    setIcon(mv::util::StyledIcon("braille"));//"eye", "braille", "database", "chart-area"
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);

    _singleCellOptionAction.setToolTip("Single Cell Option");
    _computeAvgExpressionAction.setToolTip("Compute average expression");
    _loadAvgExpressionAction.setToolTip("Load average expression");
    _labelDatasetPickerAction.setToolTip("Label Dataset used for mapping to ST");

}

void SingleCellModeAction::initialize(GeneSurferPlugin* geneSurferPlugin)
{
    connect(&_singleCellOptionAction, &ToggleAction::toggled, geneSurferPlugin, &GeneSurferPlugin::updateSingleCellOption);

    // set gProfiler as enrichment API if single cell mode is disabled
    connect(&_singleCellOptionAction, &ToggleAction::toggled, this, [this, geneSurferPlugin] ()
        {         
            if (!_singleCellOptionAction.isChecked())
            {
                geneSurferPlugin->setEnrichmentAPIOptions(QStringList({ "gProfiler" }));
                geneSurferPlugin->setEnrichmentAPI("gProfiler");
            } else             {
                geneSurferPlugin->setEnrichmentAPIOptions(QStringList({ "ToppGene", "gProfiler"}));
                geneSurferPlugin->setEnrichmentAPI("ToppGene");
            }
        });

    connect(&_labelDatasetPickerAction, &DatasetPickerAction::datasetPicked, geneSurferPlugin, &GeneSurferPlugin::setLabelDataset);

    _singleCellOptionAction.setEnabled(false);
    _computeAvgExpressionAction.setEnabled(false);

    connect(&_computeAvgExpressionAction, &TriggerAction::triggered, this, [geneSurferPlugin](bool enabled)
        {
            geneSurferPlugin->setAvgExpressionStatus(AvgExpressionStatus::COMPUTED);
            geneSurferPlugin->computeAvgExpression();
        });
    connect(&_loadAvgExpressionAction, &TriggerAction::triggered, this, [geneSurferPlugin](bool enabled)
        {
            geneSurferPlugin->setAvgExpressionStatus(AvgExpressionStatus::LOADED);
            geneSurferPlugin->loadAvgExpression();
        });

}

SingleCellModeAction::Widget::Widget(QWidget* parent, SingleCellModeAction* singleCellModeAction, const std::int32_t& widgetFlags) :
    WidgetActionWidget(parent, singleCellModeAction, widgetFlags)
{
    setToolTip("Single Cell Mode settings");
    //setStyleSheet("QToolButton { width: 36px; height: 36px; qproperty-iconSize: 18px; }");

    auto layout = new QGridLayout();

    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(singleCellModeAction->getSingleCellOptionAction().createLabelWidget(this), 0, 0);
    layout->addWidget(singleCellModeAction->getSingleCellOptionAction().createWidget(this), 0, 1);

    layout->addWidget(singleCellModeAction->getLabelDatasetPickerAction().createLabelWidget(this), 1, 0);
    layout->addWidget(singleCellModeAction->getLabelDatasetPickerAction().createWidget(this), 1, 1);

    layout->addWidget(new QLabel("Average Expression:", parent), 2, 0);
    layout->addWidget(singleCellModeAction->getComputeAvgExpressionAction().createWidget(this), 3, 0);
    layout->addWidget(singleCellModeAction->getLoadAvgExpressionAction().createWidget(this), 3, 1);

    setLayout(layout);
}


void SingleCellModeAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);

    _labelDatasetPickerAction.fromParentVariantMap(variantMap);
    _computeAvgExpressionAction.fromParentVariantMap(variantMap);
    _loadAvgExpressionAction.fromParentVariantMap(variantMap);
    _singleCellOptionAction.fromParentVariantMap(variantMap);  
}

QVariantMap SingleCellModeAction::toVariantMap() const
{
    auto variantMap = WidgetAction::toVariantMap();

    _singleCellOptionAction.insertIntoVariantMap(variantMap);
    _computeAvgExpressionAction.insertIntoVariantMap(variantMap);
    _loadAvgExpressionAction.insertIntoVariantMap(variantMap);
    _labelDatasetPickerAction.insertIntoVariantMap(variantMap);

    return variantMap;
}