#include "GeneSurferPlugin.h"

#include "ChartWidget.h"
#include "ScatterView.h"

#include <DatasetsMimeData.h>

#include <vector>
#include <random>
#include <set>
#include <unordered_map>

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#include "Compute/DataTransformations.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QMimeData>
#include <QDebug>
#include <QSplitter>

// for reading hard-coded csv files
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <chrono>



Q_PLUGIN_METADATA(IID "nl.BioVault.GeneSurferPlugin")

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

GeneSurferPlugin::GeneSurferPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _nclust(3),
    _positionDataset(),
    _scatterViews(6 + 1, nullptr),// TO DO: hard code max 6 scatterViews
    _dimView(nullptr),
    _positions(),
    _positionSourceDataset(),
    _numPoints(0),
    _colorScalars(_nclust, std::vector<float>(_numPoints, 0.0f)),
    _chartWidget(nullptr),
    _tableWidget(nullptr),
    _client(nullptr),
    _dropWidget(nullptr),
    _settingsAction(this, "Settings Action"),
    _primaryToolbarAction(this, "PrimaryToolbar"),
    _secondaryToolbarAction(this, "SecondaryToolbar"),
    _tertiaryToolbarAction(this, "TertiaryToolbar"),
    _selectedDimIndex(-1),
    _selectedClusterIndex(-1), // -1 means no view selected
    _colorMapAction(this, "Color map", "RdYlBu")

{
    _primaryToolbarAction.addAction(&_settingsAction.getClusteringAction(), 1, GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_settingsAction.getDimensionSelectionAction(), 2, GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_settingsAction.getCorrelationModeAction(), -1, GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_settingsAction.getSingleCellModeAction());

    _secondaryToolbarAction.addAction(&_settingsAction.getEnrichmentAction());

    _tertiaryToolbarAction.addAction(&_settingsAction.getSectionAction(),1, GroupAction::Horizontal);
    _tertiaryToolbarAction.addAction(&_settingsAction.getPositionAction(), -1, GroupAction::Horizontal);
    _tertiaryToolbarAction.addAction(&_settingsAction.getPointPlotAction(), -1, GroupAction::Horizontal);

    for (int i = 0; i < 6; i++)//TO DO: hard code max 6 scatterViews
    {
        _scatterViews[i] = new ScatterView(this);
    }

    _dimView = new ScatterView(this);

    for (int i = 0; i < 6; i++) {//TO DO: hard code max 6 scatterViews
        connect(_scatterViews[i], &ScatterView::initialized, this, [this, i]() {_scatterViews[i]->setColorMap(_colorMapAction.getColorMapImage().mirrored(false, true)); });
        connect(_scatterViews[i], &ScatterView::viewSelected, this, [this, i]() { _selectedClusterIndex = i; updateClick(); });
    }

    connect(_dimView, &ScatterView::initialized, this, [this]() {_dimView->setColorMap(_colorMapAction.getColorMapImage().mirrored(false, true)); });
    connect(_dimView, &ScatterView::viewSelected, this, [this]() { _selectedClusterIndex = 6; updateClick(); });  
}

void GeneSurferPlugin::init()
{
    getWidget().setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    // Create layout
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_primaryToolbarAction.createWidget(&getWidget()));

    // Create the splitter for the barchart and tableDimLayout
    QSplitter* splitterChartTable = new QSplitter(Qt::Horizontal, &getWidget());

    // Create barchart widget and set html contents of webpage 
    _chartWidget = new ChartWidget(this);
    _chartWidget->setPage(":gene_surfer/chart/bar_chart.html", "qrc:/gene_surfer/chart/");
    _chartWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    // Add label for filtering on top of the barchart
    _filterLabel = new QLabel(_chartWidget);
    QFont sansFont("Helvetica [Cronyx]", 18);
    _filterLabel->setFont(sansFont);   
    _filterLabel->setGeometry(10, 10, 400, 30);
    _filterLabel->setText("Filter genes by:" + _corrFilter.getCorrFilterTypeAsString());
    
    _tableWidget = new MyTableWidget();


    auto widget = _secondaryToolbarAction.createWidget(&getWidget());
    widget->setMinimumWidth(10);// Fix the width of the secondary toolbar for splitter to work properly

    // Container widget for table and dim view
    QWidget* tableDimWidget = new QWidget();
    auto tableDimLayout = new QVBoxLayout(tableDimWidget);
    tableDimLayout->addWidget(widget);
    tableDimLayout->addWidget(_tableWidget, 50);
    tableDimLayout->addWidget(_dimView, 50);
    //tableDimWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    // Add widgets to splitter, set stretch factors and add splitter to layout
    splitterChartTable->addWidget(_chartWidget);
    splitterChartTable->addWidget(tableDimWidget);
    splitterChartTable->setStretchFactor(0, 60);
    splitterChartTable->setStretchFactor(1, 40);
    splitterChartTable->setChildrenCollapsible(false);

    layout->addWidget(splitterChartTable, 50);// same stretch factor as clusterViewMainLayout


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

    layout->addWidget(_tertiaryToolbarAction.createWidget(&getWidget()));

    layout->addLayout(clusterViewMainLayout, 50);// same stretch factor as splitter

    // Apply the layout
    getWidget().setLayout(layout);

    // Instantiate new drop widget: See ExampleViewPlugin for details
    _dropWidget = new DropWidget(_chartWidget);
    _dropWidget->setDropIndicatorWidget(new DropWidget::DropIndicatorWidget(&getWidget(), "No data loaded", "Drag the GeneSurferData in this view"));

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
                        updateSlice(0);// TODO: hard code 0?? March29
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
    connect(&_positionDataset, &Dataset<Points>::changed, this, &GeneSurferPlugin::positionDatasetChanged);

    // update data when data set changed
    //connect(&_positionDataset, &Dataset<Points>::dataChanged, this, &GeneSurferPlugin::convertDataAndUpdateChart);

    // Update the selection from JS
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passSelectionToCore, this, &GeneSurferPlugin::publishSelection);

    connect(&_floodFillDataset, &Dataset<Points>::dataChanged, this, [this]() {
        // Use the flood fill dataset to update the cell subset
        
        // update flag for point selection
        _selectedByFlood = true;
        //qDebug() << "_selectedByFlood = true";

        //qDebug() << ">>>>>GeneSurferPlugin::_floodFillDataset::dataChanged";
        if (!_sliceDataset.isValid()) {
            _computeSubset.updateFloodFill(_floodFillDataset, _numPoints, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex);
        }
        else {
            _computeSubset.updateFloodFill(_floodFillDataset, _numPoints, _onSliceIndices, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex, _isFloodOnSlice, _onSliceFloodIndices);
        }      
        updateSelection();
        });

    connect(&_positionDataset, &Dataset<Points>::dataSelectionChanged, this, [this]() {
        // Use the selected metadata to update the cell subset
        auto selection = _positionDataset->getSelection<Points>();
        
        if (selection->indices.size() <= 1) {
            return;
        }
        //qDebug() << "GeneSurferPlugin::_positionDataset::dataSelectionChanged(): selected indices size: " << selection->indices.size();
        //qDebug() << "Before computeSubset" << "_sliceDataset.isValid = " << _sliceDataset.isValid();

        // update flag for point selection
        _selectedByFlood = false;
        //qDebug() << "_selectedByFlood = false";

        if (!_sliceDataset.isValid()) 
        {
            //qDebug() << "Before computeSubset 2D";
            _computeSubset.updateSelectedData(_positionDataset, selection, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex);
        }
        else 
        {
            //qDebug() << "Before computeSubset 3D";
            _computeSubset.updateSelectedData(_positionDataset, selection, _onSliceIndices, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex, _isFloodOnSlice, _onSliceFloodIndices); 
            // TODO check if _onSliceFloodIndices is needed
            /*qDebug() << "_sortedFloodIndices.size = " << _sortedFloodIndices.size();
            qDebug() << "_onSliceFloodIndices.size = " << _onSliceFloodIndices.size();*/
        }

        updateSelection();
     });

    _client = new EnrichmentAnalysis(this);
    connect(_client, &EnrichmentAnalysis::enrichmentDataReady, this, &GeneSurferPlugin::updateEnrichmentTable);
    connect(_client, &EnrichmentAnalysis::enrichmentDataNotExists, this, &GeneSurferPlugin::noDataEnrichmentTable);

    connect(_tableWidget, &QTableWidget::cellClicked, this, &GeneSurferPlugin::onTableClicked); // on table clicked
}

void GeneSurferPlugin::loadData(const mv::Datasets& datasets)
{
    // Exit if there is nothing to load
    if (datasets.isEmpty())
        return;

    // Load the first dataset
    _positionDataset = datasets.first();
}

void GeneSurferPlugin::positionDatasetChanged()
{
    if (!_positionDataset.isValid())
        return;

    qDebug() << "GeneSurferPlugin::positionDatasetChanged(): New data dropped";

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

    //qDebug() << "GeneSurferPlugin::positionDatasetChanged(): enabledDimensions size: " << _enabledDimNames.size();

    if (!_clusterScalars.isValid())
    {
        if (!_loadingFromProject)
        {
            _clusterScalars = mv::data().createDataset<Points>("Points", "Scalars");
            events().notifyDatasetAdded(_clusterScalars);
            // initialize the clusterScalars dataset with 0s
            std::vector<float> tempInit(_numPoints, 0.0f);
            _clusterScalars->setData(tempInit.data(), tempInit.size(), 1);
        }
    }

    qDebug() << "GeneSurferPlugin::positionDatasetChanged(): start converting dataset ... ";
    convertToEigenMatrix(_positionDataset, _positionSourceDataset, _dataStore.getBaseData());
    convertToEigenMatrixProjection(_positionDataset, _dataStore.getBaseFullProjection());

    standardizeData(_dataStore.getBaseData(), _dataStore.getVariances()); // getBaseData() is standardized here TO DO: temporarily disabled
    //normalizeDataEigen(_dataStore.getBaseData(), _dataStore.getBaseNormalizedData());TO DO: getBaseData() or getBaseNormalizedData()
    qDebug() << "GeneSurferPlugin::positionDatasetChanged(): finish converting dataset ... ";

    _dataStore.createDataView();
    updateSelectedDim();

    updateFloodFillDataset();

    _dataInitialized = true;

}

void GeneSurferPlugin::convertDataAndUpdateChart()
{
    if (!_positionDataset.isValid()) {
        qDebug() << "GeneSurferPlugin::convertDataAndUpdateChart: No data to convert";
        return;
    }

    if (_toClearBarchart) {
        qDebug() << "GeneSurferPlugin::convertDataAndUpdateChart: Clear barchart";
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

    for (size_t i = 0; i < _enabledDimNames.size(); ++i) {
        if (_dimNameToClusterLabel.find(_enabledDimNames[i]) != _dimNameToClusterLabel.end()) {
            filteredAndSortedGenes.emplace_back(_enabledDimNames[i], _corrGeneVector[i]);
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

    QVariantMap payloadMap;
    payloadMap["data"] = payload;
    if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::MORAN)
        payloadMap["FilterType"] = "Moran";
    else 
        payloadMap["FilterType"] = "Others";


    qDebug() << "GeneSurferPlugin::convertDataAndUpdateChart: Send data from Qt cpp to D3 js";
    emit _chartWidget->getCommunicationObject().qt_js_setDataAndPlotInJS(payloadMap);
}

void GeneSurferPlugin::publishSelection(const QString& selection)
{
    _selectedDimName = selection;
    updateDimView(_selectedDimName);

    //qDebug() << "GeneSurferPlugin::publishSelection: selection: " << selection;
}

QString GeneSurferPlugin::getCurrentDataSetID() const
{
    if (_positionDataset.isValid())
        return _positionDataset->getId();
    else
        return QString{};
}

void GeneSurferPlugin::updateFloodFillDataset()
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

    //qDebug() << "GeneSurferPlugin::updateFloodFillDataset: dataSets size: " << _floodFillDataset->getNumPoints();

    if (_floodFillDataset->getNumPoints() == 0)
    {
        qDebug() << "Warning: No data in floodFillDataset named allFloodNodesIndices!";
        return;
    }
     
    // update flag for point selection
    _selectedByFlood = true;
    //qDebug() << "_selectedByFlood = true";

    if (!_sliceDataset.isValid()) {
        _computeSubset.updateFloodFill(_floodFillDataset, _numPoints, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex);
    }
    else {
        _computeSubset.updateFloodFill(_floodFillDataset, _numPoints, _onSliceIndices, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex, _isFloodOnSlice, _onSliceFloodIndices);
    }

    updateSelection();
}

void GeneSurferPlugin::updateSelectedDim() {
    int xDim = _settingsAction.getPositionAction().getXDimensionPickerAction().getCurrentDimensionIndex();
    int yDim = _settingsAction.getPositionAction().getYDimensionPickerAction().getCurrentDimensionIndex();

    //qDebug() << "GeneSurferPlugin::updateSelectedDim(): xDim: " << xDim << " yDim: " << yDim;

    _dataStore.createProjectionView(xDim, yDim);

    //qDebug()<< "GeneSurferPlugin::updateSelectedDim(): getProjectionSize()"<< _dataStore.getProjectionSize();

    _positions.clear();
    _positions.resize(_dataStore.getProjectionView().rows());

    for (int i = 0; i < _dataStore.getProjectionView().rows(); i++) {
        _positions[i].set(_dataStore.getProjectionView()(i, 0), _dataStore.getProjectionView()(i, 1));
    }

    updateViewData(_positions);
}

void GeneSurferPlugin::updateViewData(std::vector<Vector2f>& positions) {

    // TO DO: can save some time here only computing data bounds once
    // pass the 2d points to the scatter plot widget
    for (int i = 0; i < _nclust; i++) {// TO DO: hard code max 6 scatterViews
        _scatterViews[i]->setData(&positions);
    }

    _dimView->setData(&positions);

}

void GeneSurferPlugin::updateShowDimension() {
    int shownDimension = _settingsAction.getDimensionSelectionAction().getDimensionAction().getCurrentDimensionIndex();
    QString shownDimensionName = _settingsAction.getDimensionSelectionAction().getDimensionAction().getCurrentDimensionName();

    //qDebug() << "GeneSurferPlugin::updateShowDimension(): shownDimension: " << shownDimension;
    qDebug() << "GeneSurferPlugin::updateShowDimension(): shownDimensionName: " << shownDimensionName;

    if (shownDimension < 0) {
        qDebug() << "GeneSurferPlugin::updateShowDimension(): shownDimension < 0: " << shownDimension;
        return;
    }

    _selectedDimName = shownDimensionName;
    updateDimView(_selectedDimName);
}

void GeneSurferPlugin::computeAvgExpression() {
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
    
    //qDebug() << "GeneSurferPlugin::computeAvgExpression(): scSourceDataset name: " << scSourceDataset->getGuiName();
    //qDebug() << "GeneSurferPlugin::computeAvgExpression(): scSourceDataset numPoints: " << scSourceDataset->getNumPoints();
    
    int numPoints = scSourceDataset->getNumPoints();
    int numClusters = labelClusters.size();
    int numGenes = scSourceDataset->getNumDimensions();
    //qDebug() << "GeneSurferPlugin::computeAvgExpression(): numPoints: " << numPoints << " numClusters: " << numClusters << " numGenes: " << numGenes;

    Eigen::MatrixXf scSourceMatrix;
    convertToEigenMatrixProjection(scSourceDataset, scSourceMatrix);
    //qDebug() << "GeneSurferPlugin::computeAvgExpression(): scSourceMatrix size: " << scSourceMatrix.rows() << " " << scSourceMatrix.cols();

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
    //qDebug() << "GeneSurferPlugin::computeAvgExpression(): clusterToIndicesMap size: " << clusterToIndicesMap.size();

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

    //qDebug() << "GeneSurferPlugin::computeAvgExpression(): _avgExpr size: " << _avgExpr.rows() << " " << _avgExpr.cols();
    
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

    //qDebug() << "GeneSurferPlugin::computeAvgExpression(): _geneNamesAvgExpr size: " << _geneNamesAvgExpr.size();
    //qDebug() << "GeneSurferPlugin::computeAvgExpression(): _clusterNamesAvgExpr size: " << _clusterNamesAvgExpr.size();

    _clusterAliasToRowMap.clear();// _clusterAliasToRowMap: first element is label name, second element is row index in _avgExpr
    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
    }


    // Create and store the dataset
    if (!_avgExprDataset.isValid()) {
        //qDebug() << "avgExprDataset not valid";
        _avgExprDataset = mv::data().createDataset<Points>("Points", "avgExprDataset");
        events().notifyDatasetAdded(_avgExprDataset);
    }   
    _avgExprDataset->setData(allData.data(), numClusters, numGenes); 
    _avgExprDataset->setDimensionNames(_geneNamesAvgExpr);
    events().notifyDatasetDataChanged(_avgExprDataset);

    _avgExprDatasetExists = true;
    _settingsAction.getSingleCellModeAction().getSingleCellOptionAction().setEnabled(_avgExprDatasetExists);

    loadLabelsFromSTDataset();

    qDebug() << "computeAvgExpression() finished ";

}

void GeneSurferPlugin::loadAvgExpression() {
    qDebug() << "ONLY FOR ABCAtlas";

    loadAvgExpressionABCAtlas();// TO DO: hard coded only for ABC Atlas
    loadLabelsFromSTDatasetABCAtlas();

    _avgExprDatasetExists = true;

    // enable single cell toggle - TO DO: use signal or set directly
    //emit avgExprDatasetExistsChanged(_avgExprDatasetExists);
    _settingsAction.getSingleCellModeAction().getSingleCellOptionAction().setEnabled(_avgExprDatasetExists);

    qDebug() << "load AvgExpression finished ";

}

void GeneSurferPlugin::loadLabelsFromSTDataset() {
    // this is loading label from ST dataset!!!
    // Different from loading from singlecell datset!!!

    QString stParentName = _positionDataset->getParent()->getGuiName();
    qDebug() << "GeneSurferPlugin::loadLabelsFromSTDataset(): stParentName: " << stParentName;

    QString selectedDataName = _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentText();
    qDebug() << "GeneSurferPlugin::loadLabelsFromSTDataset(): selectedDataName: " << selectedDataName;

    Dataset<Clusters> labelDataset;
    
    for (const auto& data : mv::data().getAllDatasets())
    {
        //qDebug() << data->getGuiName();
        if (data->getGuiName() == selectedDataName) {
            qDebug() << "data->getParent()->getGuiName() " << data->getParent()->getGuiName();
            if (data->getParent()->getGuiName() == stParentName) {
                labelDataset = data;
                qDebug() << "GeneSurferPlugin::loadLabelsFromSTDataset(): labelDataset name: " << labelDataset->getGuiName();
                break;
            }
        }
    }

    QVector<Cluster> labelClusters = labelDataset->getClusters();

    qDebug() << "GeneSurferPlugin::loadLabelsFromSTDataset(): labelClusters size: " << labelClusters.size();

    // add weighting for each cluster of whole data
    _countsAll.resize(_clusterNamesAvgExpr.size()); // number of clusters in SC

    // precompute the cell-label array
    _cellLabels.clear();
    _cellLabels.resize(_numPoints);

    int numClustersNotInST = 0;

    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
        QString clusterName = _clusterNamesAvgExpr[i];
        bool found = false;

        //search for the cluster name in labelClusters
        for (const auto& cluster : labelClusters) {
            if (cluster.getName() == clusterName) {
                const auto& ptIndices = cluster.getIndices();
                for (int j = 0; j < ptIndices.size(); ++j) {
                    int ptIndex = ptIndices[j];
                    _cellLabels[ptIndex] = clusterName;
                }
                // add weighting for each cluster of whole data
                _countsAll[i] = ptIndices.size(); // number of pt in each cluster
                found = true;
                break;
            }
        }
        if (!found) {
            // If the cluster is not found in labelClusters
            //qDebug() << "GeneSurferPlugin::loadLabelsFromSTDataset(): cluster in SC" << clusterName << " not found in ST";
            numClustersNotInST++;
            _countsAll[i] = 0;
        }
    }

    qDebug() << "Warning! loadLabelsFromSTDataset: " << numClustersNotInST << " annotations not found in ST";
    //qDebug() << "GeneSurferPlugin::loadLabelsFromSTDataset(): _cellLabels size: " << _cellLabels.size();
}

void GeneSurferPlugin::setLabelDataset() {
    //qDebug() << _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentText();

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

void GeneSurferPlugin::updateEnrichmentAPI()
{
    _currentEnrichmentAPI = _settingsAction.getEnrichmentAction().getEnrichmentAPIPickerAction().getCurrentText();
    qDebug() << "Enrichment API changed to: " << _currentEnrichmentAPI;
    
    if (_dimNameToClusterLabel.size() == 0)
        {
        qDebug() << "GeneSurferPlugin::updateEnrichmentAPI(): _dimNameToClusterLabel is empty";
        return;
    }

    getFuntionalEnrichment();
}


void GeneSurferPlugin::setEnrichmentAPI(QString apiName)
{
    _settingsAction.getEnrichmentAction().getEnrichmentAPIPickerAction().setCurrentText(apiName);
}

void GeneSurferPlugin::setEnrichmentAPIOptions(QStringList options)
{
    _settingsAction.getEnrichmentAction().getEnrichmentAPIPickerAction().setOptions(options);
}


void GeneSurferPlugin::updateEnrichmentSpecies()
{
    QString selectedSpecies = _settingsAction.getEnrichmentAction().getSpeciesPickerAction().getCurrentText();

    if (selectedSpecies == "Mus musculus")
    {
        _currentEnrichmentSpecies = "mmusculus";
        qDebug() << "Enrichment species changed to: " << _currentEnrichmentSpecies;
        getFuntionalEnrichment();

    }
    else if (selectedSpecies == "Homo sapiens")
    {
        _currentEnrichmentSpecies = "hsapiens";
        qDebug() << "Enrichment species changed to: " << _currentEnrichmentSpecies;
        getFuntionalEnrichment();
    }
    /*else
        qDebug() << "ERROR: no valid species selected for enrichment analysis";*/ // TODO check if needed?
}

void GeneSurferPlugin::updateSelection()
{
    // clear table content
    _tableWidget->clearContents(); 

    if (!_positionDataset.isValid())
        return;

    if (_isFloodIndex.empty()) {
        qDebug() << "GeneSurferPlugin::updateSelection(): _isFloodIndex is empty";
        return;
    }

    //qDebug() << "GeneSurferPlugin::updateSelection(): start... ";
    //auto start = std::chrono::high_resolution_clock::now();

    ////////////////////
    // Compute subset //
    ////////////////////
    if (!_isSingleCell && !_sliceDataset.isValid()) {
        qDebug() << "Compute subset: 2D + ST";
        _computeSubset.computeSubsetData(_dataStore.getBaseData(), _sortedFloodIndices, _subsetData);
    }
    if (!_isSingleCell && _sliceDataset.isValid()) {
        qDebug() << "Compute subset: 3D + ST";
        //subset data only contains the onSliceFloodIndice
        //qDebug() << "GeneSurferPlugin::updateSelection(): _onSliceFloodIndices size: " << _onSliceFloodIndices.size();
        _computeSubset.computeSubsetData(_dataStore.getBaseData(), _onSliceFloodIndices, _subsetData); //TODO: check if needed
        //qDebug() << "GeneSurferPlugin::updateSelection(): _subsetData size: " << _subsetData.rows() << " " << _subsetData.cols();
        //subset data contains all floodfill indices     
        //qDebug() << "GeneSurferPlugin::updateSelection(): _sortedFloodIndices size: " << _sortedFloodIndices.size();
        _computeSubset.computeSubsetData(_dataStore.getBaseData(), _sortedFloodIndices, _subsetData3D);
        //qDebug() << "GeneSurferPlugin::updateSelection(): _subsetData3D size: " << _subsetData3D.rows() << " " << _subsetData3D.cols();
    }
    if (_isSingleCell && !_sliceDataset.isValid()) {
        qDebug() << "Compute subset: 2D + SingleCell";
        countLabelDistribution();
        _computeSubset.computeSubsetDataAvgExpr(_avgExpr, _clustersToKeep, _clusterAliasToRowMap, _subsetDataAvgOri);
        _subsetData.resize(_subsetDataAvgOri.rows(), _subsetDataAvgOri.cols());
        _subsetData = _subsetDataAvgOri;
    }
    if (_isSingleCell && _sliceDataset.isValid()) {
        qDebug() << "Compute subset: 3D + SingleCell";
        countLabelDistribution();
        _computeSubset.computeSubsetDataAvgExpr(_avgExpr, _clustersToKeep, _clusterAliasToRowMap, _subsetDataAvgOri);
        _subsetData3D.resize(_subsetDataAvgOri.rows(), _subsetDataAvgOri.cols());
        _subsetData3D = _subsetDataAvgOri;
    }

    
    //////////////////////////////////////////////
    // Compute correlation for filtering genes //
    /////////////////////////////////////////////
    
    // -------------- Diff --------------
    if (!_isSingleCell && !_sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::DIFF) {
        qDebug() << "Compute filtering: 2D + ST + Diff";
        _corrFilter.getDiffFilter().computeDiff(_subsetData, _dataStore.getBaseData(), _corrGeneVector);
    }
    if (!_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::DIFF) {
        qDebug() << "Compute filtering: 3D + ST + Diff";
        _corrFilter.getDiffFilter().computeDiff(_subsetData3D, _dataStore.getBaseData(), _corrGeneVector);
    }
    if (_isSingleCell && _corrFilter.getFilterType() == corrFilter::CorrFilterType::DIFF) {
        qDebug() << "Compute filtering: SingleCell +Diff";
        //_corrFilter.getDiffFilter().computeDiff(_subsetDataAvgOri, _avgExpr, _corrGeneVector); //without weighting
        
        // add weighting for number of cells in each cluster
        Eigen::VectorXf ratioCountsSubset = _countsSubset / _sortedFloodIndices.size() * _subsetDataAvgOri.rows();
        Eigen::VectorXf ratioCountsAll = _countsAll / _numPoints * _avgExpr.rows();

       /* qDebug() << "_countsSubset size: " << _countsSubset.size() << "_countsSubset[0]: " << _countsSubset[0] << "_sortedFloodIndices.size()" << _sortedFloodIndices.size() <<"_subsetDataAvgOri.rows()" << _subsetDataAvgOri.rows();
        qDebug() << "ratioCountsSubset[0]: " << ratioCountsSubset[0];
        qDebug() << "max element in _countsSubset: " << *std::max_element(_countsSubset.begin(), _countsSubset.end());
        qDebug() << "_countsAll size: " << _countsAll.size() << "_countsAll[0]: " << _countsAll[0];
        qDebug() << "ratioCountsAll[0]: " << ratioCountsAll[0];
        qDebug() << "max element in _countsAll: " << *std::max_element(_countsAll.begin(), _countsAll.end());

        qDebug() << "_subsetDataAvgOri size: " << _subsetDataAvgOri.rows() << " " << _subsetDataAvgOri.cols();
        qDebug() << "ratioCountsSubset size" << ratioCountsSubset.size();
        qDebug() << "_avgExpr size: " << _avgExpr.rows() << " " << _avgExpr.cols();
        qDebug() << "ratioCountsAll size" << ratioCountsAll.size();*/

        Eigen::MatrixXf weightedSubsetData = _subsetDataAvgOri.array().colwise() * ratioCountsSubset.array();
        Eigen::MatrixXf weightedAvgExpr = _avgExpr.array().colwise() * ratioCountsAll.array();
        _corrFilter.getDiffFilter().computeDiff(weightedSubsetData, weightedAvgExpr, _corrGeneVector);    
    }
    // -------------- Moran's I -------------- // TO DO: add weighting for SC
    
    // TODO temporary code only for 2D data and is very slow 
    if (!_isSingleCell && !_sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::MORAN) {
        qDebug() << "Compute filtering: 2D + ST + Moran";
        _corrFilter.getMoranFilter().computeMoranVector(_sortedFloodIndices, _subsetData, _positions, _corrGeneVector);
    }
    if (!_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::MORAN)
    {
        qDebug() << "Compute filtering: 3D + ST + Moran";
        std::vector<float> xPositions;
        _positionDataset->extractDataForDimension(xPositions, 2);
        std::vector<float> yPositions;
        _positionDataset->extractDataForDimension(yPositions, 1);
        std::vector<float> zPositions;
        _positionDataset->extractDataForDimension(zPositions, 0);
        _corrFilter.getMoranFilter().computeMoranVector(_sortedFloodIndices, _subsetData3D, xPositions, yPositions, zPositions, _corrGeneVector);
    }
    if (_isSingleCell && !_sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::MORAN)
    {
        qDebug() << "Compute filtering: 2D + SingleCell + Moran";
        DataMatrix populatedSubsetAvg = populateAvgExprToSpatial();
        _corrFilter.getMoranFilter().computeMoranVector(_sortedFloodIndices, populatedSubsetAvg, _positions, _corrGeneVector);
     }
    if (_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::MORAN)
    {
        qDebug() << "Compute filtering: 3D + SingleCell + Moran";
        std::vector<float> xAvg;
        std::vector<float> yAvg;
        std::vector<float> zAvg;
        computeMeanCoordinatesByCluster(xAvg, yAvg, zAvg);
        /*qDebug() << "GeneSurferPlugin::updateSelection(): xAvg size: " << xAvg.size() << "yAvg size " << yAvg.size() << "zAvg size " << zAvg.size();
        qDebug() << "GeneSurferPlugin::updateSelection(): _subsetDataAvgOri size: " << _subsetDataAvgOri.rows() << " " << _subsetDataAvgOri.cols();
        qDebug() << "xAvg[0] " << xAvg[0] << "yAvg[0] " << yAvg[0] << "zAvg[0] " << zAvg[0];
        qDebug() << "_subsetDataAvgOri(0, 0) " << _subsetDataAvgOri(0, 0);*/

        _corrFilter.getMoranFilter().computeMoranVector(_subsetDataAvgOri, xAvg, yAvg, zAvg, _corrGeneVector);
    }

    // -------------- Spatial z --------------
    if (!_sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALZ) {
        //qDebug() << "Compute filtering: 2D + ST + SpatialCorrZ";
        qDebug() << "ERROR: no z axis in 2D dataset";
        return;
    }
    if (!_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALZ) {
        qDebug() << "Compute filtering: 3D + ST + SpatialZ";
        std::vector<float> zPositions;
        _positionDataset->extractDataForDimension(zPositions, 0);
        _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_sortedFloodIndices, _subsetData3D, zPositions, _corrGeneVector);
    }
    if (_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALZ) {
        qDebug() << "Compute filtering: 3D + SingleCell + SpatialCorrZ";
        std::vector<float> xAvg;
        std::vector<float> yAvg;
        std::vector<float> zAvg;
        computeMeanCoordinatesByCluster(xAvg, yAvg, zAvg);
        //_corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_subsetDataAvgOri, zAvg, _corrGeneVector);// without weighting
        _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_subsetDataAvgOri, zAvg, _countsSubset, _corrGeneVector);// with weighting
    }
    // -------------- Spatial y --------------
    if (!_isSingleCell && !_sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALY) {
        qDebug() << "Compute filtering: 2D + ST + SpatialCorrY";
        std::vector<float> yPositions;
        _positionDataset->extractDataForDimension(yPositions, 1);
        _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_sortedFloodIndices, _subsetData, yPositions, _corrGeneVector);
    }
    if (!_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALY) {
        qDebug() << "Compute filtering: 3D + ST + SpatialCorrY";

        std::vector<float> yPositions;
        _positionDataset->extractDataForDimension(yPositions, 1);
        _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_sortedFloodIndices, _subsetData3D, yPositions, _corrGeneVector);
    }
    if (_isSingleCell && !_sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALY) {
        qDebug() << "Compute filtering: 2D + SingleCell + SpatialCorrY";
        std::vector<float> yPositions;
        _positionDataset->extractDataForDimension(yPositions, 1);
        DataMatrix populatedSubsetAvg = populateAvgExprToSpatial();
        _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_sortedFloodIndices, populatedSubsetAvg, yPositions, _corrGeneVector);// no need for weighting
    }
    if (_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALY) {
        qDebug() << "Compute filtering: 3D + SingleCell + SpatialCorrY";
        std::vector<float> xAvg;
        std::vector<float> yAvg;
        std::vector<float> zAvg;
        computeMeanCoordinatesByCluster(xAvg, yAvg, zAvg);
        // _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_subsetDataAvgOri, yAvg, _corrGeneVector);// without weighting
        _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_subsetDataAvgOri, yAvg, _countsSubset, _corrGeneVector);// with weighting
    }
    

    ////////////////////
    // Clustering //
    ////////////////////
    clusterGenes();
    qDebug() << "updateSelection(): data clustered";


    ////////////////////
    // Update Plots //
    ////////////////////
    for (const auto& pair : _numGenesInCluster) {
        QString clusterIdx = QString::number(pair.first);
        QString numGenesInThisCluster = QString::number(pair.second);
        _scatterViews[pair.first]->setProjectionName("Cluster " + clusterIdx + " (" + numGenesInThisCluster + " genes)");
    }

    convertDataAndUpdateChart();
    updateScatterColors();
    updateScatterOpacity();

    /*auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "updateSelection() Elapsed time: " << elapsed.count() << " ms\n";*/

    //qDebug() << "GeneSurferPlugin::updateSelection(): end... ";
}

DataMatrix GeneSurferPlugin::populateAvgExprToSpatial() {
    // populate the data in subset for singlecell option
    DataMatrix populatedSubsetAvg(_sortedFloodIndices.size(), _geneNamesAvgExpr.size());

#pragma omp parallel for
    for (int i = 0; i < _sortedFloodIndices.size(); ++i)
    {
        int index = _sortedFloodIndices[i];
        QString label = _cellLabels[index];
        auto row = _avgExpr(_clusterAliasToRowMap[label], Eigen::all);
        populatedSubsetAvg.row(i) = row;
    }

    qDebug() << "populatedSubsetAvg size: " << populatedSubsetAvg.rows() << " " << populatedSubsetAvg.cols();

    return populatedSubsetAvg;
}

void GeneSurferPlugin::computeMeanWaveNumbersByCluster(std::vector<float>& waveAvg) {
    std::unordered_map<QString, int> clusterWaveNumberSums;

    for (int index = 0; index < _sortedFloodIndices.size(); ++index) {
        int ptIndex = _sortedFloodIndices[index];
        QString label = _cellLabels[ptIndex];
        clusterWaveNumberSums[label] += _sortedWaveNumbers[index]; // for computing the average wave number
    }

    for (int i = 0; i < _clustersToKeep.size(); ++i) {
        QString label = _clustersToKeep[i];
        int count = _countsMap[label];
        float average = (count > 0) ? static_cast<float>(clusterWaveNumberSums[label]) / count : 0.0f;
        waveAvg.push_back(average);
    }

    qDebug() << "computeMeanWaveNumbersByCluster(): waveAvg size: " << waveAvg.size();
}

void GeneSurferPlugin::computeMeanCoordinatesByCluster(std::vector<float>& xAvg, std::vector<float>& yAvg, std::vector<float>& zAvg) {
    std::unordered_map<QString, float> clusterXSums; 
    std::unordered_map<QString, float> clusterYSums;
    std::unordered_map<QString, float> clusterZSums;

    std::vector<float> xPositions;
    _positionDataset->extractDataForDimension(xPositions, 2);

    std::vector<float> yPositions;
    _positionDataset->extractDataForDimension(yPositions, 1);
    std::vector<float> zPositions;
    _positionDataset->extractDataForDimension(zPositions, 0);

    qDebug() << "computeMeanCoordinatesByCluster(): _sortedFloodIndices.size(): " << _sortedFloodIndices.size();

    for (int index = 0; index < _sortedFloodIndices.size(); ++index) {
        int ptIndex = _sortedFloodIndices[index];

        if (ptIndex >= zPositions.size())

           qDebug() << "ERROR! ptIndex " << ptIndex << " >= zPositions.size() " << zPositions.size();


        QString label = _cellLabels[ptIndex];

        clusterXSums[label] += xPositions[ptIndex];
        clusterYSums[label] += yPositions[ptIndex];
        clusterZSums[label] += zPositions[ptIndex];
    }

    xAvg.clear();
    yAvg.clear();
    zAvg.clear();

    /*qDebug() << "computeMeanCoordinatesByCluster(): _clustersToKeep.size(): " << _clustersToKeep.size();
    qDebug() << "_clustersToKeep[0]" << _clustersToKeep[0];
    qDebug() << "_clustersToKeep[_clustersToKeep.size()-1] " << _clustersToKeep[_clustersToKeep.size() - 1];
    qDebug() << "_countsMap.size(): " << _countsMap.size();*/

    for (int i = 0; i < _clustersToKeep.size(); ++i) {
        QString label = _clustersToKeep[i];
        int count = _countsMap[label];

        float averageX = (count > 0) ? static_cast<float>(clusterXSums[label]) / count : 0.0f;
        xAvg.push_back(averageX);

        float averageY = (count > 0) ? static_cast<float>(clusterYSums[label]) / count : 0.0f;
        yAvg.push_back(averageY);

        float averageZ = (count > 0) ? static_cast<float>(clusterZSums[label]) / count : 0.0f;
        zAvg.push_back(averageZ);
    }

    // output for manual check
    // ---------------------------
    /*for (int i = 0; i < xAvg.size(); ++i) {
        qDebug() << "GeneSurferPlugin::computeMeanCoordinatesByCluster(): cluster: " << _clustersToKeep[i] << " x: " << xAvg[i] << " y: " << yAvg[i] << " z: " << zAvg[i];
    }
    for (int i = 0; i < xAvg.size(); ++i) {
        float stdDevX = 0.0f;
        float stdDevY = 0.0f;
        float stdDevZ = 0.0f;
        for (int index = 0; index < _sortedFloodIndices.size(); ++index) {
            int ptIndex = _sortedFloodIndices[index];
            QString label = _cellLabels[ptIndex];

            if (label == _clustersToKeep[i]) {
                stdDevX += pow(_positions[ptIndex].x - xAvg[i], 2);
                stdDevY += pow(_positions[ptIndex].y - yAvg[i], 2);
                stdDevZ += pow(zPositions[ptIndex] - zAvg[i], 2);
            }
        }
        stdDevX = sqrt(stdDevX / _countsMap[_clustersToKeep[i]]);
        stdDevY = sqrt(stdDevY / _countsMap[_clustersToKeep[i]]);
        stdDevZ = sqrt(stdDevZ / _countsMap[_clustersToKeep[i]]);       
        qDebug() << "GeneSurferPlugin::computeMeanCoordinatesByCluster(): cluster: " << _clustersToKeep[i] << " stdDevX: " << stdDevX << " stdDevY: " << stdDevY << " stdDevZ: " << stdDevZ;
    }*/
}

void GeneSurferPlugin::updateSingleCellOption() {
    //qDebug() << "GeneSurferPlugin::updateSingleCellOption(): start... ";

    _settingsAction.getSingleCellModeAction().getSingleCellOptionAction().isChecked() ? _isSingleCell = true : _isSingleCell = false;
    //qDebug() << "GeneSurferPlugin::updateSingleCellOption(): _isSingleCell: " << _isSingleCell;

    if (_isSingleCell) {
        //qDebug() << "Using Single Cell";

        if (_avgExpr.size() == 0) {
            /*qDebug() << "GeneSurferPlugin::updateSingleCellOption(): _avgExpr is empty";
            qDebug() << "_loadingFromProject = " << _loadingFromProject;
            qDebug() << "_avgExprDataset.isValid() = " << _avgExprDataset.isValid();*/

            if (_avgExprDataset.isValid()) {
                //qDebug() << "_avgExprDataset is valid...";
                switch (_avgExprStatus)
                {
                case AvgExpressionStatus::NONE:
                    //qDebug() << "Status: NONE";
                    break;
                case AvgExpressionStatus::COMPUTED:
                    //qDebug() << "Status: COMPUTED";

                    // in oder to avoid computing again - TO DO: seperate this part in the function
                    //qDebug() << "_avgExprDataset.isValid() = " << _avgExprDataset.isValid();
                    convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);
                    _geneNamesAvgExpr.clear();
                    _geneNamesAvgExpr = _avgExprDataset->getDimensionNames();

                    _clusterAliasToRowMap.clear();
                    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
                        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
                    }
                    loadLabelsFromSTDataset();
                    break;
                case AvgExpressionStatus::LOADED:
                    //qDebug() << "Status: LOADED";

                    // in oder to avoid computing again - TO DO: seperate this part in the function
                    //qDebug() << "_avgExprDataset.isValid() = " << _avgExprDataset.isValid();
                    convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);
                    _geneNamesAvgExpr.clear();
                    _geneNamesAvgExpr = _avgExprDataset->getDimensionNames();

                    _clusterAliasToRowMap.clear();
                    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
                        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
                    }
                    loadLabelsFromSTDatasetABCAtlas();

                    break;
                }
            }
            else {
                return;
            }         
        } 
   
        setDimensionNamesForDimensionPicker();
        
        // update _enabledDimNames
        _enabledDimNames.clear();
        _enabledDimNames = _geneNamesAvgExpr;

        // update max number of genes in _numGenesThresholdAction
        _settingsAction.getClusteringAction().getNumGenesThresholdAction().setMaximum(_enabledDimNames.size());

        updateSelection();
    }
    else {
        //qDebug() << "Using Spatial";
        setDimensionNamesForDimensionPicker();
        
        const auto& dimNames = _positionSourceDataset->getDimensionNames();
        auto enabledDimensions = _positionSourceDataset->getDimensionsPickerAction().getEnabledDimensions();
        _enabledDimNames.clear();
        for (int i = 0; i < enabledDimensions.size(); i++)
        {
            if (enabledDimensions[i])
                _enabledDimNames.push_back(dimNames[i]);
        }

        // update max number of genes in _numGenesThresholdAction
        _settingsAction.getClusteringAction().getNumGenesThresholdAction().setMaximum(_enabledDimNames.size());

        updateSelection();
    }
}

void GeneSurferPlugin::setDimensionNamesForDimensionPicker() {
    // Work around for dimensionPickerAction
    // sort the strings to avoid the issue of the original dimensionPickerAction: 
    // e.g.select "Hoxb5" but "Hoxb5os" was selected because "Hoxb5os" was the first matched one in the list

    std::vector<QString> sortedGeneNames;

    if (_isSingleCell) {
        sortedGeneNames = _geneNamesAvgExpr;
    }
    else {
        sortedGeneNames = _positionSourceDataset->getDimensionNames();
    }
    //qDebug() << "GeneSurferPlugin::setDimensionNamesForDimensionPicker(): sortedGeneNames size: " << sortedGeneNames.size();

    // sort the gene names, to make the shortest in the front if one is a prefix of the other
    std::sort(sortedGeneNames.begin(), sortedGeneNames.end(), [](const QString& a, const QString& b) {
        // Compare alphabetically first
        int minLength = std::min(a.length(), b.length());
        for (int i = 0; i < minLength; i++) {
            if (a[i] != b[i]) {
                return a[i] < b[i];
            }
        }
        // If one is a prefix of the other, or they are identical up to the minLength, sort by length
        return a.length() < b.length();
     });
    //qDebug() << "GeneSurferPlugin::setDimensionNamesForDimensionPicker(): sortedGeneNames sorted";

    QStringList sortedGeneNamesList;
    for (const auto& str : sortedGeneNames) {
        sortedGeneNamesList.append(str);
    }
    //qDebug() << "GeneSurferPlugin::setDimensionNamesForDimensionPicker(): sortedGeneNamesList size: " << sortedGeneNamesList.size();

    _settingsAction.getDimensionSelectionAction().getDimensionAction().setDimensionNames(sortedGeneNamesList);
    _settingsAction.getDimensionSelectionAction().getDimensionAction().setCurrentDimensionIndex(-1);

}

void GeneSurferPlugin::updateNumCluster()
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
        qDebug() << "GeneSurferPlugin::updateNumCluster(): _isFloodIndex is empty";
        return;
    }

    updateViewData(_positions);
    updateScatterPointSize();
    updateSelection();
}

void GeneSurferPlugin::updateCorrThreshold() {
    _numGenesThreshold = _settingsAction.getClusteringAction().getNumGenesThresholdAction().getValue();

    // cannot be changed before plotting
    if (_isFloodIndex.empty()) {
        qDebug() << "GeneSurferPlugin::updateCorrThreshold: _isFloodIndex is empty";
        return;
    }

    updateSelection();
}

void GeneSurferPlugin::updateScatterPointSize()
{
    for (int i = 0; i < _nclust; i++) {
        _scatterViews[i]->setSourcePointSize(_settingsAction.getPointPlotAction().getPointSizeAction().getValue());
    }
    _dimView->setSourcePointSize(_settingsAction.getPointPlotAction().getPointSizeAction().getValue());
}

void GeneSurferPlugin::updateFilterLabel()
{
    _filterLabel->setText("Filter genes by:" + _corrFilter.getCorrFilterTypeAsString());
}


void GeneSurferPlugin::updateScatterOpacity()
{
    if (!_positionDataset.isValid())
        return;

    if (_isFloodIndex.empty()) {
        qDebug() << "GeneSurferPlugin::updateScatterOpacity: _isFloodIndex is empty";
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
}

void GeneSurferPlugin::updateScatterColors()
{
    if (!_positionDataset.isValid())
        return;

    // Clear clicked frame - _scatterViews
    if (_selectedClusterIndex >= 0 && _selectedClusterIndex < _nclust) {
        _scatterViews[_selectedClusterIndex]->selectView(false);
        _selectedClusterIndex = -1;// reset the selected cluster index to -1
        //qDebug() << "_selectedClusterIndex" << _selectedClusterIndex;
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
            qDebug() << "GeneSurferPlugin::updateScatterColors: _colorScalars[0] is empty";
            return;
        }

        for (int j = 0; j < _nclust; j++) {
            if (j >= _colorScalars.size()) {
                qDebug() << "GeneSurferPlugin::updateScatterColors: Row index out of range: j=" << j;
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
                    //break; // TO DO: remove this break statement
                }
            }
            _scatterViews[j]->setScalars(viewScalars, selection[0]);
        }
    }
}

void GeneSurferPlugin::updateDimView(const QString& selectedDimName)
{
    // Clear clicked frame - _dimView
    if (_selectedClusterIndex == 6) {
        _dimView->selectView(false);
        _selectedClusterIndex = -1;// reset the selected cluster index to -1
        //qDebug() << "_selectedClusterIndex" << _selectedClusterIndex;
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

        //qDebug() << "Access _positionSourceData";

        // TO DO: remove debugging info
        if (_selectedDimIndex == -1)
        {
            qDebug() << "Warning! updateDimView(): _selectedDimIndex is -1";
            return;
        }
            
        _positionSourceDataset->extractDataForDimension(dimV, _selectedDimIndex);// align with _positionSourceDataset
    }
    else {
        for (int i = 0; i < _enabledDimNames.size(); ++i) {
            if (_enabledDimNames[i] == selectedDimName) {
                _selectedDimIndex = i;
                break;
            }
        }

        // TO DO: remove debugging info
        if (_selectedDimIndex == -1)
        {
            qDebug() << "Warning! updateDimView(): _selectedDimIndex is -1";
            return;
        }

        dimValues = _avgExpr(Eigen::all, _selectedDimIndex);
        dimV.assign(dimValues.data(), dimValues.data() + dimValues.size());
    }
    
   
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

void GeneSurferPlugin::loadAvgExpressionABCAtlas() {
    qDebug() << "GeneSurferPlugin::loadAvgExpressionABCAtlas(): start... ";
    std::ifstream file;

    // temporary code to load avg expression from file 
    // TO DO: generalize to select file from GUI
    //if (_positionSourceDataset->getGuiName() == "SEAAD_MTG_MERFISH") {
    //    qDebug() << "Load avg expression for SEAAD dataset";
    //    file.open("SEAAD_average_expression_supertype.csv"); // in this file column:gene symbol, row:supertype
    //}
    //else if (_positionSourceDataset->getGuiName() == "SubCortex_Xenium-CJ23")
    //{
    //    qDebug() << "Load avg expression for test dataset";
    //    file.open("marm_Cluster_v4_metacell_onlyNumber.csv"); // in this file column:gene symbol, row:cluster alias
    //}
    //else {
    //    qDebug() << "Load avg expression for ABC Atlas";
    //    file.open("precomputed_stats_ABC_revision_230821_alias_symbol.csv"); // in this file column:gene symbol, row:cluster alias
    //}



    QString filePath = QFileDialog::getOpenFileName(
        nullptr,
        "Select Average Expression CSV File",
        "",
        "CSV Files (*.csv);;All Files (*)"
    );

    if (filePath.isEmpty()) {
        qDebug() << "No file selected. Aborting.";
        return; 
    }

    file.open(filePath.toStdString());

    if (!file.is_open()) {
        qDebug() << "GeneSurferPlugin::loadAvgExpressionABCAtlas Error: Could not open the avg expr file.";
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

    /*qDebug() << "GeneSurferPlugin::loadAvgExpressionABCAtlas()" << numGenes << " genes and " << numClusters << " clusters"; 
    qDebug() << "GeneSurferPlugin::loadAvgExpressionABCAtlas(): finished";*/

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

    qDebug() << "GeneSurferPlugin::loadAvgExpressionABCAtlas(): allData size: " << allData.size();

    if (!_avgExprDataset.isValid()) {
        qDebug() << "Create an avgExprDataset";
        _avgExprDataset = mv::data().createDataset<Points>("Points", "avgExprDataset");
        events().notifyDatasetAdded(_avgExprDataset);
    }
    
    _avgExprDataset->setData(allData.data(), numClusters, numGenes); // Assuming this function signature is (data, rows, columns)
    _avgExprDataset->setDimensionNames(_geneNamesAvgExpr);
    events().notifyDatasetDataChanged(_avgExprDataset);
    qDebug() << "GeneSurferPlugin::loadAvgExpressionABCAtlas(): _avgExprDataset dataset created";

    // convert to eigen
    _avgExpr.resize(numClusters, numGenes);
    convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);

}

void GeneSurferPlugin::loadLabelsFromSTDatasetABCAtlas() {
    // this is loading label from ST dataset!!!
    // Different from loading from singlecell datset!!!
    QString labelDatasetName;
    /*if (_positionSourceDataset->getGuiName() == "SEAAD_MTG_MERFISH") {
        qDebug() << "Load labels for SEAAD dataset";
        labelDatasetName = "Supertype";
    }
    else if (_positionSourceDataset->getGuiName() == "SubCortex_Xenium-CJ23")
    {
        qDebug() << "Load labels for test dataset";
        labelDatasetName = "CDM_Cluster_label";
    }
    else {
        qDebug() << "Load labels for ABC Atlas";
        labelDatasetName = "cluster_alias";
    }*/

    labelDatasetName = _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentText();
    qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetABCAtlas(): labelDatasetName: " << labelDatasetName;

    Dataset<Clusters> labelDataset;
    for (const auto& data : mv::data().getAllDatasets())
    {
        //qDebug() << data->getGuiName();
        if (data->getGuiName() == labelDatasetName) {
            labelDataset = data;
        }
    }

    QVector<Cluster> labelClusters = labelDataset->getClusters();

    //qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetABCAtlas(): labelClusters size: " << labelClusters.size();

    // add weighting for each cluster of whole data
    _countsAll.resize(_clusterNamesAvgExpr.size()); // number of clusters in SC

    // precompute the cell-label array
    _cellLabels.clear();
    _cellLabels.resize(_numPoints);

    int numClustersNotInST = 0;


    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
        QString clusterName = _clusterNamesAvgExpr[i];
        bool found = false;

        //search for the cluster name in labelClusters
        for (const auto& cluster : labelClusters) {
            if (cluster.getName() == clusterName) {
                const auto& ptIndices = cluster.getIndices();
                for (int j = 0; j < ptIndices.size(); ++j) {
                    int ptIndex = ptIndices[j];
                    _cellLabels[ptIndex] = clusterName;
                }
                // add weighting for each cluster of whole data
                _countsAll[i] = ptIndices.size(); // number of pt in each cluster
                found = true;
                break;
            }
        }

        if (!found) {
            // If the cluster is not found in labelClusters
            //qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetABCAtlas(): cluster in SC" << clusterName << " not found in ST";
            numClustersNotInST++;
            _countsAll[i] = 0;
        }
    }

    qDebug() << "Warning! GeneSurferPlugin::loadLabelsFromSTDatasetABCAtlas: " << numClustersNotInST << " clusters not found in ST";

    if (numClustersNotInST == _clusterNamesAvgExpr.size()) {
        qDebug() << "ERROR: None of the clusters from loaded csv file were found in the selected ST label dataset.";
        QMessageBox::warning(
            nullptr,
            "Label Matching Error",
            "None of the scRNA-seq clusters were found in the selected ST label dataset."
        );
    }


    /*qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetABCAtlas(): _cellLabels size: " << _cellLabels.size();
    qDebug() << "_cellLabels[0]" << _cellLabels[0];*/
}

void GeneSurferPlugin::countLabelDistribution() 
{
    std::unordered_map<QString, int> clusterPointCounts;

    for (int index = 0; index < _sortedFloodIndices.size(); ++index) {
        int ptIndex = _sortedFloodIndices[index];
        QString label = _cellLabels[ptIndex];
        clusterPointCounts[label]++;
    }
    _countsMap.clear();
    _countsMap = clusterPointCounts;

    matchLabelInSubset();
}

void GeneSurferPlugin::matchLabelInSubset()
{
    int numClusters = _avgExpr.rows();
    int numGenes = _avgExpr.cols();
    //qDebug() << "GeneSurferPlugin::matchLabelInSubset(): before matching numClusters: " << numClusters << " numGenes: " << numGenes;

    std::vector<QString> clustersToKeep; // it is cluster names 1
    for (int i = 0; i < numClusters; ++i) {
        QString clusterName = _clusterNamesAvgExpr[i]; // here cannot use _clusterAliasToRowMap, because need to keep the order in clustersToKeep?
        if (_countsMap.find(clusterName) != _countsMap.end()) {
            clustersToKeep.push_back(clusterName);
        }
    }

    // to check if any columns are not in _columnNamesAvgExpr
    if (clustersToKeep.size() != _countsMap.size()) {
        qDebug() << "Warning! GeneSurferPlugin::matchLabelInSubset(): " << _countsMap.size() - clustersToKeep.size() << "clusters not found in avgExpr";
        // output the cluster names that are not in
        QString output;
        for (const auto& pair : _countsMap) {
            QString clusterName = pair.first;
            if (std::find(_clusterNamesAvgExpr.begin(), _clusterNamesAvgExpr.end(), clusterName) == _clusterNamesAvgExpr.end()) {
                output += clusterName + " ";
            }
        }
        //qDebug() << "GeneSurferPlugin::matchLabelInSubset(): not found cluster names: " << output;
    }
    else {
        //qDebug() << "GeneSurferPlugin::matchLabelInSubset(): all clusters found in avgExpr";
    }

    // Handle case where no columns are to be kept // TO DO: check if needed
    if (clustersToKeep.empty()) {
        qDebug() << "GeneSurferPlugin::matchLabelInSubset(): No cluster to keep";
        return;
    }

    _clustersToKeep.clear();
    _clustersToKeep = clustersToKeep; // TO DO: dirty copy
    //qDebug() << "GeneSurferPlugin::matchLabelInSubset(): after matching numClusters: " << _clustersToKeep.size();

    // prepare the subset counting for adding weighting to the subset
    _countsSubset.resize(_clustersToKeep.size());
    for (int i = 0; i < _clustersToKeep.size(); ++i) {
        const QString& clusterName = _clustersToKeep[i];
        _countsSubset[i] = static_cast<float>(_countsMap[clusterName]); // number of pt in each cluster WITHIN the selection
    }

}

void GeneSurferPlugin::clusterGenes()
{
    //qDebug() << "clusterGenes start...";

    // filter genes based on the defined number of genes
    if (_numGenesThreshold > _enabledDimNames.size()) {
        qDebug() << "ERROR! clusterGenes(): _numGenesThreshold is larger than the number of genes";
        return;
    }

    // create a vector of pairs (absolute correlation value, index)
    std::vector<std::pair<float, int>> pairs(_corrGeneVector.size());
    for (size_t i = 0; i < _corrGeneVector.size(); ++i) {
        pairs[i] = std::make_pair(std::abs(_corrGeneVector[i]), i);
    }

    // partially sort to find the top _numGenesThreshold elements
    std::nth_element(pairs.begin(), pairs.begin() + _numGenesThreshold, pairs.end(), std::greater<>());

    std::vector<QString> filteredDimNames;
    std::vector<int> filteredDimIndices;
    for (int i = 0; i < _numGenesThreshold; ++i) {
        filteredDimNames.push_back(_enabledDimNames[pairs[i].second]);
        filteredDimIndices.push_back(pairs[i].second); //indices in _enabledDimNames TO DO: might not work with modified _enabledDimNames
    }
     
    //qDebug() << "GeneSurferPlugin::clusterGenes(): filteredDimNames size: " << filteredDimNames.size();

    if (filteredDimNames.size() < _nclust) {
        qDebug() << "GeneSurferPlugin::clusterGenes(): Not enough genes for clustering";

        _toClearBarchart = true;

        // emit an empty payload to JS and to clear the barchart
        //QVariantList payload;
        QVariantMap payloadMap;
        emit _chartWidget->getCommunicationObject().qt_js_setDataAndPlotInJS(payloadMap);

        return;
    }
    else {
        _toClearBarchart = false;
    }

    // compute the correlation between each pair of the filtered genes
    //auto start2 = std::chrono::high_resolution_clock::now();

    Eigen::MatrixXf corrFilteredGene(filteredDimNames.size(), filteredDimNames.size());


    if (!_sliceDataset.isValid()) {
        //qDebug() << "computePairwiseCorrelationVector: 2D dataset";
        if (!_isSingleCell) {
            _corrFilter.computePairwiseCorrelationVector(filteredDimNames, filteredDimIndices, _subsetData, corrFilteredGene);// TO DO: dimNames not needed in this function
        }
        else {
            // add weighting 
            _corrFilter.computePairwiseCorrelationVector(filteredDimNames, filteredDimIndices, _subsetData, _countsSubset, corrFilteredGene);// SC: with weighting
        }
    } 
    else {
        //qDebug() << "computePairwiseCorrelationVector: 3D dataset";
        if (!_isSingleCell) {
           _corrFilter.computePairwiseCorrelationVector(filteredDimNames, filteredDimIndices, _subsetData3D, corrFilteredGene);// ST: without weighting
        }
        else {
            // add weighting 
           /* qDebug() << "computePairwiseCorrelationVector: 3D dataset singlecell";
            qDebug() << "computePairwiseCorrelationVector: _subsetData3D size: " << _subsetData3D.rows() << " " << _subsetData3D.cols();
            qDebug() << "computePairwiseCorrelationVector: _countsSubset size: " << _countsSubset.size() << "_countsSubset[0] " << _countsSubset[0];
            qDebug() << "computePairwiseCorrelationVector: filteredDimNames size: " << filteredDimNames.size() << " filteredDimIndices size: " << filteredDimIndices.size();*/

            _corrFilter.computePairwiseCorrelationVector(filteredDimNames, filteredDimIndices, _subsetData3D, _countsSubset, corrFilteredGene);// SC: with weighting

            // output the element in the first row and first column of corrFilteredGene
            /*qDebug() << "GeneSurferPlugin::clusterGenes(): corrFilteredGene(0,0): " << corrFilteredGene(0, 0);
            qDebug() << "GeneSurferPlugin::clusterGenes(): corrFilteredGene(1,0): " << corrFilteredGene(1, 0);
            qDebug() << " Mean of corrFilteredGene.row(0) : " << corrFilteredGene.row(0).mean();*/

            // test without weighting
            Eigen::MatrixXf corrFilteredGeneTEST(filteredDimNames.size(), filteredDimNames.size());
            _corrFilter.computePairwiseCorrelationVector(filteredDimNames, filteredDimIndices, _subsetData3D, corrFilteredGeneTEST);

            // compare the two correlation matrices
            /*std::cout << "Mean of corrFilteredGene: " << corrFilteredGene.mean() << std::endl;
            std::cout << "Mean of corrFilteredGeneTEST: " << corrFilteredGeneTEST.mean() << std::endl;

            std::cout << "Norm of corrFilteredGene: " << corrFilteredGene.norm() << std::endl;
            std::cout << "Norm of corrFilteredGeneTEST: " << corrFilteredGeneTEST.norm() << std::endl;*/

            /*Eigen::MatrixXf diffMatrix = corrFilteredGene - corrFilteredGeneTEST;
            std::cout << "Norm of Difference Matrix: " << diffMatrix.norm() << std::endl;
            std::cout << "Max difference in matrices: " << diffMatrix.cwiseAbs().maxCoeff() << std::endl;

            std::cout << "Correlation Matrix with Weighting: \n" << corrFilteredGene << std::endl;
            std::cout << "Correlation Matrix without Weighting: \n" << corrFilteredGeneTEST << std::endl;
            std::cout << "Difference Matrix: \n" << diffMatrix << std::endl;*/
        }
            
    }

    /*auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed2 = end2 - start2;
    std::cout << "clusterGenes() computeCorrFilteredGene Elapsed time: " << elapsed2.count() << " ms\n";
    qDebug() << "GeneSurferPlugin::clusterGenes():corrFilteredGene is computed";*/

    // substract correlation from 1 to get distance
    //auto start3 = std::chrono::high_resolution_clock::now();

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

    /*auto end3 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed3 = end3 - start3;
    std::cout << "clusterGenes() Apply clustering Elapsed time: " << elapsed3.count() << " ms\n";*/
 

    // temporary code: output the number of genes in each cluster
    _numGenesInCluster.clear();
    for (int i = 0; i < filteredDimNames.size(); ++i) {
        _numGenesInCluster[labels[i]]++;
    }

    /*auto start5 = std::chrono::high_resolution_clock::now();
    computeEntireClusterScalars(filteredDimIndices, labels);   
    auto end5 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed5 = end5 - start5;
    std::cout << "computeEntireClusterScalars Elapsed time: " << elapsed5.count() << " ms\n";*/

    if (_isSingleCell != true) {
        //auto start5 = std::chrono::high_resolution_clock::now();

        computeFloodedClusterScalars(filteredDimIndices, labels);

        /*auto end5 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed5 = end5 - start5;
        std::cout << "clusterGenes() computeFloodedClusterScalars Elapsed time: " << elapsed5.count() << " ms\n";*/
    }
    else {

        //auto start5 = std::chrono::high_resolution_clock::now();

        computeFloodedClusterScalarsSingleCell(filteredDimIndices, labels);

        /*auto end5 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed5 = end5 - start5;
        std::cout << "clusterGenes() computeFloodedClusterScalarsSingleCell Elapsed time: " << elapsed5.count() << " ms\n";*/
    }

    delete[] distmat;
    delete[] merge;
    delete[] height;
    delete[] labels;

}

void GeneSurferPlugin::computeEntireClusterScalars(const std::vector<int> filteredDimIndices, const int* labels)
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

void GeneSurferPlugin::computeFloodedClusterScalars(const std::vector<int> filteredDimIndices, const int* labels)
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

void GeneSurferPlugin::computeFloodedClusterScalarsSingleCell(const std::vector<int> filteredDimIndices, const int* labels) {
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

void GeneSurferPlugin::updateClusterScalarOutput(const std::vector<float>& scalars)
{
    _clusterScalars->setData<float>(scalars.data(), scalars.size(), 1);
    events().notifyDatasetDataChanged(_clusterScalars);
    //qDebug() << "GeneSurferPlugin::updateClusterScalarOutput(): finished";
}

void GeneSurferPlugin::getFuntionalEnrichment()
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

        if (_currentEnrichmentAPI == "ToppGene") {
        // ToppGene
        _client->lookupSymbolsToppGene(geneNamesInCluster);
        }

        if (_currentEnrichmentAPI == "gProfiler") {
            // gProfiler
            QStringList backgroundGeneNames;
            if (_isSingleCell != true) {
                for (const auto& name : _enabledDimNames) {
                    backgroundGeneNames.append(name);
                }
                //qDebug() << "getFuntionalEnrichment(): ST mode, with background";
            }
            else {
                // in single cell mode, background is empty
                //qDebug() << "getFuntionalEnrichment(): single cell mode, without background";
            }
            //qDebug() << "getFuntionalEnrichment(): backgroundGeneNames size: " << backgroundGeneNames.size();
            _client->postGeneGprofiler(geneNamesInCluster, backgroundGeneNames, _currentEnrichmentSpecies);
        }

        //EnrichmentAnalysis* tempClient = new EnrichmentAnalysis(this);
    }

    // output the gene names
    for (int i = 0; i < geneNamesInCluster.size(); i++) {
        QString item = geneNamesInCluster[i];
        if (i == 0)
            std::cout << "Genes in this cluster: " << item.toUtf8().constData() << " ";
        else 
            std::cout << item.toUtf8().constData() << " ";
    }
    std::cout << std::endl;
}

void GeneSurferPlugin::updateEnrichmentTable(const QVariantList& data) {
    //qDebug() << "GeneSurferPlugin::updateEnrichmentTable(): start";

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

    qDebug() << "GeneSurferPlugin::updateEnrichmentTable(): finished";
}

void GeneSurferPlugin::noDataEnrichmentTable() {
    _tableWidget->clearContents();
    _tableWidget->setRowCount(1);
    _tableWidget->setColumnCount(1);

    QStringList header = { "Message" };
    _tableWidget->setHorizontalHeaderLabels(header);
    QTableWidgetItem* item = new QTableWidgetItem("No enrichment analysis result available.");

    _tableWidget->setItem(0, 0, item);
    _tableWidget->resizeColumnsToContents();
}

void GeneSurferPlugin::onTableClicked(int row, int column) {
    //qDebug() << "Cell clicked in row:" << row << "column:" << column;

    // get gene symbols of the selected row
    QVariantMap selectedItemMap = _enrichmentResult[row].toMap();
    QString geneSymbols = selectedItemMap["Symbol"].toString();
    //qDebug() << "Gene symbols in the selected item:" << geneSymbols;

    QStringList geneSymbolList = geneSymbols.split(",");

    // match the gene symbols with the gene names in the cluster - gene symbols returned from toppGene are all capitals
    QVariantList geneNamesForHighlighting;
    for (const QString& symbol : geneSymbolList) {
        
        QString searchGeneSymbol;


        // ToppGene Properly format the symbol (first letter uppercase, rest lowercase) - gene symbols returned from ToppGene are all upper case
        if (_currentEnrichmentAPI == "ToppGene") {

            // first check if the gene symbol is already in the same format as the data
            if (_enabledDimNames[0][1].isUpper()) // check if the second letter is uppercase
            {
                searchGeneSymbol = symbol;
            }
            else
            {
                searchGeneSymbol = symbol.toLower(); // convert to lower case
                searchGeneSymbol[0] = searchGeneSymbol[0].toUpper();
            }
        }

        // gProfiler
        if (_currentEnrichmentAPI == "gProfiler") {
            searchGeneSymbol = symbol; // TO DO: check if the gene symbols are already in the correct format
        }

        // Attempt to find original, indexed gene names using the reverse mapping
        QStringList originalGeneNames = _simplifiedToIndexGeneMapping.value(searchGeneSymbol);

        if (!originalGeneNames.isEmpty()) {
            //qDebug() << "Original gene names found for symbol" << searchGeneSymbol << ":" << originalGeneNames;
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

void GeneSurferPlugin::updateClick() {
    if (_selectedClusterIndex == -1) {
        qDebug() << "Warning! updateClick(): _selectedClusterIndex is -1, no view is selected";
        return;
    }

    if (_sortedFloodIndices.empty()) {
        qDebug() << "ERROR! updateClick(): point selection is empty";
        return;
    }

    // clear clicked frames
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

            // May 2 - check the scalar values within selection
            /*std::vector<float> dimVSelection;
            for (int i = 0; i < _sortedFloodIndices.size(); i++) {
                dimVSelection.push_back(dimV[_sortedFloodIndices[i]]);
            }*/

            /*qDebug() << "dimVSelection max: " << *std::max_element(dimVSelection.begin(), dimVSelection.end());
            qDebug() << "dimVSelection min: " << *std::min_element(dimVSelection.begin(), dimVSelection.end());
            qDebug() << "dimVSelection mean: " << std::accumulate(dimVSelection.begin(), dimVSelection.end(), 0.0) / dimVSelection.size();*/
            // compare to the original dimV
            /*qDebug() << "dimV max: " << *std::max_element(dimV.begin(), dimV.end());
            qDebug() << "dimV min: " << *std::min_element(dimV.begin(), dimV.end());
            qDebug() << "dimV mean: " << std::accumulate(dimV.begin(), dimV.end(), 0.0) / dimV.size();*/
            // temporarily only show the selection in VolumeViewer and normalize within selection
            /*std::vector<float> dimVTEST(dimV.size(), 0.0f);
            for (int i = 0; i < _sortedFloodIndices.size(); i++) {
                dimVTEST[_sortedFloodIndices[i]] = dimV[_sortedFloodIndices[i]];
            }
            dimV = dimVTEST;*/
            // end of May 2
            

            normalizeVector(dimV);
            updateClusterScalarOutput(dimV);
        }
    }
}

void GeneSurferPlugin::updateSlice(int sliceIndex) {
    //qDebug() << "GeneSurferPlugin::updateSlice(): sliceIndex = " << sliceIndex;

    _currentSliceIndex = sliceIndex;

    // TODO: should set the value in ScatterView with eventFilter
    // Otherwise if updateSlice is called by settingsAction, this is repeated
    _settingsAction.getSectionAction().getSliceAction().setValue(_currentSliceIndex);

    if (!_sliceDataset.isValid()) {
        qDebug() << "GeneSurferPlugin::updateSlice(): _sliceDataset is not valid";
        return;
    }

    QString clusterName = _sliceDataset->getClusters()[_currentSliceIndex].getName();

    std::vector<uint32_t>& uindices = _sliceDataset->getClusters()[_currentSliceIndex].getIndices();
    std::vector<int> indices;
    indices.assign(uindices.begin(), uindices.end());

    _onSliceIndices.clear();
    _onSliceIndices = indices;
    
    _dataStore.createDataView(indices);
    updateSelectedDim();

    // update floodfill mask on 2D
    if (_isFloodIndex.empty()) {
        qDebug() << "GeneSurferPlugin::updateSlice(): _isFloodIndex is empty";
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

    if (_selectedDimName != "NoneSelected")
        updateDimView(_selectedDimName);
}

////////////////////
// Serialization ///
////////////////////
void GeneSurferPlugin::fromVariantMap(const QVariantMap& variantMap)
{
    //qDebug() << "GeneSurferPlugin::fromVariantMap() 1 ";
    _loadingFromProject = true;

    ViewPlugin::fromVariantMap(variantMap);

    _avgExprStatus = static_cast<AvgExpressionStatus>(variantMap["AvgExpressionStatus"].toInt());
    //qDebug() << "GeneSurferPlugin::fromVariantMap() 2 ";

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
        //qDebug() << "GeneSurferPlugin::fromVariantMap(): clusterNamesAvgExpr.size();" << _clusterNamesAvgExpr.size();
        //qDebug() << "GeneSurferPlugin::fromVariantMap(): clusterNamesAvgExpr[0];" << _clusterNamesAvgExpr[0];
    }

    variantMapMustContain(variantMap, "SettingsAction");
    _settingsAction.fromVariantMap(variantMap["SettingsAction"].toMap());
    //qDebug() << "GeneSurferPlugin::fromVariantMap() 3 ";

    QString clusterScalarsId = variantMap["ClusterScalars"].toString();
    _clusterScalars = mv::data().getDataset<Points>(clusterScalarsId);

    if (_sliceDataset.isValid())
    {
        //qDebug() << "GeneSurferPlugin::fromVariantMap() 4 ";
        _currentSliceIndex = variantMap["CurrentSliceIdx"].toInt();// TODO: check if this is still needed if _sectionAction is serialized
        updateSlice(_currentSliceIndex);
    }

    _selectedDimName = variantMap["SelectedDimName"].toString();
    if (_selectedDimName != "NoneSelected")
        updateDimView(_selectedDimName);

    if (_avgExprStatus != AvgExpressionStatus::NONE)
    {
        // to set singlecell option enabled
        _settingsAction.getSingleCellModeAction().getSingleCellOptionAction().setEnabled(true);
    }

    // loading selection of points
    _selectedByFlood = variantMap["SelectionFlag"].toBool();
    //qDebug() << "GeneSurferPlugin::fromVariantMap(): _selectedByFlood = " << _selectedByFlood;
    if (_selectedByFlood)
    {
        updateFloodFillDataset();
    }
    else 
    {
        auto selection = _positionDataset->getSelection<Points>();
        if (!_sliceDataset.isValid())
           _computeSubset.updateSelectedData(_positionDataset, selection, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex);
        else
          _computeSubset.updateSelectedData(_positionDataset, selection, _onSliceIndices, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex, _isFloodOnSlice, _onSliceFloodIndices);
        updateSelection();
    }

    // load the selected view
    _selectedClusterIndex = variantMap["SelectedClusterIndex"].toInt();
    updateClick();


    _loadingFromProject = false;

}

QVariantMap GeneSurferPlugin::toVariantMap() const
{
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _primaryToolbarAction.insertIntoVariantMap(variantMap);
    _settingsAction.insertIntoVariantMap(variantMap);

    variantMap.insert("AvgExpressionStatus", static_cast<int>(_avgExprStatus));

    variantMap.insert("ClusterScalars", _clusterScalars.getDatasetId());

    variantMap.insert("SelectedDimName", _selectedDimName);

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

    variantMap.insert("SelectionFlag", _selectedByFlood);

    variantMap.insert("SelectedClusterIndex", _selectedClusterIndex);

    return variantMap;
}


// =============================================================================
// Plugin Factory 
// =============================================================================

GeneSurferPluginFactory::GeneSurferPluginFactory()
{
    setIconByName("bullseye");
    getProjectsDsnsAction().addString("https://osf.io/download/68517acd4d3926945cb68775/");
}

ViewPlugin* GeneSurferPluginFactory::produce()
{
    return new GeneSurferPlugin(this);
}

mv::DataTypes GeneSurferPluginFactory::supportedDataTypes() const
{
    // This example analysis plugin is compatible with points datasets
    DataTypes supportedTypes;
    supportedTypes.append(PointType);
    return supportedTypes;
}

mv::gui::PluginTriggerActions GeneSurferPluginFactory::getPluginTriggerActions(const mv::Datasets& datasets) const
{
    PluginTriggerActions pluginTriggerActions;

    const auto getPluginInstance = [this]() -> GeneSurferPlugin* {
        return dynamic_cast<GeneSurferPlugin*>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets >= 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {
        auto pluginTriggerAction = new PluginTriggerAction(const_cast<GeneSurferPluginFactory*>(this), this, "Gene Surfer", "Gene Surfer visualization", StyledIcon("braille"), [this, getPluginInstance, datasets](PluginTriggerAction& pluginTriggerAction) -> void {
            for (auto dataset : datasets)
                getPluginInstance()->loadData(Datasets({ dataset }));

            });

        pluginTriggerActions << pluginTriggerAction;
    }

    return pluginTriggerActions;
}


