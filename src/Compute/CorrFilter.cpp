#include "CorrFilter.h"

#include "graphics/Vector2f.h"
#include "graphics/Vector3f.h"

#include <numeric>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <QDebug>

using namespace mv;

namespace corrFilter
{
    void CorrFilter::computePairwiseCorrelationVector(const std::vector<QString>& dimNames, const std::unordered_map<QString, int>& dimNameToIndex, const DataMatrix& dataMatrix, DataMatrix& corrMatrix) const
    {
        std::vector<Eigen::VectorXf> centeredVectors(dimNames.size());// precompute mean 
        std::vector<float> norms(dimNames.size());

        for (int i = 0; i < dimNames.size(); ++i) {
            //int index = dimNameToIndex[dimNames[i]];
            int index = dimNameToIndex.at(dimNames[i]);
            Eigen::VectorXf centered = dataMatrix.col(index) - Eigen::VectorXf::Constant(dataMatrix.col(index).size(), dataMatrix.col(index).mean());
            centeredVectors[i] = centered;
            norms[i] = centered.squaredNorm();
        }

        #pragma omp parallel for
        for (int col1 = 0; col1 < dimNames.size(); ++col1) {
            for (int col2 = col1; col2 < dimNames.size(); ++col2) {
                float correlation = centeredVectors[col1].dot(centeredVectors[col2]) / std::sqrt(norms[col1] * norms[col2]);
                if (std::isnan(correlation)) { correlation = 0.0f; } // TO DO: check if this is a good way to handle nan in corr computation

                corrMatrix(col1, col2) = correlation;
                corrMatrix(col2, col1) = correlation;
            }
        }
    }

    void SpatialCorr::computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, std::vector<float>& corrVector) const
    {
        std::vector<float> xCoords, yCoords;
        for (int index : floodIndices) {
            xCoords.push_back(positions[index].x);
            yCoords.push_back(positions[index].y);
        }

        Eigen::VectorXf xVector = Eigen::Map<Eigen::VectorXf>(xCoords.data(), xCoords.size());
        Eigen::VectorXf yVector = Eigen::Map<Eigen::VectorXf>(yCoords.data(), yCoords.size());

        // Precompute means and norms for xVector, yVector
        float meanX = xVector.mean(), meanY = yVector.mean();
        Eigen::VectorXf centeredX = xVector - Eigen::VectorXf::Constant(xVector.size(), meanX);
        Eigen::VectorXf centeredY = yVector - Eigen::VectorXf::Constant(yVector.size(), meanY);
        float normX = centeredX.squaredNorm(), normY = centeredY.squaredNorm();

        std::vector<float> correlationsX(dataMatrix.cols());
        std::vector<float> correlationsY(dataMatrix.cols());

        corrVector.clear();
        corrVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i) {
            Eigen::VectorXf column = dataMatrix.col(i);
            float meanColumn = column.mean();
            Eigen::VectorXf centeredColumn = column - Eigen::VectorXf::Constant(column.size(), meanColumn);
            float normColumn = centeredColumn.squaredNorm();

            float correlationX = centeredColumn.dot(centeredX) / std::sqrt(normColumn * normX);
            float correlationY = centeredColumn.dot(centeredY) / std::sqrt(normColumn * normY);

            if (std::isnan(correlationX)) { correlationX = 0.0f; }
            if (std::isnan(correlationY)) { correlationY = 0.0f; }
            correlationsX[i] = correlationX;
            correlationsY[i] = correlationY;
            corrVector[i] = (std::abs(correlationX) + std::abs(correlationY))/2;
        }
    }

    void SpatialCorr::computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, const std::vector<float>& zPositions, std::vector<float>& corrVector) const
    {

        std::vector<float> xCoords, yCoords, zCoords;
        for (int index : floodIndices) {
            xCoords.push_back(positions[index].x);
            yCoords.push_back(positions[index].y);
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

        std::vector<float> correlationsX(dataMatrix.cols());
        std::vector<float> correlationsY(dataMatrix.cols());
        std::vector<float> correlationsZ(dataMatrix.cols());

        corrVector.clear();
        corrVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i) {
            Eigen::VectorXf column = dataMatrix.col(i);

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
            corrVector[i] = std::abs(correlationX) + std::abs(correlationY) + std::abs(correlationZ);
        }
    }
  
    void HDCorr::computeCorrelationVector(const std::vector<float>& waveNumbers, const DataMatrix& dataMatrix, std::vector<float>& corrVector) const
    {
        Eigen::VectorXf eigenWaveNumbers(waveNumbers.size());
        std::copy(waveNumbers.begin(), waveNumbers.end(), eigenWaveNumbers.data());

        Eigen::VectorXf centeredWaveNumbers = eigenWaveNumbers - Eigen::VectorXf::Constant(eigenWaveNumbers.size(), eigenWaveNumbers.mean());
        float waveNumbersNorm = centeredWaveNumbers.squaredNorm();

        corrVector.clear();
        corrVector.resize(dataMatrix.cols());
#pragma omp parallel for
        for (int col = 0; col < dataMatrix.cols(); ++col) {
            Eigen::VectorXf centered = dataMatrix.col(col) - Eigen::VectorXf::Constant(dataMatrix.col(col).size(), dataMatrix.col(col).mean());
            float correlation = centered.dot(centeredWaveNumbers) / std::sqrt(centered.squaredNorm() * waveNumbersNorm);
            if (std::isnan(correlation)) { correlation = 0.0f; }
            corrVector[col] = correlation;
        }
    }

    void HDCorr::computeCorrelationVector(const std::vector<int>& waveNumbers, const DataMatrix& dataMatrix, std::vector<float>& corrVector) const
    {
        Eigen::VectorXf eigenWaveNumbers(waveNumbers.size());
        std::copy(waveNumbers.begin(), waveNumbers.end(), eigenWaveNumbers.data());

        Eigen::VectorXf centeredWaveNumbers = eigenWaveNumbers - Eigen::VectorXf::Constant(eigenWaveNumbers.size(), eigenWaveNumbers.mean());
        float waveNumbersNorm = centeredWaveNumbers.squaredNorm();

        corrVector.clear();
        corrVector.resize(dataMatrix.cols());
#pragma omp parallel for
        for (int col = 0; col < dataMatrix.cols(); ++col) {
            Eigen::VectorXf centered = dataMatrix.col(col) - Eigen::VectorXf::Constant(dataMatrix.col(col).size(), dataMatrix.col(col).mean());
            float correlation = centered.dot(centeredWaveNumbers) / std::sqrt(centered.squaredNorm() * waveNumbersNorm);
            if (std::isnan(correlation)) { correlation = 0.0f; }
            corrVector[col] = correlation;
        }
    }

}
