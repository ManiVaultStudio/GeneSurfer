#pragma once

#include <ViewPlugin.h>

#include "compute/DataStore.h"
#include "compute/DataMatrix.h"
#include "Compute/EnrichmentAnalysis.h"
#include "Compute/fastcluster.h"

#include "Actions/SettingsAction.h"

#include <actions/HorizontalToolbarAction.h>
#include <actions/ColorMap1DAction.h>
#include <Dataset.h>
#include <PointData/PointData.h>
#include <ClusterData/ClusterData.h>

#include <widgets/DropWidget.h>

#include <Eigen/Dense>

#include <QWidget>

//#include "Compute/HsneHierarchy.h"
//#include "Compute/HsneParameters.h"// TO DO remove HSNE dependency

#include <QNetworkReply>
#include <QTableWidget>
#include <QTableWidgetItem>


/** All plugin related classes are in the mv plugin namespace */
using namespace mv::plugin;

/** Drop widget used in this plugin is located in the mv gui namespace */
using namespace mv::gui;

/** Dataset reference used in this plugin is located in the mv util namespace */
using namespace mv::util;

enum class AvgExpressionStatus
{
    NONE,
    COMPUTED,
    LOADED
};

class ChartWidget;
class ScatterView;

class ExampleViewJSPlugin : public ViewPlugin
{
    Q_OBJECT

public:
    /**
     * Constructor
     * @param factory Pointer to the plugin factory
     */
    ExampleViewJSPlugin(const PluginFactory* factory);

    /** Destructor */
    ~ExampleViewJSPlugin() override = default;
    
    /** This function is called by the core after the view plugin has been created */
    void init() override;

    /** Store a private reference to the data set that should be displayed */
    void loadData(const mv::Datasets& datasets) override;
 
    void updateNumCluster(); 

    void updateCorrThreshold(); 

    /** update the selected dim in the scatter plot */
    void updateSelectedDim();

    /** Update the selection of mouse of floodfill data */
    void updateFloodFill();

    /** Update the point size of _scatterViews and _dimView */
    void updateScatterPointSize();

    /** Update the opacity of non-flooded nodes in _scatterViews and _dimView */
    void updateScatterOpacity();

    void updateSlice();

    void setCorrelationMode(bool mode);// test

    /** Update the chosen single cell option */
    void updateSingleCellOption();

    /** Set the single cell dataset - which label to use for computing average expression*/
    void setLabelDataset();

    /** Update the average expression of each cluster */
    void computeAvgExpression();

    /** Load the average expression of each cluster */
    void loadAvgExpression();

    /** Update the color of _dimView */
    void updateShowDimension();


    
public slots:
    /** Converts ManiVault's point data to a json-like data structure that Qt can pass to the JS code */
    void convertDataAndUpdateChart();

    /** Update the enrichment analysis table with the current data */
    void updateEnrichmentTable(const QVariantList& data);

    /** Update the enrichment analysis table when data is not avaliable */
    void noDataEnrichmentTable();

private slots:
    /** Invoked when the selection of the cell in the table changes */
    void onTableClicked(int row, int column);


private:
    /** Published selections received from the JS side to ManiVault's core */
    void publishSelection(const QString& selection);

    QString getCurrentDataSetID() const;

    /** Read _floodFillDataset from data hierarchy */
    void updateFloodFillDataset();

    /** Update the selection of mouse in GradientViewer */
    void updateSelection();

    /** Update the highlighted selection of mouse in _scatterViews */
    void updateScatterSelection();

    /** Update the color scalars in _scatterViews */
    void updateScatterColors();

    /** Update the _dimView */
    void updateDimView(const QString& selectedDim);

    /** Compute correlations of gene and floodfill wave */
    void computeCorrWave();

    /** Compute correlations of gene and spatial coordinates - partial x y z */
    void computeCorrSpatial();

    void computeSubsetData(const DataMatrix& dataMatrix, const std::vector<int>& sortedIndices, DataMatrix& subsetDataMatrix);

    /** order the 1D vector according to the spatial coordinates */
    void orderSpatially(const std::vector<int>& unsortedIndices, const std::vector<int>& unsortedWave, std::vector<mv::Vector2f>& sortedCoordinates, std::vector<int>& sortedIndices, std::vector<int>& sortedWave);

    /** Compute the correlation between two vectors using Eigen */
    float computeCorrelation(const Eigen::VectorXf& a, const Eigen::VectorXf& b);

    /** Cluster genes based on their pairwise correlations */
    void clusterGenes();

    /** Compute the scalar values of each cluster for the entire scatter plot */
    void computeEntireClusterScalars(const std::vector<int> filteredDimIndices, const int* labels);

    /** Compute the scalar values of each cluster for the only flooded cells */
    void computeFloodedClusterScalars(const std::vector<int> filteredDimIndices, const int* labels);

    /** Compute the scalar values of each cluster for the only flooded cells - for single cell option */
    void computeFloodedClusterScalarsSingleCell(const std::vector<int> filteredDimIndices, const int* labels);

    /** Update the scalar values - to use in Volume Viewer */
    void updateClusterScalarOutput(const std::vector<float>& scalars);

    /** call to do functional enrichment of genes in each cluster */
    void getFuntionalEnrichment();

    void updateClick();

    /** set data for the views */
    void updateViewData(std::vector<Vector2f>& positions);

    /** count distribution of labels within floodfill */
    void countLabelDistribution();

    /** load data for labels from ST dataset*/
    void loadLabelsFromSTDataset();

    /** load data for labels from ST dataset - ONLY FOR ABCAtlas*/
    void loadLabelsFromSTDatasetABCAtlas();

    /** load data for average expression of each cluster - ONLY FOR ABCAtlas*/
    void loadAvgExpressionABCAtlas();

    /** get a subset of average expression of each cluster */
    void computeAvgExprSubset();

private:
    DataStorage             _dataStore;

    ChartWidget*            _chartWidget;       // WebWidget that sets up the HTML page - bar chart

    QTableWidget*           _tableWidget; // table widget for enrichment analysis

    DropWidget*             _dropWidget;        // Widget for drag and drop behavior
    mv::Dataset<Points>   _currentDataSet;    // Reference to currently shown data set
    mv::Dataset<Points>   _positionDataset;   /** Smart pointer to points dataset for point position */
    mv::Dataset<Points>   _positionSourceDataset;     /** Smart pointer to source of the points dataset for point position (if any) */
    int32_t                 _numPoints;                 /** Number of point positions */
    std::vector<QString>    _enabledDimNames;

    bool                    _dataInitialized = false;
    std::vector<bool>       _isFloodIndex; //direct mapping for flood indices
    mv::Dataset<Points>   _floodFillDataset; // dataset for flood fill

    //int32_t                 _cursorPoint = 0; // index of the point under the cursor

    std::vector<int>        _sortedFloodIndices;// spatially sorted indices of flood fill at the current cursor position
    std::vector<int>        _sortedWaveNumbers;// spatially sorted wave numbers of flood fill at the current cursor position
    Eigen::MatrixXf         _subsetData; // subset of flooded data, sorted spatially

    std::vector<mv::Vector2f>     _positions;                 /** Point positions */ 
    std::vector<float>      _corrGeneWave;
    std::vector<float>      _corrGeneSpatial;
    

    float                   _corrThreshold;

    int                     _nclust;// number of clusters
    std::unordered_map<QString, int>  _dimNameToClusterLabel; // map dimension name to cluster label

    int                     _selectedClusterIndex;
    int                     _selectedDimIndex; // current selected dimension index for _dimView

    //HsneHierarchy           _hierarchy; // TO DO remove HSNE dependency

    std::vector<std::vector<float>> _colorScalars; // scalars for the color of each scatter view

    Dataset<Points>                 _clusterScalars; // scalars for plotting in volume viewer

    int                             _currentSliceIndex = 0; // current slice index for 3D slice dataset
    Dataset<Clusters>               _sliceDataset; // dataset for 3D slices
    std::vector<int>                _onSliceIndices; // pt indices on the slice
    std::vector<int>                _onSliceFloodIndices; // flood indices on the slice
    std::vector<int>                _onSliceWaveNumbers; // wave numbers on the slice
    std::vector<bool>               _isFloodOnSlice; // direct mapping for flood indices on the slice
    Eigen::MatrixXf                 _subsetData3D;// subset of flooded data 3D

    EnrichmentAnalysis*             _client;// enrichment analysis client 
    QVariantList                    _enrichmentResult; // cached enrichment analysis result, in case the user clicks on one cell

    bool                            _isCorrSpatial = false; // whether to use spatial coordinates for correlation
    bool                            _isSingleCell = false; // whether to use avg expression from single cell data
    bool                            _toClearBarchart = false; //whether to clear the bar chart


    std::vector<float>          _waveAvg; // avg of wave numbers for each cluster - same order as subset columns

    // for avg expression of single cell data
    std::vector<QString>            _geneNamesAvgExpr; // from avg expr single cell data
    Eigen::MatrixXf                 _avgExpr; // average expression of each cluster
    Eigen::MatrixXf                 _subsetDataAvgWeighted; // subset of average expression of each cluster - weighted
    Eigen::MatrixXf                 _subsetDataAvgOri; // subset of average expression of each cluster - NOT weighted - TO DO

    bool                            _avgExprDatasetExists = false; // whether the avg expression dataset exists
    mv::Dataset<Points>             _avgExprDataset; // Point dataset for average expression of each cluster - test
    std::unordered_map<QString, QString> _identifierToSymbolMap; // map from gene identifier to gene symbol
    QMap<QString, QStringList>      _simplifiedToIndexGeneMapping; // map for simplified gene names to extra indexed gene names - for duplicate gene symbols 

    // TEST: generalize singlecell option 
    std::vector<QString>                _clusterNamesAvgExpr;// from avg expr single cell data
    std::unordered_map<QString, int>    _clusterAliasToRowMap; // first element is label as a QString, second element is row index in _avgExpr
    std::vector<QString>            _cellLabels;// labels for each point
    std::map<QString, int>          _countsLabel; // count distribution of labels within floodfill, first element is the label 
    std::map<QString, float>        _clusterWaveNumbers; // avg of wave numbers for each cluster, first element is the label
    std::vector<QString>                _clustersToKeep;// clusters to keep for avg expression - same order as subset row - cluster alias name 

    // serialization
    bool                    _loadingFromProject = false;
    AvgExpressionStatus     _avgExprStatus = AvgExpressionStatus::NONE;
    QString                 _selectedDimName = "NoneSelected"; // selected dimension name of _selectedDimIndex

    

public:
    bool isDataInitialized() { return _dataInitialized; }
    bool isUsingSingleCell() { return _isSingleCell; }
    //HsneHierarchy& getHierarchy() { return _hierarchy; }


public: // Data loading
    /** Invoked when the position points dataset changes */
    void positionDatasetChanged();

public:
    /** Get smart pointer to points dataset for point position */
    mv::Dataset<Points>& getPositionDataset() { return _positionDataset; }

    /** Get smart pointer to source dataset */
    mv::Dataset<Points>& getPositionSourceDataset() { return _positionSourceDataset; }

    mv::Dataset<Points>& getAvgExprDataset() { return _avgExprDataset; }

    mv::Dataset<Clusters>& getSliceDataset() { return _sliceDataset; }

    //SettingsAction& getSettingsAction() { return _settingsAction; }

public: 
    void setAvgExpressionStatus(AvgExpressionStatus status) { _avgExprStatus = status; }

public: 
    /** Get reference to the scatter plot widget */
    std::vector<ScatterView*>& getProjectionViews() { return _scatterViews; }

public: // Serialization
    /**
    * Load plugin from variant map
    * @param Variant map representation of the plugin
    */
    void fromVariantMap(const QVariantMap& variantMap) override;

    /**
    * Save plugin to variant map
    * @return Variant map representation of the plugin
    */
    QVariantMap toVariantMap() const override;

signals:
    void avgExprDatasetExistsChanged(bool exists);

protected:
    std::vector<ScatterView*>    _scatterViews; // scatter plots for cluster images
    ScatterView*                 _dimView; // scatter plot for selected dimensions
    SettingsAction               _settingsAction;/** Settings action */
    ColorMap1DAction             _colorMapAction;/** Color map action */
    HorizontalToolbarAction      _primaryToolbarAction;      /** Horizontal toolbar for primary content */

};

/**
 * Example view plugin factory class
 *
 * Note: Factory does not need to be altered (merely responsible for generating new plugins when requested)
 */
class ExampleViewJSPluginFactory : public ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "nl.BioVault.ExampleViewJSPlugin"
                      FILE  "ExampleViewJSPlugin.json")

public:

    /** Default constructor */
    ExampleViewJSPluginFactory() {}

    /** Destructor */
    ~ExampleViewJSPluginFactory() override {}
    
    /** Get plugin icon */
    QIcon getIcon(const QColor& color = Qt::black) const override;

    /** Creates an instance of the example view plugin */
    ViewPlugin* produce() override;

    /** Returns the data types that are supported by the example view plugin */
    mv::DataTypes supportedDataTypes() const override;

    /**
     * Get plugin trigger actions given \p datasets
     * @param datasets Vector of input datasets
     * @return Vector of plugin trigger actions
     */
    PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;


};
