#pragma once

#include "DataMatrix.h"

#include <vector>
#include <QString>


namespace corrFilter
{
    enum class CorrFilterType
    {
        SPATIAL,
        HD,
        DIFF
    };

    class SpatialCorr
    {
    public:
        // 2D
        void computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, std::vector<float>& corrVector) const;
        // 3D all flood indices
        void computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, const std::vector<float>& zPositions, std::vector<float>& corrVector) const;
        // 3D cluster with mean position
        void computeCorrelationVector(const DataMatrix& dataMatrix, std::vector<float>& xPositions, std::vector<float>& yPositions, std::vector<float>& zPositions, std::vector<float>& corrVector) const;
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

        QString getCorrFilterTypeAsString();

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
        CorrFilterType        _type{CorrFilterType::HD};
        SpatialCorr          _spatialCorr;
        HDCorr          _highdimCorr;
        Diff          _diff;
    };
}
