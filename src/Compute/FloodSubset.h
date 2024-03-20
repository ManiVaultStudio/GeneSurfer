#pragma once

#include "PointData/PointData.h"
#include "DataMatrix.h"

#include <vector>
#include <QDebug>

class FloodSubset
{
public:
    // for 2D data
    void updateFloodFill(mv::Dataset<Points> floodFillDataset, const int numPoints, std::vector<int>& floodIndices, std::vector<int>& waveNumbers, std::vector<bool>& isFloodIndex);
    // overloaded for 3D data
    void updateFloodFill(mv::Dataset<Points> floodFillDataset, const int numPoints, const std::vector<int>& onSliceIndices, std::vector<int>& floodIndices, std::vector<int>& waveNumbers, std::vector<bool>& isFloodIndex, std::vector<bool>& isFloodOnSlice, std::vector<int>& onSliceFloodIndices);

    void computeSubsetData(const DataMatrix& dataMatrix, const std::vector<int>& indices, DataMatrix& subsetDataMatrix);

    void computeSubsetDataAvgExpr(const DataMatrix& dataMatrix, const std::vector<QString>& clusterNames, const std::unordered_map<QString, int>& clusterToRowMap, DataMatrix& subsetDataMatrix);

private:

    void processFloodFillDataset(mv::Dataset<Points> floodFillDataset, std::vector<int>& floodIndices, std::vector<int>& waveNumbers);

    void updateIsFloodIndex(const int numPoints, const std::vector<int>& floodIndices, std::vector<bool>& isFloodIndex);

    void updateFloodFillOnSlice(const std::vector<bool>& isFloodIndex, const std::vector<int>& floodIndices, const std::vector<int>& onSliceIndices, std::vector<bool>& isFloodOnSlice, std::vector<int>& onSliceFloodIndices);
};
