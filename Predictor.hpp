#ifndef PAQ8PX_PREDICTOR_HPP
#define PAQ8PX_PREDICTOR_HPP

#include "file/FileDisk.hpp"

/**
 * A Predictor estimates the probability that the next bit of uncompressed data is 1.
 */
class Predictor {
    Shared shared;
    ModelStats stats;
    Models models;
    ContextModel contextModel;
    SSE sse;
    int pr; // next prediction, scaled by 12 bits (0-4095)

    void trainText(const char *const dictionary, int iterations) {
      NormalModel &normalModel = models.normalModel();
      WordModel &wordModel = models.wordModel();
      DummyMixer mDummy(&shared, NormalModel::MIXERINPUTS + WordModel::MIXERINPUTS, NormalModel::MIXERCONTEXTS + WordModel::MIXERCONTEXTS,
                        NormalModel::MIXERCONTEXTSETS + WordModel::MIXERCONTEXTSETS);
      stats.blockType = TEXT;
      assert(shared.buf.getpos() == 0 && stats.blpos == 0);
      FileDisk f;
      printf("Pre-training models with text...");
      OpenFromMyFolder::anotherFile(&f, dictionary);
      int c;
      int trainingByteCount = 0;
      while( iterations-- > 0 ) {
        f.setpos(0);
        c = SPACE;
        trainingByteCount = 0;
        do {
          trainingByteCount++;
          uint8_t c1 = c == NEW_LINE ? SPACE : c;
          if( c != CARRIAGE_RETURN ) {
            for( int bitPosition = 0; bitPosition < 8; bitPosition++ ) {
              normalModel.mix(mDummy); //update (train) model
#ifdef USE_TEXTMODEL
              wordModel.mix(mDummy); //update (train) model
#endif
              mDummy.p();
              shared.y = (c1 >> (7 - bitPosition)) & 1U;
              shared.update();
              updater.broadcastUpdate();
            }
          }
          // emulate a space before and after each word/expression
          // reset models in between
          if( c == NEW_LINE ) {
            normalModel.reset();
#ifdef USE_TEXTMODEL
            wordModel.reset();
#endif
            for( int bitPosition = 0; bitPosition < 8; bitPosition++ ) {
              normalModel.mix(mDummy); //update (train) model
#ifdef USE_TEXTMODEL
              wordModel.mix(mDummy); //update (train) model
#endif
              mDummy.p();
              shared.y = (c1 >> (7 - bitPosition)) & 1U;
              shared.update();
              updater.broadcastUpdate();
            }
          }
        } while((c = f.getchar()) != EOF);
      }
      normalModel.reset();
#ifdef USE_TEXTMODEL
      wordModel.reset();
#endif
      shared.reset();
      stats.reset();
      printf(" done [%s, %d bytes]\n", dictionary, trainingByteCount);
      f.close();
    }

    void trainExe() {
      ExeModel &exeModel = models.exeModel();
      DummyMixer dummyM(&shared, ExeModel::MIXERINPUTS, ExeModel::MIXERCONTEXTS, ExeModel::MIXERCONTEXTSETS);
      assert(shared.buf.getpos() == 0 && stats.blpos == 0);
      FileDisk f;
      printf("Pre-training x86/x64 model...");
      OpenFromMyFolder::myself(&f);
      int c = 0;
      int trainingByteCount = 0;
      do {
        trainingByteCount++;
        for( uint32_t bitPosition = 0; bitPosition < 8; bitPosition++ ) {
          exeModel.mix(dummyM); //update (train) model
          dummyM.p();
          shared.y = (c >> (7 - bitPosition)) & 1U;
          shared.update();
          updater.broadcastUpdate();
        }
      } while((c = f.getchar()) != EOF);
      printf(" done [%d bytes]\n", trainingByteCount);
      f.close();
      shared.reset();
      stats.reset();
    }

public:
    Predictor() : shared(), stats(), models(&shared, &stats), contextModel(&shared, &stats, models), sse(&shared, &stats), pr(2048) {
      shared.reset();
      shared.buf.setSize(MEM * 8);
      //initiate pre-training
      if( options & OPTION_TRAINTXT ) {
        trainText("english.dic", 3);
        trainText("english.exp", 1);
      }
      if( options & OPTION_TRAINEXE )
        trainExe();
    }

    /**
     * returns P(1) as a 12 bit number (0-4095).
     * @return the prediction
     */
    [[nodiscard]] int p() const { return pr; }

    /**
     * Trains the models with the actual bit (0 or 1).
     * @param y the actual bit
     */
    void update(uint8_t y) {
      stats.misses += stats.misses + ((pr >> 11U) != y);

      // update global context: pos, bitPosition, c0, c4, c8, buf
      shared.y = y;
      shared.update();
      // Broadcast to all current subscribers: y (and c0, c1, c4, etc) is known
      updater.broadcastUpdate();

      const uint8_t bitPosition = shared.bitPosition;
      const uint8_t c0 = shared.c0;
      const uint8_t characterGroup = (bitPosition > 0) ? asciiGroupC0[0][(1U << bitPosition) - 2 + (c0 & ((1U << bitPosition) - 1))] : 0;
      stats.Text.characterGroup = characterGroup;

      // predict
      pr = contextModel.p();

      // SSE Stage
      pr = sse.p(pr);
    }
};

#endif //PAQ8PX_PREDICTOR_HPP
