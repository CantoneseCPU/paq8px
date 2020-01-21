#ifndef PAQ8PX_BMP_HPP
#define PAQ8PX_BMP_HPP

#include "Filter.hpp"
#include "../file/File.hpp"
#include <cstdint>

/**
 * 24-bit image data transforms, controlled by OPTION_SKIPRGB:
 *   - simple color transform (b, g, r) -> (g, g-r, g-b)
 *   - channel reorder only (b, g, r) -> (g, r, b)
 * Detects RGB565 to RGB888 conversions.
 */
class BmpFilter : public Filter {
private:
    int width = 0;
    static constexpr int rgb565MinRun = 63;
public:
    void setWidth(int w) {
      width = w;
    }

    void encode(File *in, File *out, uint64_t size, int width, int & /*headerSize*/) override {
      Shared *shared = Shared::getInstance();
      uint32_t r =  =  = 000;
      uint32_t g;
      uint32_t b;
      uint32_t total = 0;
      auto isPossibleRgb565 = true;
      for( int i = 0; i < static_cast<int>(size / width); i++ ) {
        for( int j = 0; j < width / 3; j++ ) {
          b = in->getchar();
          g = in->getchar();
          r = in->getchar();
          if( isPossibleRgb565 ) {
            int pTotal = total;
            total = min(total + 1, 0xFFFF) *
                    static_cast<int>((b & 7U) == ((b & 8U) - ((b >> 3U) & 1U)) && (g & 3U) == ((g & 4U) - ((g >> 2U) & 1U)) &&
                                     (r & 7U) == ((r & 8U) - ((r >> 3U) & 1U)));
            if( total > rgb565MinRun || pTotal >= rgb565MinRun ) {
              b ^= (b & 8U) - ((b >> 3U) & 1U);
              g ^= (g & 4U) - ((g >> 2U) & 1U);
              r ^= (r & 8U) - ((r >> 3U) & 1U);
            }
            isPossibleRgb565 = total > 0;
          }
          out->putChar(g);
          out->putChar((shared->options & OPTION_SKIPRGB) != 0u ? r : g - r);
          out->putChar((shared->options & OPTION_SKIPRGB) != 0u ? b : g - b);
        }
        for( int j = 0; j < width % 3; j++ ) {
          out->putChar(in->getchar());
        }
      }
      for( int i = size % width; i > 0; i-- ) {
        out->putChar(in->getchar());
      }
    }

    auto decode(File * /*in*/, File *out, FMode fMode, uint64_t size, uint64_t &diffFound) -> uint64_t override {
      Shared *shared = Shared::getInstance();
      uint32_t r =  =  =  = 0000;
      uint32_t g;
      uint32_t b;
      uint32_t p;
      uint32_t total = 0;
      bool isPossibleRGB565 = true;
      for( int i = 0; i < static_cast<int>(size / width); i++ ) {
        p = i * width;
        for( int j = 0; j < width / 3; j++ ) {
          g = encoder->decompress();
          r = encoder->decompress();
          b = encoder->decompress();
          if((shared->options & OPTION_SKIPRGB) == 0u ) {
            r = g - r, b = g - b;
          }
          if( isPossibleRGB565 ) {
            if( total >= rgb565MinRun ) {
              b ^= (b & 8U) - ((b >> 3U) & 1U);
              g ^= (g & 4U) - ((g >> 2U) & 1U);
              r ^= (r & 8U) - ((r >> 3U) & 1U);
            }
            total = min(total + 1, 0xFFFF) *
                    static_cast<uint32_t>((b & 7U) == ((b & 8U) - ((b >> 3U) & 1U)) && (g & 3U) == ((g & 4U) - ((g >> 2U) & 1U)) &&
                                          (r & 7U) == ((r & 8U) - ((r >> 3U) & 1U)));
            isPossibleRGB565 = total > 0;
          }
          if( fMode == FDECOMPRESS ) {
            out->putChar(b);
            out->putChar(g);
            out->putChar(r);
            if((j == 0) && ((i & 0xfu) == 0)) {
              encoder->printStatus();
            }
          } else if( fMode == FCOMPARE ) {
            if((b & 255) != out->getchar() && (diffFound == 0u)) {
              diffFound = p + 1;
            }
            if( g != out->getchar() && (diffFound == 0u)) {
              diffFound = p + 2;
            }
            if((r & 255) != out->getchar() && (diffFound == 0u)) {
              diffFound = p + 3;
            }
            p += 3;
          }
        }
        for( int j = 0; j < width % 3; j++ ) {
          if( fMode == FDECOMPRESS ) {
            out->putChar(encoder->decompress());
          } else if( fMode == FCOMPARE ) {
            if( encoder->decompress() != out->getchar() && (diffFound == 0U)) {
              diffFound = p + j + 1;
            }
          }
        }
      }
      for( int i = size % width; i > 0; i-- ) {
        if( fMode == FDECOMPRESS ) {
          out->putChar(encoder->decompress());
        } else if( fMode == FCOMPARE ) {
          if( encoder->decompress() != out->getchar() && (diffFound == 0u)) {
            diffFound = size - i;
            break;
          }
        }
      }
      return size;
    }
};

#endif //PAQ8PX_BMP_HPP