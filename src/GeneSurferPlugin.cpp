#include "GeneSurferPlugin.h"

#include "ChartWidget.h"
#include "ScatterView.h"

#include <util/Serialization.h>
#include <DatasetsMimeData.h>
#include <util/Serialization.h>

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
#include <QMessageBox>

// for reading hard-coded csv files
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <chrono>


Q_PLUGIN_METADATA(IID "nl.BioVault.GeneSurferPlugin")

using namespace mv;
using namespace mv::util;

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

    template<typename ActionType>
    ActionType* findActionByPath(mv::plugin::Plugin* plugin, const QString& path) {
        return dynamic_cast<ActionType*>(plugin->findChildByPath(path));
    }
}

GeneSurferPlugin::GeneSurferPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _nclust(3),
    _positionDataset(),
    _positions(),
    _positionSourceDataset(),
    _numPoints(0),
    _chartWidget(nullptr),
    _dropWidget(nullptr),
    _settingsAction(this, "Settings Action"),
    _primaryToolbarAction(this, "PrimaryToolbar"),
    _secondaryToolbarAction(this, "SecondaryToolbar"),
    _tertiaryToolbarAction(this, "TertiaryToolbar"),
    _selectedDimIndex(-1),
    _colorMapAction(this, "Color map", "RdYlBu"),
    _saveToCsvAction(&getWidget(), "Save As...")

{
    { // save to CSV
        _saveToCsvAction.setIcon(mv::util::StyledIcon("file-csv"));
        _saveToCsvAction.setShortcut(tr("Ctrl+S"));
        _saveToCsvAction.setShortcutContext(Qt::WidgetWithChildrenShortcut);

        connect(&_saveToCsvAction, &TriggerAction::triggered, this, [this]() -> void {
            saveDataToCsvAction();
            });
    }

    _primaryToolbarAction.addAction(&_settingsAction.getClusteringAction(), 1, GroupAction::Horizontal);// TODO: remove ClusteringAction
    //_primaryToolbarAction.addAction(&_settingsAction.getDimensionSelectionAction(), 2, GroupAction::Horizontal); // Disabled
    _primaryToolbarAction.addAction(&_settingsAction.getCorrelationModeAction(), -1, GroupAction::Horizontal);
    //_primaryToolbarAction.addAction(&_settingsAction.getSingleCellModeAction()); // Disabled for publishing project. Will need it for generating projects.

}

void GeneSurferPlugin::init()
{
    getWidget().setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    // Create layout
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_primaryToolbarAction.createWidget(&getWidget()), 1);

    // Create barchart widget and set html contents of webpage 
    _chartWidget = new ChartWidget(this);
    _chartWidget->setPage(":gene_surfer/chart/bar_chart.html", "qrc:/gene_surfer/chart/");
    _chartWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    _chartWidget->addAction(&_saveToCsvAction);

    // Add label for filtering on top of the barchart
    _filterLabel = new QLabel(_chartWidget);
    _filterLabel->setFont(QFont("Arial", 10));
    _filterLabel->setGeometry(10, 5, 150, 15);

    _seedDimensionLabel = new QLabel(_chartWidget);
    _seedDimensionLabel->setFont(QFont("Arial", 10));
    _seedDimensionLabel->setGeometry(10, 20, 200, 15);

    _filterLabel->setStyleSheet("background: white; color: black;");
    _seedDimensionLabel->setStyleSheet("background: white; color: black;");

    // also show query dimension for ATAC<->RNA
    if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::ATACtoRNA || _corrFilter.getFilterType() == corrFilter::CorrFilterType::RNAtoATAC)
    {
        _filterLabel->setText("Filter by:" + _corrFilter.getCorrFilterTypeAsString());
        _seedDimensionLabel->setText("Seed: " + _queryDimensionForATACRNA);
    }
    else
    {
        _filterLabel->setText("Filter dimensions by:" + _corrFilter.getCorrFilterTypeAsString());
        _seedDimensionLabel->setText("");
    }

    layout->addWidget(_chartWidget, 100);

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
        }

        updateSelection();
        });
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
        //entry["cluster"] = "Gene cluster " + QString::number(clusterLabel);
        entry["cluster"] = "Dimension cluster " + QString::number(clusterLabel);

        payload.push_back(entry);
    }

    QVariantMap payloadMap;
    payloadMap["data"] = payload;
    if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::MORAN)
        payloadMap["FilterType"] = "Moran";
    else
        payloadMap["FilterType"] = "Others";

    // automatically update scalars to the top dimension
    publishSelection(filteredAndSortedGenes.back().first);

    qDebug() << "GeneSurferPlugin::convertDataAndUpdateChart: Send data from Qt cpp to D3 js";
    emit _chartWidget->getCommunicationObject().qt_js_setDataAndPlotInJS(payloadMap);
}

void GeneSurferPlugin::saveDataToCsvAction()
{
    // Prepare the sorted list - FIXME: duplicate code with convertDataAndUpdateChart()
    std::vector<std::pair<QString, float>> filteredAndSortedGenes;
    for (size_t i = 0; i < _enabledDimNames.size(); ++i) {
        if (_dimNameToClusterLabel.find(_enabledDimNames[i]) != _dimNameToClusterLabel.end()) {
            filteredAndSortedGenes.emplace_back(_enabledDimNames[i], _corrGeneVector[i]);
        }
    }

    std::sort(filteredAndSortedGenes.begin(), filteredAndSortedGenes.end(),
        [](const std::pair<QString, float>& a, const std::pair<QString, float>& b) {
            return a.second > b.second; // descending
        });

    QString fileName = QFileDialog::getSaveFileName(
        nullptr, "Save Gene Correlations", "", "CSV files (*.csv);;All files (*.*)");
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not open file for writing";
        return;
    }

    QTextStream out(&file);

    // Write header information
    if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::DIFF)
    {
        out << "Filter type: DIFF (normed to [0-1] for plotting)\n";
    }
    else if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::MORAN)
    {
        out << "Filter type: Moran's I\n";
    }
    else if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALZ)
    {
        out << "Filter type: SPATIALZ\n";
    }
    else if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::SPATIALY)
    {
        out << "Filter type: SPATIALY\n";
    }
    else if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::RNAtoATAC)
    {
        out << "Filter type: RNAtoATAC,";
        qDebug() << "Current query dimension before saving" << _queryDimensionForATACRNA;
        out << "Query gene: " << _queryDimensionForATACRNA << ",";

        // Write the number of selected points
        out << "Number of selected points: " << _sortedFloodIndices.size() << "\n";
    }
    else if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::ATACtoRNA)
    {
        out << "Filter type: ATACtoRNA,";
        qDebug() << "Current query dimension before saving" << _queryDimensionForATACRNA;
        out << "Query ATAC: " << _queryDimensionForATACRNA << ",";

        // Write the number of selected points
        out << "Number of selected points: " << _sortedFloodIndices.size() << "\n";
    }

    // Write column headers
    if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::DIFF) {
        out << "Dimension,Diff\n";
    }
    else {
        out << "Dimension,Correlation\n";
    }

    // Write rows (one line per gene)
    for (const auto& genePair : filteredAndSortedGenes) {
        const QString& geneName = genePair.first;
        float corrValue = genePair.second;

        out << geneName << ","
            << corrValue << "\n";
    }

    file.close();
    qDebug() << "Correlations exported to" << fileName;

}

void GeneSurferPlugin::publishSelection(const QString& selection)
{
    _selectedDimName = selection;

    if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::ATACtoRNA)
    {
        if (!_mappedRNAonSpatialDataset.isValid())
        {
            qDebug() << "_mappedRNAonSpatialDataset is NOT valid";
            return;
        }
        auto* dimensionPickerAction = dynamic_cast<DimensionPickerAction*>(_mappedRNAonSpatialDataset->findChildByPath("Settings/Averages Dataset Dimension"));
        if (!dimensionPickerAction) {
            qDebug() << "DimensionPickerAction not found for plugin: " << _mappedRNAonSpatialDataset->getGuiName();
            return;
        }
        const auto& availableDimensionNames = dimensionPickerAction->getDimensionNames();
        if (!availableDimensionNames.contains(_selectedDimName)) {
            qDebug() << "Selected dimension " << _selectedDimName << " not found in available dimensions of Project Averages plugin: " << _mappedRNAonSpatialDataset->getGuiName();
            return;
        }
        dimensionPickerAction->setCurrentDimensionName(_selectedDimName);

    }
    //else if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::RNAtoATAC)
    else
    {
        if (!_mappedATAConSpatialDataset.isValid())
        {
            qDebug() << "mappedATAConSpatialDataset is NOT valid";
            return;
        }
        auto* dimensionPickerAction = dynamic_cast<DimensionPickerAction*>(_mappedATAConSpatialDataset->findChildByPath("Settings/Averages Dataset Dimension"));
        if (!dimensionPickerAction) {
            qDebug() << "DimensionPickerAction not found for plugin: " << _mappedATAConSpatialDataset->getGuiName();
            return;
        }
        const auto& availableDimensionNames = dimensionPickerAction->getDimensionNames();
        if (!availableDimensionNames.contains(_selectedDimName)) {
            qDebug() << "Selected dimension " << _selectedDimName << " not found in available dimensions of Project Averages plugin: " << _mappedATAConSpatialDataset->getGuiName();
            return;
        }
        dimensionPickerAction->setCurrentDimensionName(_selectedDimName);
    }
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
    // TODO: remove, not used for ATAC viewer
}

void GeneSurferPlugin::updateShowDimension() {
    // TODO: remove, not used anymore
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

    int numPoints = scSourceDataset->getNumPoints();
    int numClusters = labelClusters.size();
    int numGenes = scSourceDataset->getNumDimensions();

    Eigen::MatrixXf scSourceMatrix;
    convertToEigenMatrixProjection(scSourceDataset, scSourceMatrix);

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

    loadAvgExpressionFromFile();

    if (!_avgExprDataset.isValid()) // skip if it's not valid
    {
        qDebug() << "loadAvgExpression aborted: _avgExprDataset is not valid";
        return;
    }

    loadLabelsFromSTDatasetFromFile();

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
}

void GeneSurferPlugin::setLabelDataset() {
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

    //TODO: should trigger loadLabelsFromSTDatasetFromFile() if load avgExprDataset from file
    qDebug() << "Warning: only the label dataset is set, avgExprDataset still needs to be computed or loaded";

}

void GeneSurferPlugin::updateEnrichmentAPI()
{
    //TODO: remove, not used anymore
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
    // TODO: remove, not used anymore
}

void GeneSurferPlugin::updateSelection()
{
    // clear table content
    //_tableWidget->clearContents();

    if (!_positionDataset.isValid())
        return;

    if (_isFloodIndex.empty()) {
        qDebug() << "GeneSurferPlugin::updateSelection(): _isFloodIndex is empty";
        return;
    }

    ////////////////////
    // Compute subset //
    ////////////////////
    if (_ATACtoRNA)
    {
        if (_isSingleCell && _sliceDataset.isValid()) {
            countLabelDistribution();

            _computeSubset.computeSubsetDataAvgExpr(_avgExprRNA, _clustersToKeep, _clusterAliasToRowMap, _subsetDataAvgOri);

        }
    }
    else {
        if (_isSingleCell && _sliceDataset.isValid()) {
            qDebug() << "Compute subset: 3D + SingleCell";
            countLabelDistribution();

            _computeSubset.computeSubsetDataAvgExpr(_avgExpr, _clustersToKeep, _clusterAliasToRowMap, _subsetDataAvgOri);
        }
    }

    //////////////////////////////////////////////
    // Compute correlation for filtering genes //
    /////////////////////////////////////////////
    // -------------- Diff --------------
    if (_isSingleCell && _corrFilter.getFilterType() == corrFilter::CorrFilterType::DIFF) {
        qDebug() << "Compute filtering: SingleCell +Diff";
        //_corrFilter.getDiffFilter().computeDiff(_subsetDataAvgOri, _avgExpr, _corrGeneVector); //without weighting

        // add weighting for number of cells in each cluster
        _corrFilter.getDiffFilter().computeWeightedDiff(_subsetDataAvgOri, _avgExpr, _countsSubset, static_cast<std::uint64_t>(_sortedFloodIndices.size()), 
            _countsAll, static_cast<std::uint64_t>(_numPoints), _corrGeneVector);

    }


    // -------------- RNA-seq gene to ATAC (RNA-seq as seed, identify similar peaks) --------------
    // Only for 3D + singlecell right now
    if (_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::RNAtoATAC) {
        qDebug() << "Compute filtering: RNAtoATAC";

        std::vector<float> dimAvg;

        std::unordered_map<QString, float> clusterDimSums;
        std::vector<float> dimSpatial;

        if (!_mappedRNAonSpatialDataset.isValid())
        {
            updateMappedDatasets();
        }
        _mappedRNAonSpatialDataset->extractDataForDimension(dimSpatial, 0);
        _queryDimensionForATACRNA = _mappedRNAonSpatialDataset->getDimensionNames()[0];
        _seedDimensionLabel->setText("Seed: " + _queryDimensionForATACRNA);


        for (int index = 0; index < _sortedFloodIndices.size(); ++index) {
            int ptIndex = _sortedFloodIndices[index];
            if (ptIndex >= dimSpatial.size())
                qDebug() << "ERROR! ptIndex " << ptIndex << " >= dimSpatial.size() " << dimSpatial.size();
            QString label = _cellLabels[ptIndex];
            clusterDimSums[label] += dimSpatial[ptIndex];
        }

        for (int i = 0; i < _clustersToKeep.size(); ++i) {
            QString label = _clustersToKeep[i];
            int count = _countsMap[label];

            float averageDim = (count > 0) ? static_cast<float>(clusterDimSums[label]) / count : 0.0f;
            dimAvg.push_back(averageDim);

        }
        //qDebug() << "dimAvg size: " << dimAvg.size();

        // _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_subsetDataAvgOri, dimAvg, _corrGeneVector);// without weighting
        _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_subsetDataAvgOri, dimAvg, _countsSubset, _corrGeneVector);// with weighting

    }

    // // -------------- ATAC to RNA-seq genes (ATAC peak as seed, identify similar RNA-seq genes) --------------
    // Only for 3D + singlecell right now
    if (_isSingleCell && _sliceDataset.isValid() && _corrFilter.getFilterType() == corrFilter::CorrFilterType::ATACtoRNA) {

        qDebug() << "Compute filtering: ATACtoRNA";

        // get the vector of a specific ATAC-seq peak
        std::vector<float> dimAvg;
        std::unordered_map<QString, float> clusterDimSums;
        std::vector<float> dimSpatial;

        if (!_mappedATAConSpatialDataset.isValid())
        {
            updateMappedDatasets();
            return;
        }
        _mappedATAConSpatialDataset->extractDataForDimension(dimSpatial, 0);
        _queryDimensionForATACRNA = _mappedATAConSpatialDataset->getDimensionNames()[0];
        _seedDimensionLabel->setText("Seed: " + _queryDimensionForATACRNA);

        for (int index = 0; index < _sortedFloodIndices.size(); ++index) {
            int ptIndex = _sortedFloodIndices[index];
            if (ptIndex >= dimSpatial.size())
                qDebug() << "ERROR! ptIndex " << ptIndex << " >= dimSpatial.size() " << dimSpatial.size();
            QString label = _cellLabels[ptIndex];
            clusterDimSums[label] += dimSpatial[ptIndex];
        }

        for (int i = 0; i < _clustersToKeep.size(); ++i) {
            QString label = _clustersToKeep[i];
            int count = _countsMap[label];

            float averageDim = (count > 0) ? static_cast<float>(clusterDimSums[label]) / count : 0.0f;
            dimAvg.push_back(averageDim);

        }
        //qDebug() << "dimAvg size: " << dimAvg.size();
        _corrFilter.getSpatialCorrFilter().computeCorrelationVectorOneDimension(_subsetDataAvgOri, dimAvg, _countsSubset, _corrGeneVector);// with weighting
    };

    ////////////////////
    // Clustering //
    ////////////////////
    // Keep the results struture without clustering
    std::vector<std::pair<float, int>> pairs(_corrGeneVector.size());
    for (int i = 0; i < _corrGeneVector.size(); ++i) {
        pairs[i] = std::make_pair(std::abs(_corrGeneVector[i]), i);
    }
    std::nth_element(pairs.begin(), pairs.begin() + _numGenesThreshold, pairs.end(), std::greater<>());

    std::vector<QString> filteredDimNames;
    std::vector<int> filteredDimIndices;
    for (int i = 0; i < _numGenesThreshold; ++i) {
        filteredDimNames.push_back(_enabledDimNames[pairs[i].second]);
        filteredDimIndices.push_back(pairs[i].second);
    }

    _dimNameToClusterLabel.clear();
    int sameLabel = 0;
    for (const auto& name : filteredDimNames) {
        _dimNameToClusterLabel[name] = sameLabel;
    }

    _numGenesInCluster.clear();
    _numGenesInCluster[sameLabel] = static_cast<int>(filteredDimNames.size());

    ////////////////////
    // Update Plots //
    ////////////////////
    convertDataAndUpdateChart();

    // unselect the selection, to show no selection on scatter plot. 
    // problem: will not be able to highlight the selected cells on the scatter plot anymore
    /*std::vector<std::uint32_t> emptySelection;
    _positionDataset->setSelectionIndices(emptySelection);
    events().notifyDatasetDataSelectionChanged(_positionDataset);*/

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
}

void GeneSurferPlugin::updateSingleCellOption() {
    qDebug() << "GeneSurferPlugin::updateSingleCellOption(): start... ";

    _settingsAction.getSingleCellModeAction().getSingleCellOptionAction().isChecked() ? _isSingleCell = true : _isSingleCell = false;
    //qDebug() << "GeneSurferPlugin::updateSingleCellOption(): _isSingleCell: " << _isSingleCell;

    if (_isSingleCell) {
        //qDebug() << "Using Single Cell";

        if (_avgExpr.size() == 0) {
            //qDebug() << "GeneSurferPlugin::updateSingleCellOption(): _avgExpr is empty";
            //qDebug() << "_loadingFromProject = " << _loadingFromProject;
            //qDebug() << "_avgExprDataset.isValid() = " << _avgExprDataset.isValid();

            if (_avgExprDataset.isValid()) {
                //qDebug() << "_avgExprDataset is valid...";
                switch (_avgExprStatus)
                {
                case AvgExpressionStatus::NONE:
                    qDebug() << "Status: NONE";
                    break;
                case AvgExpressionStatus::COMPUTED:
                    qDebug() << "Status: COMPUTED";

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
                    qDebug() << "Status: LOADED";

                    // in oder to avoid computing again - TO DO: seperate this part in the function
                    //qDebug() << "_avgExprDataset.isValid() = " << _avgExprDataset.isValid();
                    convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);// FIXME: is it necessary?
                    _geneNamesAvgExpr.clear();
                    _geneNamesAvgExpr = _avgExprDataset->getDimensionNames();

                    //qDebug() << "_geneNamesAvgExpr size: " << _geneNamesAvgExpr.size();
                    //qDebug() << "_clusterNamesAvgExpr size: " << _clusterNamesAvgExpr.size() << " _clusterNamesAvgExpr[0]: " << _clusterNamesAvgExpr[0];

                    _clusterAliasToRowMap.clear();
                    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
                        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
                    }
                    //qDebug() << "_clusterAliasToRowMap size: " << _clusterAliasToRowMap.size();
                    loadLabelsFromSTDatasetFromFile();

                    break;
                }
            }
            else {
                return;
            }
        }
        else {
            // _avgExpr is already loaded or computed
            // in case switch from ATACtoRNA, only update _clusterAliasToRowMap and loadLabelsFromSTDatasetFromFile()
            //qDebug() << "_geneNamesAvgExpr size: " << _geneNamesAvgExpr.size();
            //qDebug() << "_clusterNamesAvgExpr size: " << _clusterNamesAvgExpr.size() << " _clusterNamesAvgExpr[0]: " << _clusterNamesAvgExpr[0];

            _clusterAliasToRowMap.clear();
            for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
                _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
            }
            //qDebug() << "_clusterAliasToRowMap size: " << _clusterAliasToRowMap.size();
            loadLabelsFromSTDatasetFromFile();
        }

        _settingsAction.getDimensionSelectionAction().getDimensionAction().setPointsDataset(_avgExprDataset);

        // update _enabledDimNames
        _enabledDimNames.clear();
        _enabledDimNames = _geneNamesAvgExpr;

        // update max number of genes in _numGenesThresholdAction
        _settingsAction.getClusteringAction().getNumGenesThresholdAction().setMaximum(static_cast<int>(_enabledDimNames.size()));

        updateSelection();
    }
    else {
        //qDebug() << "Using Spatial";

        _settingsAction.getDimensionSelectionAction().getDimensionAction().setPointsDataset(_positionSourceDataset);

        const auto& dimNames = _positionSourceDataset->getDimensionNames();
        auto enabledDimensions = _positionSourceDataset->getDimensionsPickerAction().getEnabledDimensions();
        _enabledDimNames.clear();
        for (int i = 0; i < enabledDimensions.size(); i++)
        {
            if (enabledDimensions[i])
                _enabledDimNames.push_back(dimNames[i]);
        }

        // update max number of genes in _numGenesThresholdAction
        _settingsAction.getClusteringAction().getNumGenesThresholdAction().setMaximum(static_cast<int>(_enabledDimNames.size()));

        updateSelection();
    }
}

void GeneSurferPlugin::updateNumCluster()
{
    // TODO: Remove, _scatterViews are no longer needed
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
    // TODO: Remove, _scatterViews are no longer needed
}

void GeneSurferPlugin::updateFilterLabel()
{
    if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::ATACtoRNA || _corrFilter.getFilterType() == corrFilter::CorrFilterType::RNAtoATAC)
    {
        _filterLabel->setText("Filter by:" + _corrFilter.getCorrFilterTypeAsString());
        _seedDimensionLabel->setText("Seed: " + _queryDimensionForATACRNA);
    }
    else
    {
        _filterLabel->setText("Filter dimensions by:" + _corrFilter.getCorrFilterTypeAsString());
        _seedDimensionLabel->setText("");
    }

    _selectedDimIndex = -1; // reset to no selection

    updateMappedDatasets();

    if (_corrFilter.getFilterType() == corrFilter::CorrFilterType::ATACtoRNA) {
        ////////////////////
        // ATAC to RNA   //
        ////////////////////
        _ATACtoRNA = true;
        qDebug() << "GeneSurferPlugin::updateFilterLabel(): set _ATACtoRNA = true";
        updateRNAData();

    }
    else {
        ////////////////////
        // RNA to ATAC //
        ////////////////////
        _ATACtoRNA = false;
        qDebug() << "GeneSurferPlugin::updateFilterLabel(): set _ATACtoRNA = false";
        updateSingleCellOption();

    }
}

void GeneSurferPlugin::updateMappedDatasets()
{
    // FIXME: dataset names are hardcoded here
    // check if mapping datasets are valid
    if (!_mappedRNAonSpatialDataset.isValid())
    {
        // try to find the mapped RNA dataset
        for (const auto& data : mv::data().getAllDatasets())
        {
            if (data->getGuiName() == "Mapped RNA dataset")
            {
                _mappedRNAonSpatialDataset = data;
                qDebug() << "Found Mapped RNA dataset";
                break;
            }
        }
    }

    if (!_mappedATAConSpatialDataset.isValid())
    {
        // try to find the mapped ATAC dataset
        for (const auto& data : mv::data().getAllDatasets())
        {
            if (data->getGuiName() == "Mapped ATAC dataset")
            {
                _mappedATAConSpatialDataset = data;
                qDebug() << "Found Mapped ATAC dataset";
                break;
            }
        }
    }
}

void GeneSurferPlugin::updateRNAData()
{
    if (!_avgExprDatasetRNA.isValid())
    {
        for (const auto& data : mv::data().getAllDatasets())
        {
            if (data->getGuiName() == "marm_Cluster_v4_metacell")
            {
                _avgExprDatasetRNA = data;
                qDebug() << "updateRNAData(): Found RNA averages dataset: marm_Cluster_v4_metacell";
                break;
            }
        }

        // check again
        if (!_avgExprDatasetRNA.isValid()) {
            qDebug() << "updateRNAData(): ERROR: no valid RNA averages dataset found!";
            return;
        }
    }
    else
    {
        qDebug() << "updateRNAData(): RNA averages dataset is valid" << _avgExprDatasetRNA->getGuiName();
    }

    if (_avgExprRNA.size() == 0) {

        if (_avgExprDatasetRNA.isValid()) {
            //qDebug() << "_avgExprDataset is valid...";
            switch (_avgExprStatusRNA)
            {
            case AvgExpressionStatus::NONE:
                //qDebug() << "Status: NONE";
                break;
            case AvgExpressionStatus::COMPUTED:
                //qDebug() << "Status: COMPUTED";
                qDebug() << "ERROR in updateRNAData(): not implemented";
                break;
            case AvgExpressionStatus::LOADED:
                //qDebug() << "Status: LOADED";

                // in oder to avoid computing again - TO DO: seperate this part in the function
                //qDebug() << "_avgExprDatasetRNA.isValid() = " << _avgExprDatasetRNA.isValid();
                convertToEigenMatrixProjection(_avgExprDatasetRNA, _avgExprRNA);

                _geneNamesAvgExprRNA.clear();
                _geneNamesAvgExprRNA = _avgExprDatasetRNA->getDimensionNames();

                _clusterNamesAvgExprRNA.clear();

                // FIXME: try to set _clusterNamesAvgExprRNA based on the metadata?? See if it is corresponding
                Dataset<Clusters> clusterLabelDatasetRNA;
                QVector<Cluster> clustersRNA;
                for (const auto& data : _avgExprDatasetRNA->getChildren())
                {
                    if (data->getGuiName() == "_index") // TODO: hardcoded name
                    {
                        clusterLabelDatasetRNA = data;
                        qDebug() << "Found RNA averages label dataset: _index";
                        break;
                    }
                }
                if (clusterLabelDatasetRNA.isValid())
                {
                    clustersRNA = clusterLabelDatasetRNA->getClusters();
                }
                else {
                    qDebug() << "updateRNAData(): ERROR: no valid RNA averages label dataset found!";
                    return;
                }

                // Mapping from cluster name to row (temp)
                std::map<QString, int> clusterToRowMap;
                for (int i = 0; i < clustersRNA.size(); ++i) {
                    QString clusterName = clustersRNA[i].getName();
                    const auto& ptIndices = clustersRNA[i].getIndices();
                    if (ptIndices.size() == 1)
                        clusterToRowMap[clusterName] = ptIndices[0];
                    else
                    {
                        qDebug() << "ERROR!!!!!updateRNAData() ptIndices.size() != 1";// should not happen
                    }
                }
                //qDebug() << "RNA clusterToRowMap.size() = " << clusterToRowMap.size();

                _clusterNamesAvgExprRNA.clear();
                _clusterNamesAvgExprRNA.resize(clusterToRowMap.size());

                for (const auto& [clusterName, idx] : clusterToRowMap) {
                    if (idx >= 0 && idx < _clusterNamesAvgExprRNA.size())
                        _clusterNamesAvgExprRNA[idx] = clusterName;
                }

                _clusterAliasToRowMap.clear();
                for (int i = 0; i < _clusterNamesAvgExprRNA.size(); ++i) {
                    _clusterAliasToRowMap[_clusterNamesAvgExprRNA[i]] = i;
                }
                qDebug() << "_clusterAliasToRowMap.size = " << _clusterAliasToRowMap.size();

                loadLabelsFromSTDatasetFromFileForRNA();

                break;
            }
        }
        else {
            return;
        }
    }
    else {
        // _avgExprRNA is already loaded or computed
        // in case switch from DIMENSION, only update _clusterAliasToRowMap and loadLabelsFromSTDatasetFromFile()
        //qDebug() << "_avgExprRNA is already loaded or computed" << _avgExprRNA.size();
        _clusterAliasToRowMap.clear();
        for (int i = 0; i < _clusterNamesAvgExprRNA.size(); ++i) {
            _clusterAliasToRowMap[_clusterNamesAvgExprRNA[i]] = i;

        }
        //qDebug() << "_clusterAliasToRowMap size: " << _clusterAliasToRowMap.size();
        loadLabelsFromSTDatasetFromFileForRNA();
    }

    _settingsAction.getDimensionSelectionAction().getDimensionAction().setPointsDataset(_avgExprDatasetRNA);

    // update _enabledDimNames
    _enabledDimNames.clear();
    _enabledDimNames = _geneNamesAvgExprRNA;

    // update max number of genes in _numGenesThresholdAction
    _settingsAction.getClusteringAction().getNumGenesThresholdAction().setMaximum(static_cast<int>(_enabledDimNames.size()));

    updateSelection();
}

void GeneSurferPlugin::updateScatterOpacity()
{ 
    // TODO: Remove, _scatterViews are no longer needed
}

void GeneSurferPlugin::updateScatterColors()
{
    // TODO: Remove, _scatterViews are no longer needed
}

void GeneSurferPlugin::updateDimView(const QString& selectedDimName)
{
    // TODO: remove, not used anymore
}

void GeneSurferPlugin::loadAvgExpressionFromFile() {
    qDebug() << "GeneSurferPlugin::loadAvgExpressionFromFile(): start... ";

    std::ifstream file;

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
        qDebug() << "GeneSurferPlugin::loadAvgExpressionFromFile Error: Could not open the avg expr file.";
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

    int numClusters = static_cast<int>(_clusterNamesAvgExpr.size());
    int numGenes = static_cast<int>(_geneNamesAvgExpr.size());

    file.close();

    _clusterAliasToRowMap.clear();// _clusterAliasToRowMap: first element is label name, second element is row index in _avgExpr
    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
        _clusterAliasToRowMap[_clusterNamesAvgExpr[i]] = i;
    }

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
    size_t totalElements = static_cast<size_t>(numClusters) * static_cast<size_t>(numGenes);

    std::vector<float> allData;
    allData.reserve(totalElements);
    for (const auto& row : matrixData) {
        allData.insert(allData.end(), row.begin(), row.end());
    }

    qDebug() << "GeneSurferPlugin::loadAvgExpressionFromFile(): allData size: " << allData.size();

    if (!_avgExprDataset.isValid()) {
        qDebug() << "Create an avgExprDataset";
        _avgExprDataset = mv::data().createDataset<Points>("Points", "avgExprDataset");
        events().notifyDatasetAdded(_avgExprDataset);
    }

    _avgExprDataset->setData(allData.data(), numClusters, numGenes); // Assuming this function signature is (data, rows, columns)
    _avgExprDataset->setDimensionNames(_geneNamesAvgExpr);
    events().notifyDatasetDataChanged(_avgExprDataset);
    qDebug() << "GeneSurferPlugin::loadAvgExpressionFromFile(): _avgExprDataset dataset created";

    // convert to eigen
    _avgExpr.resize(numClusters, numGenes);
    convertToEigenMatrixProjection(_avgExprDataset, _avgExpr);

}

void GeneSurferPlugin::loadLabelsFromSTDatasetFromFile() {
    // this is loading label from ST dataset!!!
    // Different from loading from singlecell datset!!!

    if (!_avgExprDataset.isValid()) // skip if it's not valid
    {
        qDebug() << "loadLabelsFromSTDatasetFromFile: _avgExprDataset is not valid";
        return;
    }

    QString labelDatasetName;

    labelDatasetName = _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentText();
    qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetFromFile(): labelDatasetName: " << labelDatasetName;

    Dataset<Clusters> labelDataset;
    for (const auto& data : mv::data().getAllDatasets())
    {
        //qDebug() << data->getGuiName();
        if (data->getGuiName() == labelDatasetName) {
            labelDataset = data;
            break;
        }
    }

    if (!labelDataset.isValid())
    {
        qDebug() << "ERROR: Could not find label dataset with name " << labelDatasetName;
        return;
    }

    QVector<Cluster> labelClusters = labelDataset->getClusters();

    //qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetFromFile(): labelClusters size: " << labelClusters.size();

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
            //qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetFromFile(): cluster in SC" << clusterName << " not found in ST";
            numClustersNotInST++;
            _countsAll[i] = 0;
        }
    }

    qDebug() << "Warning! GeneSurferPlugin::loadLabelsFromSTDatasetFromFile: " << numClustersNotInST << " clusters not found in ST";

    if (numClustersNotInST == _clusterNamesAvgExpr.size()) {
        qDebug() << "ERROR: None of the clusters from loaded csv file were found in the selected ST label dataset.";
        QMessageBox::warning(
            nullptr,
            "Label Matching Error",
            "None of the scRNA-seq clusters were found in the selected ST label dataset."
        );
    }

    /*qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetFromFile(): _cellLabels size: " << _cellLabels.size();
    qDebug() << "_cellLabels[0]" << _cellLabels[0];*/
}

void GeneSurferPlugin::loadLabelsFromSTDatasetFromFileForRNA() {
    // this is loading label from ST dataset!!!
    // Different from loading from singlecell datset!!!

    if (!_avgExprDatasetRNA.isValid()) // skip if it's not valid
    {
        qDebug() << "loadLabelsFromSTDatasetFromFile: _avgExprDatasetRNA is not valid";
        return;
    }

    QString labelDatasetName;

    labelDatasetName = _settingsAction.getSingleCellModeAction().getLabelDatasetPickerAction().getCurrentText();
    qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetFromFileForRNA(): labelDatasetName: " << labelDatasetName;

    Dataset<Clusters> labelDataset;
    for (const auto& data : mv::data().getAllDatasets())
    {
        //qDebug() << data->getGuiName();
        if (data->getGuiName() == labelDatasetName) {
            labelDataset = data;
            break;
        }
    }

    if (!labelDataset.isValid())
    {
        qDebug() << "ERROR: Could not find label dataset with name " << labelDatasetName;
        return;
    }

    QVector<Cluster> labelClusters = labelDataset->getClusters();

    // add weighting for each cluster of whole data
    _countsAllRNA.resize(_clusterNamesAvgExprRNA.size()); // number of clusters in RNA

    // precompute the cell-label array
    _cellLabels.clear();
    _cellLabels.resize(_numPoints);

    int numClustersNotInST = 0;

    for (int i = 0; i < _clusterNamesAvgExprRNA.size(); ++i) {
        QString clusterName = _clusterNamesAvgExprRNA[i];
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
                _countsAllRNA[i] = ptIndices.size(); // number of pt in each cluster
                found = true;
                break;
            }
        }

        if (!found) {
            // If the cluster is not found in labelClusters
            //qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetFromFile(): cluster in SC" << clusterName << " not found in ST";
            numClustersNotInST++;
            _countsAllRNA[i] = 0;
        }
    }

    qDebug() << "Warning! GeneSurferPlugin::loadLabelsFromSTDatasetFromFileForRNA: " << numClustersNotInST << " clusters not found in ST";
    //qDebug() << "_clusterNamesAvgExprRNA.size = " << _clusterNamesAvgExprRNA.size() << "_countsAllRNA.size = " << _countsAllRNA.size();

    if (numClustersNotInST == _clusterNamesAvgExprRNA.size()) {
        qDebug() << "ERROR: None of the clusters from loaded csv file were found in the selected ST label dataset.";
        QMessageBox::warning(
            nullptr,
            "Label Matching Error",
            "None of the scRNA-seq clusters were found in the selected ST label dataset."
        );
    }

    /*qDebug() << "GeneSurferPlugin::loadLabelsFromSTDatasetFromFile(): _cellLabels size: " << _cellLabels.size();
    qDebug() << "_cellLabels[0]" << _cellLabels[0];*/
}

void GeneSurferPlugin::countLabelDistribution()
{
    _countsMap.clear();

    for (const auto rawIndex : _sortedFloodIndices)
    {
        const auto ptIndex = static_cast<std::uint64_t>(rawIndex);
        const QString& label = _cellLabels[ptIndex];
        ++_countsMap[label];
    }

    if (_ATACtoRNA)
    { 
        matchLabelInSubsetForRNA();
    }

    else
    {
        matchLabelInSubset();
    }

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

void GeneSurferPlugin::matchLabelInSubsetForRNA()
{
    int numClusters = _avgExprRNA.rows();
    int numGenes = _avgExprRNA.cols();
    //qDebug() << "GeneSurferPlugin::matchLabelInSubset(): before matching numClusters: " << numClusters << " numGenes: " << numGenes;

    std::vector<QString> clustersToKeep; // it is cluster names 1
    for (int i = 0; i < numClusters; ++i) {
        QString clusterName = _clusterNamesAvgExprRNA[i]; // here cannot use _clusterAliasToRowMap, because need to keep the order in clustersToKeep?
        if (_countsMap.find(clusterName) != _countsMap.end()) {
            clustersToKeep.push_back(clusterName);
        }
    }

    // to check if any columns are not in _columnNamesAvgExpr
    if (clustersToKeep.size() != _countsMap.size()) {
        qDebug() << "Warning! GeneSurferPlugin::matchLabelInSubsetForRNA(): " << _countsMap.size() - clustersToKeep.size() << "clusters not found in avgExprRNA";
        // output the cluster names that are not in
        QString output;
        for (const auto& pair : _countsMap) {
            QString clusterName = pair.first;
            if (std::find(_clusterNamesAvgExprRNA.begin(), _clusterNamesAvgExprRNA.end(), clusterName) == _clusterNamesAvgExprRNA.end()) {
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
        qDebug() << "GeneSurferPlugin::matchLabelInSubsetForRNA(): No cluster to keep";
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
    // TODO: remove, not used anymore
}


void GeneSurferPlugin::computeFloodedClusterScalars(const std::vector<int> filteredDimIndices, const int* labels)
{
    // TODO: Remove, not used anymore
}

void GeneSurferPlugin::computeFloodedClusterScalarsSingleCell(const std::vector<int> filteredDimIndices, const int* labels) {
    // TODO: Remove, not used anymore
}

void GeneSurferPlugin::updateClusterScalarOutput(const std::vector<float>& scalars)
{
    // TODO: remove, not used anymore
}

void GeneSurferPlugin::getFuntionalEnrichment()
{
    // TODO: Remove, not used anymore
}

void GeneSurferPlugin::updateEnrichmentTable(const QVariantList& data) {
    // TODO: Remove, not used anymore
}

void GeneSurferPlugin::noDataEnrichmentTable() {
    // TODO: Remove, not used anymore
}

void GeneSurferPlugin::onTableClicked(int row, int column) {
    // TODO: Remove, not used anymore
}

void GeneSurferPlugin::updateClick() {
    // TODO: Remove, not used anymore
}

void GeneSurferPlugin::updateSlice(int sliceIndex) {
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

    if (_sliceDataset.isValid())
    {
        //qDebug() << "GeneSurferPlugin::fromVariantMap() 4 ";
        _currentSliceIndex = variantMap["CurrentSliceIdx"].toInt();// TODO: check if this is still needed if _sectionAction is serialized
        updateSlice(_currentSliceIndex);
    }

    _selectedDimName = variantMap["SelectedDimName"].toString();

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
        // if selection is empty, no need to update the selected data
        if (selection->indices.size() == 0)
        {
            qDebug() << "GeneSurferPlugin::fromVariantMap(): selection is empty, no need to update the selected data";
            return;
        }

        if (!_sliceDataset.isValid())
            _computeSubset.updateSelectedData(_positionDataset, selection, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex);
        else
            _computeSubset.updateSelectedData(_positionDataset, selection, _onSliceIndices, _sortedFloodIndices, _sortedWaveNumbers, _isFloodIndex, _isFloodOnSlice, _onSliceFloodIndices);
        updateSelection();
    }


    // TODO: needed for generating project
    // generate a cluster dataset for _clusterNamesAvgExpr and attach it to parent dataset _avgExprDataset 
    /*Dataset<Clusters> clusterDataset = mv::data().createDataset<Clusters>("Cluster", "Clusters", _avgExprDataset);
    QVector<Cluster> clusters;
    for (int i = 0; i < _clusterNamesAvgExpr.size(); ++i) {
        Cluster cluster;
        cluster.setName(_clusterNamesAvgExpr[i]);
        std::vector<uint32_t> indices = { static_cast<uint32_t>(i) };
        cluster.setIndices(indices);
        clusters.push_back(cluster);
    }
    clusterDataset->setClusters(clusters);
    mv::events().notifyDatasetAdded(clusterDataset);
    mv::events().notifyDatasetDataChanged(clusterDataset);
    qDebug() << "GeneSurferPlugin::fromVariantMap: clusterDataset created with " << clusters.size() << " clusters";*/

    _loadingFromProject = false;

}

QVariantMap GeneSurferPlugin::toVariantMap() const
{
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _primaryToolbarAction.insertIntoVariantMap(variantMap);
    _settingsAction.insertIntoVariantMap(variantMap);

    variantMap.insert("AvgExpressionStatus", static_cast<int>(_avgExprStatus));

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

    return variantMap;
}


// =============================================================================
// Plugin Factory 
// =============================================================================

GeneSurferPluginFactory::GeneSurferPluginFactory()
{
    setIconByName("bullseye");
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