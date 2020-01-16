#ifndef PAQ8PX_ENCODER_HPP
#define PAQ8PX_ENCODER_HPP

#include <cstdint>
#include <cstdio>
#include <cassert>
#include "Predictor.hpp"
#include "Shared.hpp"

typedef enum {
    COMPRESS, DECOMPRESS
} Mode;

/**
 * An Encoder does arithmetic encoding.
 * If shared->level is 0, then data is stored without arithmetic coding.
 */
class Encoder {
private:
    Predictor predictor;
    const Mode mode; // Compress or decompress?
    File *archive; // Compressed data file
    uint32_t x1, x2; // Range, initially [0, 1), scaled by 2^32
    uint32_t x; // Decompress mode: last 4 input bytes of archive
    File *alt; // decompress() source in COMPRESS mode
    float p1 {}, p2 {}; // percentages for progress indicator: 0.0 .. 1.0
    Shared *shared = Shared::getInstance();

    /**
     * code(i) in COMPRESS mode compresses bit i (0 or 1) to file f.
     * code() in DECOMPRESS mode returns the next decompressed bit from file f.
     * Global y is set to the last bit coded or decoded by code().
     * @param i the bit to be compressed
     * @return
     */
    int code(int i = 0);
public:
    /**
     * Encoder(COMPRESS, f) creates encoder for compression to archive f, which
     * must be open past any header for writing in binary mode.
     * Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
     * which must be open past any header for reading in binary mode.
     * @param m the mode to operate in
     * @param f the file to read from or write to
     */
    Encoder(Mode m, File *f);
    [[nodiscard]] Mode getMode() const;
    /**
     * size() returns current length of archive
     * @return length of archive so far
     */
    [[nodiscard]] uint64_t size() const;
    /**
     * flush() should be called exactly once after compression is done and
     * before closing f. It does nothing in DECOMPRESS mode.
     */
    void flush();
    /**
     * setFile(f) sets alternate source to File *f for decompress() in COMPRESS
     * mode (for testing transforms).
     * @param f
     */
    void setFile(File *f);
    /**
     * compress(c) in COMPRESS mode compresses one byte.
     * @param c the byte to be compressed
     */
    void compress(int c);
    /**
     * decompress() in DECOMPRESS mode decompresses and returns one byte.
     * @return the decompressed byte
     */
    int decompress();
    /**
     * @todo Large file support
     * @param blockSize
     */
    void encodeBlockSize(uint64_t blockSize);
    /**
     * @todo Large file support
     * @return
     */
    uint64_t decodeBlockSize();
    void setStatusRange(float perc1, float perc2);
    void printStatus(uint64_t n, uint64_t size);
    void printStatus();
};


#endif //PAQ8PX_ENCODER_HPP
