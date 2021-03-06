#include "ContextModel.hpp"

ContextModel::ContextModel(ModelStats *st, Models &models) : stats(st), models(models) {
  auto mf = new MixerFactory();
  m = mf->createMixer(1 + //bias
                      MatchModel::MIXERINPUTS + NormalModel::MIXERINPUTS + SparseMatchModel::MIXERINPUTS + SparseModel::MIXERINPUTS +
                      RecordModel::MIXERINPUTS + CharGroupModel::MIXERINPUTS + TextModel::MIXERINPUTS + WordModel::MIXERINPUTS +
                      IndirectModel::MIXERINPUTS + DmcForest::MIXERINPUTS + NestModel::MIXERINPUTS + XMLModel::MIXERINPUTS +
                      LinearPredictionModel::MIXERINPUTS + ExeModel::MIXERINPUTS,
                      MatchModel::MIXERCONTEXTS + NormalModel::MIXERCONTEXTS + SparseMatchModel::MIXERCONTEXTS +
                      SparseModel::MIXERCONTEXTS + RecordModel::MIXERCONTEXTS + CharGroupModel::MIXERCONTEXTS + TextModel::MIXERCONTEXTS +
                      WordModel::MIXERCONTEXTS + IndirectModel::MIXERCONTEXTS + DmcForest::MIXERCONTEXTS + NestModel::MIXERCONTEXTS +
                      XMLModel::MIXERCONTEXTS + LinearPredictionModel::MIXERCONTEXTS + ExeModel::MIXERCONTEXTS,
                      MatchModel::MIXERCONTEXTSETS + NormalModel::MIXERCONTEXTSETS + SparseMatchModel::MIXERCONTEXTSETS +
                      SparseModel::MIXERCONTEXTSETS + RecordModel::MIXERCONTEXTSETS + CharGroupModel::MIXERCONTEXTSETS +
                      TextModel::MIXERCONTEXTSETS + WordModel::MIXERCONTEXTSETS + IndirectModel::MIXERCONTEXTSETS +
                      DmcForest::MIXERCONTEXTSETS + NestModel::MIXERCONTEXTSETS + XMLModel::MIXERCONTEXTSETS +
                      LinearPredictionModel::MIXERCONTEXTSETS + ExeModel::MIXERCONTEXTSETS);
}

auto ContextModel::p() -> int {
  uint32_t &blockPosition = stats->blPos;
  // Parse block type and block size
  if( shared->bitPosition == 0 ) {
    --blockSize;
    blockPosition++;
    if( blockSize == -1 ) {
      nextBlockType = static_cast<BlockType>(shared->c1); //got blockType but don't switch (we don't have all the info yet)
      bytesRead = 0;
      readSize = true;
    } else if( blockSize < 0 ) {
      if( readSize ) {
        bytesRead |= int(shared->c1 & 0x7FU) << ((-blockSize - 2) * 7);
        if((shared->c1 >> 7U) == 0 ) {
          readSize = false;
          if( !hasInfo(nextBlockType)) {
            blockSize = bytesRead;
            if( hasRecursion(nextBlockType)) {
              blockSize = 0;
            }
            blockPosition = 0;
          } else {
            blockSize = -1;
          }
        }
      } else if( blockSize == -5 ) {
        blockSize = bytesRead;
        blockInfo = shared->c4;
        blockPosition = 0;
      }
    }

    if( blockPosition == 0 ) {
      blockType = nextBlockType; //got all the info - switch to next blockType
    }
    if( blockSize == 0 ) {
      blockType = DEFAULT;
    }

    stats->blockType = blockType;
  }

  m->add(256); //network bias

  MatchModel &matchModel = models.matchModel();
  matchModel.mix(*m);
  NormalModel &normalModel = models.normalModel();
  normalModel.mix(*m);

  // Test for special block types
  switch( blockType ) {
    case IMAGE1: {
      Image1BitModel &image1BitModel = Models::image1BitModel();
      image1BitModel.setParam(blockInfo);
      image1BitModel.mix(*m);
      break;
    }
    case IMAGE4: {
      Image4BitModel &image4BitModel = models.image4BitModel();
      image4BitModel.setParam(blockInfo);
      m->setScaleFactor(2048, 256);
      return image4BitModel.mix(*m), m->p();
    }
    case IMAGE8: {
      Image8BitModel &image8BitModel = models.image8BitModel();
      image8BitModel.setParam(blockInfo, 0, 0);
      m->setScaleFactor(2048, 128);
      return image8BitModel.mix(*m), m->p();
    }
    case IMAGE8GRAY: {
      Image8BitModel &image8BitModel = models.image8BitModel();
      image8BitModel.setParam(blockInfo, 1, 0);
      m->setScaleFactor(2048, 128);
      return image8BitModel.mix(*m), m->p();
    }
    case IMAGE24: {
      Image24BitModel &image24BitModel = models.image24BitModel();
      image24BitModel.setParam(blockInfo, 0, 0);
      m->setScaleFactor(1024, 128);
      return image24BitModel.mix(*m), m->p();
    }
    case IMAGE32: {
      Image24BitModel &image24BitModel = models.image24BitModel();
      image24BitModel.setParam(blockInfo, 1, 0);
      m->setScaleFactor(2048, 128);
      return image24BitModel.mix(*m), m->p();
    }
    case PNG8: {
      Image8BitModel &image8BitModel = models.image8BitModel();
      image8BitModel.setParam(blockInfo, 0, 1);
      m->setScaleFactor(2048, 128);
      return image8BitModel.mix(*m), m->p();
    }
    case PNG8GRAY: {
      Image8BitModel &image8BitModel = models.image8BitModel();
      image8BitModel.setParam(blockInfo, 1, 1);
      m->setScaleFactor(2048, 128);
      return image8BitModel.mix(*m), m->p();
    }
    case PNG24: {
      Image24BitModel &image24BitModel = models.image24BitModel();
      image24BitModel.setParam(blockInfo, 0, 1);
      m->setScaleFactor(1024, 128);
      return image24BitModel.mix(*m), m->p();
    }
    case PNG32: {
      Image24BitModel &image24BitModel = models.image24BitModel();
      image24BitModel.setParam(blockInfo, 1, 1);
      m->setScaleFactor(2048, 128);
      return image24BitModel.mix(*m), m->p();
    }
#ifndef DISABLE_AUDIOMODEL
    case AUDIO:
    case AUDIO_LE: {
      RecordModel &recordModel = models.recordModel();
      recordModel.mix(*m);
      if((blockInfo & 2U) == 0 ) {
        Audio8BitModel &audio8BitModel = models.audio8BitModel();
        audio8BitModel.setParam(blockInfo);
        m->setScaleFactor(1024, 128);
        return audio8BitModel.mix(*m), m->p();
      }
      Audio16BitModel &audio16BitModel = models.audio16BitModel();
      audio16BitModel.setParam(blockInfo);
      m->setScaleFactor(1024, 128);
      return audio16BitModel.mix(*m), m->p();

    }
#endif //DISABLE_AUDIOMODEL
    case JPEG: {
      JpegModel &jpegModel = models.jpegModel();
      m->setScaleFactor(1024, 256);
      if( jpegModel.mix(*m) != 0 ) {
        return m->p();
      }
    }
    case DEFAULT:
    case HDR:
    case FILECONTAINER:
    case EXE:
    case CD:
    case ZLIB:
    case BASE64:
    case GIF:
    case TEXT:
    case TEXT_EOL:
    case RLE:
    case LZW:
      break;
  }

  normalModel.mixPost(*m);

  if( blockType != IMAGE1 ) {
    SparseMatchModel &sparseMatchModel = models.sparseMatchModel();
    sparseMatchModel.mix(*m);
    SparseModel &sparseModel = models.sparseModel();
    sparseModel.mix(*m);
    RecordModel &recordModel = models.recordModel();
    recordModel.mix(*m);
    CharGroupModel &charGroupModel = models.charGroupModel();
    charGroupModel.mix(*m);
#ifndef DISABLE_TEXTMODEL
    TextModel &textModel = models.textModel();
    textModel.mix(*m);
    WordModel &wordModel = models.wordModel();
    wordModel.mix(*m);
#endif //DISABLE_TEXTMODEL
    IndirectModel &indirectModel = models.indirectModel();
    indirectModel.mix(*m);
    DmcForest &dmcForest = models.dmcForest();
    dmcForest.mix(*m);
    NestModel &nestModel = models.nestModel();
    nestModel.mix(*m);
    XMLModel &xmlModel = models.xmlModel();
    xmlModel.mix(*m);
    if( blockType != TEXT && blockType != TEXT_EOL ) {
      LinearPredictionModel &linearPredictionModel = Models::linearPredictionModel();
      linearPredictionModel.mix(*m);
      ExeModel &exeModel = models.exeModel();
      exeModel.mix(*m);
    }
  }

  m->setScaleFactor(1024, 128);
  return m->p();
}

ContextModel::~ContextModel() {
  delete m;
}