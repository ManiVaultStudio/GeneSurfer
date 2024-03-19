#pragma once

#include "DataMatrix.h"

#include <vector>
#include <QString>


namespace corrFilter
{
    enum class CorrFilterType
    {
        SPATIAL,
        HD
    };

    class SpatialCorr
    {
    public:
        void computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, std::vector<float>& corrVector) const;
        void computeCorrelationVector(const std::vector<int>& floodIndices, const DataMatrix& dataMatrix, const std::vector<mv::Vector2f>& positions, const std::vector<float>& zPositions, std::vector<float>& corrVector) const;
    };

    class HDCorr
    {
    public:
        void computeCorrelationVector(const std::vector<float>& waveNumbers, const DataMatrix& dataMatrix, std::vector<float>& corrVector) const;
        void computeCorrelationVector(const std::vector<int>& waveNumbers, const DataMatrix& dataMatrix, std::vector<float>& corrVector) const;
    };

    class CorrFilter
    {

    public:
        void setFilterType(CorrFilterType type) { _type = type; }

        void computePairwiseCorrelationVector(const std::vector<QString>& dimNames, const std::unordered_map<QString, int>& dimNameToIndex, const DataMatrix& dataMatrix, DataMatrix& corrMatrix) const;

        // Non-const member functions
        SpatialCorr&         getSpatialCorrFilter()  { return _spatialCorr; }
        HDCorr&         getHDCorrFilter()       { return _highdimCorr; }

        // Const member functions
        CorrFilterType                 getFilterType()         const { return _type; }
        const SpatialCorr&   getSpatialCorrFilter()  const { return _spatialCorr; }
        const HDCorr&   getHDCorrFilter()       const { return _highdimCorr; }

    private:
        CorrFilterType        _type{CorrFilterType::HD};
        SpatialCorr          _spatialCorr;
        HDCorr          _highdimCorr;
    };
}
