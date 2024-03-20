#include "FloodSubset.h"



void FloodSubset::updateFloodFill(mv::Dataset<Points> floodFillDataset, const int numPoints, std::vector<int>& floodIndices, std::vector<int>& waveNumbers, std::vector<bool>& isFloodIndex)
{
    if (!floodFillDataset.isValid())
    {
        qDebug() << "WARNING: floodFillDataset is not valid";
        return;
    }

    qDebug() << "FloodSubset::updateFloodFill(): numPoints: " << numPoints;
    
    FloodSubset::processFloodFillDataset(floodFillDataset, floodIndices, waveNumbers); // updates floodIndices and waveNumbers

    qDebug() << "FloodSubset::updateFloodFill(): floodIndices size: " << floodIndices.size();

    updateIsFloodIndex(numPoints, floodIndices, isFloodIndex);// updates isFloodIndex

    qDebug() << "FloodSubset::updateFloodFill(): isFloodIndex size: " << isFloodIndex.size();

    

}

void FloodSubset::processFloodFillDataset(mv::Dataset<Points> floodFillDataset, std::vector<int>& floodIndices, std::vector<int>& waveNumbers)
{
    // get flood fill nodes from the core
    std::vector<float> floodNodesWave(floodFillDataset->getNumPoints());
    floodFillDataset->populateDataForDimensions < std::vector<float>, std::vector<float>>(floodNodesWave, { 0 });
    qDebug() << "FloodSubset::processFloodFillDataset(): floodNodesWave size: " << floodNodesWave.size();

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

void FloodSubset::updateIsFloodIndex(const int numPoints, const std::vector<int>& floodIndices, std::vector<bool>& isFloodIndex)
{
    // must be put after orderSpatially() and before computeSubsetData()
    isFloodIndex.clear();
    isFloodIndex.resize(numPoints, false);
    for (int idx : floodIndices) {
        if (idx < isFloodIndex.size()) {
            isFloodIndex[idx] = true;
        }
        else {
            qDebug() << "FloodSubset::updateIsFloodIndex(): idx out of range: " << idx;
            return;
        }
    }
}
