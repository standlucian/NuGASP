#ifndef MATRIXREADER_H
#define MATRIXREADER_H

#include <QString>
#include <vector>
#include <cstdint>
#include <memory>

enum class MatrixBgMode {
    None = 0,
    Common = 1,   // GASPware Common/Projection: fraction of gate counts * corrFactor
    Normal = 2,   // Local background estimation across gate
    Auto = 3      // Automated SNIP continuum subtraction
};

struct MatrixBackgroundConfig {
    bool enabled{true};
    MatrixBgMode mode{MatrixBgMode::Normal};
    double correctionFactor{1.0};
};

struct MatrixGateRegion {
    int minCh{0};
    int maxCh{0};
};

/**
 * @brief MatrixReader reads and extracts projections and coincidence gate slices
 *        from GASPware compressed coincidence matrices (.cmat, version 5).
 */
class MatrixReader {
public:
    MatrixReader();
    ~MatrixReader();

    /**
     * @brief Opens and parses the .cmat file structure, reading descriptors and 1D projections.
     */
    bool open(const QString &filePath, QString *errorMessage = nullptr);

    /**
     * @brief Closes the matrix and resets cached data.
     */
    void close();

    bool isOpen() const { return m_isOpen; }
    QString getFilePath() const { return m_filePath; }
    QString getFileName() const;

    // Header & metadata
    int getVersion() const { return m_version; }
    int getDimensions() const { return m_ndim; }
    int getResolutionX() const { return m_resX; }
    int getResolutionY() const { return m_resY; }
    int getStepX() const { return m_stepX; }
    int getStepY() const { return m_stepY; }
    int getNumDivX() const { return m_ndivX; }
    int getNumDivY() const { return m_ndivY; }
    int getTileSize() const { return m_segsize; }
    int getNextra() const { return m_nextra; }
    int getTotalSegments() const { return m_ntotseg; }
    bool isSymmetric() const { return m_matmode == 1; }

    // Statistics
    qint64 getFileSize() const { return m_fileSize; }
    qint64 getUncompressedSize() const;
    double getCompressionRatio() const;
    qint64 getTotalCounts() const { return m_totalCounts; }

    // Background Subtraction Configuration
    void setBackgroundConfig(const MatrixBackgroundConfig &cfg) { m_bgConfig = cfg; }
    const MatrixBackgroundConfig& getBackgroundConfig() const { return m_bgConfig; }
    MatrixBackgroundConfig& getBackgroundConfig() { return m_bgConfig; }

    /**
     * @brief Returns the 1D projection spectrum onto the X-axis (horizontal).
     */
    const std::vector<double>& getProjectionX() const { return m_projectionX; }

    /**
     * @brief Returns the 1D projection spectrum onto the Y-axis (vertical).
     *        For symmetric matrices, this is identical to getProjectionX().
     */
    const std::vector<double>& getProjectionY() const { return m_projectionY; }

    /**
     * @brief Convenience method returning the primary projection (X).
     */
    const std::vector<double>& getProjection() const { return m_projectionX; }

    /**
     * @brief Extracts the raw (unsubtracted) 1D coincidence spectrum.
     */
    std::vector<double> getRawGateSlice(int chMin, int chMax, int gateAxis = 1) const;

    /**
     * @brief Computes the estimated background 1D spectrum for the given gate window.
     * @param outPfacs Optional pointer to receive the calculated pfacs scaling factor.
     */
    std::vector<double> computeBackgroundSlice(int chMin, int chMax, int gateAxis = 1, double *outPfacs = nullptr) const;

    /**
     * @brief Slices a 1D coincidence spectrum for channels in [chMin, chMax] inclusive,
     *        with background subtraction applied according to the active configuration.
     * @param chMin           Starting channel of gate
     * @param chMax           Ending channel of gate
     * @param gateAxis        1 = gate on Y to project onto X; 0 = gate on X to project onto Y
     * @param applyBackground If true and background is enabled, subtracts estimated background
     */
    std::vector<double> getGateSlice(int chMin, int chMax, int gateAxis = 1, bool applyBackground = true) const;

    /**
     * @brief Slices a 1D coincidence spectrum from multiple gate regions (e.g. 1 peak gate + 2 background gates).
     *        For MatrixBgMode::Normal, subtracts the normalized background gates slices from the peak gate slice
     *        following GASPware trackn.F:6476.
     *        For MatrixBgMode::Common, sums all gates and subtracts scaled projection background.
     *        For MatrixBgMode::Auto, sums gates and applies SNIP continuum filter.
     */
    std::vector<double> getMultiGateSlice(const std::vector<MatrixGateRegion> &gates,
                                          int peakGateIndex = 0,
                                          int gateAxis = 1,
                                          bool applyBackground = true,
                                          double *outBackfac = nullptr,
                                          double *outBgCounts = nullptr,
                                          std::vector<double> *outBgSlice = nullptr) const;

    /**
     * @brief Computes automated SNIP background filter for a 1D spectrum or projection.
     */
    std::vector<double> computeSnipBackground(const std::vector<double> &spectrum,
                                              int iterations = 20,
                                              double factor = 1.0) const;

private:
    bool readDescriptorTable(FILE *f);
    bool readCmtHeader(FILE *f);
    bool readProjections(FILE *f);

    int getSegmentIndex(int tx, int ty) const;

    bool m_isOpen{false};
    QString m_filePath;
    qint64 m_fileSize{0};

    // Header block 0
    int m_version{5};
    int m_ntotseg{0};
    int m_ndescblk{0};

    // IVF Segment Descriptors: pairs of (nblocks, startblk)
    struct SegmentDescriptor {
        int32_t nblocks{0};
        int32_t startblk{0};
    };
    std::vector<SegmentDescriptor> m_descriptors;

    // CMT Header
    int m_ndim{2};
    int m_matmode{1}; // 1 = symmetric, 0 = asymmetric
    int m_resX{0};
    int m_resY{0};
    int m_stepX{0};
    int m_stepY{0};
    int m_ndivX{0};
    int m_ndivY{0};
    int m_segsize{0};
    int m_nextra{5};

    qint64 m_totalCounts{0};
    std::vector<double> m_projectionX;
    std::vector<double> m_projectionY;

    MatrixBackgroundConfig m_bgConfig;
};

#endif // MATRIXREADER_H
