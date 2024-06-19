#pragma once

#include "DataMatrix.h"

#include <vector>
#include <QString>


namespace corrFilter
{
    enum class CorrFilterType
    {
        SPATIALZ,//anterior-posterior in ABC Atlas
        SPATIALY,//dorsal-ventral in ABC Atlas
        DIFF,
        MORAN
    };

    class SpatialCorr
    {
    public:       
        // 2D + 3D all flood indices + one dimension
        void computeCorrelationVectorOneDimension(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<float>& positionsOneDimension, std::vector<float>& corrVector) const;
        // 3D cluster with mean position + one dimension
        void computeCorrelationVectorOneDimension(const DataMatrix& dataMatrix, std::vector<float>& positionsOneDimension, std::vector<float>& corrVector) const;
        
        // 3D cluster with mean position + one dimension + weighting for singlecell
        void computeCorrelationVectorOneDimension(const DataMatrix& dataMatrix, std::vector<float>& positionsOneDimension, const Eigen::VectorXf& weights, std::vector<float>& corrVector) const;
    
    };

    class Diff
    {
    public:
        void computeDiff(const DataMatrix& selectionDataMatrix, const DataMatrix& allDataMatrix, std::vector<float>& diffVector);
    };

    class Moran
    {
    public:
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
    };

    class CorrFilter
    {

    public:
        void setFilterType(CorrFilterType type) { _type = type; }
        QString getCorrFilterTypeAsString() const;

        void computePairwiseCorrelationVector(const std::vector<QString>& dimNames, const std::vector<int>& dimIndices, const DataMatrix& dataMatrix, DataMatrix& corrMatrix) const;
        // for 3D cluster with mean position + weighting
        void computePairwiseCorrelationVector(const std::vector<QString>& dimNames, const std::vector<int>& dimIndices, const DataMatrix& dataMatrix, const Eigen::VectorXf& weights, DataMatrix& corrMatrix) const;

        // Non-const member functions
        SpatialCorr&         getSpatialCorrFilter()  { return _spatialCorr; }
        Diff&         getDiffFilter()       { return _diff; }
        Moran&        getMoranFilter()      { return _moran; }

        // Const member functions
        CorrFilterType                 getFilterType()         const { return _type; }      
        const SpatialCorr&   getSpatialCorrFilter()  const { return _spatialCorr; }
        const Diff&   getDiffFilter()       const { return _diff; }
        const Moran&  getMoranFilter()      const { return _moran; }

    private:
        CorrFilterType       _type{CorrFilterType::DIFF};
        SpatialCorr          _spatialCorr;
        Diff                 _diff;
        Moran                _moran;
    };
}
