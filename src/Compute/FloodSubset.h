#pragma once

#include <vector>
#include "PointData/PointData.h"
#include <QDebug>

class FloodSubset
{
public:

    void updateFloodFill(mv::Dataset<Points> floodFillDataset, const int numPoints, std::vector<int>& floodIndices, std::vector<int>& waveNumbers, std::vector<bool>& isFloodIndex);


private:

    void processFloodFillDataset(mv::Dataset<Points> floodFillDataset, std::vector<int>& floodIndices, std::vector<int>& waveNumbers);

    void updateIsFloodIndex(const int numPoints, const std::vector<int>& floodIndices, std::vector<bool>& isFloodIndex);


};
