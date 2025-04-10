

#include "GenePickerAction.h"
#include "Application.h"

#include <QHBoxLayout>

using namespace mv;

GenePickerAction::GenePickerAction(QObject* parent, const QString& title) :
    WidgetAction(parent, title),
    _dimensionNames(),
    _currentDimensionAction(this, "Select dimension"),
    _searchThreshold(DEFAULT_SEARCH_THRESHOLD)
{
    setText(title);
    setIconByName("adjust");

    _currentDimensionAction.setToolTip("Current dimension");

    // Selection changed notifier
    const auto selectionChanged = [this]() {
        emit currentDimensionIndexChanged(_currentDimensionAction.getCurrentIndex());
    };

    // Relay current index/text changed signal from the current dimension action
    connect(&_currentDimensionAction, &OptionAction::currentIndexChanged, this, selectionChanged);
}

void GenePickerAction::setPointsDataset()
{
    // removed the code that was here
}

void GenePickerAction::setDimensionNames(const QStringList& dimensionNames)
{
    if (dimensionNames.isEmpty()) {
        _currentDimensionAction.setOptions(QStringList());
        setCurrentDimensionIndex(-1);
        return;
    }

    _currentDimensionAction.setOptions(dimensionNames);

    if (!dimensionNames.isEmpty()) {
        setCurrentDimensionIndex(0);
    }
}

QStringList GenePickerAction::getDimensionNames() const
{
    return _currentDimensionAction.getOptions();
}

std::uint32_t GenePickerAction::getNumberOfDimensions() const
{
    return _currentDimensionAction.getNumberOfOptions();
}

std::int32_t GenePickerAction::getCurrentDimensionIndex() const
{
    return _currentDimensionAction.getCurrentIndex();
}

QString GenePickerAction::getCurrentDimensionName() const
{
    return _currentDimensionAction.getCurrentText();
}

void GenePickerAction::setCurrentDimensionIndex(const std::int32_t& dimensionIndex)
{
    _currentDimensionAction.setCurrentIndex(dimensionIndex);
}

void GenePickerAction::setCurrentDimensionName(const QString& dimensionName)
{
    _currentDimensionAction.setCurrentText(dimensionName);
}

std::uint32_t GenePickerAction::getSearchThreshold() const
{
    return _searchThreshold;
}

void GenePickerAction::setSearchThreshold(const std::uint32_t& searchThreshold)
{
    _searchThreshold = searchThreshold;
}

bool GenePickerAction::maySearch() const
{
    return _currentDimensionAction.getNumberOfOptions() >= _searchThreshold;
}


void GenePickerAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);

    if (variantMap.contains("DimensionNames")) {
        _dimensionNames = variantMap.value("DimensionNames").toStringList();
        setDimensionNames(_dimensionNames);
    }

    _currentDimensionAction.fromParentVariantMap(variantMap);
}

QVariantMap GenePickerAction::toVariantMap() const
{
    auto variantMap = WidgetAction::toVariantMap();

    _currentDimensionAction.insertIntoVariantMap(variantMap);

    variantMap.insert("DimensionNames", QVariant::fromValue(_dimensionNames));

    return variantMap;
}

GenePickerAction::Widget::Widget(QWidget* parent, GenePickerAction* genePickerAction) :
    WidgetActionWidget(parent, genePickerAction)
{
    auto layout = new QHBoxLayout();

    layout->setContentsMargins(0, 0, 0, 0);

    // Create widgets
    auto comboBoxWidget = genePickerAction->getCurrentDimensionAction().createWidget(this, OptionAction::ComboBox);
    auto lineEditWidget = genePickerAction->getCurrentDimensionAction().createWidget(this, OptionAction::LineEdit);

    // Gets search icon to decorate the line edit
    const auto searchIcon = mv::util::StyledIcon("search");

    // Add the icon to the line edit
    lineEditWidget->findChild<QLineEdit*>("LineEdit")->addAction(searchIcon, QLineEdit::TrailingPosition);

    // Add widgets to layout
    layout->addWidget(comboBoxWidget);
    layout->addWidget(lineEditWidget);

    // Update widgets visibility
    const auto updateWidgetsVisibility = [genePickerAction, comboBoxWidget, lineEditWidget]() {
        
        // Get whether the option action may be searched
        const auto maySearch = genePickerAction->maySearch();

        // Update visibility
        comboBoxWidget->setVisible(!maySearch);
        lineEditWidget->setVisible(maySearch);
    };

    // Update widgets visibility when the dimensions change
    connect(&genePickerAction->getCurrentDimensionAction(), &OptionAction::modelChanged, this, updateWidgetsVisibility);

    // Initial update of widgets visibility
    updateWidgetsVisibility();

    // Apply layout
    setLayout(layout);
}
