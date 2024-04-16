#pragma once
#include <actions/VerticalGroupAction.h>
#include <actions/DecimalAction.h>
#include <actions/IntegralAction.h>

using namespace mv::gui;

class GeneSurferPlugin;

/**
 * Clustering setting action class
 *
 * Action class for choosing correlation mode setting
 */
class ClusteringAction : public VerticalGroupAction
{
    Q_OBJECT


public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE ClusteringAction(QObject* parent, const QString& title);


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

public: // Action getters

   IntegralAction& getNumClusterAction() { return _numClusterAction; }
   //DecimalAction& getCorrThresholdAction() { return _corrThresholdAction; }
   IntegralAction& getNumGenesThresholdAction() { return _numGenesThresholdAction; }

private:
    IntegralAction          _numClusterAction;        /** number of cluster action */
    //DecimalAction           _corrThresholdAction;        /** correlation threshold action */
    IntegralAction          _numGenesThresholdAction;        /** number of gene threshold action */
};

Q_DECLARE_METATYPE(ClusteringAction)

inline const auto clusteringActionMetaTypeId = qRegisterMetaType<ClusteringAction*>("ClusteringAction");