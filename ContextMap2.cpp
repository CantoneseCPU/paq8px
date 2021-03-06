#include "ContextMap2.hpp"

ContextMap2::ContextMap2(const uint64_t size, const uint32_t contexts, const int scale, const uint32_t uw) : C(contexts), table(size / sizeof(Bucket)),
        bitState(contexts), bitState0(contexts), byteHistory(contexts), contexts(contexts), checksums(contexts),
        runMap(contexts, (1U << 12U), 127, StateMap::Run),         /* StateMap : s, n, lim, init */ // 63-255
        stateMap(contexts, (1U << 8U), 511, StateMap::BitHistory), /* StateMap : s, n, lim, init */ // 511-1023
        bhMap8B(contexts, (1U << 8U), 511, StateMap::Generic),     /* StateMap : s, n, lim, init */ // 511-1023
        bhMap12B(contexts, (1U << 12U), 511, StateMap::Generic),   /* StateMap : s, n, lim, init */ // 255-1023
        index(0), mask(uint32_t(table.size() - 1)), hashBits(ilog2(mask + 1)), validFlags(0), scale(scale), useWhat(uw) {
#ifdef VERBOSE
  printf("Created ContextMap2 with size = %" PRIu64 ", contexts = %d, scale = %d, uw = %d\n", size, contexts, scale, uw);
#endif
  assert(size >= 64 && isPowerOf2(size));
  static_assert(sizeof(Bucket) == 64, "Size of Bucket should be 64!");
  assert(C <= (int) sizeof(validFlags) * 8); // validFlags is 64 bits - it can't support more than 64 contexts
  for( uint32_t i = 0; i < C; i++ ) {
    bitState[i] = bitState0[i] = &table[i].bitState[0][0];
    byteHistory[i] = bitState[i] + 3;
  }
}

void ContextMap2::set(const uint64_t ctx) {
  assert(index >= 0 && index < C);
  const uint32_t ctx0 = contexts[index] = finalize64(ctx, hashBits);
  const uint16_t chk0 = checksums[index] = static_cast<uint16_t>(checksum64(ctx, hashBits, 16));
  uint8_t *base = bitState[index] = bitState0[index] = table[ctx0].find(chk0, shared->chosenSimd);
  byteHistory[index] = &base[3];
  const uint8_t runCount = base[3];
  if( runCount == 255 ) { // pending
    // update pending bit histories for bits 2-7
    // in case of a collision updating (mixing) is slightly better (but slightly slower) then resetting, so we update
    const int c = base[4] + 256;
    uint8_t *p1A = table[(ctx0 + (c >> 6U)) & mask].find(chk0, shared->chosenSimd);
    StateTable::update(p1A, ((c >> 5U) & 1), rnd);
    StateTable::update(p1A + 1 + ((c >> 5) & 1), ((c >> 4) & 1), rnd);
    StateTable::update(p1A + 3 + ((c >> 4U) & 3), ((c >> 3) & 1), rnd);
    uint8_t *p1B = table[(ctx0 + (c >> 3)) & mask].find(chk0, shared->chosenSimd);
    StateTable::update(p1B, (c >> 2) & 1, rnd);
    StateTable::update(p1B + 1 + ((c >> 2) & 1), (c >> 1) & 1, rnd);
    StateTable::update(p1B + 3 + ((c >> 1) & 3), c & 1, rnd);
    base[3] = 1; // runCount: flag for having completed storing all the 8 bits of the first byte
  } else {
    const uint8_t byteState = base[0];
    if( byteState == 0 ) { // empty slot, new context
      base[3] = 255; // runCount: flag for skipping updating bits 2..7
    }
  }
  index++;
  validFlags = (validFlags << 1U) + 1;
}

void ContextMap2::skip() {
  assert(index >= 0 && index < C);
  index++;
  validFlags <<= 1U;
}

void ContextMap2::update() {
  INJECT_SHARED_y
  for( uint32_t i = 0; i < index; i++ ) {
    if(((validFlags >> (index - 1 - i)) & 1U) != 0 ) {
      if( bitState[i] != nullptr ) {
        StateTable::update(bitState[i], y, rnd);
      }

      auto byteHistoryPtr = byteHistory[i];
      const auto runCount = byteHistoryPtr[0];

      if( runCount == 255 && shared->bitPosition >= 2 ) {
        bitState[i] = nullptr; // shadow non-reserved slots for bits 2..7 and skip update temporarily
      } else {
        switch( shared->bitPosition ) {
          case 0: {
            // update byte history
            const auto byteState = byteHistoryPtr[-3];
            if( byteState < 3 ) { // 1st byte has just become known
              byteHistoryPtr[1] = byteHistoryPtr[2] = byteHistoryPtr[3] = shared->c1; // set all byte candidates to shared->c1
            } else { // 2nd byte is known
              const auto isMatch = byteHistoryPtr[1] == shared->c1;
              if( isMatch ) {
                if( runCount < 253 ) {
                  byteHistoryPtr[0] = runCount + 1;
                }
              } else {
                byteHistoryPtr[0] = 1; //runCount
                // scroll byte candidates
                byteHistoryPtr[3] = byteHistoryPtr[2];
                byteHistoryPtr[2] = byteHistoryPtr[1];
                byteHistoryPtr[1] = shared->c1;
              }
            }
            break;
          }
          case 2:
          case 5: {
            const uint32_t ctx = contexts[i];
            const uint16_t chk = checksums[i];
            bitState[i] = bitState0[i] = table[(ctx + shared->c0) & mask].find(chk, shared->chosenSimd);
            break;
          }
          case 1:
          case 3:
          case 6:
            bitState[i] = bitState0[i] + 1 + y;
            break;
          case 4:
          case 7:
            bitState[i] = bitState0[i] + 3 + (shared->c0 & 3U);
            break;
          default:
            assert(false);
        }
      }
    }
  }
  if( shared->bitPosition == 0 ) {
    index = 0;
    validFlags = 0;
  } // start over
}

void ContextMap2::setScale(const int Scale) { scale = Scale; }

void ContextMap2::mix(Mixer &m) {
  shared->updateBroadcaster->subscribe(this);
  stateMap.subscribe();
  if((useWhat & CM_USE_RUN_STATS) != 0U ) {
    runMap.subscribe();
  }
  if((useWhat & CM_USE_BYTE_HISTORY) != 0U ) {
    bhMap8B.subscribe();
    bhMap12B.subscribe();
  }
  order = 0;
  for( uint32_t i = 0; i < index; i++ ) {
    if(((validFlags >> (index - 1 - i)) & 1) != 0 ) {
      const int state = bitState[i] != nullptr ? *bitState[i] : 0;
      const int n0 = StateTable::next(state, 2);
      const int n1 = StateTable::next(state, 3);
      const int bitIsUncertain = int(n0 != 0 && n1 != 0);

      // predict from last byte(s) in context
      auto byteHistoryPtr = byteHistory[i];
      uint8_t byteState = byteHistoryPtr[-3];
      const uint8_t byte1 = byteHistoryPtr[1];
      const uint8_t byte2 = byteHistoryPtr[2];
      const uint8_t byte3 = byteHistoryPtr[3];
      const bool complete1 = (byteState >= 3) || (byteState >= 1 && shared->bitPosition == 0);
      const bool complete2 = (byteState >= 7) || (byteState >= 3 && shared->bitPosition == 0);
      const bool complete3 = (byteState >= 15) || (byteState >= 7 && shared->bitPosition == 0);
      if((useWhat & CM_USE_RUN_STATS) != 0U ) {
        const int bp = (0xFEA4U >> (shared->bitPosition << 1U)) & 3U; // {0}->0  {1}->1  {2,3,4}->2  {5,6,7}->3
        bool skipRunMap = true;
        if( complete1 ) {
          if(((byte1 + 256) >> (8 - shared->bitPosition)) == shared->c0 ) { // 1st candidate (last byte seen) matches
            const int predictedBit = (byte1 >> (7 - shared->bitPosition)) & 1U;
            const int byte1IsUncertain = static_cast<const int>(byte2 != byte1);
            const int runCount = byteHistoryPtr[0]; // 1..254
            m.add(stretch(runMap.p2(i, runCount << 4U | bp << 2U | byte1IsUncertain << 1 | predictedBit)) >> (1 + byte1IsUncertain));
            skipRunMap = false;
          } else if( complete2 && ((byte2 + 256) >> (8 - shared->bitPosition)) == shared->c0 ) { // 2nd candidate matches
            const int predictedBit = (byte2 >> (7 - shared->bitPosition)) & 1U;
            const int byte2IsUncertain = static_cast<const int>(byte3 != byte2);
            m.add(stretch(runMap.p2(i, bitIsUncertain << 1U | predictedBit)) >> (2 + byte2IsUncertain));
            skipRunMap = false;
          }
          // remark: considering the 3rd byte is not beneficial in most cases, except for some 8bpp images
        }
        if( skipRunMap ) {
          runMap.skip(i);
          m.add(0);
        }
      }
      // predict from bit context
      if( state == 0 ) {
        stateMap.skip(i);
        m.add(0);
        m.add(0);
        m.add(0);
        m.add(0);
      } else {
        const int p1 = stateMap.p2(i, state);
        const int st = (stretch(p1) * scale) >> 8;
        const int contextIsYoung = int(state <= 2);
        m.add(st >> contextIsYoung);
        m.add(((p1 - 2048) * scale) >> 9U);
        m.add((bitIsUncertain - 1) & st); // when both counts are nonzero add(0) otherwise add(st)
        const int p0 = 4095 - p1;
        m.add((((p1 & (-!n0)) - (p0 & (-!n1))) * scale) >> 10U);
        order++;
      }

      if((useWhat & CM_USE_BYTE_HISTORY) != 0U ) {
        const int bhBits = (((byte1 >> (7 - shared->bitPosition)) & 1)) | (((byte2 >> (7 - shared->bitPosition)) & 1) << 1) |
                           (((byte3 >> (7 - shared->bitPosition)) & 1) << 2);

        int bhState = 0; // 4 bit
        if( complete3 ) {
          bhState = 8U | (bhBits); //we have seen 3 bytes (at least)
        } else if( complete2 ) {
          bhState = 4U | (bhBits & 3U); //we have seen 2 bytes
        } else if( complete1 ) {
          bhState = 2U | (bhBits & 1U); //we have seen 1 byte only
        }
        //else new context (bhState=0)

        const uint8_t stateGroup = StateTable::group(state); //0..31
        m.add(stretch(bhMap8B.p2(i, bitIsUncertain << 7U | (bhState << 3U) | shared->bitPosition))
                      >> 2); // using bitIsUncertain is generally beneficial except for some 8bpp image (noticeable loss)
        m.add(stretch(bhMap12B.p2(i, stateGroup << 7U | (bhState << 3U) | shared->bitPosition)) >> 2U);
      }
    } else { //skipped context
      if((useWhat & CM_USE_RUN_STATS) != 0U ) {
        runMap.skip(i);
        m.add(0);
      }
      if((useWhat & CM_USE_BYTE_HISTORY) != 0U ) {
        bhMap8B.skip(i);
        m.add(0);
        bhMap12B.skip(i);
        m.add(0);
      }
      stateMap.skip(i);
      m.add(0);
      m.add(0);
      m.add(0);
      m.add(0);
    }
  }
}
