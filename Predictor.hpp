#ifndef PAQ8PX_PREDICTOR_HPP
#define PAQ8PX_PREDICTOR_HPP

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

    void trainText(const char *const Dictionary, int Iterations) {
      NormalModel &normalModel = models.normalModel();
      WordModel &wordModel = models.wordModel();
      DummyMixer m_dummy(&shared, normalModel.MIXERINPUTS + wordModel.MIXERINPUTS, normalModel.MIXERCONTEXTS + wordModel.MIXERCONTEXTS,
                         normalModel.MIXERCONTEXTSETS + wordModel.MIXERCONTEXTSETS);
      assert(shared.buf.getpos() == 0 && stats.blpos == 0);
      FileDisk f;
      printf("Pre-training models with text...");
      OpenFromMyFolder::anotherFile(&f, Dictionary);
      int c;
      int trainingByteCount = 0;
      while( Iterations-- > 0 ) {
        f.setpos(0);
        c = SPACE;
        trainingByteCount = 0;
        do {
          trainingByteCount++;
          uint8_t c1 = c == NEW_LINE ? SPACE : c;
          if( c != CARRIAGE_RETURN ) {
            for( int bpos = 0; bpos < 8; bpos++ ) {
              normalModel.mix(m_dummy); //update (train) model
#ifdef USE_TEXTMODEL
              wordModel.mix(m_dummy); //update (train) model
#endif
              m_dummy.p();
              shared.y = (c1 >> (7 - bpos)) & 1;
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
            for( int bpos = 0; bpos < 8; bpos++ ) {
              normalModel.mix(m_dummy); //update (train) model
#ifdef USE_TEXTMODEL
              wordModel.mix(m_dummy); //update (train) model
#endif
              m_dummy.p();
              shared.y = (c1 >> (7 - bpos)) & 1;
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
      printf(" done [%s, %d bytes]\n", Dictionary, trainingByteCount);
      f.close();
    }

    void trainExe() {
      ExeModel &exeModel = models.exeModel();
      DummyMixer dummy_m(&shared, exeModel.MIXERINPUTS, exeModel.MIXERCONTEXTS, exeModel.MIXERCONTEXTSETS);
      assert(shared.buf.getpos() == 0 && stats.blpos == 0);
      FileDisk f;
      printf("Pre-training x86/x64 model...");
      OpenFromMyFolder::myself(&f);
      int c = 0;
      int trainingByteCount = 0;
      do {
        trainingByteCount++;
        for( int bpos = 0; bpos < 8; bpos++ ) {
          exeModel.mix(dummy_m); //update (train) model
          dummy_m.p();
          shared.y = (c >> (7 - bpos)) & 1;
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
    int p() const { return pr; }

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

      const uint8_t bpos = shared.bitPosition;
      const uint8_t c0 = shared.c0;
      const uint8_t chargrp = (bpos > 0) ? AsciiGroupC0[0][(1U << bpos) - 2 + (c0 & ((1U << bpos) - 1))] : 0;
      stats.Text.chargrp = chargrp;

      // predict
      pr = contextModel.p();

      // SSE Stage
      pr = sse.p(pr);
    }
};

#endif //PAQ8PX_PREDICTOR_HPP
