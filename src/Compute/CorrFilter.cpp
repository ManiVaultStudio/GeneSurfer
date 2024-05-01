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

    float dot_product(const std::vector<float>& a, const std::vector<float>& b) {
        float sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            sum += a[i] * b[i];
        }
        return sum;
    }

    void normalize_weight_matrix(std::vector<std::vector<float>>& weight) {
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

    std::vector<float> CorrFilter::normalize(const std::vector<float>& x) {
        float sum = 0;
        for (float value : x) sum += value;
        float x_bar = sum / x.size();

        std::vector<float> x_norm(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            x_norm[i] = x[i] - x_bar;
        }

        return x_norm;
    }


    float CorrFilter::distanceCalculate(float x1, float y1, float x2, float y2) {
        float x = x1 - x2;
        float y = y1 - y2;
        float dist = sqrt(pow(x, 2) + pow(y, 2)); // Euclidean distance

        if (dist > 0) {
            dist = 1 / dist;
        }

        return dist;
    }

    std::vector<float> CorrFilter::calc_moran(const std::vector<float>& x, const std::vector<float>& c1, const std::vector<float>& c2) {
        std::vector<float> x_norm = normalize(x);
        int N = x.size();
        float denom = 0;
        for (float val : x_norm) denom += val * val;

        float W = 0, num = 0, S1 = 0, S2 = 0;

        for (int i = 0; i < N; ++i) {
            float S2_a = 0;
            for (int j = 0; j < N; ++j) {
                float w_ij = distanceCalculate(c1[i], c2[i], c1[j], c2[j]);
                W += w_ij;
                num += w_ij * x_norm[i] * x_norm[j];
                S1 += pow(2 * w_ij, 2);
                S2_a += w_ij;
            }
            S2 += pow(2 * S2_a, 2);
        }

        S1 /= 2;
        float ei = -(1.0 / (N - 1));
        float k = 0;
        for (float val : x_norm) k += pow(val, 4);
        k /= N;
        k /= pow(denom / N, 2);

        float sd = sqrt((N * ((pow(N, 2) - 3 * N + 3) * S1 - N * S2 + 3 * W * W) - k * (N * (N - 1) * S1 - 2 * N * S2 + 6 * W * W)) / ((N - 1) * (N - 2) * (N - 3) * W * W) - pow(ei, 2));
        float I = (N / W) * (num / denom);

        return { I, ei, sd };
    }

    std::vector<std::vector<float>> CorrFilter::computeWeightMatrix(const std::vector<float>& xCoordinates, const std::vector<float>& yCoordinates) {
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
        normalize_weight_matrix(normWeightMatrix);

        return normWeightMatrix;
    }

    std::vector<std::vector<float>> CorrFilter::computeWeightMatrix(const std::vector<float>& xCoordinates, const std::vector<float>& yCoordinates, const std::vector<float>& zCoordinates) 
    {
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
        normalize_weight_matrix(normWeightMatrix);

        return normWeightMatrix;
    }


    std::vector<float> CorrFilter::moranTest_C(const std::vector<float>& x, std::vector<std::vector<float>>& weight)
    {
     
        size_t N = weight.size();

        // weight should be normalized
        // weight has al been normalized in computeWeightMatrix()
        //std::vector<std::vector<float>> norm_weight = weight;
        //normalize_weight_matrix(norm_weight);

        // Calculate mean of x
        float x_mean = mean(x);
        std::vector<float> z(N);
        for (size_t i = 0; i < N; i++) {
            z[i] = x[i] - x_mean;
        }

        float W = 0.0f, cv = 0.0f;
        float S1 = 0.0f, S2 = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                W += weight[i][j];
                cv += weight[i][j] * z[i] * z[j];
                S1 += weight[i][j] * weight[i][j];
            }
            S2 += std::pow(std::accumulate(weight[i].begin(), weight[i].end(), 0.0f), 2);
        }

        float z_squares_sum = dot_product(z, z);
        float obs = (N / W) * (cv / z_squares_sum);
        float ei = -1.0f / (N - 1);

        float sum_z4 = std::accumulate(z.begin(), z.end(), 0.0f, [](float acc, float zi) { return acc + std::pow(zi, 4); });
        float S3 = sum_z4 / N / std::pow(z_squares_sum / N, 2);

        float Wsq = W * W;
        float Nsq = N * N;
        float S4 = (Nsq - 3 * N + 3) * S1 - N * S2 + 3 * Wsq;
        float S5 = (Nsq - N) * S1 - 2 * N * S2 + 6 * Wsq;
        float ei2 = (N * S4 - S3 * S5) / ((N - 1) * (N - 2) * (N - 3) * Wsq);
        float sd = sqrt(ei2 - (ei * ei));

        std::vector<float> results = { obs, ei, sd };// Moran's I, Expected, SD
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

        moranVector.clear();
        moranVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i)
        {
            auto col = dataMatrix.col(i);
            std::vector<float> geneExpression;
            geneExpression.assign(col.data(), col.data() + col.size());
            //std::vector<float> result = calc_moran(geneExpression, xCoordinates, yCoordinates);// experiment moran's I with moranfast
            std::vector<float> result = moranTest_C(geneExpression, distanceMat);// experiment moran's I with MERINGUE

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

    void CorrFilter::computeMoranVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, const std::vector<float>& zPositions, std::vector<float>& moranVector)
    {
        // 3D all flood indices
        qDebug() << "Compute moran's I started...";
        std::vector<float> xCoordinates;
        std::vector<float> yCoordinates;
        std::vector<float> zCoordinates;

        for (int i = 0; i < floodIndices.size(); ++i)
        {
            int index = floodIndices[i];
            xCoordinates.push_back(positions[index].x);
            yCoordinates.push_back(positions[index].y);
            zCoordinates.push_back(zPositions[index]);
        }
        std::vector<std::vector<float>> distanceMat = computeWeightMatrix(xCoordinates, yCoordinates, zCoordinates);
        qDebug() << "Compute distance matrix finished...";

        moranVector.clear();
        moranVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i)
        {
            auto col = dataMatrix.col(i);
            std::vector<float> geneExpression;
            geneExpression.assign(col.data(), col.data() + col.size());
            //std::vector<float> result = calc_moran(geneExpression, xCoordinates, yCoordinates);// experiment moran's I with moranfast
            std::vector<float> result = moranTest_C(geneExpression, distanceMat);// experiment moran's I with MERINGUE

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

    void CorrFilter::computeMoranVector(const DataMatrix& dataMatrix, std::vector<float>& xPositions, std::vector<float>& yPositions, std::vector<float>& zPositions, std::vector<float>& moranVector)
    {
        // 3D cluster with mean position
        qDebug() << "Compute moran's I started...";
        std::vector<std::vector<float>> distanceMat = computeWeightMatrix(xPositions, yPositions, zPositions);
        qDebug() << "Compute distance matrix finished...";

        moranVector.clear();
        moranVector.resize(dataMatrix.cols());

#pragma omp parallel for
        for (int i = 0; i < dataMatrix.cols(); ++i)
        {
            auto col = dataMatrix.col(i);
            std::vector<float> geneExpression;
            geneExpression.assign(col.data(), col.data() + col.size());
            //std::vector<float> result = calc_moran(geneExpression, xCoordinates, yCoordinates);// experiment moran's I with moranfast
            std::vector<float> result = moranTest_C(geneExpression, distanceMat);// experiment moran's I with MERINGUE

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

    void SpatialCorr::computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, const std::vector<float>& zPositions, std::vector<float>& corrVector) const
    {   // 3D all flood indices
        Eigen::VectorXf xVector(floodIndices.size());
        Eigen::VectorXf yVector(floodIndices.size());
        Eigen::VectorXf zVector(floodIndices.size());
        for (int i = 0; i < floodIndices.size(); ++i) {
            int index = floodIndices[i];
            xVector[i] = positions[index].x;
            yVector[i] = positions[index].y;
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

