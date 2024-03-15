// This file is derived from "DimensionPickerAction.h" originally located at :
// core/ManiVault/src/plugins/PointData/src/DimensionPickerAction.cpp
// and is licensed under the LGPL-3.0-or-later license.
//
// Modifications:
// - Adapted to use setDimensionNames instead of setPointsDataset to remove the dependency on Dataset<Points>.

#pragma once

#include "actions/WidgetAction.h"
#include "actions/OptionAction.h"


using namespace mv;
using namespace mv::gui;
using namespace mv::util;


class GenePickerAction : public WidgetAction
{
Q_OBJECT

public:

    /** Widget class for points dimension picker action */
    class Widget : public WidgetActionWidget
    {
    protected:

        /**
         * Constructor
         * @param parent Pointer to parent widget
         * @param genePickerAction Smart pointer to dimension picker action
         */
        Widget(QWidget* parent, GenePickerAction* genePickerAction);

    protected:
        friend class GenePickerAction;
    };

protected:

    /**
     * Get widget representation of the dimension picker action
     * @param parent Pointer to parent widget
     * @param widgetFlags Widget flags for the configuration of the widget (type)
     */
    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override {
        return new Widget(parent, this);
    };

public:

    /** 
     * Constructor
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE GenePickerAction(QObject* parent, const QString& title);

    /**
     * Set the points dataset from which the dimension will be picked
     * @param points Dataset reference to points dataset
     */
    void setPointsDataset();// abandoned the code that was here

    void setDimensionNames(const QStringList& dimensionNames);

    /** Get the names of the dimensions */
    QStringList getDimensionNames() const;

    /** Get the number of currently loaded dimensions */
    std::uint32_t getNumberOfDimensions() const;

    /** Get the current dimension index */
    std::int32_t getCurrentDimensionIndex() const;

    /** Get the current dimension name */
    QString getCurrentDimensionName() const;

    /**
     * Set the current dimension by index
     * @param dimensionIndex Index of the current dimension
     */
    void setCurrentDimensionIndex(const std::int32_t& dimensionIndex);

    /**
     * Set the current dimension by name
     * @param dimensionName Name of the current dimension
     */
    void setCurrentDimensionName(const QString& dimensionName);

    /** Get search threshold */
    std::uint32_t getSearchThreshold() const;

    /**
     * Set search threshold
     * @param searchThreshold Search threshold
     */
    void setSearchThreshold(const std::uint32_t& searchThreshold);

    /** Establishes whether the dimensions can be searched (number of options exceeds the search threshold) */
    bool maySearch() const;

public: // Serialization

    /**
     * Load widget action from variant map
     * @param Variant map representation of the widget action
     */
    void fromVariantMap(const QVariantMap& variantMap) override;
    
    /**
     * Save widget action to variant map
     * @return Variant map representation of the widget action
     */
    QVariantMap toVariantMap() const override;

signals:

    /**
     * Signals that the current dimension index changed
     * @param currentDimensionIndex Index of the current dimension
     */
    void currentDimensionIndexChanged(const std::int32_t& currentDimensionIndex);

    /**
     * Signals that the current dimension name changed
     * @param currentDimensionName Name of the current dimension
     */
    void currentDimensionNameChanged(const QString& currentDimensionName);

public: /** Action getters */

    OptionAction& getCurrentDimensionAction() { return _currentDimensionAction; }

protected:
    QStringList         _dimensionNames;            /** Names of the dimensions */
    OptionAction        _currentDimensionAction;    /** Current dimension action */
    std::uint32_t       _searchThreshold;           /** Select from a drop-down below the threshold and above use a search bar */

protected:
    static constexpr std::uint32_t DEFAULT_SEARCH_THRESHOLD = 100;     /** Default search threshold */

    friend class AbstractActionsManager;
};

Q_DECLARE_METATYPE(GenePickerAction)

inline const auto genePickerActionMetaTypeId = qRegisterMetaType<GenePickerAction*>("GenePickerAction");
