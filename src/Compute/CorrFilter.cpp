#include "CorrFilter.h"

#include "graphics/Vector2f.h"
#include "graphics/Vector3f.h"

#include <numeric>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <QDebug>

#include <chrono>

using namespace mv;

namespace
{
    float mean(const std::vector<float>& v) {
        return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    }

    void normalizeWeightMatrix(std::vector<std::vector<float>>& weight) {
        size_t N = weight.size();
        for (size_t i = 0; i < N; i++) {
            float row_sum = std::accumulate(weight[i].begin(), weight[i].end(), 0.0f);
            if (row_sum != 0) {
                for (size_t j = 0; j < N; j++) {
                    weight[i][j] /= row_sum;
                }
            }
        }
    }

    float euclideanDistance(float x1, float y1, float x2, float y2) {
        return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
    }

    float euclideanDistance3D(float x1, float y1, float z1, float x2, float y2, float z2) {
        return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) + (z2 - z1) * (z2 - z1));
    }
}

namespace corrFilter
{
    std::vector<std::vector<float>> CorrFilter::computeWeightMatrix(const std::vector<float>& xCoordinates, const std::vector<float>& yCoordinates) 
    { // 2D
        size_t N = xCoordinates.size();
        std::vector<std::vector<float>> weightMatrix(N, std::vector<float>(N, 0.0f));

        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < N; j++) {
                if (i != j) { // Avoid self-loops
                    float dist = euclideanDistance(xCoordinates[i], yCoordinates[i], xCoordinates[j], yCoordinates[j]);
                    if (dist != 0) {
                        weightMatrix[i][j] = 1.0f / dist; // Inverse distance weighting
                    }
                }
            }
        }

        std::vector<std::vector<float>> normWeightMatrix = weightMatrix;
        normalizeWeightMatrix(normWeightMatrix);

        return normWeightMatrix;
    }

    std::vector<std::vector<float>> CorrFilter::computeWeightMatrix(const std::vector<float>& xCoordinates, const std::vector<float>& yCoordinates, const std::vector<float>& zCoordinates) 
    { // 3D
        size_t N = xCoordinates.size();
        std::vector<std::vector<float>> weightMatrix(N, std::vector<float>(N, 0.0f));

        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < N; j++) {
                if (i != j) { // Avoid self-loops
                    float dist = euclideanDistance3D(xCoordinates[i], yCoordinates[i], zCoordinates[i], xCoordinates[j], yCoordinates[j], zCoordinates[j]);
                    if (dist != 0) {
                        weightMatrix[i][j] = 1.0f / dist; // Inverse distance weighting
                    }
                }
            }
        }

        std::vector<std::vector<float>> normWeightMatrix = weightMatrix;
        normalizeWeightMatrix(normWeightMatrix);

        return normWeightMatrix;
    }

    void CorrFilter::moranParameters(const std::vector<std::vector<float>>& weight, float& W, float& S1, float& S2, float& S4, float& S5)
    {
        size_t N = weight.size();// number of spatial units indexed by i and j

        W = 0.0f; // sum of weights
        S1 = 0.0f;
        S2 = 0.0f;

        for (size_t i = 0; i < N; ++i) {
            float rs = 0.0f;
            float cs = 0.0f;

            for (size_t j = 0; j < N; ++j) {
                W += weight[i][j];

                // for S1
                float S1Temp = weight[i][j] + weight[j][i];
                S1 += S1Temp * S1Temp;

                // for S2
                rs += weight[i][j];
                cs += weight[j][i];
            }
            S2 += std::pow((rs + cs), 2); // or S2 += (rs + cs) * (rs + cs);
        }
        S1 = S1 / 2;
        float Wsq = W * W;
        float Nsq = N * N;
        S4 = (Nsq - 3 * N + 3) * S1 - N * S2 + 3 * Wsq;
        S5 = (Nsq - N) * S1 - 2 * N * S2 + 6 * Wsq;
    }

    std::vector<float> CorrFilter::moranTest_C(const std::vector<float>& x, std::vector<std::vector<float>>& weight, const float W, const float S1, const float S2, const float S4, const float S5)
    {
        
        size_t N = weight.size();// number of spatial units indexed by i and j

        // weight should be normalized
        // weight has al been normalized in computeWeightMatrix()
        //std::vector<std::vector<float>> normWeight = weight;
        //normalizeWeightMatrix(normWeight);

        // Calculate mean of x
        float xMean = mean(x);
        std::vector<float> z(N); // vector z = x - xMean
        std::transform(x.begin(), x.end(), z.begin(), [xMean](float xi) { return xi - xMean; });

        float cv = 0.0f;
     
        for (size_t i = 0; i < N; ++i) {        
            for (size_t j = 0; j < N; ++j) {
                cv += weight[i][j] * z[i] * z[j];
            }
        }

        float Wsq = W * W;  
        float ei = -1.0f / (N - 1); // Expected value of Moran's I
   
        float sumz2 = std::accumulate(z.begin(), z.end(), 0.0f, [](float acc, float x) { return acc + x * x; }); // sum of z^2
        float obs = (N / W) * (cv / sumz2); // Moran's I
       
        float sumz4 = std::accumulate(z.begin(), z.end(), 0.0f, [](float acc, float x) { return acc + std::pow(x, 4); });// sum of z^4
        float S3 = sumz4 / N / std::pow(sumz2 / N, 2);
      
        float varI = ((N * S4 - S3 * S5) / ((N - 1) * (N - 2) * (N - 3) * Wsq)) - ei * ei; // Variance of Moran's I // TODO: check N > 3
        float sd = sqrt(varI); // Standard deviation of Moran's I

        std::vector<float> results = { obs, ei, sd };// Moran's I, Expected I, SD
        return results;
    }

    void CorrFilter::computeMoranVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, std::vector<float>& moranVector)
    {
        // 2D
        qDebug() << "Compute moran's I started...";
        std::vector<float> xCoordinates;
        std::vector<float> yCoordinates;

        for (int i = 0; i < floodIndices.size(); ++i)
        {
            int index = floodIndices[i];
            xCoordinates.push_back(positions[index].x);
            yCoordinates.push_back(positions[index].y);
        }
        std::vector<std::vector<float>> distanceMat = computeWeightMatrix(xCoordinates, yCoordinates);
        qDebug() << "Compute distance matrix finished...";

        // compute weight-related parameters 
        float W, S1, S2, S4, S5;
        moranParameters(distanceMat, W, S1, S2, S4, S5);


        moranVector.clear();
        moranVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i)
        {
            auto col = dataMatrix.col(i);
            std::vector<float> geneExpression;
            geneExpression.assign(col.data(), col.data() + col.size());
            
            std::vector<float> result = moranTest_C(geneExpression, distanceMat, W, S1, S2, S4, S5);

            float moranI = result[0];
            float expectedI = result[1];
            float sd = result[2];
            float zScore;
            if (sd != 0)
                zScore = (moranI - expectedI) / sd;
            else
                zScore = 0;

            // Check if zScore is NaN   
            if (std::isnan(zScore)) {
                zScore = 0;
                //qDebug() << "Gene name: " << _enabledDimNames[i] << " Moran's I: " << result[0] << " sd: " << sd << " Z-score: nan";
            }

            //qDebug() << "Gene name: " << _enabledDimNames[i] << " Moran's I: " << result[0] << " sd: " << sd << " Z-score: " << zScore;
            moranVector[i] = zScore;
        }
        qDebug() << "Compute moran's I finished...";

        // normalize the correlation vector to 0 to 1for plotting in the bar chart
        float minCorr = *std::min_element(moranVector.begin(), moranVector.end());
        float maxCorr = *std::max_element(moranVector.begin(), moranVector.end());
        qDebug() << "minCorr: " << minCorr << " maxCorr: " << maxCorr;

        for (int i = 0; i < moranVector.size(); ++i) {
            moranVector[i] = (moranVector[i] - minCorr) / (maxCorr - minCorr);
        }
        qDebug() << "Normalize moran's I finished...";
    }

    void CorrFilter::computeMoranVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<float>& xPositions, const std::vector<float>& yPositions, const std::vector<float>& zPositions, std::vector<float>& moranVector)
    {
         //3D all flood indices
        qDebug() << "Compute moran's I started...";
        std::vector<float> xCoordinates;
        std::vector<float> yCoordinates;
        std::vector<float> zCoordinates;

        for (int i = 0; i < floodIndices.size(); ++i)
        {
            int index = floodIndices[i];
            if (index >= xPositions.size())
            {
                qDebug() << "ERROR CorrFilter::computeMoranVector: index: " << index << " >= xPositions.size(): " << xPositions.size();
                return;
            }

            xCoordinates.push_back(xPositions[index]);
            yCoordinates.push_back(yPositions[index]);
            zCoordinates.push_back(zPositions[index]);
        }
        std::vector<std::vector<float>> distanceMat = computeWeightMatrix(xCoordinates, yCoordinates, zCoordinates);
        qDebug() << "Compute distance matrix finished...";

        // compute weight-related parameters 
        float W, S1, S2, S4, S5;
        moranParameters(distanceMat, W, S1, S2, S4, S5);

        moranVector.clear();
        moranVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i)
        {
            auto col = dataMatrix.col(i);
            std::vector<float> geneExpression;
            geneExpression.assign(col.data(), col.data() + col.size());
            std::vector<float> result = moranTest_C(geneExpression, distanceMat, W, S1, S2, S4, S5);

            float moranI = result[0];
            float expectedI = result[1];
            float sd = result[2];
            float zScore;
            if (sd != 0)
                zScore = (moranI - expectedI) / sd;
            else
                zScore = 0;

            // Check if zScore is NaN   
            if (std::isnan(zScore)) {
                zScore = 0;
                //qDebug() << "Gene name: " << _enabledDimNames[i] << " Moran's I: " << result[0] << " sd: " << sd << " Z-score: nan";
            }

            //qDebug() << "Gene name: " << _enabledDimNames[i] << " Moran's I: " << result[0] << " sd: " << sd << " Z-score: " << zScore;
            moranVector[i] = zScore;
        }
        qDebug() << "Compute moran's I finished...";

        // normalize the correlation vector to 0 to 1for plotting in the bar chart
        float minCorr = *std::min_element(moranVector.begin(), moranVector.end());
        float maxCorr = *std::max_element(moranVector.begin(), moranVector.end());
        qDebug() << "minCorr: " << minCorr << " maxCorr: " << maxCorr;

        for (int i = 0; i < moranVector.size(); ++i) {
            moranVector[i] = (moranVector[i] - minCorr) / (maxCorr - minCorr);
        }
        qDebug() << "Normalize moran's I finished...";
    }

    void CorrFilter::computeMoranVector(const DataMatrix& dataMatrix, const std::vector<float>& xPositions, const std::vector<float>& yPositions, const std::vector<float>& zPositions, std::vector<float>& moranVector)
    {
        // 3D cluster with mean position
        qDebug() << "Compute moran's I started...";
        std::vector<std::vector<float>> distanceMat = computeWeightMatrix(xPositions, yPositions, zPositions);
        qDebug() << "Compute distance matrix finished...";
        qDebug() << "distanceMat[0][0] " << distanceMat[0][0] << " distanceMat[0][1] " << distanceMat[0][1];

        // compute weight-related parameters 
        float W, S1, S2, S4, S5;
        moranParameters(distanceMat, W, S1, S2, S4, S5);

        moranVector.clear();
        moranVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i)
        {
            auto col = dataMatrix.col(i);
            std::vector<float> geneExpression;
            geneExpression.assign(col.data(), col.data() + col.size());
            std::vector<float> result = moranTest_C(geneExpression, distanceMat, W, S1, S2, S4, S5);

            float moranI = result[0];
            float expectedI = result[1];
            float sd = result[2];
            float zScore;
            if (sd != 0)
                zScore = (moranI - expectedI) / sd;
            else
                zScore = 0;

            // Check if zScore is NaN   
            if (std::isnan(zScore)) {
                zScore = 0;
                //qDebug() << "Gene name: " << _enabledDimNames[i] << " Moran's I: " << result[0] << " sd: " << sd << " Z-score: nan";
            }

            //qDebug() << "Gene name: " << _enabledDimNames[i] << " Moran's I: " << result[0] << " sd: " << sd << " Z-score: " << zScore;
            moranVector[i] = zScore;
        }
        qDebug() << "Compute moran's I finished...";

        // normalize the correlation vector to 0 to 1for plotting in the bar chart
        float minCorr = *std::min_element(moranVector.begin(), moranVector.end());
        float maxCorr = *std::max_element(moranVector.begin(), moranVector.end());
        qDebug() << "minCorr: " << minCorr << " maxCorr: " << maxCorr;
        // output the index of min and max correlation
        int minIndex = std::distance(moranVector.begin(), std::min_element(moranVector.begin(), moranVector.end()));
        int maxIndex = std::distance(moranVector.begin(), std::max_element(moranVector.begin(), moranVector.end()));
        qDebug() << "minIndex: " << minIndex << " maxIndex: " << maxIndex;

        for (int i = 0; i < moranVector.size(); ++i) {
            moranVector[i] = (moranVector[i] - minCorr) / (maxCorr - minCorr);
        }
        qDebug() << "Normalize moran's I finished...";
    }

    void CorrFilter::computePairwiseCorrelationVector(const std::vector<QString>& dimNames, const std::vector<int>& dimIndices, const DataMatrix& dataMatrix, DataMatrix& corrMatrix) const
    {
        // TO DO: dimNames not needed
        std::vector<Eigen::VectorXf> centeredVectors(dimNames.size());// precompute mean 
        std::vector<float> norms(dimNames.size());

        for (int i = 0; i < dimNames.size(); ++i) {
            //int index = dimNameToIndex[dimNames[i]];
            //int index = dimNameToIndex.at(dimNames[i]);
            int index = dimIndices[i];
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

    QString CorrFilter::getCorrFilterTypeAsString() const
    {
        switch (_type) {
        case CorrFilterType::SPATIAL:
            return "Spatial";
        case CorrFilterType::HD:
            return "HD";
        case CorrFilterType::DIFF:
            return "Diff";
        case CorrFilterType::MORAN:
            return "Moran";
        case CorrFilterType::SPATIALTEST:
            return "SpatialTest";
        }
    }

    void SpatialCorr::computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, std::vector<float>& corrVector) const
    {   // 2D
        Eigen::VectorXf xVector(floodIndices.size());
        Eigen::VectorXf yVector(floodIndices.size());
        for (int i = 0; i < floodIndices.size(); ++i) {
            int index = floodIndices[i];
            xVector[i] = positions[index].x;
            yVector[i] = positions[index].y;
        }

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

    void SpatialCorr::computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<float>& xPositions, const std::vector<float>& yPositions, const std::vector<float>& zPositions, std::vector<float>& corrVector) const
    {   // 3D all flood indices
        Eigen::VectorXf xVector(floodIndices.size());
        Eigen::VectorXf yVector(floodIndices.size());
        Eigen::VectorXf zVector(floodIndices.size());
        for (int i = 0; i < floodIndices.size(); ++i) {
            int index = floodIndices[i];
            if (index >= xPositions.size())
            {
                qDebug() << "ERROR CorrFilter::computeMoranVector: index: " << index << " >= xPositions.size(): " << xPositions.size();
                return;
            }
            xVector[i] = xPositions[index];
            yVector[i] = yPositions[index];
            zVector[i] = zPositions[index];
        }

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
            corrVector[i] = (std::abs(correlationX) + std::abs(correlationY) + std::abs(correlationZ))/3;
        }
    }

    void SpatialCorr::computeCorrelationVector(const DataMatrix& dataMatrix, std::vector<float>& xPositions, std::vector<float>& yPositions, std::vector<float>& zPositions, std::vector<float>& corrVector) const
    {   // 3D cluster with mean position
        Eigen::VectorXf xVector = Eigen::Map<Eigen::VectorXf>(xPositions.data(), xPositions.size());
        Eigen::VectorXf yVector = Eigen::Map<Eigen::VectorXf>(yPositions.data(), yPositions.size());
        Eigen::VectorXf zVector = Eigen::Map<Eigen::VectorXf>(zPositions.data(), zPositions.size());

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
            corrVector[i] = (std::abs(correlationX) + std::abs(correlationY) + std::abs(correlationZ))/3;
        }
    }

    void HDCorr::computeCorrelationVector(const std::vector<float>& waveNumbers, const DataMatrix& dataMatrix, std::vector<float>& corrVector) const
    {   // scRNA-seq
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
    {   // ST
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

    void Diff::computeDiff(const DataMatrix& selectionDataMatrix, const DataMatrix& allDataMatrix, std::vector<float>& diffVector)
    {
        // without normalization
        Eigen::VectorXf meanA = selectionDataMatrix.colwise().mean();
        Eigen::VectorXf meanB = allDataMatrix.colwise().mean();
        Eigen::VectorXf contrast = meanA - meanB;


        // with normalization - back up code
        //int numGenes = selectionDataMatrix.cols();
        //Eigen::VectorXf minValues = allDataMatrix.colwise().minCoeff();
        //Eigen::VectorXf maxValues = allDataMatrix.colwise().maxCoeff();

        ///*Eigen::VectorXf scaleFactors(numGenes);
        //for (int i = 0; i < numGenes; i++) {
        //    float range = maxValues[i] - minValues[i];
        //    if (range != 0)
        //        scaleFactors[i] = 1.0f / range;
        //    else
        //        scaleFactors[i] = 1.0f; 
        //}

        //Eigen::VectorXf meanA = selectionDataMatrix.colwise().mean();*/
        ///*for (int i = 0; i < numGenes; i++) {
        //    meanA[i] = (meanA[i] - minValues[i]) * scaleFactors[i];
        //    meanB[i] = (meanB[i] - minValues[i]) * scaleFactors[i];
        //}*/

        //Eigen::VectorXf rescaleValues = 1.0f / (maxValues - minValues).array();
        //Eigen::MatrixXf normalizedSelectionDataMatrix = (selectionDataMatrix.colwise() - minValues).array().colwise() * rescaleValues.array();
        //Eigen::VectorXf meanA = normalizedSelectionDataMatrix.colwise().mean();
        //Eigen::VectorXf meanB = allDataMatrix.colwise().mean();
        //Eigen::VectorXf contrast = meanA - meanB;

        // Norm to range [0, 1] for plotting in the bar chart
        float minContrast = contrast.minCoeff();
        float maxContrast = contrast.maxCoeff();
        float rangeContrast = maxContrast - minContrast;

        if (rangeContrast != 0) {
            contrast = (contrast.array() - minContrast) / rangeContrast;
        }
        else {
            contrast.setZero(); 
        }

        diffVector.clear();
        diffVector.assign(contrast.data(), contrast.data() + contrast.size());

    }

}

