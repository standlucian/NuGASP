#ifndef COMPLIB_H
#define COMPLIB_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decompresses a GASPware compressed block/segment.
 * 
 * @param decomp   Output array of 32-bit integers.
 * @param nch      Pointer to int containing number of channels/integers to decompress.
 * @param packed   Input pointer to packed compressed bytes (starting after the 8-byte segment header).
 * @param nbytes   Pointer to int containing number of compressed bytes available in packed.
 * @param cmode    Pointer to int compression mode (from segment header byte 0-3).
 * @param cminval  Pointer to int min offset value (from segment header byte 4-7).
 * @return int     Status (>=0 for success, negative for error).
 */
int comp_decompress_(int *decomp, int *nch, unsigned char *packed, int *nbytes, int *cmode, int *cminval);

/**
 * @brief Compresses an array of 32-bit integers into GASPware format.
 */
int comp_compress_(int *decomp, const int *nch, unsigned char *packed, int *nbytes, int *cmode, int *cminval);

#ifdef __cplusplus
}
#endif

#endif // COMPLIB_H
