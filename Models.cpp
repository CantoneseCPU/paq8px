#include "Models.hpp"

Models::Models(ModelStats *st) : stats(st) {}

auto Models::normalModel() -> NormalModel & {
  static NormalModel instance {stats, shared->mem * 32};
  return instance;
}

auto Models::dmcForest() -> DmcForest & {
  static DmcForest instance {shared->mem};
  return instance;
}

auto Models::charGroupModel() -> CharGroupModel & {
  static CharGroupModel instance {shared->mem / 2};
  return instance;
}

auto Models::recordModel() -> RecordModel & {
  static RecordModel instance {stats, shared->mem * 2};
  return instance;
}

auto Models::sparseModel() -> SparseModel & {
  static SparseModel instance {shared->mem * 2};
  return instance;
}

auto Models::matchModel() -> MatchModel & {
  static MatchModel instance {stats, shared->mem * 4};
  return instance;
}

auto Models::sparseMatchModel() -> SparseMatchModel & {
  static SparseMatchModel instance {shared->mem};
  return instance;
}

auto Models::indirectModel() -> IndirectModel & {
  static IndirectModel instance {shared->mem};
  return instance;
}

#ifdef USE_TEXTMODEL

TextModel &Models::textModel() {
  static TextModel instance {stats, shared->mem * 16};
  return instance;
}

WordModel &Models::wordModel() {
  static WordModel instance {stats, shared->mem * 16};
  return instance;
}

#else

auto wordModel() -> WordModel & {
      static WordModel instance {};
      return instance;
    }

#endif //USE_TEXTMODEL

auto Models::nestModel() -> NestModel & {
  static NestModel instance {shared->mem};
  return instance;
}

auto Models::xmlModel() -> XMLModel & {
  static XMLModel instance {shared->mem / 4};
  return instance;
}

auto Models::exeModel() -> ExeModel & {
  static ExeModel instance {stats, shared->mem * 4};
  return instance;
}

auto Models::linearPredictionModel() -> LinearPredictionModel & {
  static LinearPredictionModel instance {};
  return instance;
}

auto Models::jpegModel() -> JpegModel & {
  static JpegModel instance {shared->mem};
  return instance;
}

auto Models::image24BitModel() -> Image24BitModel & {
  static Image24BitModel instance {stats, shared->mem * 4};
  return instance;
}

auto Models::image8BitModel() -> Image8BitModel & {
  static Image8BitModel instance {stats, shared->mem * 4};
  return instance;
}

auto Models::image4BitModel() -> Image4BitModel & {
  static Image4BitModel instance {shared->mem / 2};
  return instance;
}

auto Models::image1BitModel() -> Image1BitModel & {
  static Image1BitModel instance {};
  return instance;
}

#ifdef USE_AUDIOMODEL

Audio8BitModel &Models::audio8BitModel() {
  static Audio8BitModel instance {stats};
  return instance;
}

Audio16BitModel &Models::audio16BitModel() {
  static Audio16BitModel instance {stats};
  return instance;
}

#endif //USE_AUDIOMODEL