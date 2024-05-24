#pragma once

#include "DataMatrix.h"

#include <vector>
#include <QString>


namespace corrFilter
{
    enum class CorrFilterType
    {
        SPATIAL,// x+y+z
        SPATIALZ,//z
        SPATIALY,//y
        HD,
        DIFF,
        MORAN,
        SPATIALTEST
    };

    class SpatialCorr
    {
    public:
        // 2D
        void computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, std::vector<float>& corrVector) const;
        // 3D all flood indices
        void computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<float>& xPositions, const std::vector<float>& yPositions, const std::vector<float>& zPositions, std::vector<float>& corrVector) const;
        // 3D cluster with mean position
        void computeCorrelationVector(const DataMatrix& dataMatrix, std::vector<float>& xPositions, std::vector<float>& yPositions, std::vector<float>& zPositions, std::vector<float>& corrVector) const;
        
        // 2D + 3D all flood indices + one dimension
        void computeCorrelationVectorOneDimension(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<float>& positionsOneDimension, std::vector<float>& corrVector) const;
        // 3D cluster with mean position + one dimension
        void computeCorrelationVectorOneDimension(const DataMatrix& dataMatrix, std::vector<float>& positionsOneDimension, std::vector<float>& corrVector) const;
    
    };

    class HDCorr
    {
    public:
        // scRNA-seq
        void computeCorrelationVector(const std::vector<float>& waveNumbers, const DataMatrix& dataMatrix, std::vector<float>& corrVector) const;
        // ST
        void computeCorrelationVector(const std::vector<int>& waveNumbers, const DataMatrix& dataMatrix, std::vector<float>& corrVector) const;
    };

    class Diff
    {
    public:
        void computeDiff(const DataMatrix& selectionDataMatrix, const DataMatrix& allDataMatrix, std::vector<float>& diffVector);
    };

    class CorrFilter
    {

    public:
        void setFilterType(CorrFilterType type) { _type = type; }

        void computePairwiseCorrelationVector(const std::vector<QString>& dimNames, const std::vector<int>& dimIndices, const DataMatrix& dataMatrix, DataMatrix& corrMatrix) const;

        QString getCorrFilterTypeAsString() const;

        // experiment moran's I
        std::vector<std::vector<float>> computeWeightMatrix(const std::vector<float>& xCoordinates, const std::vector<float>& yCoordinates);
        std::vector<std::vector<float>> computeWeightMatrix(const std::vector<float>& xCoordinates, const std::vector<float>& yCoordinates, const std::vector<float>& zCoordinates);// overload
        void moranParameters(const std::vector<std::vector<float>>& weight, float& W, float& S1, float& S2, float& S4, float& S5);
        std::vector<float> moranTest_C(const std::vector<float>& x, std::vector<std::vector<float>>& weight, const float W, const float S1, const float S2, const float S4, const float S5);
        // 2D
        void computeMoranVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, std::vector<float>& moranVector);
        // 3D all flood indices
        void computeMoranVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<float>& xPositions, const std::vector<float>& yPositions, const std::vector<float>& zPositions, std::vector<float>& moranVector);
        // 3D cluster with mean position
        void computeMoranVector(const DataMatrix& dataMatrix, const std::vector<float>& xPositions, const std::vector<float>& yPositions, const std::vector<float>& zPositions, std::vector<float>& moranVector);

        // Non-const member functions
        SpatialCorr&         getSpatialCorrFilter()  { return _spatialCorr; }
        HDCorr&         getHDCorrFilter()       { return _highdimCorr; }
        Diff&         getDiffFilter()       { return _diff; }

        // Const member functions
        CorrFilterType                 getFilterType()         const { return _type; }      
        const SpatialCorr&   getSpatialCorrFilter()  const { return _spatialCorr; }
        const HDCorr&   getHDCorrFilter()       const { return _highdimCorr; }
        const Diff&   getDiffFilter()       const { return _diff; }

    private:
        CorrFilterType        _type{CorrFilterType::SPATIAL};
        SpatialCorr          _spatialCorr;
        HDCorr          _highdimCorr;
        Diff          _diff;
    };
}
