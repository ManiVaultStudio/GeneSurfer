#include "SingleCellModeAction.h"
#include "src/ExampleViewJSPlugin.h"

#include <QMenu>
#include <QGroupBox>

using namespace mv::gui;

SingleCellModeAction::SingleCellModeAction(QObject* parent, const QString& title) :
    WidgetAction(parent, title),
	_exampleViewJSPlugin(nullptr),
    _singleCellOptionAction(this, "Use scRNA-seq genes", false),
    _computeAvgExpressionAction(this, "Compute average expression"),
    _loadAvgExpressionAction(this, "Load average expression")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("braille"));//"eye", "braille", "database", "chart-area"
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);

    _singleCellOptionAction.setToolTip("Single Cell Option");
    _computeAvgExpressionAction.setToolTip("Compute average expression");
    _loadAvgExpressionAction.setToolTip("Load average expression");

    if (_exampleViewJSPlugin == nullptr)
    {
        return;
    }
}

void SingleCellModeAction::initialize(ExampleViewJSPlugin* exampleViewJSPlugin)
{
    connect(&_singleCellOptionAction, &ToggleAction::toggled, exampleViewJSPlugin, &ExampleViewJSPlugin::updateSingleCellOption);

    connect(&_computeAvgExpressionAction, &TriggerAction::triggered, exampleViewJSPlugin, &ExampleViewJSPlugin::computeAvgExpression);

    connect(&_loadAvgExpressionAction, &TriggerAction::triggered, exampleViewJSPlugin, &ExampleViewJSPlugin::loadAvgExpression);

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

    // hide for now - function not implemented
    /*layout->addWidget(new QLabel("Average Expression:", parent), 1, 0);
    layout->addWidget(singleCellModeAction->getComputeAvgExpressionAction().createWidget(this), 2, 0);
    layout->addWidget(singleCellModeAction->getLoadAvgExpressionAction().createWidget(this), 2, 1);*/

    setLayout(layout);
}


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
