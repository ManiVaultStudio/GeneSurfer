#include "ExampleViewJSPlugin.h"

#include "ChartWidget.h"
#include "ScatterView.h"

#include <DatasetsMimeData.h>

#include <vector>
#include <random>
#include <set>
#include <unordered_map>

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
// #define EIGEN_DONT_PARALLELIZE // TO DO: is this necessary for controlling parallelization in eigen?

#include "Compute/DataTransformations.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QMimeData>
#include <QDebug>

// for reading hard-coded csv files
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <chrono>

#include <unordered_set> // check if needed



Q_PLUGIN_METADATA(IID "nl.BioVault.ExampleViewJSPlugin")

using namespace mv;

namespace
{
    void normalizeVector(std::vector<float>& v)
    {
        // Store scalars in floodfill dataset
        float scalarMin = std::numeric_limits<float>::max();
        float scalarMax = -std::numeric_limits<float>::max();

        // Compute min and max of scalars
        for (int i = 0; i < v.size(); i++)
        {
            if (v[i] < scalarMin) scalarMin = v[i];
            if (v[i] > scalarMax) scalarMax = v[i];
        }
        float scalarRange = scalarMax - scalarMin;

        if (scalarRange != 0)
        {
            float invScalarRange = 1.0f / (scalarMax - scalarMin);
            // Normalize the scalars
#pragma omp parallel for
            for (int i = 0; i < v.size(); i++)
            {
                v[i] = (v[i] - scalarMin) * invScalarRange;
            }
        }
    }
}

ExampleViewJSPlugin::ExampleViewJSPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _nclust(3),
    _positionDataset(),
    _scatterViews(6 + 1, nullptr),// TO DO: hard code max 6 scatterViews
    _dimView(nullptr),
    _positions(),
    _positionSourceDataset(),
    _numPoints(0),
    _subsetData(),
    _corrGeneWave(),
    _corrThreshold(0.15f),
    _dimNameToClusterLabel(),
    _colorScalars(_nclust, std::vector<float>(_numPoints, 0.0f)),
    _chartWidget(nullptr),
    _tableWidget(nullptr),
    _client(nullptr),
    _dropWidget(nullptr),
    _settingsAction(this, "Settings Action"),
    _primaryToolbarAction(this, "PrimaryToolbar"),
    _currentDataSet(nullptr), // TO DO: Not used anymore
    _selectedDimIndex(0),
    _selectedClusterIndex(0),
    _colorMapAction(this, "Color map", "RdYlBu")

{
    _primaryToolbarAction.addAction(&_settingsAction.getPositionAction(), -1, GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_settingsAction.getPointPlotAction(), -1, GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_settingsAction.getClusteringAction(), 0, GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_settingsAction.getDimensionAction(), 0, GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_settingsAction.getCorrelationModeAction(), 0, GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_settingsAction.getSingleCellModeAction(), 0, GroupAction::Horizontal);

    for (int i = 0; i < 6; i++)//TO DO: hard code max 6 scatterViews
    {
        _scatterViews[i] = new ScatterView();
    }

    _dimView = new ScatterView();

    for (int i = 0; i < 6; i++) {//TO DO: hard code max 6 scatterViews
        connect(_scatterViews[i], &ScatterView::initialized, this, [this, i]() {_scatterViews[i]->setColorMap(_colorMapAction.getColorMapImage().mirrored(false, true)); });
        connect(_scatterViews[i], &ScatterView::viewSelected, this, [this, i]() { _selectedClusterIndex = i; updateClick(); });
    }

    connect(_dimView, &ScatterView::initialized, this, [this]() {_dimView->setColorMap(_colorMapAction.getColorMapImage().mirrored(false, true)); });
    connect(_dimView, &ScatterView::viewSelected, this, [this]() { _selectedClusterIndex = 6; updateClick(); });   
}

void ExampleViewJSPlugin::init()
{
    //getWidget().setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    // Create layout
    auto layout = new QVBoxLayout();

    auto chartTableDimLayout = new QHBoxLayout();
    auto tableDimLayout = new QVBoxLayout();

    layout->setContentsMargins(0, 0, 0, 0);

    //test
    layout->addWidget(_primaryToolbarAction.createWidget(&getWidget()));

    // Create barchart widget and set html contents of webpage 
    _chartWidget = new ChartWidget(this);
    _chartWidget->setPage(":example_chart/bar_chart.html", "qrc:/example_chart/");

    _tableWidget = new QTableWidget();

    tableDimLayout->addWidget(_tableWidget, 50);
    tableDimLayout->addWidget(_dimView, 50);

    chartTableDimLayout->addWidget(_chartWidget, 60);
    chartTableDimLayout->addLayout(tableDimLayout, 40);

    layout->addLayout(chartTableDimLayout, 1);// stretch factor same as clusterViewLayout

    auto clusterViewMainLayout = new QVBoxLayout();
    clusterViewMainLayout->setContentsMargins(6, 6, 6, 6);

    auto clusterViewRow1 = new QHBoxLayout();
    for (int i = 0; i < 3; i++) {//TO DO: initialize 3 sactter plots
        clusterViewRow1->addWidget(_scatterViews[i], 50);
    }
    clusterViewMainLayout->addLayout(clusterViewRow1);

    auto clusterViewRow2 = new QHBoxLayout();
    for (int i = 3; i < 6; i++) {//TO DO: initialize 3 sactter plots
        clusterViewRow2->addWidget(_scatterViews[i], 50);
    }
    clusterViewMainLayout->addLayout(clusterViewRow2);

    layout->addLayout(clusterViewMainLayout, 1);

    // Apply the layout
    getWidget().setLayout(layout);

    // Instantiate new drop widget: See ExampleViewPlugin for details
    _dropWidget = new DropWidget(_chartWidget);
    _dropWidget->setDropIndicatorWidget(new DropWidget::DropIndicatorWidget(&getWidget(), "No data loaded", "Drag the ExampleViewJSData in this view"));

    _dropWidget->initialize([this](const QMimeData* mimeData) -> DropWidget::DropRegions {

        // A drop widget can contain zero or more drop regions
        DropWidget::DropRegions dropRegions;

        const auto datasetsMimeData = dynamic_cast<const DatasetsMimeData*>(mimeData);


        if (datasetsMimeData == nullptr)
            return dropRegions;

        if (datasetsMimeData->getDatasets().count() > 1)
            return dropRegions;

        const auto dataset = datasetsMimeData->getDatasets().first();
        const auto datasetGuiName = dataset->text();
        const auto datasetId = dataset->getId();
        const auto dataType = dataset->getDataType();
        const auto dataTypes = DataTypes({ PointType, ClusterType });


        //check if the data type can be dropped
        if (!dataTypes.contains(dataType))
            dropRegions << new DropWidget::DropRegion(this, "Incompatible data", "This type of data is not supported", "exclamation-circle", false);

        //Points dataset is about to be dropped
        if (dataType == PointType) {

            if (datasetId == getCurrentDataSetID()) {
                dropRegions << new DropWidget::DropRegion(this, "Warning", "Data already loaded", "exclamation-circle", false);
            }
            else {
                // Get points datset from the core
                auto candidateDataset = mv::data().getDataset<Points>(datasetId);
                qDebug() << " point dataset is dropped";

                // Establish drop region description
                const auto description = QString("Visualize %1 as points ").arg(datasetGuiName);

                if (!_positionDataset.isValid()) {

                    // Load as point positions when no dataset is currently loaded
                    dropRegions << new DropWidget::DropRegion(this, "Point position", description, "map-marker-alt", true, [this, candidateDataset]() {
                        _dataInitialized = false;
                        _positionDataset = candidateDataset;
                        //positionDatasetChanged();
                        _dropWidget->setShowDropIndicator(false);
                        });
                }
                else {
                    if (_positionDataset != candidateDataset && candidateDataset->getNumDimensions() >= 2) {

                        // The number of points is equal, so offer the option to replace the existing points dataset
                        dropRegions << new DropWidget::DropRegion(this, "Point position", description, "map-marker-alt", true, [this, candidateDataset]() {
                            _dataInitialized = false;
                            _positionDataset = candidateDataset;
                            //positionDatasetChanged();
                            });
                    }
                }
            }
        }

        // Cluster dataset is about to be dropped
        if (dataType == ClusterType) {

            // Get clusters dataset from the core
            auto candidateDataset = mv::data().getDataset<Clusters>(datasetId);

            // Establish drop region description
            const auto description = QString("Use %1 as mask clusters").arg(candidateDataset->getGuiName());

            // Only allow user to color by clusters when there is a positions dataset loaded
            if (_positionDataset.isValid())
            {
                // Use the clusters set for points color
                dropRegions << new DropWidget::DropRegion(this, "Mask", description, "palette", true, [this, candidateDataset]()
                    {
                        _sliceDataset = candidateDataset;
                        updateSlice();
                    });
            }
            else {

                // Only allow user to color by clusters when there is a positions dataset loaded
                dropRegions << new DropWidget::DropRegion(this, "No points data loaded", "Clusters can only be visualized in concert with points data", "exclamation-circle", false);
            }
        }
        return dropRegions;

        });

    // Load points when the pointer to the position dataset changes
    connect(&_positionDataset, &Dataset<Points>::changed, this, &ExampleViewJSPlugin::positionDatasetChanged);

    // update data when data set changed
    //connect(&_positionDataset, &Dataset<Points>::dataChanged, this, &ExampleViewJSPlugin::convertDataAndUpdateChart);

    // Update the selection from JS
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passSelectionToCore, this, &ExampleViewJSPlugin::publishSelection);

    // Update point selection when the position dataset data changes
    //connect(&_positionDataset, &Dataset<Points>::dataSelectionChanged, this, &ExampleViewJSPlugin::updateSelection);// TO DO

    connect(&_floodFillDataset, &Dataset<Points>::dataChanged, this, &ExampleViewJSPlugin::updateFloodFill);

    _client = new EnrichmentAnalysis(this);
    connect(_client, &EnrichmentAnalysis::enrichmentDataReady, this, &ExampleViewJSPlugin::updateEnrichmentTable);
    connect(_client, &EnrichmentAnalysis::enrichmentDataNotExists, this, &ExampleViewJSPlugin::noDataEnrichmentTable);

    connect(_tableWidget, &QTableWidget::cellClicked, this, &ExampleViewJSPlugin::onTableClicked); // on table clicked

}

void ExampleViewJSPlugin::loadData(const mv::Datasets& datasets)
{
    // Exit if there is nothing to load
    if (datasets.isEmpty())
        return;

    // Load the first dataset
    _positionDataset = datasets.first();
}

void ExampleViewJSPlugin::positionDatasetChanged()
{
    if (!_positionDataset.isValid())
        return;

    qDebug() << "ExampleViewJSPlugin::positionDatasetChanged(): New data dropped";

    _dropWidget->setShowDropIndicator(!_positionDataset.isValid());

    _positionSourceDataset = _positionDataset->getSourceDataset<Points>();

    _numPoints = _positionDataset->getNumPoints();

    // Get enabled dimension names
    const auto& dimNames = _positionSourceDataset->getDimensionNames();
    auto enabledDimensions = _positionSourceDataset->getDimensionsPickerAction().getEnabledDimensions();

    _enabledDimNames.clear();
    for (int i = 0; i < enabledDimensions.size(); i++)
    {
        if (enabledDimensions[i])
            _enabledDimNames.push_back(dimNames[i]);
    }

    qDebug() << "ExampleViewJSPlugin::positionDatasetChanged(): enabledDimensions size: " << _enabledDimNames.size();

    if (!_clusterScalars.isValid())
    {
        if (!_loadingFromProject)
        {
            _clusterScalars = mv::data().createDataset<Points>("Points", "Clusters");
            events().notifyDatasetAdded(_clusterScalars);
            // initialize the clusterScalars dataset with 0s
            std::vector<float> tempInit(_numPoints, 0.0f);
            _clusterScalars->setData(tempInit.data(), tempInit.size(), 1);
        }
    }

    //if (_avgExprDataset.isValid())
    //{
    //    if (_loadingFromProject)
    //        convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);
    //}
   

    qDebug() << "ExampleViewJSPlugin::positionDatasetChanged(): start converting dataset ... ";
    convertToEigenMatrix(_positionDataset, _positionSourceDataset, _dataStore.getBaseData());
    convertToEigenMatrixProjection(_positionDataset, _dataStore.getBaseFullProjection());

    standardizeData(_dataStore.getBaseData(), _dataStore.getVariances()); // getBaseData() is standardized here TO DO: temporarily disabled
    //normalizeDataEigen(_dataStore.getBaseData(), _dataStore.getBaseNormalizedData());TO DO: getBaseData() or getBaseNormalizedData()
    qDebug() << "ExampleViewJSPlugin::positionDatasetChanged(): finish converting dataset ... ";

    _dataStore.createDataView();
    updateSelectedDim();

    updateFloodFillDataset();

    _dataInitialized = true;
}

void ExampleViewJSPlugin::convertDataAndUpdateChart()
{
    if (!_positionDataset.isValid()) {
        qDebug() << "ExampleViewJSPlugin::convertDataAndUpdateChart: No data to convert";
        return;
    }

    if (_toClearBarchart) {
        qDebug() << "ExampleViewJSPlugin::convertDataAndUpdateChart: Clear barchart";
        return;
    }

    // set colors for clustering labels
    std::vector<QString> plotlyT10Palette = {
    "#4C78A8", // blue
    "#72B7B2", // cyan
    "#FF9DA6", // pink
    "#EECA3B", // yellow
    "#54A24B", // green 
    "#E45756", // red
    "#B279A2", // purple
    "#F58518", // orange   
    "#9D755D"  // brown
    };

    // only process genes present in _dimNameToClusterLabel
    std::vector<std::pair<QString, float>> filteredAndSortedGenes;

    const auto& corrVector = _isCorrSpatial ? _corrGeneSpatial : _corrGeneWave;
    for (size_t i = 0; i < _enabledDimNames.size(); ++i) {
        if (_dimNameToClusterLabel.find(_enabledDimNames[i]) != _dimNameToClusterLabel.end()) {
            filteredAndSortedGenes.emplace_back(_enabledDimNames[i], corrVector[i]);
        }
    }

    // Sort the filtered genes by their correlation values
    std::sort(filteredAndSortedGenes.begin(), filteredAndSortedGenes.end(),
         [](const std::pair<QString, float>& a, const std::pair<QString, float>& b) {
                return a.second < b.second;
         });

    // convert data to a JSON structure
    QVariantList payload;
    for (const auto& genePair : filteredAndSortedGenes) {
        QVariantMap entry;
        entry["Gene"] = genePair.first;
        entry["Value"] = genePair.second;

        int clusterLabel = _dimNameToClusterLabel[genePair.first];
        entry["categoryColor"] = plotlyT10Palette[clusterLabel % plotlyT10Palette.size()];
        entry["cluster"] = "Gene cluster " + QString::number(clusterLabel);

        payload.push_back(entry);
    }

    qDebug() << "ExampleViewJSPlugin::convertDataAndUpdateChart: Send data from Qt cpp to D3 js";
    emit _chartWidget->getCommunicationObject().qt_js_setDataAndPlotInJS(payload);
}

void ExampleViewJSPlugin::publishSelection(const QString& selection)
{
    updateDimView(selection);

    qDebug() << "ExampleViewJSPlugin::publishSelection: selection: " << selection;
}

QString ExampleViewJSPlugin::getCurrentDataSetID() const
{
    if (_currentDataSet.isValid())
        return _currentDataSet->getId();
    else
        return QString{};
}

void ExampleViewJSPlugin::updateFloodFillDataset()
{
    bool floodFillDatasetFound = false;

    // read floodFillData from data hierarchy
    for (const auto& data : mv::data().getAllDatasets())
    {
        if (data->getGuiName() == "allFloodNodesIndices") {
            _floodFillDataset = data;
            floodFillDatasetFound = true;
            break;
        }
    }

    if (!floodFillDatasetFound) {
        qDebug() << "Warning: No floodFillDataset named allFloodNodesIndices found!";
        return;
    }

    qDebug() << "ExampleViewJSPlugin::updateFloodFillDataset: dataSets name: " << _floodFillDataset->text();
    qDebug() << "ExampleViewJSPlugin::updateFloodFillDataset: dataSets size: " << _floodFillDataset->getNumPoints();
     
    updateFloodFill();
}

void ExampleViewJSPlugin::updateSelectedDim() {
    int xDim = _settingsAction.getPositionAction().getXDimensionPickerAction().getCurrentDimensionIndex();
    int yDim = _settingsAction.getPositionAction().getYDimensionPickerAction().getCurrentDimensionIndex();

    //qDebug() << "ExampleViewJSPlugin::updateSelectedDim(): xDim: " << xDim << " yDim: " << yDim;

    _dataStore.createProjectionView(xDim, yDim);

    //qDebug()<< "ExampleViewJSPlugin::updateSelectedDim(): getProjectionSize()"<< _dataStore.getProjectionSize();

    _positions.clear();
    _positions.resize(_dataStore.getProjectionView().rows());

    for (int i = 0; i < _dataStore.getProjectionView().rows(); i++) {
        _positions[i].set(_dataStore.getProjectionView()(i, 0), _dataStore.getProjectionView()(i, 1));
    }

    //qDebug() << "ExampleViewJSPlugin::updateSelectedDim(): positions size: " << positions.size();

    updateViewData(_positions);
}

void ExampleViewJSPlugin::updateViewData(std::vector<Vector2f>& positions) {

    // TO DO: can save some time here only computing data bounds once
    // pass the 2d points to the scatter plot widget
    for (int i = 0; i < _nclust; i++) {// TO DO: hard code max 6 scatterViews
        _scatterViews[i]->setData(&positions);
    }

    _dimView->setData(&positions);

}

void ExampleViewJSPlugin::updateShowDimension() {
    int shownDimension = _settingsAction.getDimensionAction().getCurrentDimensionIndex();
    qDebug() << "ExampleViewJSPlugin::updateShowDimension(): shownDimension: " << shownDimension;

    if (shownDimension < 0) {
        return;
    }

    qDebug() << "ExampleViewJSPlugin::updateShowDimension(): shownDimension >= 0: " << shownDimension;

    //QString dimName = _enabledDimNames[shownDimension]; // align with _enabledDimNames

    const std::vector<QString> dimNamesSource = _isSingleCell ? _geneNamesAvgExpr : _positionSourceDataset->getDimensionNames();// align with positionSourceDataset, NOT _enabledDimNames

    QString dimName = dimNamesSource[shownDimension];
    qDebug() << "ExampleViewJSPlugin::updateShowDimension(): dimName: " << dimName;

    updateDimView(dimName);
}

void ExampleViewJSPlugin::computeAvgExpression() {
    qDebug() << "computeAvgExpression() started ";

    // Attention: data used below should all from a singlecell dataset

    Dataset<Clusters> scLabelDataset = _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentDataset<Clusters>();
    QVector<Cluster> labelClusters = scLabelDataset->getClusters();
  
    if (!scLabelDataset->getParent().isValid())
    {
        qDebug() << "ERROR: No valid source data for the selected label dataset!";
        return;
    }

    Dataset<Points> scSourceDataset = scLabelDataset->getParent()->getSourceDataset<Points>();
    
    qDebug() << "ExampleViewJSPlugin::computeAvgExpression(): scSourceDataset name: " << scSourceDataset->getGuiName();
    qDebug() << "ExampleViewJSPlugin::computeAvgExpression(): scSourceDataset numPoints: " << scSourceDataset->getNumPoints();
    
    int numPoints = scSourceDataset->getNumPoints();
    int numClusters = labelClusters.size();
    int numGenes = scSourceDataset->getNumDimensions();
    qDebug() << "ExampleViewJSPlugin::computeAvgExpression(): numPoints: " << numPoints << " numClusters: " << numClusters << " numGenes: " << numGenes;

    Eigen::MatrixXf scSourceMatrix;
    convertToEigenMatrixProjection(scSourceDataset, scSourceMatrix);
    qDebug() << "ExampleViewJSPlugin::computeAvgExpression(): scSourceMatrix size: " << scSourceMatrix.rows() << " " << scSourceMatrix.cols();

    std::vector<float> scCellLabels(numPoints, 0);

    // Mapping from cluster name to list of cell indices
    std::map<QString, std::vector<int>> clusterToIndicesMap;

    for (int i = 0; i < numClusters; ++i) {
        QString clusterName = labelClusters[i].getName(); 
        const auto& ptIndices = labelClusters[i].getIndices();
        for (int ptIndex : ptIndices) {
            scCellLabels[ptIndex] = i; // Use cluster index instead of name for numerical computations
            clusterToIndicesMap[clusterName].push_back(ptIndex);
        }
    }
    qDebug() << "ExampleViewJSPlugin::computeAvgExpression(): clusterToIndicesMap size: " << clusterToIndicesMap.size();

    _avgExpr.resize(numClusters, numGenes);

    // Compute the average expression for each cluster

    for (const auto& cluster : clusterToIndicesMap) {
        const auto& indices = cluster.second;

        Eigen::MatrixXf clusterExpr(indices.size(), scSourceMatrix.cols());
        #pragma omp parallel for
        for (int i = 0; i < indices.size(); ++i) {
            clusterExpr.row(i) = scSourceMatrix.row(indices[i]);
        }

        Eigen::VectorXf clusterMean = clusterExpr.colwise().mean();
        int clusterIndex = std::distance(clusterToIndicesMap.begin(), clusterToIndicesMap.find(cluster.first));
        _avgExpr.row(clusterIndex) = clusterMean;
    }

    qDebug() << "ExampleViewJSPlugin::computeAvgExpression(): _avgExpr size: " << _avgExpr.rows() << " " << _avgExpr.cols();
    
    // Flatten the Eigen::MatrixXf data to a std::vector<float>
    std::vector<float> allData(numClusters * numGenes);
    for (int i = 0; i < numClusters; ++i) {
        for (int j = 0; j < numGenes; ++j) {
            allData[i * numGenes + j] = _avgExpr(i, j);
        }
    }

    // get the gene names and cluster names
    _geneNamesAvgExpr.clear();
    _geneNamesAvgExpr = scSourceDataset->getDimensionNames();

    _clusterNamesAvgExpr.clear();
    for (const auto& cluster : clusterToIndicesMap) {
        _clusterNamesAvgExpr.push_back(cluster.first);
    }

    qDebug() << "ExampleViewJSPlugin::computeAvgExpression(): _geneNamesAvgExpr size: " << _geneNamesAvgExpr.size();
    qDebug() << "ExampleViewJSPlugin::computeAvgExpression(): _clusterNamesAvgExpr size: " << _clusterNamesAvgExpr.size();

    _clusterAliasToRowMap.clear();// _clusterAliasToRowMap: first element is label name, second element is row index in _avgExpr
    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
    }


    // Create and store the dataset
    if (!_avgExprDataset.isValid()) {
        qDebug() << "avgExprDataset not valid";
        _avgExprDataset = mv::data().createDataset<Points>("Points", "avgExprDataset");
        events().notifyDatasetAdded(_avgExprDataset);
    }
    //else {
    //    _avgExprDataset.reset();// TO DO: 
    //}     
    _avgExprDataset->setData(allData.data(), numClusters, numGenes); 
    _avgExprDataset->setDimensionNames(_geneNamesAvgExpr);
    events().notifyDatasetDataChanged(_avgExprDataset);

    _avgExprDatasetExists = true;
    _settingsAction.getSingleCellModeAction().getSingleCellOptionAction().setEnabled(_avgExprDatasetExists);

    loadLabelsFromSTDataset();

    qDebug() << "computeAvgExpression() finished ";

}

void ExampleViewJSPlugin::loadAvgExpression() {
    qDebug() << "ONLY FOR ABCAtlas";

    loadAvgExpressionABCAtlas();// TO DO: only for ABC Atlas
    qDebug() << "1";
    loadLabelsFromSTDatasetABCAtlas();
    qDebug() << "2";

    _avgExprDatasetExists = true;

    // enable single cell toggle - TO DO: use signal or set directly
    //emit avgExprDatasetExistsChanged(_avgExprDatasetExists);
    _settingsAction.getSingleCellModeAction().getSingleCellOptionAction().setEnabled(_avgExprDatasetExists);

    qDebug() << "load AvgExpression finished ";

}

void ExampleViewJSPlugin::loadLabelsFromSTDataset() {
    // this is loading label from ST dataset!!!
    // Different from loading from singlecell datset!!!

    QString stParentName = _positionDataset->getParent()->getGuiName();
    qDebug() << "ExampleViewJSPlugin::loadLabelsFromSTDataset(): stParentName: " << stParentName;

    QString selectedDataName = _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentText();
    qDebug() << "ExampleViewJSPlugin::loadLabelsFromSTDataset(): selectedDataName: " << selectedDataName;

    Dataset<Clusters> labelDataset;
    
    for (const auto& data : mv::data().getAllDatasets())
    {
        //qDebug() << data->getGuiName();
        if (data->getGuiName() == selectedDataName) {
            qDebug() << "data->getParent()->getGuiName() " << data->getParent()->getGuiName();
            if (data->getParent()->getGuiName() == stParentName) {
                labelDataset = data;
                qDebug() << "ExampleViewJSPlugin::loadLabelsFromSTDataset(): labelDataset name: " << labelDataset->getGuiName();
                break;
            }
        }
    }

    QVector<Cluster> labelClusters = labelDataset->getClusters();

    qDebug() << "ExampleViewJSPlugin::loadLabelsFromSTDataset(): labelClusters size: " << labelClusters.size();

    // precompute the cell-label array
    _cellLabels.clear();
    _cellLabels.resize(_numPoints);

    for (int i = 0; i < labelClusters.size(); ++i) {
        QString clusterName = labelClusters[i].getName();
        const auto& ptIndices = labelClusters[i].getIndices();
        for (int j = 0; j < ptIndices.size(); ++j) {
            int ptIndex = ptIndices[j];
            _cellLabels[ptIndex] = clusterName;
        }
    }

    qDebug() << "ExampleViewJSPlugin::loadLabelsFromSTDataset(): _cellLabels size: " << _cellLabels.size();
}


void ExampleViewJSPlugin::setLabelDataset() {
    qDebug() << _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentText();

    // check if there are two datasets with the same selected name
    QString selectedDataName = _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentText();

    int count = 0;
    for (const auto& data : mv::data().getAllDatasets())
    {
        if (data->getGuiName() == selectedDataName) {
            count++;
        }
    }

    if (count == 2) {
        qDebug() << "There are " << count << " datasets with the name '" << selectedDataName << "'.";
    }
    else {
        qDebug() << "There are " << count << " datasets with the name '" << selectedDataName << "'.";
        return;
    }

    _settingsAction.getSingleCellModeAction().getComputeAvgExpressionAction().setEnabled(true);

}

void ExampleViewJSPlugin::computeCorrSpatial() {
    if (_isFloodIndex.empty()) {
        qDebug() << "ExampleViewJSPlugin::computeCorrWave(): _isFloodIndex is empty";
        return;
    }

    if (!_sliceDataset.isValid()) {
        // 2D dataset 
        std::vector<float> xCoords, yCoords;
        for (int index : _sortedFloodIndices) {
            xCoords.push_back(_positions[index].x);
            yCoords.push_back(_positions[index].y);
        }

        Eigen::VectorXf xVector = Eigen::Map<Eigen::VectorXf>(xCoords.data(), xCoords.size());
        Eigen::VectorXf yVector = Eigen::Map<Eigen::VectorXf>(yCoords.data(), yCoords.size());

        // Precompute means and norms for xVector, yVector
        float meanX = xVector.mean(), meanY = yVector.mean();
        Eigen::VectorXf centeredX = xVector - Eigen::VectorXf::Constant(xVector.size(), meanX);
        Eigen::VectorXf centeredY = yVector - Eigen::VectorXf::Constant(yVector.size(), meanY);
        float normX = centeredX.squaredNorm(), normY = centeredY.squaredNorm();

        std::vector<float> correlationsX(_subsetData.cols());
        std::vector<float> correlationsY(_subsetData.cols());
        _corrGeneSpatial.clear();
        _corrGeneSpatial.resize(_subsetData.cols());

#pragma omp parallel for
        for (int i = 0; i < _subsetData.cols(); ++i) {
            Eigen::VectorXf column = _subsetData.col(i);
            float meanColumn = column.mean();
            Eigen::VectorXf centeredColumn = column - Eigen::VectorXf::Constant(column.size(), meanColumn);
            float normColumn = centeredColumn.squaredNorm();

            float correlationX = centeredColumn.dot(centeredX) / std::sqrt(normColumn * normX);
            float correlationY = centeredColumn.dot(centeredY) / std::sqrt(normColumn * normY);

            if (std::isnan(correlationX)) { correlationX = 0.0f; }
            if (std::isnan(correlationY)) { correlationY = 0.0f; }
            correlationsX[i] = correlationX;
            correlationsY[i] = correlationY;
            _corrGeneSpatial[i] = std::abs(correlationX) + std::abs(correlationY);
        }
    }
    else {
        // 3D dataset
        std::vector<float> zPositions;
        _positionDataset->extractDataForDimension(zPositions, 2);

        std::vector<float> xCoords, yCoords, zCoords;
        for (int index : _sortedFloodIndices) {
            xCoords.push_back(_positions[index].x);
            yCoords.push_back(_positions[index].y);
            zCoords.push_back(zPositions[index]);
        }

        Eigen::VectorXf xVector = Eigen::Map<Eigen::VectorXf>(xCoords.data(), xCoords.size());
        Eigen::VectorXf yVector = Eigen::Map<Eigen::VectorXf>(yCoords.data(), yCoords.size());
        Eigen::VectorXf zVector = Eigen::Map<Eigen::VectorXf>(zCoords.data(), zCoords.size());

        // Precompute means and norms for xVector, yVector, zVector
        float meanX = xVector.mean(), meanY = yVector.mean(), meanZ = zVector.mean();
        Eigen::VectorXf centeredX = xVector - Eigen::VectorXf::Constant(xVector.size(), meanX);
        Eigen::VectorXf centeredY = yVector - Eigen::VectorXf::Constant(yVector.size(), meanY);
        Eigen::VectorXf centeredZ = zVector - Eigen::VectorXf::Constant(zVector.size(), meanZ);
        float normX = centeredX.squaredNorm(), normY = centeredY.squaredNorm(), normZ = centeredZ.squaredNorm();

        std::vector<float> correlationsX(_subsetData3D.cols());
        std::vector<float> correlationsY(_subsetData3D.cols());
        std::vector<float> correlationsZ(_subsetData3D.cols());
        _corrGeneSpatial.clear();
        _corrGeneSpatial.resize(_subsetData3D.cols());

#pragma omp parallel for
        for (int i = 0; i < _subsetData3D.cols(); ++i) {
            Eigen::VectorXf column = _subsetData3D.col(i);
            float meanColumn = column.mean();
            Eigen::VectorXf centeredColumn = column - Eigen::VectorXf::Constant(column.size(), meanColumn);
            float normColumn = centeredColumn.squaredNorm();

            float correlationX = centeredColumn.dot(centeredX) / std::sqrt(normColumn * normX);
            float correlationY = centeredColumn.dot(centeredY) / std::sqrt(normColumn * normY);
            float correlationZ = centeredColumn.dot(centeredZ) / std::sqrt(normColumn * normZ);

            if (std::isnan(correlationX)) { correlationX = 0.0f; }
            if (std::isnan(correlationY)) { correlationY = 0.0f; }
            if (std::isnan(correlationZ)) { correlationZ = 0.0f; }
            correlationsX[i] = correlationX;
            correlationsY[i] = correlationY;
            correlationsZ[i] = correlationZ;
            _corrGeneSpatial[i] = std::abs(correlationX) + std::abs(correlationY) + std::abs(correlationZ);
        }

    }
}

void ExampleViewJSPlugin::updateSelection()
{
    //TO DO: seperate plotting part and computation part, only update plotting part when plotting settings are changed

    _tableWidget->clearContents(); // clear table content

    if (!_positionDataset.isValid())
        return;

    if (_isFloodIndex.empty()) {
        qDebug() << "ExampleViewJSPlugin::updateSelection(): _isFloodIndex is empty";
        return;
    }

    qDebug() << "ExampleViewJSPlugin::updateSelection(): start... ";
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < _nclust; i++) {
        QString clusterIdx = QString::number(i);
        _scatterViews[i]->setProjectionName("Gene cluster: " + clusterIdx); // TO DO: remove redundant code
    }

    // compute subset and correlation
    if (_isSingleCell != true) {
        
        if (!_sliceDataset.isValid()) {
            computeSubsetData(_dataStore.getBaseData(), _sortedFloodIndices, _subsetData); // TO DO: getBaseData() or getBaseNormalizedData()      
        }
        else {
            // 3D dataset

            //subset data only contains the onSliceFloodIndice
            auto start1 = std::chrono::high_resolution_clock::now();
            computeSubsetData(_dataStore.getBaseData(), _onSliceFloodIndices, _subsetData);// TO DO: getBaseData() or getBaseNormalizedData()
            auto end1 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed1 = end1 - start1;
            std::cout << "computeSubsetData() on slice Elapsed time: " << elapsed1.count() << " ms\n";

            //subset data contains all floodfill indices
            auto start2 = std::chrono::high_resolution_clock::now();
            computeSubsetData(_dataStore.getBaseData(), _sortedFloodIndices, _subsetData3D);
            auto end2 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed2 = end2 - start2;
            std::cout << "computeSubsetData() 3D Elapsed time: " << elapsed2.count() << " ms\n";
        }

        if (_isCorrSpatial) {
            auto start3 = std::chrono::high_resolution_clock::now();
            computeCorrSpatial();
            auto end3 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed3 = end3 - start3;
            std::cout << "computeCorrSpatial() Elapsed time: " << elapsed3.count() << " ms\n";
        }
        else {
            auto start3 = std::chrono::high_resolution_clock::now();
            computeCorrWave();
            auto end3 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed3 = end3 - start3;
            std::cout << "computeCorrWave() Elapsed time: " << elapsed3.count() << " ms\n";
        }
    } else {
        // single cell option
        if (!_sliceDataset.isValid()) {
            // single cell + 2D
            // To DO: needs to be reorganised - avoid redundant code
            countLabelDistribution();
            computeAvgExprSubset();

            _subsetData.resize(_subsetDataAvgOri.rows(), _subsetDataAvgOri.cols());
            _subsetData = _subsetDataAvgOri; // row for clusters, col for genes
            qDebug() << "ExampleViewJSPlugin::updateSelection(): _subsetData size: " << _subsetData.rows() << " " << _subsetData.cols();

            // compute corrWave here - test temp code
            int numEnabledDims = _subsetData.cols(); // genes
            Eigen::VectorXf eigenWaveNumbers(_waveAvg.size()); // to do: should check if _sortedWaveNumbers is empty
            std::copy(_waveAvg.begin(), _waveAvg.end(), eigenWaveNumbers.data());
            qDebug() << "ExampleViewJSPlugin::updateSelection(): eigenWaveNumbers size: " << eigenWaveNumbers.size();

            Eigen::VectorXf centeredWaveNumbers = eigenWaveNumbers - Eigen::VectorXf::Constant(eigenWaveNumbers.size(), eigenWaveNumbers.mean());
            float waveNumbersNorm = centeredWaveNumbers.squaredNorm();
            qDebug() << "ExampleViewJSPlugin::updateSelection(): waveNumbersNorm: " << waveNumbersNorm;

            _corrGeneWave.clear();
            _corrGeneWave.resize(numEnabledDims);
#pragma omp parallel for
            for (int col = 0; col < numEnabledDims; ++col) {
                Eigen::VectorXf centered = _subsetData.col(col) - Eigen::VectorXf::Constant(_subsetData.col(col).size(), _subsetData.col(col).mean());
                float correlation = centered.dot(centeredWaveNumbers) / std::sqrt(centered.squaredNorm() * waveNumbersNorm);
                if (std::isnan(correlation)) { correlation = 0.0f; }
                _corrGeneWave[col] = correlation;
            }
            qDebug() << "ExampleViewJSPlugin::updateSelection(): _corrGeneWave size: " << _corrGeneWave.size();

            // output corrWave
            /*for (float value : _corrGeneWave) {
                std::cout << value << " ";
            }
            std::cout << std::endl;*/

            // output mean and stdDev of corrWave
            float mean = std::accumulate(_corrGeneWave.begin(), _corrGeneWave.end(), 0.0f) / _corrGeneWave.size();
            float sumOfSquares = 0.0f;
            for (float value : _corrGeneWave) {
                sumOfSquares += (value - mean) * (value - mean);
            }
            float stdDev = std::sqrt(sumOfSquares / _corrGeneWave.size());
            qDebug() << "ExampleViewJSPlugin::updateSelection(): corrWave mean: " << mean << " stdDev: " << stdDev;

        }
        else {
            // single cell + 3D
            // temporal code for corrWave - single cell option
            // test temp code only for ABC Atlas
            countLabelDistribution();
            computeAvgExprSubset();

            _subsetData3D.resize(_subsetDataAvgOri.rows(), _subsetDataAvgOri.cols());
            _subsetData3D = _subsetDataAvgOri; // row for clusters, col for genes
            qDebug() << "ExampleViewJSPlugin::updateSelection(): _subsetData3D size: " << _subsetData3D.rows() << " " << _subsetData3D.cols();

            // compute corrWave here - test temp code
            int numEnabledDims = _subsetData3D.cols(); // genes
            Eigen::VectorXf eigenWaveNumbers(_waveAvg.size()); // to do: should check if _sortedWaveNumbers is empty
            std::copy(_waveAvg.begin(), _waveAvg.end(), eigenWaveNumbers.data());
            qDebug() << "ExampleViewJSPlugin::updateSelection(): eigenWaveNumbers size: " << eigenWaveNumbers.size();

            Eigen::VectorXf centeredWaveNumbers = eigenWaveNumbers - Eigen::VectorXf::Constant(eigenWaveNumbers.size(), eigenWaveNumbers.mean());
            float waveNumbersNorm = centeredWaveNumbers.squaredNorm();
            qDebug() << "ExampleViewJSPlugin::updateSelection(): waveNumbersNorm: " << waveNumbersNorm;

            _corrGeneWave.clear();
            _corrGeneWave.resize(numEnabledDims);
#pragma omp parallel for
            for (int col = 0; col < numEnabledDims; ++col) {
                Eigen::VectorXf centered = _subsetData3D.col(col) - Eigen::VectorXf::Constant(_subsetData3D.col(col).size(), _subsetData3D.col(col).mean());
                float correlation = centered.dot(centeredWaveNumbers) / std::sqrt(centered.squaredNorm() * waveNumbersNorm);
                if (std::isnan(correlation)) { correlation = 0.0f; }
                _corrGeneWave[col] = correlation;
            }
            qDebug() << "ExampleViewJSPlugin::updateSelection(): _corrGeneWave size: " << _corrGeneWave.size();

            // output corrWave
            /*for (float value : _corrGeneWave) {
                std::cout << value << " ";
            }
            std::cout << std::endl;*/

            // output mean and stdDev of corrWave
            float mean = std::accumulate(_corrGeneWave.begin(), _corrGeneWave.end(), 0.0f) / _corrGeneWave.size();
            float sumOfSquares = 0.0f;
            for (float value : _corrGeneWave) {
                sumOfSquares += (value - mean) * (value - mean);
            }
            float stdDev = std::sqrt(sumOfSquares / _corrGeneWave.size());
            qDebug() << "ExampleViewJSPlugin::updateSelection(): corrWave mean: " << mean << " stdDev: " << stdDev;
        }
    }

    clusterGenes();

    qDebug() << "updateSelection(): data clustered";

    auto start4 = std::chrono::high_resolution_clock::now();
    convertDataAndUpdateChart();
    auto end4 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed4 = end4 - start4;
    std::cout << "convertDataAndUpdateChart() Elapsed time: " << elapsed4.count() << " ms\n";


    updateScatterColors();
    updateScatterOpacity();
    updateScatterSelection();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "updateSelection() Elapsed time: " << elapsed.count() << " ms\n";
    qDebug() << "ExampleViewJSPlugin::updateSelection(): end... ";

}

void ExampleViewJSPlugin::setCorrelationMode(bool mode) {
    _isCorrSpatial = mode;
    qDebug() << "ExampleViewJSPlugin::setCorrelationMode(): _isCorrSpatial: " << _isCorrSpatial;

    updateSelection();
}

void ExampleViewJSPlugin::updateSingleCellOption() {
    qDebug() << "ExampleViewJSPlugin::updateSingleCellOption(): start... ";

    _settingsAction.getSingleCellModeAction().getSingleCellOptionAction().isChecked() ? _isSingleCell = true : _isSingleCell = false;
    qDebug() << "ExampleViewJSPlugin::updateSingleCellOption(): _isSingleCell: " << _isSingleCell;

    if (_isSingleCell) {
        qDebug() << "Using Single Cell";

        if (_avgExpr.size() == 0) {
            qDebug() << "ExampleViewJSPlugin::updateSingleCellOption(): _avgExpr is empty";
            qDebug() << "_loadingFromProject = " << _loadingFromProject;

            if (_loadingFromProject) {
                qDebug() << "Loading from project...";
                switch (_avgExprStatus)
                {
                case AvgExpressionStatus::NONE:
                    qDebug() << "Status: NONE";
                    break;
                case AvgExpressionStatus::COMPUTED:
                    qDebug() << "Status: COMPUTED";
                    //computeAvgExpression();

                    // in oder to avoid computing again - TO DO: seperate this part in the function
                    qDebug() << "_avgExprDataset.isValid() = " << _avgExprDataset.isValid();
                    convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);
                    _geneNamesAvgExpr.clear();
                    _geneNamesAvgExpr = _avgExprDataset->getDimensionNames();

                    _clusterAliasToRowMap.clear();
                    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
                        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
                    }
                    qDebug() << "ExampleViewJSPlugin::updateSingleCellOption(): _clusterAliasToRowMap size: " << _clusterAliasToRowMap.size();
                    loadLabelsFromSTDataset();
                    break;
                case AvgExpressionStatus::LOADED:
                    qDebug() << "Status: LOADED";
                    //loadAvgExpression();

                    // in oder to avoid computing again - TO DO: seperate this part in the function
                    qDebug() << "_avgExprDataset.isValid() = " << _avgExprDataset.isValid();
                    convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);
                    _geneNamesAvgExpr.clear();
                    _geneNamesAvgExpr = _avgExprDataset->getDimensionNames();

                    _clusterAliasToRowMap.clear();
                    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
                        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
                    }
                    qDebug() << "ExampleViewJSPlugin::updateSingleCellOption(): _clusterAliasToRowMap size: " << _clusterAliasToRowMap.size();
                    loadLabelsFromSTDatasetABCAtlas();

                    break;
                }
            }
            else {
                return;
            }
            
        } 

        qDebug() << "ExampleViewJSPlugin::updateSingleCellOption(): _avgExpr size: " << _avgExpr.rows() << " " << _avgExpr.cols();
        _settingsAction.getDimensionAction().setPointsDataset(_avgExprDataset);

        // update _enabledDimNames
        _enabledDimNames.clear();
        _enabledDimNames = _geneNamesAvgExpr;
        qDebug() << "ExampleViewJSPlugin::updateSingleCellOption(): _enabledDimNames size: " << _enabledDimNames.size();

        // TO DO: temporary code to avoid too many genes for single cell option
        if (_settingsAction.getClusteringAction().getCorrThresholdAction().getValue() < 0.6)
        {
            _settingsAction.getClusteringAction().getCorrThresholdAction().setValue(0.6);
            qDebug() << "Warning: _corrThreshold is set to 0.6";
        }
        
        updateSelection();

    }
    else {
        qDebug() << "Using Spatial";
        _settingsAction.getDimensionAction().setPointsDataset(_positionSourceDataset);

        const auto& dimNames = _positionSourceDataset->getDimensionNames();
        auto enabledDimensions = _positionSourceDataset->getDimensionsPickerAction().getEnabledDimensions();
        _enabledDimNames.clear();
        for (int i = 0; i < enabledDimensions.size(); i++)
        {
            if (enabledDimensions[i])
                _enabledDimNames.push_back(dimNames[i]);
        }

        updateSelection();
    }
}

void ExampleViewJSPlugin::updateNumCluster()
{
    int newNCluster = _settingsAction.getClusteringAction().getNumClusterAction().getValue();
    if (newNCluster < _nclust) {
        int diff = _nclust - newNCluster;
        for (int i = newNCluster; i < _nclust; i++) {
            _scatterViews[i]->clearData();
        }
    }
    _nclust = newNCluster;

    // cannot be changed before plotting
    if (_isFloodIndex.empty()) {
        qDebug() << "ExampleViewJSPlugin::updateNumCluster(): _isFloodIndex is empty";
        return;
    }

    updateViewData(_positions);

    updateScatterPointSize();
    updateSelection();
}

void ExampleViewJSPlugin::updateCorrThreshold() {
    _corrThreshold = _settingsAction.getClusteringAction().getCorrThresholdAction().getValue();

    // cannot be changed before plotting
    if (_isFloodIndex.empty()) {
        qDebug() << "ExampleViewJSPlugin::updateCorrThreshold: _isFloodIndex is empty";
        return;
    }

    updateSelection();
}

void ExampleViewJSPlugin::updateScatterPointSize()
{
    for (int i = 0; i < _nclust; i++) {
        _scatterViews[i]->setSourcePointSize(_settingsAction.getPointPlotAction().getPointSizeAction().getValue());
    }
    _dimView->setSourcePointSize(_settingsAction.getPointPlotAction().getPointSizeAction().getValue());
}

void ExampleViewJSPlugin::updateScatterSelection()
{
    if (!_positionDataset.isValid())
        return;

    // highlight the current selection
    auto selection = _positionDataset->getSelection<Points>();

    std::vector<bool> selected;
    std::vector<char> highlights;

    _positionDataset->selectedLocalIndices(selection->indices, selected);

    highlights.resize(_numPoints, 0);

    for (int i = 0; i < selected.size(); i++)
        highlights[i] = selected[i] ? 1 : 0;

    for (int i = 0; i < _nclust; i++)
        _scatterViews[i]->setHighlights(highlights, static_cast<std::int32_t>(selection->indices.size()));
    //qDebug() << " ExampleViewJSPlugin::updateScatterSelection(): scatterViews highlighted";

}

void ExampleViewJSPlugin::updateScatterOpacity()
{
    if (!_positionDataset.isValid())
        return;

    if (_isFloodIndex.empty()) {
        qDebug() << "ExampleViewJSPlugin::updateScatterOpacity: _isFloodIndex is empty";
        return;
    }

    if (!_sliceDataset.isValid()) {
        // for 2D dataset
        std::vector<float> opacityScalars(_isFloodIndex.size());
        float defaultOpacity = _settingsAction.getPointPlotAction().getPointOpacityAction().getValue();
#pragma omp parallel for
        for (int i = 0; i < _isFloodIndex.size(); ++i) {
            opacityScalars[i] = _isFloodIndex[i] ? 1.0f : defaultOpacity;
        }

        for (int i = 0; i < _nclust; i++)
        {
            _scatterViews[i]->setPointOpacityScalars(opacityScalars);
        }
        _dimView->setPointOpacityScalars(opacityScalars);
    }
    else {
        // for 3D dataset
        std::vector<float> opacityScalars(_isFloodOnSlice.size());
        float defaultOpacity = _settingsAction.getPointPlotAction().getPointOpacityAction().getValue();
#pragma omp parallel for
        for (int i = 0; i < _isFloodOnSlice.size(); ++i) {
            opacityScalars[i] = _isFloodOnSlice[i] ? 1.0f : defaultOpacity;
        }
        
        for (int i = 0; i < _nclust; i++)
        {
            _scatterViews[i]->setPointOpacityScalars(opacityScalars);
        }
        _dimView->setPointOpacityScalars(opacityScalars);
    }
    qDebug() << "ExampleViewJSPlugin::updateScatterOpacity: finished ...";
}

void ExampleViewJSPlugin::updateScatterColors()
{
    if (!_positionDataset.isValid())
        return;

    // Clear clicked frame - _scatterViews
    if (_selectedClusterIndex >= 0 && _selectedClusterIndex < _nclust) {
        _scatterViews[_selectedClusterIndex]->selectView(false);
        qDebug() << "_selectedClusterIndex" << _selectedClusterIndex;
    }

    std::vector<uint32_t>& selection = _positionDataset->getSelectionIndices();

    if (!_sliceDataset.isValid()) {
        //2D dataset
        for (int i = 0; i < _nclust; i++)
        {
            std::vector<float> dimV = _colorScalars[i];
            _scatterViews[i]->setScalars(dimV, selection[0]);// TO DO: hard-coded the idx of point // selection not working?
        }
    }
    else {
        // 3D dataset

        if (_colorScalars[0].empty()) {
            qDebug() << "ExampleViewJSPlugin::updateScatterColors: _colorScalars[0] is empty";
            return;
        }

        for (int j = 0; j < _nclust; j++) {
            if (j >= _colorScalars.size()) {
                qDebug() << "ExampleViewJSPlugin::updateScatterColors: Row index out of range: j=" << j;
                break;
            }

            std::vector<float> viewScalars(_onSliceIndices.size());
#pragma omp parallel for
            for (int i = 0; i < _onSliceIndices.size(); i++)
            {
                if (_onSliceIndices[i] < _colorScalars[j].size()) {
                    viewScalars[i] = _colorScalars[j][_onSliceIndices[i]];
                }
                else {
                    qDebug() << "Column index out of range: j=" << j << " i=" << i << " _onSliceIndices[i]=" << _onSliceIndices[i];
                    break;
                }
            }
            _scatterViews[j]->setScalars(viewScalars, selection[0]);
        }
    }
}

void ExampleViewJSPlugin::updateDimView(const QString& selectedDimName)
{
    // Clear clicked frame - _dimView
    if (_selectedClusterIndex == 6) {
        _dimView->selectView(false);
        qDebug() << "_selectedClusterIndex" << _selectedClusterIndex;
    }

    QString dimName = selectedDimName;
    _dimView->setProjectionName("Selected: " + selectedDimName);

    Eigen::VectorXf dimValues;
    std::vector<float> dimV;
    if (_isSingleCell != true) {
        const std::vector<QString> dimNames = _positionSourceDataset->getDimensionNames();
        for (int i = 0; i < dimNames.size(); ++i) {
            if (dimNames[i] == selectedDimName) {
                _selectedDimIndex = i;
                break;
            }
        }

        qDebug() << "Access _positionSourceData";
        //dimValues = _dataStore.getBaseData().col(_selectedDimIndex);// align with _enabledDimNames
        _positionSourceDataset->extractDataForDimension(dimV, _selectedDimIndex);// align with _positionSourceDataset
    }
    else {
        for (int i = 0; i < _enabledDimNames.size(); ++i) {
            if (_enabledDimNames[i] == selectedDimName) {
                _selectedDimIndex = i;
                break;
            }
        }

        qDebug() << "Access _avgExpr";
        dimValues = _avgExpr(Eigen::all, _selectedDimIndex);
        dimV.assign(dimValues.data(), dimValues.data() + dimValues.size());
    }
    
    qDebug() << "ExampleViewJSPlugin::updateDimView: selected dim Name :" << dimName << " _selectedDimIndex: " << _selectedDimIndex;
    qDebug() << "ExampleViewJSPlugin::updateDimView: dimV size: " << dimV.size();

    if(!_sliceDataset.isValid()) {
        // 2D dataset
        if (_isSingleCell != true) {
            // ST data
            _dimView->setScalars(dimV, 1);// TO DO: hard-coded the idx of point
        } 
        else {
            // singlecell data - assign _avgExpr values to ST points

            std::vector<float> viewScalars(_numPoints);
            for (size_t i = 0; i < _numPoints; ++i) {
                QString label = _cellLabels[i]; // Get the cluster alias label name of the cell
                viewScalars[i] = dimV[_clusterAliasToRowMap[label]];
            }

            _dimView->setScalars(viewScalars, 1);// TO DO: hard-coded the idx of point
           
        }
       
        
     } else {
        // 3D dataset
        std::vector<float> viewScalars(_onSliceIndices.size());

        if (_isSingleCell != true) {
            // ST data     
            for (int i = 0; i < _onSliceIndices.size(); i++)
            {
                viewScalars[i] = dimV[_onSliceIndices[i]];
            }        
        }
        else {
            // singlecell data - assign _avgExpr values to ST points

            for (size_t i = 0; i < _onSliceIndices.size(); ++i) {
                int cellIndex = _onSliceIndices[i]; // Get the actual cell index
                QString label = _cellLabels[cellIndex]; // Get the cluster alias label name of the cell
                viewScalars[i] = dimV[_clusterAliasToRowMap[label]];
            }
        }
        _dimView->setScalars(viewScalars, 1);// TO DO: hard-coded the idx of point
              
    }  

}


float ExampleViewJSPlugin::computeCorrelation(const Eigen::VectorXf& a, const Eigen::VectorXf& b) {
    // TO DO: might not needed? or only for 2D dataset
    Eigen::VectorXf a_centered = a - Eigen::VectorXf::Constant(a.size(), a.mean());
    Eigen::VectorXf b_centered = b - Eigen::VectorXf::Constant(b.size(), b.mean());
    float correlation = a_centered.dot(b_centered) /
        std::sqrt(a_centered.squaredNorm() * b_centered.squaredNorm());
    if (std::isnan(correlation)) { return 0.0f; } // TO DO: check if this is a good way to handle nan in corr computation
    return correlation;
}

void ExampleViewJSPlugin::computeSubsetData(const DataMatrix& dataMatrix, const std::vector<int>& sortedIndices, DataMatrix& subsetDataMatrix)
{
    if (_isFloodIndex.empty()) {
        qDebug() << "ExampleViewJSPlugin::computeSubsetData: _isFloodIndex is empty";
        return;
    } 

    int rows = sortedIndices.size();
    int cols = dataMatrix.cols();

    subsetDataMatrix.resize(rows, cols);

#pragma omp parallel for
    for (int i = 0; i < rows; ++i)
    {
        int index = sortedIndices[i];
        subsetDataMatrix.row(i) = dataMatrix.row(index);
    }

    qDebug() << "ExampleViewJSPlugin::computeSubsetData: finished";

}

void ExampleViewJSPlugin::computeCorrWave()
{
    if (_isFloodIndex.empty()) {
        qDebug() << "ExampleViewJSPlugin::computeCorrWave(): _isFloodIndex is empty";
        return;
    }

    int numEnabledDims = _enabledDimNames.size();

    // compute correlation between local gene expressions and waveNumbers (sorted)
    // convert sortedWaveNumbers to an Eigen vector

    if (!_sliceDataset.isValid()) {
        // 2D dataset
        Eigen::VectorXf eigenWaveNumbers(_sortedWaveNumbers.size());
        std::copy(_sortedWaveNumbers.begin(), _sortedWaveNumbers.end(), eigenWaveNumbers.data());

        _corrGeneWave.clear();
        for (int col = 0; col < numEnabledDims; ++col) {
            float correlation = computeCorrelation(_subsetData.col(col), eigenWaveNumbers);
            _corrGeneWave.push_back(correlation);
        }
    }
    else {
        // 3D dataset

       // compute the correlation for floodfill in 3D
        Eigen::VectorXf eigenWaveNumbers(_sortedWaveNumbers.size());
        std::copy(_sortedWaveNumbers.begin(), _sortedWaveNumbers.end(), eigenWaveNumbers.data());

        Eigen::VectorXf centeredWaveNumbers = eigenWaveNumbers - Eigen::VectorXf::Constant(eigenWaveNumbers.size(), eigenWaveNumbers.mean());
        float waveNumbersNorm = centeredWaveNumbers.squaredNorm();

        _corrGeneWave.clear();
        _corrGeneWave.resize(numEnabledDims); 
#pragma omp parallel for
        for (int col = 0; col < numEnabledDims; ++col) {
            Eigen::VectorXf centered = _subsetData3D.col(col) - Eigen::VectorXf::Constant(_subsetData3D.col(col).size(), _subsetData3D.col(col).mean());
            float correlation = centered.dot(centeredWaveNumbers) / std::sqrt(centered.squaredNorm() * waveNumbersNorm);
            if (std::isnan(correlation)) { correlation = 0.0f; }
            _corrGeneWave[col] = correlation;
        }
    }

    qDebug() << "ExampleViewJSPlugin::computeCorrWave():corrGeneWave is computed";
}

void ExampleViewJSPlugin::updateFloodFill()
{
    qDebug() << "ExampleViewJSPlugin::updateFloodfill: start... ";
    auto start = std::chrono::high_resolution_clock::now();

    if (!_floodFillDataset.isValid())
    {
        qDebug() << "ExampleViewJSPlugin::updateFloodfill: _floodFillDataset is not valid";
        return;
    }
    
    // get flood fill nodes from the core
    auto start1 = std::chrono::high_resolution_clock::now();
    std::vector<float> floodNodesWave(_floodFillDataset->getNumPoints());
    _floodFillDataset->populateDataForDimensions < std::vector<float>, std::vector<float>>(floodNodesWave, { 0 });
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed1 = end1 - start1;
    std::cout << "populateDataForDimensions Elapsed time: " << elapsed1.count() << " ms\n";

    qDebug() << "ExampleViewJSPlugin::updateFloodfill: floodNodesWave size: " << floodNodesWave.size();

    // process the selected flood-fill 
    auto start2 = std::chrono::high_resolution_clock::now();
    std::vector<int> floodIndices;
    std::vector<int> waveNumbers;
    int tempWave = 0;

    for (int i = 0; i < floodNodesWave.size(); ++i) {
        float node = floodNodesWave[i];
        if (node == -1.0f) {
            tempWave += 1;
        }
        else {
            floodIndices.push_back(static_cast<int>(node));
            waveNumbers.push_back(tempWave);
        }
    }

    int maxWaveNumber = std::max_element(waveNumbers.begin(), waveNumbers.end())[0];

    for (int i = 0; i < waveNumbers.size(); ++i) {
        waveNumbers[i] = maxWaveNumber + 1 - waveNumbers[i];
    }

    
    std::vector<Vector2f> sortedFloodedCoordinates(floodIndices.size());// To check maybe not needed

    // TO DO: test without ordering spatially
    //orderSpatially(floodIndices, waveNumbers, sortedFloodedCoordinates, _sortedFloodIndices, _sortedWaveNumbers); 
    _sortedFloodIndices.clear();
    _sortedWaveNumbers.clear();
    _sortedFloodIndices = floodIndices;
    _sortedWaveNumbers = waveNumbers;
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed2 = end2 - start2;
    std::cout << "process floodfill data Elapsed time: " << elapsed2.count() << " ms\n";

    // must be put after orderSpatially() and before computeSubsetData()
    auto start3 = std::chrono::high_resolution_clock::now();
    _isFloodIndex.clear();
    _isFloodIndex.resize(_numPoints, false);
    for (int idx : _sortedFloodIndices) {
        if (idx < _isFloodIndex.size()) {
            _isFloodIndex[idx] = true;
        }
        else {
            qDebug() << "ExampleViewJSPlugin::updateFloodfill: idx out of range: " << idx;
            return;
        }       
    }
    auto end3 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed3 = end3 - start3;
    std::cout << "update _isFloodIndex Elapsed time: " << elapsed3.count() << " ms\n";


    if (_sliceDataset.isValid()) {      
        auto start4 = std::chrono::high_resolution_clock::now();
        _isFloodOnSlice.clear();
        _isFloodOnSlice.resize(_onSliceIndices.size(), false);
        for (int i = 0; i < _onSliceIndices.size(); i++)
        {
            _isFloodOnSlice[i] = _isFloodIndex[_onSliceIndices[i]];
        }
        auto end4 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed4 = end4 - start4;
        std::cout << "update _isFloodOnSlice Elapsed time: " << elapsed4.count() << " ms\n";

        qDebug() << "ExampleViewJSPlugin::updateFloodfill: _isFloodOnSlice size" << _isFloodOnSlice.size();

        // TO DO: no spatial sorting here
        auto start5 = std::chrono::high_resolution_clock::now();
        _onSliceFloodIndices.clear();
        _onSliceWaveNumbers.clear();

        std::set<int> sliceIndicesSet(_onSliceIndices.begin(), _onSliceIndices.end());
        for (int i = 0; i < _sortedFloodIndices.size(); ++i) {
            if (sliceIndicesSet.find(_sortedFloodIndices[i]) != sliceIndicesSet.end()) {
                _onSliceFloodIndices.push_back(_sortedFloodIndices[i]);
                _onSliceWaveNumbers.push_back(_sortedWaveNumbers[i]);
            }
        }
        auto end5 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed5 = end5 - start5;
        std::cout << "update _onSliceFloodIndices Elapsed time: " << elapsed5.count() << " ms\n";
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "updateFloodfill() Elapsed time: " << elapsed.count() << " ms\n";

    updateSelection();

    qDebug() << "ExampleViewJSPlugin::updateFloodfill: finished";

}

void ExampleViewJSPlugin::loadAvgExpressionABCAtlas() {
    qDebug() << "ExampleViewJSPlugin::loadAvgExpressionABCAtlas(): start... ";

    /*load file of avg expr - file from Michael */
    //std::ifstream file("precomputed_stats_ABC_revision_230821_alias_identifier.csv"); // in this file column:gene identifier, row:cluster alias
    std::ifstream file("precomputed_stats_ABC_revision_230821_alias_symbol.csv"); // in this file column:gene symbol, row:cluster alias
    //std::ifstream file("avg_MERFISH_aliasgenesymbol_august.csv");// august data, self computed from MERFISH data

    if (!file.is_open()) {
        qDebug() << "ExampleViewJSPlugin::loadAvgExpressionABCAtlas Error: Could not open the avg expr file.";
        return;
    }

    _clusterNamesAvgExpr.clear();
    _geneNamesAvgExpr.clear();

    std::string line, cell;
    // Read the first line to extract column headers (gene names)
    if (std::getline(file, line)) {
        std::stringstream lineStream(line);
        bool firstColumn = true;
        while (std::getline(lineStream, cell, ',')) {
            if (firstColumn) {
                // Skip the first cell of the first row
                firstColumn = false;
                continue;
            }
            _geneNamesAvgExpr.push_back(QString::fromStdString(cell));
        }
    }

    // Prepare a vector of vectors to hold the matrix data temporarily
    std::vector<std::vector<float>> matrixData;
    while (std::getline(file, line)) {
        std::stringstream lineStream(line);
        std::vector<float> rowData;
        std::string clusterNameStr;
        if (std::getline(lineStream, clusterNameStr, ',')) {
            try {
                _clusterNamesAvgExpr.push_back(QString::fromStdString(clusterNameStr));
            }
            catch (const std::exception& e) {
                std::cerr << "Error converting cluster name to int: " << e.what() << std::endl;
                continue;
            }
        }

        // Read each cell in the row
        while (std::getline(lineStream, cell, ',')) {
            try {
                rowData.push_back(std::stof(cell));
            }
            catch (const std::exception& e) {
                std::cerr << "Error converting cell to float: " << e.what() << std::endl;
            }
        }

        matrixData.push_back(rowData);
    }

    int numClusters = _clusterNamesAvgExpr.size();
    int numGenes = _geneNamesAvgExpr.size();

    file.close();

    _clusterAliasToRowMap.clear();// _clusterAliasToRowMap: first element is label name, second element is row index in _avgExpr
    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
    }

    qDebug() << "ExampleViewJSPlugin::loadAvgExpressionABCAtlas()" << numGenes << " genes and " << numClusters << " clusters"; 
    qDebug() << "ExampleViewJSPlugin::loadAvgExpressionABCAtlas(): finished";

    // identify duplicate gene symbols and append an index to them
    std::map<QString, int> geneSymbolCount;
    for (const auto& geneName : _geneNamesAvgExpr) {
        geneSymbolCount[geneName]++;
    }
    std::map<QString, int> geneSymbolIndex;
    for (auto& geneName : _geneNamesAvgExpr) {
        if (geneSymbolCount[geneName] > 1) {
            geneSymbolIndex[geneName]++;// Duplicate found
            geneName += "_copy" + QString::number(geneSymbolIndex[geneName]);// Append index to the gene symbol
        }
    }

    // store the evg expression matrix as a dataset
    size_t totalElements = numClusters * numGenes; // total number of data points
    std::vector<float> allData;
    allData.reserve(totalElements);
    for (const auto& row : matrixData) {
        allData.insert(allData.end(), row.begin(), row.end());
    }

    qDebug() << "ExampleViewJSPlugin::loadAvgExpressionABCAtlas(): allData size: " << allData.size();

    if (!_avgExprDataset.isValid()) {
        qDebug() << "Create an avgExprDataset";
        _avgExprDataset = mv::data().createDataset<Points>("Points", "avgExprDataset");
        events().notifyDatasetAdded(_avgExprDataset);
    }
    /*else {
        _avgExprDataset.reset();
    }*/
    
    _avgExprDataset->setData(allData.data(), numClusters, numGenes); // Assuming this function signature is (data, rows, columns)
    _avgExprDataset->setDimensionNames(_geneNamesAvgExpr);
    events().notifyDatasetDataChanged(_avgExprDataset);
    qDebug() << "ExampleViewJSPlugin::loadAvgExpressionABCAtlas(): _avgExprDataset dataset created";

    // test convert to eigen
    // TO DO: maybe connect _avgExprDataset to _avgExpr directly??
    _avgExpr.resize(numClusters, numGenes);
    convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);

}

void ExampleViewJSPlugin::loadLabelsFromSTDatasetABCAtlas() {
    // this is loading label from ST dataset!!!
    // Different from loading from singlecell datset!!!

    Dataset<Clusters> labelDataset;
    for (const auto& data : mv::data().getAllDatasets())
    {
        //qDebug() << data->getGuiName();
        if (data->getGuiName() == "cluster_alias") {
            labelDataset = data;
        }
    }

    QVector<Cluster> labelClusters = labelDataset->getClusters();

    qDebug() << "ExampleViewJSPlugin::loadLabelsFromSTDatasetABCAtlas(): labelClusters size: " << labelClusters.size();

    // precompute the cell-label array
    _cellLabels.clear();
    _cellLabels.resize(_numPoints);


    for (int i = 0; i < labelClusters.size(); ++i) {
        QString clusterName = labelClusters[i].getName();
        const auto& ptIndices = labelClusters[i].getIndices();
        for (int j = 0; j < ptIndices.size(); ++j) {
            int ptIndex = ptIndices[j];
            _cellLabels[ptIndex] = clusterName;
        }
    }

    qDebug() << "ExampleViewJSPlugin::loadLabelsFromSTDatasetABCAtlas(): _cellLabels size: " << _cellLabels.size();
    qDebug() << "_cellLabels[0]" << _cellLabels[0];
}

void ExampleViewJSPlugin::countLabelDistribution() {

    // count the number of cells in each cluster using the precomputed array _cellLabels
    std::map<QString, int> clusterPointCounts;
    std::map<QString, int> clusterWaveNumberSums; // test corrWave - use cluster instead of cell points


    std::map<QString, std::map<int, int>> waveNumberDistribution; // New map for wave number distribution
    for (int index = 0; index < _sortedFloodIndices.size(); ++index) {
        int ptIndex = _sortedFloodIndices[index];
        QString label = _cellLabels[ptIndex];
        clusterPointCounts[label]++;

        clusterWaveNumberSums[label] += _sortedWaveNumbers[index]; // for computing the average wave number

        int waveNumber = _sortedWaveNumbers[index]; // for outputting the distribution of wave numbers
        waveNumberDistribution[label][waveNumber]++;
    }

    _countsLabel.clear(); // TO DO: can direct assign to _countsLabel - avoid copy
    _countsLabel = clusterPointCounts;

    std::map<QString, float> clusterWaveAvg;
    int i = 0;
    for (const auto& pair : clusterPointCounts) {
        QString label = pair.first;
        int count = pair.second;
        float average = (count > 0) ? static_cast<float>(clusterWaveNumberSums[label]) / count : 0.0f;
        clusterWaveAvg[label] = average;
        i++;
    }
    _clusterWaveNumbers.clear(); // TO DO: can direct assign to _clusterWaveNumbers - avoid copy
    _clusterWaveNumbers = clusterWaveAvg;

    // output the distribution of wave numbers within each cluster
    /*for (const auto& cluster : waveNumberDistribution) {
        int label = cluster.first;
        const auto& distribution = cluster.second;
        std::cout << "Cluster " << label << " Avg Wave: " << clusterWaveAvg[label] << " Distri: ";
        for (const auto& waveNumberCount : distribution) {
            std::cout << "Wave " << waveNumberCount.first << ": " << waveNumberCount.second << ", ";
        }
        std::cout << std::endl;
    }*/

   // sort the counts
   /* std::vector<std::pair<QString, int>> sortedCounts;
    for (const auto& pair : clusterPointCounts) {
        sortedCounts.push_back(pair);
    }
    std::sort(sortedCounts.begin(), sortedCounts.end(),
        [](const std::pair<QString, int>& a, const std::pair<QString, int>& b) {
            return a.second > b.second;
        });
    for (const auto& pair : sortedCounts) {
        qDebug() << pair.first << ": " << pair.second;
    }*/
}

void ExampleViewJSPlugin::computeAvgExprSubset() {

    // sum of counts for later normalization
    int sumOfCounts = 0;
    for (const auto& pair : _countsLabel) {
        sumOfCounts += pair.second;
    }
    qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): sumOfCounts: " << sumOfCounts;

    // Create a map for counts
    std::unordered_map<QString, int> countsMap;
    for (const auto& pair : _countsLabel) {
        QString clusterName = pair.first;
        countsMap[clusterName] = pair.second;
    }
    qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): countsMap size: " << countsMap.size();

    // TEST Feb29
    int numClusters = _avgExpr.rows();
    int numGenes = _avgExpr.cols();
    int numClusters2 = _avgExprDataset->getNumPoints();
    int numGenes2 = _avgExprDataset->getNumDimensions();
    qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): before matching numClusters: " << numClusters << " numGenes: " << numGenes;
    qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): before matching numClusters2: " << numClusters2 << " numGenes2: " << numGenes2;


    std::vector<QString> clustersToKeep; // it is cluster names 1
    for (int i = 0; i < numClusters; ++i) {
        QString clusterName = _clusterNamesAvgExpr[i];
        if (countsMap.find(clusterName) != countsMap.end()) {
            clustersToKeep.push_back(clusterName);
        }
    }

    // to check if any columns are not in _columnNamesAvgExpr
    if (clustersToKeep.size() != countsMap.size()) {
        qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): " << countsMap.size() - clustersToKeep.size() << "clusters not found in avgExpr";
        // output the cluster names that are not in
        QString output;
        for (const auto& pair : countsMap) {
            QString clusterName = pair.first;
            if (std::find(_clusterNamesAvgExpr.begin(), _clusterNamesAvgExpr.end(), clusterName) == _clusterNamesAvgExpr.end()) {
                output += clusterName + " ";
            }
        }
        qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): not found cluster names: " << output;
    }
    else {
        qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): all clusters found in avgExpr";
    }

    // Handle case where no columns are to be kept // TO DO: check if needed
    if (clustersToKeep.empty()) {
        qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): No cluster to keep";
        return;
    }

    _subsetDataAvgWeighted.resize(clustersToKeep.size(), numGenes); // here, numCluster might reduce
    _subsetDataAvgOri.resize(clustersToKeep.size(), numGenes);
    qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): after matching numClusters: " << clustersToKeep.size();

    for (int i = 0; i < clustersToKeep.size(); ++i) {
        QString clusterName = clustersToKeep[i];

        auto it = std::find(_clusterNamesAvgExpr.begin(), _clusterNamesAvgExpr.end(), clusterName);
        if (it == _clusterNamesAvgExpr.end()) {
            qDebug() << "Error: clusterName " << clusterName << " not found in _clusterNamesAvgExpr";
            continue;
        }
        int clusterIndex = std::distance(_clusterNamesAvgExpr.begin(), it);

        if (countsMap.find(clusterName) == countsMap.end()) {
            qDebug() << "Error: clusterName " << clusterName << " not found in countsMap";
            continue;
        }

        for (int j = 0; j < numGenes; ++j) {
            float weightedValue = (_avgExpr(clusterIndex, j) * countsMap[clusterName]) / static_cast<float>(sumOfCounts);
            _subsetDataAvgWeighted(i, j) = weightedValue;
        }
        _subsetDataAvgOri.row(i) = _avgExpr.row(clusterIndex);
    }


    qDebug() << "subsetDataAvgExprWeighted num rows (clusters): " << _subsetDataAvgWeighted.rows() << ", num columns (genes): " << _subsetDataAvgWeighted.cols();
    qDebug() << "subsetDataAvgExprOri num rows (clusters): " << _subsetDataAvgOri.rows() << ", num columns (genes): " << _subsetDataAvgOri.cols();

    // match _clusterWaveNumbers to clustersToKeep, i.e. match the column order of subset1
    _waveAvg.clear();
    for (QString clusterName : clustersToKeep) {
        _waveAvg.push_back(_clusterWaveNumbers[clusterName]);
    }
    qDebug() << "ExampleViewJSPlugin::computeAvgExprSubset(): _waveAvg size: " << _waveAvg.size();

    _clustersToKeep.clear();
    _clustersToKeep = clustersToKeep; // TO DO: dirty copy

}

void ExampleViewJSPlugin::orderSpatially(const std::vector<int>& unsortedIndices, const std::vector<int>& unsortedWave, std::vector<Vector2f>& sortedCoordinates, std::vector<int>& sortedIndices, std::vector<int>& sortedWave)
{
    // TO DO:compute once for the entire data and lookup every time

    std::vector<mv::Vector2f> unsortedCoordinates;
    for (int index : unsortedIndices)
    {
        unsortedCoordinates.push_back(_positions[index]);
    }

    // a map for reordering the vectors
    std::vector<int> indiceMap(unsortedCoordinates.size());
    std::iota(indiceMap.begin(), indiceMap.end(), 0);

    // sort spatially
    std::sort(indiceMap.begin(), indiceMap.end(), [&](int a, int b) {
        if (unsortedCoordinates[a].y > unsortedCoordinates[b].y) return true;
        if (unsortedCoordinates[a].y < unsortedCoordinates[b].y) return false;
        return unsortedCoordinates[a].x < unsortedCoordinates[b].x;
        });

    sortedCoordinates.resize(unsortedCoordinates.size());
    sortedIndices.resize(unsortedIndices.size());
    sortedWave.resize(unsortedWave.size());

    // apply the map to reorder selectedCoordinates and floodIndices
    for (int i = 0; i < indiceMap.size(); ++i) {
        sortedCoordinates[i] = unsortedCoordinates[indiceMap[i]];
        sortedIndices[i] = unsortedIndices[indiceMap[i]];
        sortedWave[i] = unsortedWave[indiceMap[i]];
    }

    qDebug() << "ExampleViewJSPlugin::orderSpatially(): spatial sorting finished";
}

void ExampleViewJSPlugin::clusterGenes()
{
    qDebug() << "clusterGenes start...";

    std::vector<float> corr;
    if (_isCorrSpatial) {
        corr = _corrGeneSpatial;
    }
    else {
        corr = _corrGeneWave;
    }

    auto start1 = std::chrono::high_resolution_clock::now();
    // filter genes based on corrWave  
    std::vector<QString> filteredDimNames;
    std::vector<int> filteredDimIndices;

    for (int i = 0; i < _enabledDimNames.size(); ++i) {

        if (std::abs(corr[i]) >= _corrThreshold) {
            filteredDimNames.push_back(_enabledDimNames[i]);
            filteredDimIndices.push_back(i);
        }
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed1 = end1 - start1;
    std::cout << "clusterGenes() filtering Elapsed time: " << elapsed1.count() << " ms\n";

    qDebug() << "ExampleViewJSPlugin::clusterGenes(): filteredDimNames size: " << filteredDimNames.size();

    if (filteredDimNames.size() < _nclust) {
        qDebug() << "ExampleViewJSPlugin::clusterGenes(): Not enough genes for clustering";

        _toClearBarchart = true;

        // emit an empty payload to JS and to clear the barchart
        QVariantList payload;
        emit _chartWidget->getCommunicationObject().qt_js_setDataAndPlotInJS(payload);

        return;
    }
    else {
        _toClearBarchart = false;
    }

    // compute the correlation between each pair of the filtered genes
    // TO DO: can call computeCorrWave() here to avoid repeated code
    auto start2 = std::chrono::high_resolution_clock::now();
    Eigen::MatrixXf corrFilteredGene(filteredDimNames.size(), filteredDimNames.size());

    std::unordered_map<QString, int> dimNameToIndex;
    for (int i = 0; i < _enabledDimNames.size(); ++i) {
        dimNameToIndex[_enabledDimNames[i]] = i;
    }

    if (!_sliceDataset.isValid()) {
        // 2D 
        for (int col1 = 0; col1 < filteredDimNames.size(); ++col1) {
            for (int col2 = col1; col2 < filteredDimNames.size(); ++col2) {
                int index1 = dimNameToIndex[filteredDimNames[col1]];
                int index2 = dimNameToIndex[filteredDimNames[col2]];

                float correlation = computeCorrelation(_subsetData.col(index1), _subsetData.col(index2));
                corrFilteredGene(col1, col2) = correlation;
                corrFilteredGene(col2, col1) = correlation;
            }
        }
    } 
    else {
        // 3D dataset - compute correlation for all floodfill indices

        std::vector<Eigen::VectorXf> centeredVectors(filteredDimNames.size());// precompute mean 
        std::vector<float> norms(filteredDimNames.size());

        for (int i = 0; i < filteredDimNames.size(); ++i) {
            int index = dimNameToIndex[filteredDimNames[i]];
            Eigen::VectorXf centered = _subsetData3D.col(index) - Eigen::VectorXf::Constant(_subsetData3D.col(index).size(), _subsetData3D.col(index).mean());
            centeredVectors[i] = centered;
            norms[i] = centered.squaredNorm();
        }

#pragma omp parallel for
        for (int col1 = 0; col1 < filteredDimNames.size(); ++col1) {
            for (int col2 = col1; col2 < filteredDimNames.size(); ++col2) {
                float correlation = centeredVectors[col1].dot(centeredVectors[col2]) / std::sqrt(norms[col1] * norms[col2]);
                if (std::isnan(correlation)) { correlation = 0.0f; } // TO DO: check if this is a good way to handle nan in corr computation

                corrFilteredGene(col1, col2) = correlation;
                corrFilteredGene(col2, col1) = correlation;
            }
        }
    }

    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed2 = end2 - start2;
    std::cout << "clusterGenes() computeCorrFilteredGene Elapsed time: " << elapsed2.count() << " ms\n";
    qDebug() << "ExampleViewJSPlugin::clusterGenes():corrFilteredGene is computed";

    // substract correlation from 1 to get distance
    auto start3 = std::chrono::high_resolution_clock::now();
    Eigen::MatrixXf distanceMatrix = Eigen::MatrixXf::Ones(corrFilteredGene.rows(), corrFilteredGene.cols()) - corrFilteredGene;

    // Format input distance for fastcluster
    int n = distanceMatrix.rows();
    double* distmat = new double[(n * (n - 1)) / 2]; //a condensed distance matrix, upper triangle (without the diagonal elements) of the full distance matrix
    int k = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            distmat[k] = distanceMatrix(i, j);
            k++;
        }
    }


    // Apply clustering
    int* merge = new int[2 * (n - 1)];// dendrogram in the encoding of the R function hclust
    double* height = new double[n - 1];// cluster distance for each step
    hclust_fast(n, distmat, HCLUST_METHOD_AVERAGE, merge, height);

    int* labels = new int[n];// cluster label of observable x[i]
    cutree_k(n, merge, _nclust, labels);

    // inspect dendrogram ----------------------------------------begin
    //for (int i = 0; i < n - 1; ++i) { // For each merge step
    //    int cluster1 = merge[2 * i];
    //    int cluster2 = merge[2 * i + 1];

    //    std::cout << "Merge Step " << (i + 1) << ": ";

    //    // Decode cluster1
    //    if (cluster1 < 0) {
    //        std::cout << "Data Point " << (-cluster1 - 1);
    //    }
    //    else {
    //        std::cout << "Cluster formed at step " << (cluster1 - n);
    //    }
    //    std::cout << " merged with ";

    //    // Decode cluster2
    //    if (cluster2 < 0) {
    //        std::cout << "Data Point " << (-cluster2 - 1);
    //    }
    //    else {
    //        std::cout << "Cluster formed at step " << (cluster2 - n);
    //    }

    //    std::cout << std::endl;
    //}
    // inspect dendrogram ----------------------------------------end


    // Mapping labels back to dimension names
    _dimNameToClusterLabel.clear();
    for (int i = 0; i < n; ++i) {
        _dimNameToClusterLabel[filteredDimNames[i]] = labels[i];
    }

    auto end3 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed3 = end3 - start3;
    std::cout << "clusterGenes() Apply clustering Elapsed time: " << elapsed3.count() << " ms\n";
 
    //qDebug() << "ExampleViewJSPlugin::clusterGenes():hcluster finished";

    // temporary code: out put gene names in each cluster
   /* for (int cluster = 0; cluster < _nclust; ++cluster) {
        qDebug() << "Cluster " << cluster << "contains :";
        for (int i = 0; i < filteredDimNames.size(); ++i) {
            if (labels[i] == cluster) {
                qDebug().noquote() << filteredDimNames[i];
            }
        }
    }*/

    // temporary code: output the number of genes in each cluster
    std::map<int, int> clusterCounts;
    for (int i = 0; i < filteredDimNames.size(); ++i) {
        clusterCounts[labels[i]]++;
    }
    for (const auto& pair : clusterCounts) {
        qDebug() << "Cluster " << pair.first << " contains " << pair.second << " genes";
    }


    /*auto start5 = std::chrono::high_resolution_clock::now();
    computeEntireClusterScalars(filteredDimIndices, labels);   
    auto end5 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed5 = end5 - start5;
    std::cout << "computeEntireClusterScalars Elapsed time: " << elapsed5.count() << " ms\n";*/

    if (_isSingleCell != true) {
        auto start5 = std::chrono::high_resolution_clock::now();
        computeFloodedClusterScalars(filteredDimIndices, labels);
        auto end5 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed5 = end5 - start5;
        std::cout << "clusterGenes() computeFloodedClusterScalars Elapsed time: " << elapsed5.count() << " ms\n";
    }
    else {
        qDebug() << "ExampleViewJSPlugin::clusterGenes(): _isSingleCell is true";
        auto start5 = std::chrono::high_resolution_clock::now();
        computeFloodedClusterScalarsSingleCell(filteredDimIndices, labels);
        auto end5 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed5 = end5 - start5;
        std::cout << "clusterGenes() computeFloodedClusterScalarsSingleCell Elapsed time: " << elapsed5.count() << " ms\n";
    }

    delete[] distmat;
    delete[] merge;
    delete[] height;
    delete[] labels;

}

void ExampleViewJSPlugin::computeEntireClusterScalars(const std::vector<int> filteredDimIndices, const int* labels)
{
    // Compute mean expression for each cluster
    // for the entire spaial map
   
    _colorScalars.clear();
    _colorScalars.resize(_nclust, std::vector<float>(_numPoints, 0.0f));

    const auto& baseData = _dataStore.getBaseData();

   /* auto start1 = std::chrono::high_resolution_clock::now();
    Eigen::MatrixXf allMeans1;
    allMeans1.resize(_nclust, baseData.rows());
    std::vector<int> dimensionsPerCluster1(_nclust, 0);

    for (int d = 0; d < filteredDimIndices.size(); ++d) {
        int cluster = labels[d];
        allMeans1.row(cluster) += baseData.col(filteredDimIndices[d]);
        dimensionsPerCluster1[cluster]++;
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed1 = end1 - start1;
    std::cout << "1 old Elapsed time: " << elapsed1.count() << " ms\n";*/

    auto start11 = std::chrono::high_resolution_clock::now();
    Eigen::MatrixXf allMeans = Eigen::MatrixXf::Zero(_nclust, _dataStore.getBaseData().rows());
    std::vector<int> dimensionsPerCluster(_nclust, 0);

    #pragma omp parallel for  
    for (int cluster = 0; cluster < _nclust; ++cluster) {
        for (int d = 0; d < filteredDimIndices.size(); ++d) {
            if (labels[d] == cluster) {
                allMeans.row(cluster) += baseData.col(filteredDimIndices[d]);
                dimensionsPerCluster[cluster]++;
            }
        }
    }
    auto end11 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed11 = end11 - start11;
    std::cout << "1 new Elapsed time: " << elapsed11.count() << " ms\n"; 


   /* auto start12 = std::chrono::high_resolution_clock::now();
    Eigen::MatrixXf allMeans12 = Eigen::MatrixXf::Zero(_nclust, _dataStore.getBaseData().rows());
    std::vector<int> dimensionsPerCluster12(_nclust, 0);
    std::vector<std::vector<int>> indicesPerCluster(_nclust);

    for (int d = 0; d < filteredDimIndices.size(); ++d) {
        int cluster = labels[d];
        indicesPerCluster[cluster].push_back(filteredDimIndices[d]);
    }
#pragma omp parallel for
    for (int cluster = 0; cluster < _nclust; ++cluster) {
        for (int idx : indicesPerCluster[cluster]) {
            allMeans12.row(cluster) += baseData.col(idx);
            dimensionsPerCluster12[cluster]++;
        }
    }
    auto end12 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed12 = end12 - start12;
    std::cout << "1 new 2 Elapsed time: " << elapsed12.count() << " ms\n";*/


    auto start2 = std::chrono::high_resolution_clock::now();
    for (int cluster = 0; cluster < _nclust; ++cluster) {
        if (dimensionsPerCluster[cluster] != 0) {
            allMeans.row(cluster) /= dimensionsPerCluster[cluster];  // sum/num_genes
        }
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed2 = end2 - start2;
    std::cout << "2 Elapsed time: " << elapsed2.count() << " ms\n";



    ////Populate _colorScalars with the data from allMeans
    //for (int cluster = 0; cluster < _nclust; ++cluster) {
    //    for (int i = 0; i < _numPoints; ++i) {
    //        _colorScalars[cluster][i] = allMeans(cluster, i);
    //    }
    //}

    auto start3 = std::chrono::high_resolution_clock::now();
    // new Populate _colorScalars with the data from allMeans
    for (int cluster = 0; cluster < _nclust; ++cluster) {
        Eigen::Map<Eigen::VectorXf>(&_colorScalars[cluster][0], _numPoints) = allMeans.row(cluster);
    }
    auto end3 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed3 = end3 - start3;
    std::cout << "3 Elapsed time: " << elapsed3.count() << " ms\n";

}

void ExampleViewJSPlugin::computeFloodedClusterScalars(const std::vector<int> filteredDimIndices, const int* labels)
{
    // Compute mean expression for each cluster
    // only for flooded cells, others are filled with the lowest value
    _colorScalars.clear();
    _colorScalars.resize(_nclust, std::vector<float>(_numPoints, 0.0f));

    DataMatrix subsetData;
    
    if (!_sliceDataset.isValid()) {
        // 2D dataset
        subsetData = _subsetData;
    }
    else {
        // 3D dataset
        //computeSubsetData(_dataStore.getBaseData(), _sortedFloodIndices, subsetData);
        subsetData = _subsetData3D;
    }

    Eigen::MatrixXf subsetMeans = Eigen::MatrixXf::Zero(_nclust, subsetData.rows());
    std::vector<int> dimensionsPerCluster(_nclust, 0);

    #pragma omp parallel for
    for (int d = 0; d < filteredDimIndices.size(); ++d) {
        int cluster = labels[d];
        #pragma omp critical
        {
            subsetMeans.row(cluster) += subsetData.col(filteredDimIndices[d]);
            dimensionsPerCluster[cluster]++;
        }
    }
    for (int cluster = 0; cluster < _nclust; ++cluster) {
        subsetMeans.row(cluster) /= dimensionsPerCluster[cluster];  // sum/num_genes
    }

    // Populate _colorScalars
    for (int cluster = 0; cluster < _nclust; ++cluster) {
        for (int i = 0; i < _sortedFloodIndices.size(); ++i) {
            _colorScalars[cluster][_sortedFloodIndices[i]] = subsetMeans(cluster, i);
        }
    }

    // fill in the empty spaces with the lowest value in every row of _colorScalars

#pragma omp parallel for
    for (int clusterIndex = 0; clusterIndex < _colorScalars.size(); ++clusterIndex) {
        std::vector<float>& clusterScalar = _colorScalars[clusterIndex];

        float minValue = *std::min_element(clusterScalar.begin(), clusterScalar.end());
        for (int i = 0; i < _numPoints; ++i) {
            if (!_isFloodIndex[i]) {
                clusterScalar[i] = minValue;
            }
        }
    }
}

void ExampleViewJSPlugin::computeFloodedClusterScalarsSingleCell(const std::vector<int> filteredDimIndices, const int* labels) {
    // Compute mean expression for each cluster
    // only for flooded cells, others are filled with the lowest value
    _colorScalars.clear();
    _colorScalars.resize(_nclust, std::vector<float>(_numPoints, 0.0f));

    std::unordered_map<QString, int> clusterAliasToRowMapSubset;// ATTENTION here only cluster alias within the subset!!
    for (int i = 0; i < _clustersToKeep.size(); ++i) {
        clusterAliasToRowMapSubset[_clustersToKeep[i]] = i;
    }

    auto subsetData = _subsetDataAvgOri;

    Eigen::MatrixXf subsetMeans = Eigen::MatrixXf::Zero(_nclust, subsetData.rows());
    std::vector<int> dimensionsPerCluster(_nclust, 0);

#pragma omp parallel for
    for (int d = 0; d < filteredDimIndices.size(); ++d) {
        int cluster = labels[d];
#pragma omp critical
        {
            subsetMeans.row(cluster) += subsetData.col(filteredDimIndices[d]);
            dimensionsPerCluster[cluster]++;
        }
    }

    for (int cluster = 0; cluster < _nclust; ++cluster) {
        subsetMeans.row(cluster) /= dimensionsPerCluster[cluster];  // sum/num_genes
    }

    // Populate _colorScalars
    for (size_t i = 0; i < _sortedFloodIndices.size(); ++i) {
        int cellIndex = _sortedFloodIndices[i]; // Get the actual cell index
        QString label = _cellLabels[cellIndex]; // Get the cluster alias label name of the cell
        int columnIndex = clusterAliasToRowMapSubset[label]; // Get the column index of the cluster alias label name [in the subset]

        // Assuming label is within the column range of subsetMeans2
        for (int cluster = 0; cluster < _nclust; ++cluster) {
            _colorScalars[cluster][cellIndex] = subsetMeans(cluster, columnIndex);
        }
    }

#pragma omp parallel for
    for (int clusterIndex = 0; clusterIndex < _colorScalars.size(); ++clusterIndex) {
        std::vector<float>& clusterScalar = _colorScalars[clusterIndex];

        float minValue = *std::min_element(clusterScalar.begin(), clusterScalar.end());
        for (int i = 0; i < _numPoints; ++i) {
            if (!_isFloodIndex[i]) {
                clusterScalar[i] = minValue;
            }
        }
    }
}

void ExampleViewJSPlugin::updateClusterScalarOutput(const std::vector<float>& scalars)
{
    _clusterScalars->setData<float>(scalars.data(), scalars.size(), 1);
    events().notifyDatasetDataChanged(_clusterScalars);
    qDebug() << "ExampleViewJSPlugin::updateClusterScalarOutput(): finished";
}

void ExampleViewJSPlugin::getFuntionalEnrichment()
{
    QStringList geneNamesInCluster;
    _simplifiedToIndexGeneMapping.clear();
    for (const auto& pair : _dimNameToClusterLabel) {
        if (pair.second == _selectedClusterIndex) {
            QString geneName = pair.first;
            QString simplifiedGeneName = geneName;// copy for potential modification

            // check if gene name contains an _copy index - for modified duplicate gene symbols in ABC Atlas
            int index = geneName.lastIndexOf("_copy");
            if (index != -1) {
                simplifiedGeneName = geneName.left(index);// remove the index
                _simplifiedToIndexGeneMapping[simplifiedGeneName].append(geneName);
            }

            geneNamesInCluster.append(simplifiedGeneName);
        }
    }

    // output _simplifiedToIndexGeneMapping if not empty
    if (!_simplifiedToIndexGeneMapping.empty()) {
        for (auto it = _simplifiedToIndexGeneMapping.constBegin(); it != _simplifiedToIndexGeneMapping.constEnd(); ++it) {
            qDebug() << it.key() << ":";
            for (const QString& value : it.value()) {
                qDebug() << value;
            }
        }
    }

    if (!geneNamesInCluster.isEmpty()) {
        // ToppGene
        //_client->lookupSymbolsToppGene(geneNamesInCluster);

        // gProfiler
        QStringList backgroundGeneNames;
        if (_isSingleCell != true) {         
            for (const auto& name : _enabledDimNames) {
                backgroundGeneNames.append(name);
            }
            qDebug() << "getFuntionalEnrichment(): ST mode, with background";
        }
        else {
            // in single cell mode, background is empty
            qDebug() << "getFuntionalEnrichment(): single cell mode, without background";
        }
        qDebug() << "getFuntionalEnrichment(): backgroundGeneNames size: " << backgroundGeneNames.size();
        _client->postGeneGprofiler(geneNamesInCluster, backgroundGeneNames); 

        //EnrichmentAnalysis* tempClient = new EnrichmentAnalysis(this);
    }

    // output the gene names
    int counter = 0;
    for (const auto& item : geneNamesInCluster) {
        if (counter >= 30) {
            qDebug() << "more genes are not shown";
            break;
        }

        qDebug() << item.toUtf8().constData();
        ++counter;
    }

}

void ExampleViewJSPlugin::updateEnrichmentTable(const QVariantList& data) {
    //qDebug() << "ExampleViewJSPlugin::updateEnrichmentTable(): start";

    _enrichmentResult = data;

    /*for (const QVariant& item : data) {
        QVariantMap dataMap = item.toMap();
        for (auto key : dataMap.keys()) {
            qDebug() << key << ":" << dataMap[key].toString();
        }
    }*/

    // automatically extracting headers from the keys of the first item
    QStringList headers;
    QList<QString> keys = data.first().toMap().keys();
    for (int i = 0; i < keys.size(); ++i) {
        headers.append(keys[i]);
    }

    _tableWidget->clearContents();
    _tableWidget->setRowCount(data.size());
    _tableWidget->setColumnCount(keys.size());

    _tableWidget->setHorizontalHeaderLabels(headers);

    // automatically populate the table with data
    for (int row = 0; row < data.size(); ++row) {
        QVariantMap dataMap = data.at(row).toMap();

        for (int col = 0; col < headers.size(); ++col) {
            QString key = headers.at(col);
            QTableWidgetItem* tableItem = new QTableWidgetItem(dataMap[key].toString());
            _tableWidget->setItem(row, col, tableItem);
        }
    }

    //_tableWidget->resizeRowsToContents();
    _tableWidget->resizeColumnsToContents();

    _tableWidget->show();

    qDebug() << "ExampleViewJSPlugin::updateEnrichmentTable(): finished";
}

void ExampleViewJSPlugin::noDataEnrichmentTable() {
    _tableWidget->clearContents();
    _tableWidget->setRowCount(1);
    _tableWidget->setColumnCount(1);

    QStringList header = { "Message" };
    _tableWidget->setHorizontalHeaderLabels(header);
    QTableWidgetItem* item = new QTableWidgetItem("No enrichment analysis result available.");

    _tableWidget->setItem(0, 0, item);
    _tableWidget->resizeColumnsToContents();
}

void ExampleViewJSPlugin::onTableClicked(int row, int column) {
    qDebug() << "Cell clicked in row:" << row << "column:" << column;

    // get gene symbols of the selected row
    QVariantMap selectedItemMap = _enrichmentResult[row].toMap();
    QString geneSymbols = selectedItemMap["Symbol"].toString();
    qDebug() << "Gene symbols in the selected item:" << geneSymbols;

    QStringList geneSymbolList = geneSymbols.split(",");

    // match the gene symbols with the gene names in the cluster - gene symbols returned from topGene are all capitals
    QVariantList geneNamesForHighlighting;
    for (const QString& symbol : geneSymbolList) {
        // Properly format the symbol (first letter uppercase, rest lowercase) - gene symbols returned from topGene are all capitals -not needed using gProfiler
        /*QString searchGeneSymbol = symbol.toLower();
        searchGeneSymbol[0] = searchGeneSymbol[0].toUpper();*/
        QString searchGeneSymbol = symbol; // TO DO: check if the gene symbols are already in the correct format

        // Attempt to find original, indexed gene names using the reverse mapping
        QStringList originalGeneNames = _simplifiedToIndexGeneMapping.value(searchGeneSymbol);

        if (!originalGeneNames.isEmpty()) {
            qDebug() << "Original gene names found for symbol" << searchGeneSymbol << ":" << originalGeneNames;
            // original, indexed versions exist, add them for highlighting
            for (const QString& originalGeneName : originalGeneNames) {
                geneNamesForHighlighting.append(QVariant(originalGeneName));
            }          
        }
        else {
            // no indexed version found, use the modified search symbol
            geneNamesForHighlighting.append(QVariant(searchGeneSymbol));
        }
    }
    emit _chartWidget->getCommunicationObject().qt_js_highlightInJS(geneNamesForHighlighting);
}

void ExampleViewJSPlugin::updateClick() {
    for (int i = 0; i < 6; i++) {// TO DO: hard coded for the current layout
        _scatterViews[i]->selectView(false);
    }
    _dimView->selectView(false);

    ScatterView* selectedView = nullptr;
    for (int i = 0; i < _nclust; i++) {
        if (_selectedClusterIndex == i) {
            selectedView = _scatterViews[i];
        }
    }

    if (selectedView != nullptr)
    {
        // one of the scatterViews is selected
        selectedView->selectView(true);
        getFuntionalEnrichment();

        // TO DO: seperate the enrichment analysis part and the update scalars part
        std::vector<float> dimV = _colorScalars[_selectedClusterIndex]; // TO DO: get the dimV for 3D

        // assign the lowest value to the non-flooded cells
        float minValue = *std::min_element(dimV.begin(), dimV.end());
#pragma omp parallel for
        for (int i = 0; i < _numPoints; ++i) {
            if (!_isFloodIndex[i]) {
                dimV[i] = minValue;
            }
        }
        normalizeVector(dimV);
        updateClusterScalarOutput(dimV);
    }
    else {
        // dimView is selected
        if (_selectedClusterIndex == 6) {// TO DO: hard coded for the current layout
            _dimView->selectView(true);

            Eigen::VectorXf dimValue;
            std::vector<float> dimV;
            if (!_isSingleCell) {
                //dimValue = _dataStore.getBaseData()(Eigen::all, _selectedDimIndex); //align with _enabledDimNames
                _positionSourceDataset->extractDataForDimension(dimV, _selectedDimIndex);// align with _positionSourceDataset
            }
            else {
                // for singlecell option
                const Eigen::VectorXf avgValue = _avgExpr(Eigen::all, _selectedDimIndex);

                //populate the avg expr values to all ST points
                dimValue.resize(_numPoints);
#pragma omp parallel for
                for (int i = 0; i < _numPoints; ++i) {
                    QString label = _cellLabels[i]; // Get the cluster alias label name of the cell
                    dimValue[i] = avgValue(_clusterAliasToRowMap[label]);
                }
                dimV.assign(dimValue.data(), dimValue.data() + dimValue.size());
            }

            normalizeVector(dimV);
            updateClusterScalarOutput(dimV);
        }
    }
}

void ExampleViewJSPlugin::updateSlice() {
   
    _currentSliceIndex = _settingsAction.getSliceAction().getValue();

    if (!_sliceDataset.isValid()) {
        qDebug() << "ExampleViewJSPlugin::updateSlice(): _sliceDataset is not valid";
        return;
    }

    //QVector<Cluster>& clusters = _sliceDataset->getClusters();
    QString clusterName = _sliceDataset->getClusters()[_currentSliceIndex].getName();

    std::vector<uint32_t>& uindices = _sliceDataset->getClusters()[_currentSliceIndex].getIndices();
    std::vector<int> indices;
    indices.assign(uindices.begin(), uindices.end());
    //qDebug() << "ExampleViewJSPlugin::updateSlice(): clusterName: " << clusterName << " uindices size: " << uindices.size();

    _onSliceIndices.clear();
    _onSliceIndices = indices;
    //qDebug() << "ExampleViewJSPlugin::updateSlice(): _onSliceIndices size: " << _onSliceIndices.size(); 
    
    _dataStore.createDataView(indices);
    updateSelectedDim();

    // update floodfill mask on 2D

    if (_isFloodIndex.empty()) {
        qDebug() << "ExampleViewJSPlugin::updateSlice(): _isFloodIndex is empty";
    }
    else {
        _isFloodOnSlice.clear(); // TO DO: repeated code with updateFloodFill()
        _isFloodOnSlice.resize(_onSliceIndices.size(), false);
        for (int i = 0; i < _onSliceIndices.size(); i++)
        {
            _isFloodOnSlice[i] = _isFloodIndex[_onSliceIndices[i]];
        }
    }    

    updateScatterOpacity();
    updateScatterColors();

}

// serialization
void ExampleViewJSPlugin::fromVariantMap(const QVariantMap& variantMap)
{
    qDebug() << "ExampleViewJSPlugin::fromVariantMap() 1 ";
    _loadingFromProject = true;

    ViewPlugin::fromVariantMap(variantMap);

    _avgExprStatus = static_cast<AvgExpressionStatus>(variantMap["AvgExpressionStatus"].toInt());
    qDebug() << "ExampleViewJSPlugin::fromVariantMap() 2 ";

    if (_avgExprStatus != AvgExpressionStatus::NONE)
    {
        const auto clusterNamesAvgExprVariant = variantMap["clusterNamesAvgExpr"].toMap();
        _clusterNamesAvgExpr.clear();
        _clusterNamesAvgExpr.resize(clusterNamesAvgExprVariant.size());
        for (const QString& key : clusterNamesAvgExprVariant.keys()) {
            int index = key.toInt();
            if (index >= 0 && index < clusterNamesAvgExprVariant.size()) {
                _clusterNamesAvgExpr[index] = clusterNamesAvgExprVariant[key].toString();
            }
        }
        qDebug() << ">>>>>ExampleViewJSPlugin::fromVariantMap(): clusterNamesAvgExpr.size();" << _clusterNamesAvgExpr.size();
        qDebug() << ">>>>>ExampleViewJSPlugin::fromVariantMap(): clusterNamesAvgExpr[0];" << _clusterNamesAvgExpr[0];
    }

    variantMapMustContain(variantMap, "SettingsAction");
    _settingsAction.fromVariantMap(variantMap["SettingsAction"].toMap());

    qDebug() << "ExampleViewJSPlugin::fromVariantMap() 3 ";

    QString clusterScalarsId = variantMap["ClusterScalars"].toString();
    _clusterScalars = mv::data().getDataset<Points>(clusterScalarsId);

    if (_sliceDataset.isValid())
    {
        qDebug() << "ExampleViewJSPlugin::fromVariantMap() 4 ";
        _currentSliceIndex = variantMap["CurrentSliceIdx"].toInt();
        updateSlice();
    }

    _loadingFromProject = false;

}

QVariantMap ExampleViewJSPlugin::toVariantMap() const
{
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _primaryToolbarAction.insertIntoVariantMap(variantMap);
    _settingsAction.insertIntoVariantMap(variantMap);

    variantMap.insert("AvgExpressionStatus", static_cast<int>(_avgExprStatus));

    variantMap.insert("ClusterScalars", _clusterScalars.getDatasetId());

    if (_sliceDataset.isValid())
    {
        variantMap.insert("CurrentSliceIdx", _currentSliceIndex);
    }

    if (_avgExprStatus != AvgExpressionStatus::NONE)
    {
        // TO DO: can use populateVariantMapFromDataBuffer()
        QVariantMap clusterNamesAvgExprVariant;
        for (size_t i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
            clusterNamesAvgExprVariant[QString::number(i)] = _clusterNamesAvgExpr[i];
        }
        variantMap.insert("clusterNamesAvgExpr", clusterNamesAvgExprVariant);
    }

    return variantMap;
}


// =============================================================================
// Plugin Factory 
// =============================================================================

QIcon ExampleViewJSPluginFactory::getIcon(const QColor& color /*= Qt::black*/) const
{
    return mv::Application::getIconFont("FontAwesome").getIcon("bullseye", color);
}

ViewPlugin* ExampleViewJSPluginFactory::produce()
{
    return new ExampleViewJSPlugin(this);
}

mv::DataTypes ExampleViewJSPluginFactory::supportedDataTypes() const
{
    // This example analysis plugin is compatible with points datasets
    DataTypes supportedTypes;
    supportedTypes.append(PointType);
    return supportedTypes;
}

mv::gui::PluginTriggerActions ExampleViewJSPluginFactory::getPluginTriggerActions(const mv::Datasets& datasets) const
{
    PluginTriggerActions pluginTriggerActions;

    const auto getPluginInstance = [this]() -> ExampleViewJSPlugin* {
        return dynamic_cast<ExampleViewJSPlugin*>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets >= 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {
        auto pluginTriggerAction = new PluginTriggerAction(const_cast<ExampleViewJSPluginFactory*>(this), this, "Example JS", "View JavaScript visualization", getIcon(), [this, getPluginInstance, datasets](PluginTriggerAction& pluginTriggerAction) -> void {
            for (auto dataset : datasets)
                getPluginInstance()->loadData(Datasets({ dataset }));

            });

        pluginTriggerActions << pluginTriggerAction;
    }

    return pluginTriggerActions;
}


