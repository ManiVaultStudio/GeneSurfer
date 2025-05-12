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
        int N = weight.size();
//#pragma omp parallel for
        for (int i = 0; i < N; i++) {
            float row_sum = std::accumulate(weight[i].begin(), weight[i].end(), 0.0f);
            if (row_sum != 0) {
                for (int j = 0; j < N; j++) {
                    weight[i][j] /= row_sum;
                }
            }
        }
    }

}

namespace corrFilter
{

    void CorrFilter::computePairwiseCorrelationVector(const std::vector<QString>& dimNames, const std::vector<int>& dimIndices, const DataMatrix& dataMatrix, DataMatrix& corrMatrix) const
    {   // without weighting
        //qDebug() << "Compute pairwise correlation started...";

        //qDebug() << "dimNames.size(): " << dimNames.size() << " dimIndices.size(): " << dimIndices.size();
        //qDebug() << "dataMatrix.rows(): " << dataMatrix.rows() << " dataMatrix.cols(): " << dataMatrix.cols();

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

        //qDebug() << "Compute centeredVectors and norms finished...";

        #pragma omp parallel for
        for (int col1 = 0; col1 < dimNames.size(); ++col1) {
            for (int col2 = col1; col2 < dimNames.size(); ++col2) {
                float correlation = centeredVectors[col1].dot(centeredVectors[col2]) / std::sqrt(norms[col1] * norms[col2]);
                if (std::isnan(correlation)) { correlation = 0.0f; } // TO DO: check if this is a good way to handle nan in corr computation

                corrMatrix(col1, col2) = correlation;
                corrMatrix(col2, col1) = correlation;
            }
        }
        //qDebug() << "Compute pairwise correlation finished...";
    }

    void CorrFilter::computePairwiseCorrelationVector(const std::vector<QString>& dimNames, const std::vector<int>& dimIndices, const DataMatrix& dataMatrix, const Eigen::VectorXf& weights, DataMatrix& corrMatrix) const
    {   // with weighting
        //qDebug() << "Compute pairwise correlation with weighting started...";

        //qDebug() << "dimNames.size(): " << dimNames.size() << " dimIndices.size(): " << dimIndices.size();
        //qDebug() << "dataMatrix.rows(): " << dataMatrix.rows() << " dataMatrix.cols(): " << dataMatrix.cols();

        // check if the size of weights is the same as dataMatrix.rows()
        if (weights.size() != dataMatrix.rows())
        {
            qDebug() << "ERROR CorrFilter::computeCorrelationVectorOneDimension: weights.size(): " << weights.size() << " != dataMatrix.rows(): " << dataMatrix.rows();
            return;
        }

        // Precompute weighted centered vectors and norms
        std::vector<Eigen::VectorXf> centeredVectors(dimNames.size());
        std::vector<float> norms(dimNames.size());

        for (int i = 0; i < dimNames.size(); ++i) {
            int index = dimIndices[i];

            // Compute weighted mean of the column
            float weightedMean = (weights.array() * dataMatrix.col(index).array()).sum() / weights.sum();

            // Compute weighted centered vector
            Eigen::VectorXf centered = dataMatrix.col(index).array() - weightedMean;
            Eigen::VectorXf weightedCentered = centered.array() * weights.array().sqrt();
            centeredVectors[i] = weightedCentered;
            norms[i] = weightedCentered.squaredNorm();
        }

        //qDebug() << "Compute centeredVectors and norms finished...";

#pragma omp parallel for
        for (int col1 = 0; col1 < dimNames.size(); ++col1) {
            for (int col2 = col1; col2 < dimNames.size(); ++col2) {
                float weightedDotProduct = (centeredVectors[col1].array() * centeredVectors[col2].array()).sum();
                float correlation = weightedDotProduct / std::sqrt(norms[col1] * norms[col2]);
                if (std::isnan(correlation)) { correlation = 0.0f; } // Handle NaN in correlation computation

                corrMatrix(col1, col2) = correlation;
                corrMatrix(col2, col1) = correlation;
            }
        }

        //qDebug() << "Compute pairwise correlation finished...";
    
    }

    QString CorrFilter::getCorrFilterTypeAsString() const
    {
        switch (_type) {
        case CorrFilterType::DIFF:
            return "Diff";
        case CorrFilterType::MORAN:
            return "Moran";
        case CorrFilterType::SPATIALZ:
            return "Spatial Z";
        case CorrFilterType::SPATIALY:
            return "Spatial Y";
        }
    }

    void SpatialCorr::computeCorrelationVectorOneDimension(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<float>& positionsOneDimension, std::vector<float>& corrVector) const
    {
        // 2D or 3D all flood indices
        //qDebug() << "Compute spatial correlation started...";
        Eigen::VectorXf vector(floodIndices.size());
        for (int i = 0; i < floodIndices.size(); ++i) {
            int index = floodIndices[i];
            if (index >= positionsOneDimension.size())
            {
                qDebug() << "ERROR CorrFilter::computeCorrelationVector: index: " << index << " >= positionsOneDimension.size(): " << positionsOneDimension.size();
                return;
            }
            vector[i] = positionsOneDimension[index];
        }
        //qDebug() << "vector size" << vector.size();

        // Precompute means and norms
        float mean = vector.mean();
        Eigen::VectorXf centered = vector - Eigen::VectorXf::Constant(vector.size(), mean);
        float norm = centered.squaredNorm();
        //qDebug() << "Compute centeredVectors and norms finished...";

        std::vector<float> correlations(dataMatrix.cols());

        corrVector.clear();
        corrVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i) {
            Eigen::VectorXf column = dataMatrix.col(i);
            float meanColumn = column.mean();
            Eigen::VectorXf centeredColumn = column - Eigen::VectorXf::Constant(column.size(), meanColumn);
            float normColumn = centeredColumn.squaredNorm();
            float correlation = centeredColumn.dot(centered) / std::sqrt(normColumn * norm);
            if (std::isnan(correlation)) { correlation = 0.0f; }
            corrVector[i] = correlation;
        }
        //qDebug() << "corrVector size " << corrVector.size();
    }

    void SpatialCorr::computeCorrelationVectorOneDimension(const DataMatrix& dataMatrix, std::vector<float>& positionsOneDimension, std::vector<float>& corrVector) const
    {
        // 3D cluster with mean position
        Eigen::VectorXf vector = Eigen::Map<Eigen::VectorXf>(positionsOneDimension.data(), positionsOneDimension.size());

        // Precompute means and norms for xVector, yVector, zVector
        float mean = vector.mean();
        Eigen::VectorXf centered = vector - Eigen::VectorXf::Constant(vector.size(), mean);
        float norm = centered.squaredNorm();

        std::vector<float> correlations(dataMatrix.cols());

        corrVector.clear();
        corrVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i) {
            Eigen::VectorXf column = dataMatrix.col(i);
            float meanColumn = column.mean();
            Eigen::VectorXf centeredColumn = column - Eigen::VectorXf::Constant(column.size(), meanColumn);
            float normColumn = centeredColumn.squaredNorm();
            float correlation = centeredColumn.dot(centered) / std::sqrt(normColumn * norm);
            if (std::isnan(correlation)) { correlation = 0.0f; } 
            corrVector[i] = correlation;
        }
    }

    void SpatialCorr::computeCorrelationVectorOneDimension(const DataMatrix& dataMatrix, std::vector<float>& positionsOneDimension, const Eigen::VectorXf& weights, std::vector<float>& corrVector) const
    {
        // test with weighting
        // 3D cluster with mean position
         
        // check if the size of weights is the same as positionsOneDimension
        if (weights.size() != positionsOneDimension.size())
        {
            qDebug() << "ERROR CorrFilter::computeCorrelationVectorOneDimension: weights.size(): " << weights.size() << " != positionsOneDimension.size(): " << positionsOneDimension.size();
            return;
        }
      
        Eigen::VectorXf vector = Eigen::Map<Eigen::VectorXf>(positionsOneDimension.data(), positionsOneDimension.size());

        // Compute weighted mean for vector
        float weightedMean = (weights.array() * vector.array()).sum() / weights.sum();

        // Compute weighted centered vector and norm
        Eigen::VectorXf centered = vector.array() - weightedMean;
        Eigen::VectorXf weightedCentered = centered.array() * weights.array().sqrt();
        float norm = weightedCentered.squaredNorm();

        std::vector<float> correlations(dataMatrix.cols());

        corrVector.clear();
        corrVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i) {
            Eigen::VectorXf column = dataMatrix.col(i);

            // Compute weighted mean of the column
            float meanColumn = (weights.array() * column.array()).sum() / weights.sum();

            // Compute weighted centered column and norm
            Eigen::VectorXf centeredColumn = column.array() - meanColumn;
            Eigen::VectorXf weightedCenteredColumn = centeredColumn.array() * weights.array().sqrt();
            float normColumn = weightedCenteredColumn.squaredNorm();

            // Compute the weighted dot product
            float weightedDotProduct = (weightedCenteredColumn.array() * weightedCentered.array()).sum();

            float correlation = weightedDotProduct / std::sqrt(normColumn * norm);

            if (std::isnan(correlation)) { correlation = 0.0f; }
            corrVector[i] = correlation;
        }
    }

    std::vector<std::vector<float>> Moran::computeWeightMatrix(const std::vector<float>& xCoordinates, const std::vector<float>& yCoordinates)
    { // 2D
        size_t N = xCoordinates.size();
        std::vector<std::vector<float>> weightMatrix(N, std::vector<float>(N, 0.0f));

        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                if (i != j) { // Avoid self-loops
                    //float dist = euclideanDistance(xCoordinates[i], yCoordinates[i], xCoordinates[j], yCoordinates[j]);
                    float dist = sqrt((xCoordinates[i] - xCoordinates[j]) * (xCoordinates[i] - xCoordinates[j]) + (yCoordinates[i] - yCoordinates[j]) * (yCoordinates[i] - yCoordinates[j]));
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

    std::vector<std::vector<float>> Moran::computeWeightMatrix(const std::vector<float>& xCoordinates, const std::vector<float>& yCoordinates, const std::vector<float>& zCoordinates)
    { // 3D
        size_t N = xCoordinates.size();
        std::vector<std::vector<float>> weightMatrix(N, std::vector<float>(N, 0.0f));

        //#pragma omp parallel for
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                if (i != j) { // Avoid self-loops
                    //float dist = euclideanDistance3D(xCoordinates[i], yCoordinates[i], zCoordinates[i], xCoordinates[j], yCoordinates[j], zCoordinates[j]);
                    float dist = sqrt((xCoordinates[i] - xCoordinates[j]) * (xCoordinates[i] - xCoordinates[j]) + (yCoordinates[i] - yCoordinates[j]) * (yCoordinates[i] - yCoordinates[j]) + (zCoordinates[i] - zCoordinates[j]) * (zCoordinates[i] - zCoordinates[j]));
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

    void Moran::moranParameters(const std::vector<std::vector<float>>& weight, float& W, float& S1, float& S2, float& S4, float& S5)
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

    std::vector<float> Moran::moranTest_C(const std::vector<float>& x, std::vector<std::vector<float>>& weight, const float W, const float S1, const float S2, const float S4, const float S5)
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

        //auto start1 = std::chrono::high_resolution_clock::now();
        float cv = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                cv += weight[i][j] * z[i] * z[j];
            }
        }
        //auto end1 = std::chrono::high_resolution_clock::now();
        //std::chrono::duration<double> elapsed1 = end1 - start1;
        //qDebug() << "Elapsed time for cv: " << elapsed1.count();

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

    void Moran::computeMoranVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, std::vector<float>& moranVector)
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
        //qDebug() << "Compute distance matrix finished...";

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
        //qDebug() << "Compute moran's I finished...";

        // normalize the correlation vector to 0 to 1for plotting in the bar chart
        float minCorr = *std::min_element(moranVector.begin(), moranVector.end());
        float maxCorr = *std::max_element(moranVector.begin(), moranVector.end());
        //qDebug() << "minCorr: " << minCorr << " maxCorr: " << maxCorr;

       /* for (int i = 0; i < moranVector.size(); ++i) {
            moranVector[i] = (moranVector[i] - minCorr) / (maxCorr - minCorr);
        }
        qDebug() << "Normalize moran's I finished...";*/
    }

    void Moran::computeMoranVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<float>& xPositions, const std::vector<float>& yPositions, const std::vector<float>& zPositions, std::vector<float>& moranVector)
    {
        //3D all flood indices
        qDebug() << "Compute moran's I started...";

        //auto start1 = std::chrono::high_resolution_clock::now();

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

        //auto end1 = std::chrono::high_resolution_clock::now();
        //std::chrono::duration<double> elapsed1 = end1 - start1;
        //qDebug() << "Elapsed time for computeWeightMatrix: " << elapsed1.count();
        //qDebug() << "Compute distance matrix finished...";

        // compute weight-related parameters 
        //auto start2 = std::chrono::high_resolution_clock::now();

        float W, S1, S2, S4, S5;
        moranParameters(distanceMat, W, S1, S2, S4, S5);

        //auto end2 = std::chrono::high_resolution_clock::now();
        //std::chrono::duration<double> elapsed2 = end2 - start2;
        //qDebug() << "Elapsed time for moranParameters: " << elapsed2.count();

        //auto start3 = std::chrono::high_resolution_clock::now();

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

        //auto end3 = std::chrono::high_resolution_clock::now();
        //std::chrono::duration<double> elapsed3 = end3 - start3;
        //qDebug() << "Elapsed time for computeMoranVector: " << elapsed3.count();

        //qDebug() << "Compute moran's I finished...";

        // normalize the correlation vector to 0 to 1for plotting in the bar chart
        //auto start4 = std::chrono::high_resolution_clock::now();

        float minCorr = *std::min_element(moranVector.begin(), moranVector.end());
        float maxCorr = *std::max_element(moranVector.begin(), moranVector.end());
        //qDebug() << "minCorr: " << minCorr << " maxCorr: " << maxCorr;

        for (int i = 0; i < moranVector.size(); ++i) {
            moranVector[i] = (moranVector[i] - minCorr) / (maxCorr - minCorr);
        }

        //auto end4 = std::chrono::high_resolution_clock::now();
        //std::chrono::duration<double> elapsed4 = end4 - start4;
        //qDebug() << "Elapsed time for normalize: " << elapsed4.count();

        qDebug() << "Normalize moran's I finished...";
    }

    void Moran::computeMoranVector(const DataMatrix& dataMatrix, const std::vector<float>& xPositions, const std::vector<float>& yPositions, const std::vector<float>& zPositions, std::vector<float>& moranVector)
    {
        // 3D cluster with mean position
        qDebug() << "Compute moran's I started...";
        std::vector<std::vector<float>> distanceMat = computeWeightMatrix(xPositions, yPositions, zPositions);
        //qDebug() << "Compute distance matrix finished...";
        //qDebug() << "distanceMat[0][0] " << distanceMat[0][0] << " distanceMat[0][1] " << distanceMat[0][1];

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
        //qDebug() << "Compute moran's I finished...";

        // normalize the correlation vector to 0 to 1for plotting in the bar chart
        float minCorr = *std::min_element(moranVector.begin(), moranVector.end());
        float maxCorr = *std::max_element(moranVector.begin(), moranVector.end());
        //qDebug() << "minCorr: " << minCorr << " maxCorr: " << maxCorr;
        // output the index of min and max correlation
        int minIndex = std::distance(moranVector.begin(), std::min_element(moranVector.begin(), moranVector.end()));
        int maxIndex = std::distance(moranVector.begin(), std::max_element(moranVector.begin(), moranVector.end()));
        //qDebug() << "minIndex: " << minIndex << " maxIndex: " << maxIndex;

        for (int i = 0; i < moranVector.size(); ++i) {
            moranVector[i] = (moranVector[i] - minCorr) / (maxCorr - minCorr);
        }
        qDebug() << "Normalize moran's I finished...";
    }

    void Diff::computeDiff(const DataMatrix& selectionDataMatrix, const DataMatrix& allDataMatrix, std::vector<float>& diffVector)
    {
        // without normalization
        Eigen::VectorXf meanA = selectionDataMatrix.colwise().mean();
        Eigen::VectorXf meanB = allDataMatrix.colwise().mean();
        Eigen::VectorXf contrast = meanA - meanB;


        /*qDebug() << "<<<<< contrast size: " << contrast.size() << " contrast min: " << contrast.minCoeff() << " contrast max: " << contrast.maxCoeff();
        qDebug() << "contrast[0] " << contrast[0] << " contrast[1] " << contrast[1] << " contrast[2] " << contrast[2];
        qDebug() << "meanA size: " << meanA.size() << " meanA min: " << meanA.minCoeff() << " meanA max: " << meanA.maxCoeff();
        qDebug() << "meanA[0] " << meanA[0] << " meanA[1] " << meanA[1] << " meanA[2] " << meanA[2];
        qDebug() << "meanB size: " << meanB.size() << " meanB min: " << meanB.minCoeff() << " meanB max: " << meanB.maxCoeff();
        qDebug() << "meanB[0] " << meanB[0] << " meanB[1] " << meanB[1] << " meanB[2] " << meanB[2];*/

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

