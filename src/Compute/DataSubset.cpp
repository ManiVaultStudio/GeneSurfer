#include "DataSubset.h"

#include <set>

void DataSubset::updateFloodFill(mv::Dataset<Points> floodFillDataset, const int numPoints, std::vector<int>& floodIndices, std::vector<int>& waveNumbers, std::vector<bool>& isFloodIndex)
{   // for 2D data
    if (!floodFillDataset.isValid())
    {
        qDebug() << "WARNING: floodFillDataset is not valid";
        return;
    }
    // TO DO: only keep either bool or int for isFloodIndex
  
    processFloodFillDataset(floodFillDataset, floodIndices, waveNumbers); // updates floodIndices and waveNumbers

    updateIsFloodIndex(numPoints, floodIndices, isFloodIndex);// updates isFloodIndex

}

void DataSubset::updateFloodFill(mv::Dataset<Points> floodFillDataset, const int numPoints, const std::vector<int>& onSliceIndices, std::vector<int>& floodIndices, std::vector<int>& waveNumbers, std::vector<bool>& isFloodIndex, std::vector<bool>& isFloodOnSlice, std::vector<int>& onSliceFloodIndices)
{   // for 3D data
    if (!floodFillDataset.isValid())
    {
        qDebug() << "WARNING: floodFillDataset is not valid";
        return;
    }

    // TO DO: only keep either bool or int for isFloodIndex and isFloodOnSlice

    processFloodFillDataset(floodFillDataset, floodIndices, waveNumbers); // updates floodIndices and waveNumbers

    updateIsFloodIndex(numPoints, floodIndices, isFloodIndex);// updates isFloodIndex

    updateFloodFillOnSlice(isFloodIndex, floodIndices, onSliceIndices, isFloodOnSlice, onSliceFloodIndices);// updates slice subset for 3D data

}

void DataSubset::processFloodFillDataset(mv::Dataset<Points> floodFillDataset, std::vector<int>& floodIndices, std::vector<int>& waveNumbers)
{
    // get flood fill nodes from the core
    std::vector<float> floodNodesWave(floodFillDataset->getNumPoints());
    floodFillDataset->populateDataForDimensions < std::vector<float>, std::vector<float>>(floodNodesWave, { 0 });
    //qDebug() << "DataSubset::processFloodFillDataset(): floodNodesWave size: " << floodNodesWave.size();

    // process the selected flood-fill 
    floodIndices.clear();
    waveNumbers.clear();
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
}

void DataSubset::updateIsFloodIndex(const int numPoints, const std::vector<int>& floodIndices, std::vector<bool>& isFloodIndex)
{
    // must be put after orderSpatially() and before computeSubsetData()
    isFloodIndex.clear();
    isFloodIndex.resize(numPoints, false);
    for (int idx : floodIndices) {
        if (idx < isFloodIndex.size()) {
            isFloodIndex[idx] = true;
        }
        else {
            qDebug() << "DataSubset::updateIsFloodIndex(): idx out of range: " << idx;
            qWarning() << "ERROR: allFloodNodesIndices might do not match active dataset."; // TODO: tie floodfill data to specific datasets
            return;
        }
    }
}

void DataSubset::updateFloodFillOnSlice(const std::vector<bool>& isFloodIndex, const std::vector<int>& floodIndices, const std::vector<int>& onSliceIndices, std::vector<bool>& isFloodOnSlice, std::vector<int>& onSliceFloodIndices)
{
    
    isFloodOnSlice.clear();
    isFloodOnSlice.resize(onSliceIndices.size(), false);
    for (int i = 0; i < onSliceIndices.size(); i++)
    {
        isFloodOnSlice[i] = isFloodIndex[onSliceIndices[i]];
    }

    //qDebug() << "DataSubset::updateIsFloodOnSlice(): _isFloodOnSlice size" << isFloodOnSlice.size();
  
    onSliceFloodIndices.clear();   
    std::set<int> sliceIndicesSet(onSliceIndices.begin(), onSliceIndices.end());
    for (int i = 0; i < floodIndices.size(); ++i) {
        if (sliceIndicesSet.find(floodIndices[i]) != sliceIndicesSet.end()) {
            onSliceFloodIndices.push_back(floodIndices[i]);
        }
    }

    //qDebug() << "DataSubset::updateIsFloodOnSlice(): _onSliceFloodIndices size" << onSliceFloodIndices.size();
}

void DataSubset::updateSelectedData(mv::Dataset<Points> positionDataset, mv::Dataset<Points> selection, std::vector<int>& floodIndices, std::vector<int>& waveNumbers, std::vector<bool>& isFloodIndex)
{   // for 2D data
    std::vector<bool> selected;
    positionDataset->selectedLocalIndices(selection->indices, selected);

    floodIndices.clear();
    waveNumbers.clear();
    isFloodIndex.clear();

    // TODO: check if the values in indices are within range of int
    floodIndices.assign(selection->indices.begin(), selection->indices.end());

    waveNumbers.resize(floodIndices.size(), 1);// fake wave numbers to avoid crash

    isFloodIndex.resize(positionDataset->getNumPoints(), 0);
    for (std::size_t i = 0; i < selected.size(); i++)
        isFloodIndex[i] = selected[i] ? 1 : 0;
}

void DataSubset::updateSelectedData(mv::Dataset<Points> positionDataset, mv::Dataset<Points> selection, const std::vector<int>& onSliceIndices, std::vector<int>& floodIndices, std::vector<int>& waveNumbers, std::vector<bool>& isFloodIndex, std::vector<bool>& isFloodOnSlice, std::vector<int>& onSliceFloodIndices)
{   // for 3D data
    std::vector<bool> selected;
    positionDataset->selectedLocalIndices(selection->indices, selected);

    floodIndices.clear();
    waveNumbers.clear();
    isFloodIndex.clear();

    // TODO: check if the values in indices are within range of int
    floodIndices.assign(selection->indices.begin(), selection->indices.end());

    waveNumbers.resize(floodIndices.size(), 1);// fake wave numbers to avoid crash

    isFloodIndex.resize(positionDataset->getNumPoints(), 0);
    for (std::size_t i = 0; i < selected.size(); i++)
        isFloodIndex[i] = selected[i] ? 1 : 0;

    // additional part for 3D data
    isFloodOnSlice.clear();
    onSliceFloodIndices.clear();

    isFloodOnSlice.resize(onSliceIndices.size(), false);
    for (int i = 0; i < onSliceIndices.size(); i++)
    {
        isFloodOnSlice[i] = isFloodIndex[onSliceIndices[i]];
    }

    std::set<int> sliceIndicesSet(onSliceIndices.begin(), onSliceIndices.end());
    for (int i = 0; i < floodIndices.size(); ++i) {
        if (sliceIndicesSet.find(floodIndices[i]) != sliceIndicesSet.end()) {
            onSliceFloodIndices.push_back(floodIndices[i]);
        }
    }
}

void DataSubset::computeSubsetData(const DataMatrix& dataMatrix, const std::vector<int>& indices, DataMatrix& subsetDataMatrix)
{
    if (indices.empty()) {
        qDebug() << "WARNING: DataSubset::computeSubsetData(): empty indices";
        return;
    }

    int rows = indices.size();
    int cols = dataMatrix.cols();

    subsetDataMatrix.resize(rows, cols);

#pragma omp parallel for
    for (int i = 0; i < rows; ++i)
    {
        int index = indices[i];
        subsetDataMatrix.row(i) = dataMatrix.row(index);
    }
}

void DataSubset::computeSubsetDataAvgExpr(const DataMatrix& dataMatrix, const std::vector<QString>& clusterNames, const std::unordered_map<QString, int>& clusterToRowMap, DataMatrix& subsetDataMatrix)
{
    const auto numRows = static_cast<std::uint64_t>(clusterNames.size());
    const auto numColumns = static_cast<std::uint64_t>(dataMatrix.cols());

    subsetDataMatrix.resize(static_cast<Eigen::Index>(numRows), static_cast<Eigen::Index>(numColumns));

    // Resolve QString lookups once
    std::vector<std::uint64_t> sourceRows(numRows);

    for (std::uint64_t row = 0; row < numRows; ++row)
    {
        const auto it = clusterToRowMap.find(clusterNames[row]);

        if (it == clusterToRowMap.end())
        {
            qDebug() << "Error: clusterName" << clusterNames[row] << "not found in clusterToRowMap";

            sourceRows[row] = std::numeric_limits<std::uint64_t>::max();

            continue;
        }

        sourceRows[row] = static_cast<std::uint64_t>(it->second);
    }

    // parallelize over columns
    const auto ompNumColumns = static_cast<std::int64_t>(numColumns);

#pragma omp parallel for schedule(static)
    for (std::int64_t column = 0; column < ompNumColumns; ++column)
    {
        const auto eigenColumn = static_cast<Eigen::Index>(column);

        const float* source = dataMatrix.col(eigenColumn).data();

        float* destination = subsetDataMatrix.col(eigenColumn).data();

        for (std::uint64_t row = 0; row < numRows; ++row)
        {
            const auto sourceRow = sourceRows[row];

            if (sourceRow != std::numeric_limits<std::uint64_t>::max())
            {
                destination[row] = source[sourceRow];
            }
        }
    }
}
