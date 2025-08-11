#pragma once

#include <ViewPlugin.h>

#include "Compute/DataStore.h"
#include "Compute/DataMatrix.h"
#include "Compute/EnrichmentAnalysis.h"
#include "Compute/CorrFilter.h"
#include "Compute/DataSubset.h"

#include "Actions/SettingsAction.h"
#include "TableWidget.h"

#include <actions/HorizontalToolbarAction.h>
#include <actions/ColorMap1DAction.h>
#include <Dataset.h>
#include <PointData/PointData.h>
#include <ClusterData/ClusterData.h>

#include <widgets/DropWidget.h>

#include <Eigen/Dense>
#include <fastcluster.h>

#include <QWidget>
#include <QNetworkReply>
#include <QTableWidgetItem>

#include <QFileDialog>



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

class GeneSurferPlugin : public ViewPlugin
{
    Q_OBJECT

public:
    /**
     * Constructor
     * @param factory Pointer to the plugin factory
     */
    GeneSurferPlugin(const PluginFactory* factory);

    /** Destructor */
    ~GeneSurferPlugin() override = default;
    
    /** This function is called by the core after the view plugin has been created */
    void init() override;

    /** Store a private reference to the data set that should be displayed */
    void loadData(const mv::Datasets& datasets) override;
 
    void updateNumCluster(); 

    void updateCorrThreshold(); 

    /** update the selected dim in the scatter plot */
    void updateSelectedDim();

    /** Update the point size of _scatterViews and _dimView */
    void updateScatterPointSize();

    /** Update the opacity of non-flooded nodes in _scatterViews and _dimView */
    void updateScatterOpacity();

    void updateSlice(int sliceIndex);

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

    /** Update the selection of mouse in GradientViewer */
    void updateSelection();

    void updateFilterLabel();//TODO: connect with filter type change
    
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

    /** Update the color scalars in _scatterViews */
    void updateScatterColors();

    /** Update the _dimView */
    void updateDimView(const QString& selectedDim);

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

    /** Match the annotations labels in ST and acRNA-seq data in the floodfill*/
    void matchLabelInSubset();

    /** load data for labels from ST dataset - work with computing avg*/
    void loadLabelsFromSTDataset();

    /** load data for labels from ST dataset - work with loaded csv file*/
    void loadLabelsFromSTDatasetFromFile();

    /** load data for average expression of each cluster - load from a csv file*/
    void loadAvgExpressionFromFile();

    /** populate the avg expr values to spatil domain */
    DataMatrix populateAvgExprToSpatial();

    /** compute the mean coordinates by each annotation label in the floodfill - only for 3D data */
    void computeMeanCoordinatesByCluster(std::vector<float>& xAvg, std::vector<float>& yAvg, std::vector<float>& zAvg);

    /** compute the mean floodfill wave numbers by each annotation label in the floodfill*/
    void computeMeanWaveNumbersByCluster(std::vector<float>& waveAvg);

private:

    DataStorage                        _dataStore;

    ChartWidget*                       _chartWidget;             // WebWidget that sets up the HTML page - bar chart
    MyTableWidget*                     _tableWidget;             // Customized table widget for enrichment analysis
    DropWidget*                        _dropWidget;              // Widget for drag and drop behavior

    // Data
    mv::Dataset<Points>                _positionDataset;         // Smart pointer to points dataset for point position
    mv::Dataset<Points>                _positionSourceDataset;   // Smart pointer to source of the points dataset for point position (if any)
    std::vector<mv::Vector2f>          _positions;               // Point positions - if 3D, _positions is the 2D projection of the 3D data
    int32_t                            _numPoints;               // Number of point positions
    std::vector<QString>               _enabledDimNames;
    bool                               _dataInitialized = false;

    // FloodFill subset computing
    std::vector<bool>                  _isFloodIndex;            // Direct mapping for flood indices
    mv::Dataset<Points>                _floodFillDataset;        // Dataset for flood fill
    std::vector<int>                   _sortedFloodIndices;      // Spatially sorted indices of flood fill at the current cursor position
    std::vector<int>                   _sortedWaveNumbers;       // Spatially sorted wave numbers of flood fill at the current cursor position
    Eigen::MatrixXf                    _subsetData;              // Subset of flooded data, sorted spatially
    DataSubset                         _computeSubset;             // Flood subset computing

    // Filtering genes based on correlation
    std::vector<float>                 _corrGeneVector;          // Vector of correlation values for filtering genes
    int                                _numGenesThreshold = 50;
    corrFilter::CorrFilter             _corrFilter;
    QLabel*                            _filterLabel;             // Label for filtering genes on the bar chart

    // Clustering
    int                                _nclust;                  // Number of clusters
    std::unordered_map<QString, int>   _dimNameToClusterLabel;   // Map dimension name to cluster label
    std::map<int, int>                 _numGenesInCluster;        // Number of genes in each gene-set cluster

    // Interaction
    int                                _selectedClusterIndex;
    int                                _selectedDimIndex;        // Current selected dimension index for _dimView

    std::vector<std::vector<float>>    _colorScalars;            // Scalars for the color of each scatter view
    Dataset<Points>                    _clusterScalars;          // Scalars for plotting in volume viewer

    // 3D data
    int                                _currentSliceIndex = 0;   // Current slice index for 3D slice dataset
    Dataset<Clusters>                  _sliceDataset;            // Dataset for 3D slices
    std::vector<int>                   _onSliceIndices;          // Pt indices on the current slice
    std::vector<int>                   _onSliceFloodIndices;     // Flood indices on the slice
    std::vector<int>                   _onSliceWaveNumbers;      // Wave numbers on the slice
    std::vector<bool>                  _isFloodOnSlice;          // Direct mapping for flood indices on the slice
    Eigen::MatrixXf                    _subsetData3D;            // Subset of flooded data 3D

    // Enrichment Analysis
    EnrichmentAnalysis*                _client;                  // Enrichment analysis client 
    QVariantList                       _enrichmentResult;        // Cached enrichment analysis result, in case the user clicks on one cell
    QString                            _currentEnrichmentSpecies = "mmusculus"; // current enrichment species

    // Single cell data
    mv::Dataset<Points>                _avgExprDataset;          // Point dataset for average expression of each cluster
    Eigen::MatrixXf                    _avgExpr;                 // Average expression of each cluster
    Eigen::MatrixXf                    _subsetDataAvgOri;        // Subset of average expression of each cluster - NOT weighted 
    std::vector<QString>               _geneNamesAvgExpr;        // From avg expr single cell data    
    bool                               _avgExprDatasetExists = false; // Whether the avg expression dataset exists   

    QMap<QString, QStringList>         _simplifiedToIndexGeneMapping; // Map for simplified gene names to extra indexed gene names - for duplicate gene symbols 
    std::vector<QString>               _clusterNamesAvgExpr;     // From avg expr single cell data
    std::unordered_map<QString, int>   _clusterAliasToRowMap;    // Map label (QString) to row index in _avgExpr
    std::vector<QString>               _cellLabels;              // Labels for each point
    std::unordered_map<QString, int>   _countsMap;               // Count distribution of labels WITHIN floodfill
    std::vector<QString>               _clustersToKeep;          // clusters to keep for avg expression - same order as subset row - cluster alias name 
    Eigen::VectorXf                    _countsSubset;            // counts for each label within the subset - same order as subset row
    Eigen::VectorXf                    _countsAll;               // counts for each label within the entire dataset - same order as avgExpr row

    // Flags
    bool                               _isSingleCell = false;    // whether to use avg expression from single cell data
    bool                               _toClearBarchart = false; //whether to clear the bar chart
    bool                               _loadingFromProject = false;
    AvgExpressionStatus                _avgExprStatus = AvgExpressionStatus::NONE;
    QString                            _selectedDimName = "NoneSelected"; // selected dimension name of _selectedDimIndex
    QString                            _currentEnrichmentAPI = "gProfiler"; // current enrichment API, initialized to gProfiler
    bool                               _selectedByFlood = false; // false: selected by floodfill, true: selected by selection

public:
    bool isDataInitialized() { return _dataInitialized; }
    bool isUsingSingleCell() { return _isSingleCell; }

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

public:
    int getSliceIndex() { return _currentSliceIndex; }

    corrFilter::CorrFilter& getCorrFilter() { return _corrFilter; }

public: 
    void setAvgExpressionStatus(AvgExpressionStatus status) { _avgExprStatus = status; }

    void updateEnrichmentAPI();

    void setEnrichmentAPI(QString apiName);

    void setEnrichmentAPIOptions(QStringList options);

    void updateEnrichmentSpecies();


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
    std::vector<ScatterView*>    _scatterViews;           // scatter plots for cluster images
    ScatterView*                 _dimView;                // scatter plot for selected dimensions
    SettingsAction               _settingsAction;         // Settings action
    ColorMap1DAction             _colorMapAction;         // Color map action
    HorizontalToolbarAction      _primaryToolbarAction;   // Horizontal toolbar for primary content 
    HorizontalToolbarAction      _secondaryToolbarAction; // Secondary toolbar for secondary content - for enrichment analysis settings
    HorizontalToolbarAction      _tertiaryToolbarAction;  // Tertiary toolbar for tertiary content - for slice slider
};

/**
 * GeneSurfer plugin factory class
 *
 * Note: Factory does not need to be altered (merely responsible for generating new plugins when requested)
 */
class GeneSurferPluginFactory : public ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "nl.BioVault.GeneSurferPlugin"
                      FILE  "PluginInfo.json")

public:

    /** Default constructor */
    GeneSurferPluginFactory();

    /** Destructor */
    ~GeneSurferPluginFactory() override {}

    /** Creates an instance of the gene surfer plugin */
    ViewPlugin* produce() override;

    /** Returns the data types that are supported by the gene surfer plugin */
    mv::DataTypes supportedDataTypes() const override;

    /**
     * Get plugin trigger actions given \p datasets
     * @param datasets Vector of input datasets
     * @return Vector of plugin trigger actions
     */
    PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
