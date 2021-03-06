#include "Audio8BitModel.hpp"

Audio8BitModel::Audio8BitModel(ModelStats *st) : AudioModel(st), sMap1B {
        /*nOLS: 0-3*/ {{11, 1, 6, 128}, {11, 1, 9, 128}, {11, 1, 7, 86}},
                      {{11, 1, 6, 128}, {11, 1, 9, 128}, {11, 1, 7, 86}},
                      {{11, 1, 6, 128}, {11, 1, 9, 128}, {11, 1, 7, 86}},
                      {{11, 1, 6, 86},  {11, 1, 9, 86},  {11, 1, 7, 86}},
        /*nOLS: 4-7*/
                      {{11, 1, 6, 128}, {11, 1, 9, 128}, {11, 1, 7, 86}},
                      {{11, 1, 6, 128}, {11, 1, 9, 128}, {11, 1, 7, 86}},
                      {{11, 1, 6, 128}, {11, 1, 9, 128}, {11, 1, 7, 86}},
                      {{11, 1, 6, 128}, {11, 1, 9, 128}, {11, 1, 7, 86}},
        /*nLMS: 0-2*/
                      {{11, 1, 6, 86},  {11, 1, 9, 86},  {11, 1, 7, 86}},
                      {{11, 1, 6, 86},  {11, 1, 9, 86},  {11, 1, 7, 86}},
                      {{11, 1, 6, 86},  {11, 1, 9, 86},  {11, 1, 7, 86}},
        /*nSSM: 0-2*/
                      {{11, 1, 6, 86},  {11, 1, 9, 86},  {11, 1, 7, 86}},
                      {{11, 1, 6, 86},  {11, 1, 9, 86},  {11, 1, 7, 86}},
                      {{11, 1, 6, 86},  {11, 1, 9, 86},  {11, 1, 7, 86}}} {}

void Audio8BitModel::setParam(int info) {
  if( stats->blPos == 0 && shared->bitPosition == 0 ) {
    assert((info & 2) == 0);
    stereo = (info & 1U);
    mask = 0;
    stats->wav = stereo + 1;
    wMode = info;
    for( int i = 0; i < nLMS; i++ ) {
      lms[i][0].reset(), lms[i][1].reset();
    }
  }
}

void Audio8BitModel::mix(Mixer &m) {
  if( shared->bitPosition == 0 ) {
    ch = (stereo) != 0 ? stats->blPos & 1U : 0;
    const int8_t s = int(((wMode & 4U) > 0) ? shared->c1 ^ 128U : shared->c1) - 128U;
    const int pCh = ch ^stereo;
    int i = 0;
    for( errLog = 0; i < nOLS; i++ ) {
      ols[i][pCh].update(s);
      residuals[i][pCh] = s - prd[i][pCh][0];
      const auto absResidual = static_cast<uint32_t>(abs(residuals[i][pCh]));
      mask += mask + static_cast<unsigned int>(absResidual > 4);
      errLog += square(absResidual);
    }
    for( int j = 0; j < nLMS; j++ ) {
      lms[j][pCh].update(s);
    }
    for( ; i < nSSM; i++ ) {
      residuals[i][pCh] = s - prd[i][pCh][0];
    }
    errLog = min(0xF, ilog2(errLog));
    stats->Audio = mxCtx = ilog2(min(0x1F, bitCount(mask))) * 2 + ch;

    int k1 = 90;
    int k2 = k1 - 12 * stereo;
    for( int j = (i = 1); j <= k1; j++, i += 1U << (static_cast<int>(j > 8) + static_cast<int>(j > 16) + static_cast<int>(j > 64))) {
      ols[1][ch].add(x1(i));
    }
    for( int j = (i = 1); j <= k2; j++, i += 1U
            << (static_cast<int>(j > 5) + static_cast<int>(j > 10) + static_cast<int>(j > 17) + static_cast<int>(j > 26) +
                static_cast<int>(j > 37))) {
      ols[2][ch].add(x1(i));
    }
    for( int j = (i = 1); j <= k2; j++, i += 1U
            << (static_cast<int>(j > 3) + static_cast<int>(j > 7) + static_cast<int>(j > 14) + static_cast<int>(j > 20) +
                static_cast<int>(j > 33) + static_cast<int>(j > 49))) {
      ols[3][ch].add(x1(i));
    }
    for( int j = (i = 1); j <= k2; j++, i += 1 + static_cast<int>(j > 4) + static_cast<int>(j > 8)) {
      ols[4][ch].add(x1(i));
    }
    for( int j = (i = 1); j <= k1; j++, i += 2 + (static_cast<int>(j > 3) + static_cast<int>(j > 9) + static_cast<int>(j > 19) +
                                                  static_cast<int>(j > 36) + static_cast<int>(j > 61))) {
      ols[5][ch].add(x1(i));
    }
    if( stereo != 0 ) {
      for( i = 1; i <= k1 - k2; i++ ) {
        const auto s = static_cast<double>(x2(i));
        ols[2][ch].addFloat(s);
        ols[3][ch].addFloat(s);
        ols[4][ch].addFloat(s);
      }
    }
    k1 = 28;
    k2 = k1 - 6 * stereo;
    for( i = 1; i <= k2; i++ ) {
      const auto s = static_cast<double>(x1(i));
      ols[0][ch].addFloat(s);
      ols[6][ch].addFloat(s);
      ols[7][ch].addFloat(s);
    }
    for( ; i <= 96; i++ ) {
      ols[0][ch].add(x1(i));
    }
    if( stereo != 0 ) {
      for( i = 1; i <= k1 - k2; i++ ) {
        const auto s = static_cast<double>(x2(i));
        ols[0][ch].addFloat(s);
        ols[6][ch].addFloat(s);
        ols[7][ch].addFloat(s);
      }
      for( ; i <= 32; i++ ) {
        ols[0][ch].add(x2(i));
      }
    } else {
      for( ; i <= 128; i++ ) {
        ols[0][ch].add(x1(i));
      }
    }

    for( i = 0; i < nOLS; i++ ) {
      prd[i][ch][0] = signedClip8(static_cast<int>(floor(ols[i][ch].predict())));
    }
    for( ; i < nOLS + nLMS; i++ ) {
      prd[i][ch][0] = signedClip8(static_cast<int>(floor(lms[i - nOLS][ch].predict(s))));
    }
    prd[i++][ch][0] = signedClip8(x1(1) * 2 - x1(2));
    prd[i++][ch][0] = signedClip8(x1(1) * 3 - x1(2) * 3 + x1(3));
    prd[i][ch][0] = signedClip8(x1(1) * 4 - x1(2) * 6 + x1(3) * 4 - x1(4));
    for( i = 0; i < nSSM; i++ ) {
      prd[i][ch][1] = signedClip8(prd[i][ch][0] + residuals[i][pCh]);
    }
  }
  const int8_t b = shared->c0 << (8U - shared->bitPosition);
  for( int i = 0; i < nSSM; i++ ) {
    const uint32_t ctx = (prd[i][ch][0] - b) * 8 + shared->bitPosition;
    sMap1B[i][0].set(ctx);
    sMap1B[i][1].set(ctx);
    sMap1B[i][2].set((prd[i][ch][1] - b) * 8 + shared->bitPosition);
    sMap1B[i][0].mix(m);
    sMap1B[i][1].mix(m);
    sMap1B[i][2].mix(m);
  }
  m.set((errLog << 8U) | shared->c0, 4096);
  m.set((uint8_t(mask) << 3U) | (ch << 2U) | (shared->bitPosition >> 1U), 2048);
  m.set((mxCtx << 7U) | (shared->c1 >> 1U), 1280);
  m.set((errLog << 4U) | (ch << 3U) | shared->bitPosition, 256);
  m.set(mxCtx, 10);
}
