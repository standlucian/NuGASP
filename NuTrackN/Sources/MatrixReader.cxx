#include "MatrixReader.h"

extern "C" {
#include "complib.h"
}

#include <QFileInfo>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <numeric>

MatrixReader::MatrixReader()
{
}

MatrixReader::~MatrixReader()
{
    close();
}

QString MatrixReader::getFileName() const
{
    if (m_filePath.isEmpty()) return QString();
    return QFileInfo(m_filePath).fileName();
}

void MatrixReader::close()
{
    m_isOpen = false;
    m_filePath.clear();
    m_fileSize = 0;
    m_descriptors.clear();
    m_projectionX.clear();
    m_projectionY.clear();
    m_totalCounts = 0;
}

qint64 MatrixReader::getUncompressedSize() const
{
    if (m_resX <= 0 || m_resY <= 0) return 0;
    return static_cast<qint64>(m_resX) * static_cast<qint64>(m_resY) * sizeof(int32_t);
}

double MatrixReader::getCompressionRatio() const
{
    if (m_fileSize <= 0) return 1.0;
    return static_cast<double>(getUncompressedSize()) / static_cast<double>(m_fileSize);
}

int MatrixReader::getSegmentIndex(int tx, int ty) const
{
    if (m_matmode == 1) { // symmetric
        int x = std::min(tx, ty);
        int y = std::max(tx, ty);
        int it = x + y * (y + 1) / 2;
        return it + m_nextra;
    } else { // asymmetric
        return tx + ty * m_ndivX + m_nextra;
    }
}

namespace {
constexpr int kMaxTotalSegments = 10000000;
constexpr int kMaxResolution = 1048576;
constexpr int kMaxSegmentSize = 67108864;     // 64M int32 entries
constexpr int kMaxCompressedBlocks = 131072;  // 64 MB per segment

bool seekBlockChecked(FILE *f, int64_t block1Based, int64_t numBlocks, qint64 fileSize)
{
    if (!f || block1Based <= 0 || numBlocks <= 0 || numBlocks > kMaxCompressedBlocks) {
        return false;
    }
    const int64_t offset = (block1Based - 1) * 512LL;
    const int64_t bytes = numBlocks * 512LL;
    if (offset < 0 || bytes <= 0 || offset > fileSize || bytes > (fileSize - offset)) {
        return false;
    }
#if defined(_WIN32)
    return _fseeki64(f, offset, SEEK_SET) == 0;
#else
    return fseeko(f, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
}
} // namespace

bool MatrixReader::open(const QString &filePath, QString *errorMessage)
{
    close();

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        if (errorMessage) *errorMessage = QString("Matrix file does not exist: %1").arg(filePath);
        return false;
    }

    if (fi.size() < 512) {
        if (errorMessage) *errorMessage = QString("File is too small to be a valid .cmat matrix (%1 bytes).").arg(fi.size());
        return false;
    }

    FILE *f = fopen(filePath.toLocal8Bit().constData(), "rb");
    if (!f) {
        if (errorMessage) *errorMessage = QString("Cannot open matrix file for reading: %1").arg(filePath);
        return false;
    }

    m_filePath = filePath;
    m_fileSize = fi.size();

    // 1. Read Block 0 (Header)
    int32_t h0[128];
    if (fread(h0, sizeof(int32_t), 128, f) != 128) {
        fclose(f);
        close();
        if (errorMessage) *errorMessage = "Failed to read matrix header block 0.";
        return false;
    }

    m_version = h0[127] > 0 ? h0[127] : h0[0];
    m_ntotseg = h0[1];
    m_ndescblk = h0[10];

    if (m_ntotseg <= 0 || m_ntotseg > kMaxTotalSegments ||
        m_ndescblk <= 0 ||
        static_cast<int64_t>(m_ntotseg) * 8LL > (m_fileSize - 512LL) ||
        static_cast<int64_t>(m_ndescblk) * 512LL > (m_fileSize - 512LL)) {
        fclose(f);
        close();
        if (errorMessage) *errorMessage = "Invalid or unsupported matrix header (not a valid GASPware .cmat file).";
        return false;
    }

    // 2. Read IVF Descriptors
    if (!readDescriptorTable(f, errorMessage)) {
        fclose(f);
        close();
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = "Failed to read IVF descriptor table.";
        }
        return false;
    }

    // 3. Read CMT Header (Segment 0)
    if (!readCmtHeader(f, errorMessage)) {
        fclose(f);
        close();
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = "Failed to parse CMT matrix header metadata.";
        }
        return false;
    }

    // 4. Read 1D Projections (Segment 2 for X, Segment 3 for Y in asymmetric)
    if (!readProjections(f, errorMessage)) {
        fclose(f);
        close();
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = "Failed to decompress 1D projection spectra.";
        }
        return false;
    }

    fclose(f);
    m_isOpen = true;
    return true;
}

bool MatrixReader::readDescriptorTable(FILE *f, QString *errorMessage)
{
    // IVF Table starts at block 1 (byte offset 512)
#if defined(_WIN32)
    if (_fseeki64(f, 512, SEEK_SET) != 0)
#else
    if (fseeko(f, 512, SEEK_SET) != 0)
#endif
    {
        if (errorMessage) *errorMessage = "Failed to seek to IVF descriptor table.";
        return false;
    }

    const size_t totalInts = static_cast<size_t>(m_ntotseg) * 2U;
    m_descriptors.resize(static_cast<size_t>(m_ntotseg));
    std::vector<int32_t> raw(totalInts);
    if (fread(raw.data(), sizeof(int32_t), totalInts, f) != totalInts) {
        if (errorMessage) *errorMessage = "Truncated IVF descriptor table in matrix file.";
        return false;
    }

    for (int i = 0; i < m_ntotseg; ++i) {
        m_descriptors[i].nblocks = raw[static_cast<size_t>(i) * 2U];
        m_descriptors[i].startblk = raw[static_cast<size_t>(i) * 2U + 1U];
    }
    return true;
}

bool MatrixReader::readCmtHeader(FILE *f, QString *errorMessage)
{
    if (m_descriptors.empty()) return false;

    // Segment 0 contains CMT Header
    int startblk = m_descriptors[0].startblk;
    if (startblk <= 0) {
        startblk = 1 + m_ndescblk;
    }

    if (!seekBlockChecked(f, startblk, 1, m_fileSize)) {
        if (errorMessage) *errorMessage = "CMT header block offset is out of file bounds.";
        return false;
    }

    int32_t hCmt[128];
    if (fread(hCmt, sizeof(int32_t), 128, f) != 128) {
        if (errorMessage) *errorMessage = "Failed to read CMT header block.";
        return false;
    }

    m_ndim = hCmt[0];
    m_matmode = hCmt[1];
    m_resX = hCmt[3];
    m_stepX = hCmt[4];
    m_ndivX = hCmt[5];
    m_resY = hCmt[6];
    m_stepY = hCmt[7];
    m_ndivY = hCmt[8];
    m_segsize = hCmt[123];
    m_nextra = hCmt[125];

    if (m_ndim != 2) {
        if (errorMessage) {
            *errorMessage = QString("Unsupported matrix dimensionality (%1D); only 2D GASPware .cmat matrices are supported.")
                                .arg(m_ndim);
        }
        return false;
    }

    if (m_matmode != 0 && m_matmode != 1) {
        if (errorMessage) {
            *errorMessage = QString("Invalid matrix symmetry mode (%1) in CMT header.").arg(m_matmode);
        }
        return false;
    }

    if (m_resX <= 0 || m_resX > kMaxResolution ||
        m_resY <= 0 || m_resY > kMaxResolution ||
        m_ndivX <= 0 || m_ndivX > m_resX ||
        m_ndivY <= 0 || m_ndivY > m_resY ||
        m_stepX <= 0 || m_stepY <= 0) {
        if (errorMessage) {
            *errorMessage = QString("Invalid matrix dimensions in CMT header (%1x%2, step %3x%4, div %5x%6).")
                                .arg(m_resX).arg(m_resY).arg(m_stepX).arg(m_stepY).arg(m_ndivX).arg(m_ndivY);
        }
        return false;
    }

    const int64_t tileCells = static_cast<int64_t>(m_stepX) * static_cast<int64_t>(m_stepY);
    if (m_segsize <= 0) {
        if (tileCells <= 0 || tileCells > kMaxSegmentSize) {
            if (errorMessage) *errorMessage = "Invalid tile segment size in CMT header.";
            return false;
        }
        m_segsize = static_cast<int>(tileCells);
    } else if (m_segsize > kMaxSegmentSize || tileCells > m_segsize) {
        if (errorMessage) {
            *errorMessage = QString("Invalid or inconsistent tile segment size (%1) in CMT header.").arg(m_segsize);
        }
        return false;
    }

    if (m_nextra < 0) {
        m_nextra = (m_matmode == 1) ? 5 : 8;
    }
    if (m_nextra >= m_ntotseg) {
        if (errorMessage) *errorMessage = "Invalid extra segment count in CMT header.";
        return false;
    }

    return true;
}

bool MatrixReader::readProjections(FILE *f, QString *errorMessage)
{
    if (m_descriptors.size() <= 2) {
        if (errorMessage) *errorMessage = "Matrix file is missing projection descriptors.";
        return false;
    }

    // Segment 2: Projection X
    int nbX = m_descriptors[2].nblocks;
    int sbX = m_descriptors[2].startblk;
    if (!seekBlockChecked(f, sbX, nbX, m_fileSize)) {
        if (errorMessage) *errorMessage = "Projection X segment is missing or out of file bounds.";
        return false;
    }

    const size_t bytesX = static_cast<size_t>(nbX) * 512U;
    std::vector<uint8_t> cbufX(bytesX);
    if (fread(cbufX.data(), 1, cbufX.size(), f) != cbufX.size()) {
        if (errorMessage) *errorMessage = "Failed to read compressed Projection X segment.";
        return false;
    }

    int32_t cmodeX = *reinterpret_cast<const int32_t*>(cbufX.data());
    int32_t cminvalX = *reinterpret_cast<const int32_t*>(cbufX.data() + 4);
    int nchX = m_resX;
    int dnbytesX = static_cast<int>(cbufX.size()) - 8;

    std::vector<int32_t> projIntX(static_cast<size_t>(m_resX), 0);
    int statusX = comp_decompress_(projIntX.data(), &nchX, cbufX.data() + 8, &dnbytesX, &cmodeX, &cminvalX);
    if (statusX < 0) {
        if (errorMessage) *errorMessage = "Failed to decompress Projection X spectrum.";
        return false;
    }

    m_projectionX.resize(static_cast<size_t>(m_resX));
    m_totalCounts = 0;
    for (int i = 0; i < m_resX; ++i) {
        m_projectionX[i] = static_cast<double>(projIntX[i]);
        m_totalCounts += projIntX[i];
    }

    // Segment 3: Projection Y (if asymmetric)
    if (m_matmode != 1) { // Asymmetric matrix
        if (m_descriptors.size() > 3 &&
            seekBlockChecked(f, m_descriptors[3].startblk, m_descriptors[3].nblocks, m_fileSize)) {
            int nbY = m_descriptors[3].nblocks;
            const size_t bytesY = static_cast<size_t>(nbY) * 512U;
            std::vector<uint8_t> cbufY(bytesY);
            if (fread(cbufY.data(), 1, cbufY.size(), f) == cbufY.size()) {
                int32_t cmodeY = *reinterpret_cast<const int32_t*>(cbufY.data());
                int32_t cminvalY = *reinterpret_cast<const int32_t*>(cbufY.data() + 4);
                int nchY = m_resY;
                int dnbytesY = static_cast<int>(cbufY.size()) - 8;

                std::vector<int32_t> projIntY(static_cast<size_t>(m_resY), 0);
                int statusY = comp_decompress_(projIntY.data(), &nchY, cbufY.data() + 8, &dnbytesY, &cmodeY, &cminvalY);
                if (statusY >= 0) {
                    m_projectionY.resize(static_cast<size_t>(m_resY));
                    for (int i = 0; i < m_resY; ++i) {
                        m_projectionY[i] = static_cast<double>(projIntY[i]);
                    }
                } else {
                    m_projectionY = m_projectionX;
                }
            } else {
                m_projectionY = m_projectionX;
            }
        } else {
            m_projectionY = m_projectionX;
        }
    } else {
        // Symmetric matrix: Projection X and Projection Y are identical
        m_projectionY = m_projectionX;
    }

    return true;
}

std::vector<double> MatrixReader::getRawGateSlice(int chMin, int chMax, int gateAxis) const
{
    if (!m_isOpen || m_resX <= 0 || m_resY <= 0 ||
        m_stepX <= 0 || m_stepY <= 0 || m_segsize <= 0 ||
        m_ndivX <= 0 || m_ndivY <= 0) {
        return {};
    }

    FILE *f = fopen(m_filePath.toLocal8Bit().constData(), "rb");
    if (!f) return {};

    if (m_matmode == 1) {
        // Symmetric Matrix gating
        chMin = std::max(0, std::min(chMin, m_resY - 1));
        chMax = std::max(0, std::min(chMax, m_resY - 1));
        if (chMin > chMax) std::swap(chMin, chMax);

        std::vector<double> slice(static_cast<size_t>(m_resX), 0.0);
        const int stepX = m_stepX;
        const int stepY = m_stepY;
        const int ndivX = m_ndivX;
        const int tileSize = m_segsize;

        const int gBlockMin = chMin / stepY;
        const int gBlockMax = chMax / stepY;

        std::vector<int32_t> tileData(static_cast<size_t>(tileSize), 0);
        std::vector<uint8_t> cbuf(128 * 512);

        for (int gb = gBlockMin; gb <= gBlockMax; ++gb) {
            const int gateChStart = std::max(chMin, gb * stepY);
            const int gateChEnd = std::min(chMax, (gb + 1) * stepY - 1);

            for (int tx = 0; tx < ndivX; ++tx) {
                int segIdx = getSegmentIndex(tx, gb);
                if (segIdx < 0 || segIdx >= static_cast<int>(m_descriptors.size())) continue;

                int nblocks = m_descriptors[segIdx].nblocks;
                int startblk = m_descriptors[segIdx].startblk;
                if (!seekBlockChecked(f, startblk, nblocks, m_fileSize)) continue;

                size_t reqBytes = static_cast<size_t>(nblocks) * 512U;
                if (cbuf.size() < reqBytes) {
                    cbuf.resize(reqBytes);
                }

                if (fread(cbuf.data(), 1, reqBytes, f) != reqBytes) continue;

                int32_t cmode = *reinterpret_cast<const int32_t*>(cbuf.data());
                int32_t cminval = *reinterpret_cast<const int32_t*>(cbuf.data() + 4);
                int nch = tileSize;
                int dnbytes = static_cast<int>(reqBytes) - 8;

                if (comp_decompress_(tileData.data(), &nch, cbuf.data() + 8, &dnbytes, &cmode, &cminval) < 0) {
                    continue;
                }

                const int baseOut = tx * stepX;
                for (int g = gateChStart; g <= gateChEnd; ++g) {
                    const int goff = g % stepY;

                    if (tx < gb) {
                        for (int i = 0; i < stepX; ++i) {
                            const int outIdx = baseOut + i;
                            const int tileIdx = i + goff * stepX;
                            if (outIdx >= 0 && outIdx < m_resX && tileIdx >= 0 && tileIdx < tileSize) {
                                slice[outIdx] += tileData[tileIdx];
                            }
                        }
                    } else if (tx > gb) {
                        for (int i = 0; i < stepX; ++i) {
                            const int outIdx = baseOut + i;
                            const int tileIdx = goff + i * stepX;
                            if (outIdx >= 0 && outIdx < m_resX && tileIdx >= 0 && tileIdx < tileSize) {
                                slice[outIdx] += tileData[tileIdx];
                            }
                        }
                    } else {
                        // Diagonal tile
                        for (int i = 0; i < stepX; ++i) {
                            const int outIdx = baseOut + i;
                            const int tileIdx = (i <= goff) ? (i + goff * stepX) : (goff + i * stepX);
                            if (outIdx >= 0 && outIdx < m_resX && tileIdx >= 0 && tileIdx < tileSize) {
                                slice[outIdx] += tileData[tileIdx];
                            }
                        }
                    }
                }
            }
        }

        fclose(f);
        return slice;
    } else {
        // Asymmetric Matrix gating
        if (gateAxis == 1) {
            // Gate on Y (vertical) to project onto X (horizontal)
            chMin = std::max(0, std::min(chMin, m_resY - 1));
            chMax = std::max(0, std::min(chMax, m_resY - 1));
            if (chMin > chMax) std::swap(chMin, chMax);

            std::vector<double> slice(static_cast<size_t>(m_resX), 0.0);
            const int stepX = m_stepX;
            const int stepY = m_stepY;
            const int ndivX = m_ndivX;
            const int tileSize = m_segsize;

            const int gBlockMin = chMin / stepY;
            const int gBlockMax = chMax / stepY;

            std::vector<int32_t> tileData(static_cast<size_t>(tileSize), 0);
            std::vector<uint8_t> cbuf(128 * 512);

            for (int gb = gBlockMin; gb <= gBlockMax; ++gb) {
                const int gateChStart = std::max(chMin, gb * stepY);
                const int gateChEnd = std::min(chMax, (gb + 1) * stepY - 1);

                for (int tx = 0; tx < ndivX; ++tx) {
                    int segIdx = tx + gb * ndivX + m_nextra;
                    if (segIdx < 0 || segIdx >= static_cast<int>(m_descriptors.size())) continue;

                    int nblocks = m_descriptors[segIdx].nblocks;
                    int startblk = m_descriptors[segIdx].startblk;
                    if (!seekBlockChecked(f, startblk, nblocks, m_fileSize)) continue;

                    size_t reqBytes = static_cast<size_t>(nblocks) * 512U;
                    if (cbuf.size() < reqBytes) cbuf.resize(reqBytes);

                    if (fread(cbuf.data(), 1, reqBytes, f) != reqBytes) continue;

                    int32_t cmode = *reinterpret_cast<const int32_t*>(cbuf.data());
                    int32_t cminval = *reinterpret_cast<const int32_t*>(cbuf.data() + 4);
                    int nch = tileSize;
                    int dnbytes = static_cast<int>(reqBytes) - 8;

                    if (comp_decompress_(tileData.data(), &nch, cbuf.data() + 8, &dnbytes, &cmode, &cminval) < 0) {
                        continue;
                    }

                    const int baseOut = tx * stepX;
                    for (int g = gateChStart; g <= gateChEnd; ++g) {
                        const int goff = g % stepY;
                        for (int i = 0; i < stepX; ++i) {
                            const int outIdx = baseOut + i;
                            const int tileIdx = i + goff * stepX;
                            if (outIdx >= 0 && outIdx < m_resX && tileIdx >= 0 && tileIdx < tileSize) {
                                slice[outIdx] += tileData[tileIdx];
                            }
                        }
                    }
                }
            }

            fclose(f);
            return slice;
        } else {
            // Gate on X (horizontal) to project onto Y (vertical)
            chMin = std::max(0, std::min(chMin, m_resX - 1));
            chMax = std::max(0, std::min(chMax, m_resX - 1));
            if (chMin > chMax) std::swap(chMin, chMax);

            std::vector<double> slice(static_cast<size_t>(m_resY), 0.0);
            const int stepX = m_stepX;
            const int stepY = m_stepY;
            const int ndivY = m_ndivY;
            const int tileSize = m_segsize;

            const int gBlockMin = chMin / stepX;
            const int gBlockMax = chMax / stepX;

            std::vector<int32_t> tileData(static_cast<size_t>(tileSize), 0);
            std::vector<uint8_t> cbuf(128 * 512);

            for (int gb = gBlockMin; gb <= gBlockMax; ++gb) {
                const int gateChStart = std::max(chMin, gb * stepX);
                const int gateChEnd = std::min(chMax, (gb + 1) * stepX - 1);

                for (int ty = 0; ty < ndivY; ++ty) {
                    int segIdx = gb + ty * m_ndivX + m_nextra;
                    if (segIdx < 0 || segIdx >= static_cast<int>(m_descriptors.size())) continue;

                    int nblocks = m_descriptors[segIdx].nblocks;
                    int startblk = m_descriptors[segIdx].startblk;
                    if (!seekBlockChecked(f, startblk, nblocks, m_fileSize)) continue;

                    size_t reqBytes = static_cast<size_t>(nblocks) * 512U;
                    if (cbuf.size() < reqBytes) cbuf.resize(reqBytes);

                    if (fread(cbuf.data(), 1, reqBytes, f) != reqBytes) continue;

                    int32_t cmode = *reinterpret_cast<const int32_t*>(cbuf.data());
                    int32_t cminval = *reinterpret_cast<const int32_t*>(cbuf.data() + 4);
                    int nch = tileSize;
                    int dnbytes = static_cast<int>(reqBytes) - 8;

                    if (comp_decompress_(tileData.data(), &nch, cbuf.data() + 8, &dnbytes, &cmode, &cminval) < 0) {
                        continue;
                    }

                    const int baseOut = ty * stepY;
                    for (int g = gateChStart; g <= gateChEnd; ++g) {
                        const int goff = g % stepX;
                        for (int i = 0; i < stepY; ++i) {
                            const int outIdx = baseOut + i;
                            const int tileIdx = goff + i * stepX;
                            if (outIdx >= 0 && outIdx < m_resY && tileIdx >= 0 && tileIdx < tileSize) {
                                slice[outIdx] += tileData[tileIdx];
                            }
                        }
                    }
                }
            }

            fclose(f);
            return slice;
        }
    }
}

std::vector<double> MatrixReader::computeBackgroundSlice(int chMin, int chMax, int gateAxis, double *outPfacs) const
{
    if (outPfacs) *outPfacs = 0.0;
    if (!m_isOpen || m_resX <= 0 || m_resY <= 0) return {};

    const int resG = (m_matmode == 1 || gateAxis == 1) ? m_resY : m_resX;
    const int resC = (m_matmode == 1 || gateAxis == 1) ? m_resX : m_resY;
    const std::vector<double> &projG = (m_matmode == 1 || gateAxis == 1) ? m_projectionY : m_projectionX;
    const std::vector<double> &projC = (m_matmode == 1 || gateAxis == 1) ? m_projectionX : m_projectionY;

    int ch1 = std::max(0, std::min(chMin, resG - 1));
    int ch2 = std::max(0, std::min(chMax, resG - 1));
    if (ch1 > ch2) std::swap(ch1, ch2);

    std::vector<double> bg(resC, 0.0);

    if (!m_bgConfig.enabled || m_bgConfig.mode == MatrixBgMode::None) {
        return bg;
    }

    if (m_bgConfig.mode == MatrixBgMode::Common) {
        // GASPware Xtrackn trackn.F:6550:
        // pfacs = pbacks / sbacktot(gside) * corrback
        // back(ii) = int(sback(ii, cside) * pfacs + 0.5)
        double pbacks = 0.0;
        for (int g = ch1; g <= ch2 && g < static_cast<int>(projG.size()); ++g) {
            pbacks += projG[g];
        }

        double sbacktot = 0.0;
        for (double v : projG) {
            sbacktot += v;
        }

        double pfacs = (sbacktot > 0.0) ? (pbacks / sbacktot) * m_bgConfig.correctionFactor : 0.0;
        if (outPfacs) *outPfacs = pfacs;

        for (int i = 0; i < resC && i < static_cast<int>(projC.size()); ++i) {
            bg[i] = std::round(projC[i] * pfacs);
        }
        return bg;
    }

    if (m_bgConfig.mode == MatrixBgMode::Normal) {
        // Local background: trapezoid under peak between ch1 and ch2
        double y1 = (ch1 < static_cast<int>(projG.size())) ? projG[ch1] : 0.0;
        double y2 = (ch2 < static_cast<int>(projG.size())) ? projG[ch2] : 0.0;
        double width = static_cast<double>(ch2 - ch1 + 1);
        double pbacksLocal = 0.5 * (y1 + y2) * width;

        double sbacktot = 0.0;
        for (double v : projG) {
            sbacktot += v;
        }

        double pfacs = (sbacktot > 0.0) ? (pbacksLocal / sbacktot) * m_bgConfig.correctionFactor : 0.0;
        if (outPfacs) *outPfacs = pfacs;

        for (int i = 0; i < resC && i < static_cast<int>(projC.size()); ++i) {
            bg[i] = std::round(projC[i] * pfacs);
        }
        return bg;
    }

    if (m_bgConfig.mode == MatrixBgMode::Auto) {
        // SNIP continuum filter applied on raw slice
        std::vector<double> raw = getRawGateSlice(chMin, chMax, gateAxis);
        bg = raw;
        int n = static_cast<int>(bg.size());
        int m = 20; // smoothing iterations
        for (int p = 1; p <= m; ++p) {
            for (int i = p; i < n - p; ++i) {
                double avg = 0.5 * (bg[i - p] + bg[i + p]);
                if (avg < bg[i]) bg[i] = avg;
            }
        }
        for (int i = 0; i < n; ++i) {
            bg[i] = std::round(bg[i] * m_bgConfig.correctionFactor);
        }
        if (outPfacs) *outPfacs = 0.0;
        return bg;
    }

    return bg;
}

std::vector<double> MatrixReader::getGateSlice(int chMin, int chMax, int gateAxis, bool applyBackground) const
{
    std::vector<double> slice = getRawGateSlice(chMin, chMax, gateAxis);
    if (applyBackground && m_bgConfig.enabled && m_bgConfig.mode != MatrixBgMode::None) {
        std::vector<double> bg = computeBackgroundSlice(chMin, chMax, gateAxis);
        for (size_t i = 0; i < slice.size() && i < bg.size(); ++i) {
            slice[i] -= bg[i];
        }
    }
    return slice;
}

std::vector<double> MatrixReader::getMultiGateSlice(const std::vector<MatrixGateRegion> &gates,
                                                    int peakGateIndex,
                                                    int gateAxis,
                                                    bool applyBackground,
                                                    double *outBackfac,
                                                    double *outBgCounts,
                                                    std::vector<double> *outBgSlice) const
{
    if (outBackfac) *outBackfac = 0.0;
    if (outBgCounts) *outBgCounts = 0.0;
    if (!m_isOpen || m_resX <= 0 || m_resY <= 0 || gates.empty()) return {};

    const int resC = (m_matmode == 1 || gateAxis == 1) ? m_resX : m_resY;
    if (outBgSlice) outBgSlice->assign(resC, 0.0);

    // Single gate fallback
    if (gates.size() == 1) {
        if (outBackfac) *outBackfac = 1.0;
        std::vector<double> raw = getRawGateSlice(gates[0].minCh, gates[0].maxCh, gateAxis);
        if (!applyBackground || !m_bgConfig.enabled || m_bgConfig.mode == MatrixBgMode::None) {
            return raw;
        }
        double pfacs = 0.0;
        std::vector<double> bg = computeBackgroundSlice(gates[0].minCh, gates[0].maxCh, gateAxis, &pfacs);
        if (outBackfac) *outBackfac = pfacs;
        if (outBgSlice) *outBgSlice = bg;
        double bgSum = 0.0;
        for (size_t i = 0; i < raw.size() && i < bg.size(); ++i) {
            bgSum += bg[i];
            raw[i] -= bg[i];
        }
        if (outBgCounts) *outBgCounts = bgSum;
        return raw;
    }

    if (peakGateIndex < 0 || peakGateIndex >= static_cast<int>(gates.size())) {
        peakGateIndex = 0;
    }

    // Background disabled: return raw slice of the peak gate
    if (!applyBackground || !m_bgConfig.enabled || m_bgConfig.mode == MatrixBgMode::None) {
        return getRawGateSlice(gates[peakGateIndex].minCh, gates[peakGateIndex].maxCh, gateAxis);
    }

    // 1. Normal Mode: true multi-gate subtraction (GASPware trackn.F:6476)
    if (m_bgConfig.mode == MatrixBgMode::Normal) {
        int pMin = std::min(gates[peakGateIndex].minCh, gates[peakGateIndex].maxCh);
        int pMax = std::max(gates[peakGateIndex].minCh, gates[peakGateIndex].maxCh);
        int wPeak = pMax - pMin + 1;

        int wBgTotal = 0;
        for (size_t i = 0; i < gates.size(); ++i) {
            if (static_cast<int>(i) == peakGateIndex) continue;
            int bMin = std::min(gates[i].minCh, gates[i].maxCh);
            int bMax = std::max(gates[i].minCh, gates[i].maxCh);
            wBgTotal += (bMax - bMin + 1);
        }

        double backfac = (wBgTotal > 0)
            ? (static_cast<double>(wPeak) / static_cast<double>(wBgTotal)) * m_bgConfig.correctionFactor
            : 1.0;
        if (outBackfac) *outBackfac = backfac;

        std::vector<double> peakSlice = getRawGateSlice(pMin, pMax, gateAxis);

        std::vector<double> bgSliceTotal(resC, 0.0);
        for (size_t i = 0; i < gates.size(); ++i) {
            if (static_cast<int>(i) == peakGateIndex) continue;
            int bMin = std::min(gates[i].minCh, gates[i].maxCh);
            int bMax = std::max(gates[i].minCh, gates[i].maxCh);
            std::vector<double> s = getRawGateSlice(bMin, bMax, gateAxis);
            for (int ch = 0; ch < resC && ch < static_cast<int>(s.size()); ++ch) {
                bgSliceTotal[ch] += s[ch];
            }
        }

        std::vector<double> netSlice = peakSlice;
        double bgCounts = 0.0;
        if (outBgSlice) outBgSlice->resize(resC);

        for (int ch = 0; ch < resC && ch < static_cast<int>(netSlice.size()); ++ch) {
            double sub = std::round(bgSliceTotal[ch] * backfac);
            bgCounts += sub;
            if (outBgSlice) (*outBgSlice)[ch] = sub;
            netSlice[ch] -= sub;
        }

        if (outBgCounts) *outBgCounts = bgCounts;
        return netSlice;
    }

    // 2. Common Mode: sum all gates and subtract common projection background (GASPware trackn.F:6526)
    if (m_bgConfig.mode == MatrixBgMode::Common) {
        std::vector<double> sumSlice(resC, 0.0);
        for (const auto &g : gates) {
            int g1 = std::min(g.minCh, g.maxCh);
            int g2 = std::max(g.minCh, g.maxCh);
            std::vector<double> s = getRawGateSlice(g1, g2, gateAxis);
            for (int ch = 0; ch < resC && ch < static_cast<int>(s.size()); ++ch) {
                sumSlice[ch] += s[ch];
            }
        }

        const std::vector<double> &projG = (m_matmode == 1 || gateAxis == 1) ? m_projectionY : m_projectionX;
        const std::vector<double> &projC = (m_matmode == 1 || gateAxis == 1) ? m_projectionX : m_projectionY;

        double pbacks = 0.0;
        for (const auto &g : gates) {
            int g1 = std::min(g.minCh, g.maxCh);
            int g2 = std::max(g.minCh, g.maxCh);
            for (int ch = g1; ch <= g2 && ch < static_cast<int>(projG.size()); ++ch) {
                pbacks += projG[ch];
            }
        }

        double sbacktot = 0.0;
        for (double v : projG) sbacktot += v;

        double pfacs = (sbacktot > 0.0) ? (pbacks / sbacktot) * m_bgConfig.correctionFactor : 0.0;
        if (outBackfac) *outBackfac = pfacs;

        double bgCounts = 0.0;
        if (outBgSlice) outBgSlice->resize(resC);

        for (int ch = 0; ch < resC && ch < static_cast<int>(sumSlice.size()); ++ch) {
            double sub = std::round((ch < static_cast<int>(projC.size()) ? projC[ch] : 0.0) * pfacs);
            bgCounts += sub;
            if (outBgSlice) (*outBgSlice)[ch] = sub;
            sumSlice[ch] -= sub;
        }

        if (outBgCounts) *outBgCounts = bgCounts;
        return sumSlice;
    }

    // 3. Auto Mode: SNIP filter on peak slice
    if (m_bgConfig.mode == MatrixBgMode::Auto) {
        int pMin = std::min(gates[peakGateIndex].minCh, gates[peakGateIndex].maxCh);
        int pMax = std::max(gates[peakGateIndex].minCh, gates[peakGateIndex].maxCh);
        std::vector<double> raw = getRawGateSlice(pMin, pMax, gateAxis);
        std::vector<double> bg = raw;
        int n = static_cast<int>(bg.size());
        int m = 20;
        for (int p = 1; p <= m; ++p) {
            for (int i = p; i < n - p; ++i) {
                double avg = 0.5 * (bg[i - p] + bg[i + p]);
                if (avg < bg[i]) bg[i] = avg;
            }
        }
        double bgCounts = 0.0;
        if (outBgSlice) outBgSlice->resize(resC);
        for (int i = 0; i < n && i < static_cast<int>(raw.size()); ++i) {
            double sub = std::round(bg[i] * m_bgConfig.correctionFactor);
            bgCounts += sub;
            if (outBgSlice) (*outBgSlice)[i] = sub;
            raw[i] -= sub;
        }
        if (outBgCounts) *outBgCounts = bgCounts;
        return raw;
    }

    return {};
}

std::vector<double> MatrixReader::computeSnipBackground(const std::vector<double> &spectrum,
                                                        int iterations,
                                                        double factor) const
{
    if (spectrum.empty()) return {};
    std::vector<double> bg = spectrum;
    int n = static_cast<int>(bg.size());
    int m = (iterations > 0) ? iterations : 20;
    for (int p = 1; p <= m; ++p) {
        for (int i = p; i < n - p; ++i) {
            double avg = 0.5 * (bg[i - p] + bg[i + p]);
            if (avg < bg[i]) bg[i] = avg;
        }
    }
    for (int i = 0; i < n; ++i) {
        bg[i] = std::round(bg[i] * factor);
    }
    return bg;
}
