#ifndef PAQ8PX_MODELS_HPP
#define PAQ8PX_MODELS_HPP

#include "text/TextModel.hpp"
#include "ModelStats.hpp"
#include "model/Audio16BitModel.hpp"
#include "model/Audio8BitModel.hpp"
#include "model/CharGroupModel.hpp"
#include "model/DmcForest.hpp"
#include "model/ExeModel.hpp"
#include "model/Image1BitModel.hpp"
#include "model/Image24BitModel.hpp"
#include "model/Image4BitModel.hpp"
#include "model/Image8BitModel.hpp"
#include "model/IndirectModel.hpp"
#include "model/JpegModel.hpp"
#include "model/LinearPredictionModel.hpp"
#include "model/MatchModel.hpp"
#include "model/NestModel.hpp"
#include "model/NormalModel.hpp"
#include "model/RecordModel.hpp"
#include "model/SparseMatchModel.hpp"
#include "model/SparseModel.hpp"
#include "model/WordModel.hpp"
#include "model/XMLModel.hpp"

/**
 * This is a factory class for lazy object creation for models.
 * Objects created within this class are instantiated on first use and guaranteed to be destroyed.
 */
class Models {
private:
    Shared *shared = Shared::getInstance();
    ModelStats *stats; //read-write
public:
    explicit Models(ModelStats *st);
    auto normalModel() -> NormalModel &;
    auto dmcForest() -> DmcForest &;
    auto charGroupModel() -> CharGroupModel &;
    auto recordModel() -> RecordModel &;
    auto sparseModel() -> SparseModel &;
    auto matchModel() -> MatchModel &;
    auto sparseMatchModel() -> SparseMatchModel &;
    auto indirectModel() -> IndirectModel &;
    auto textModel() -> TextModel &;
    auto wordModel() -> WordModel &;
    auto nestModel() -> NestModel &;
    auto xmlModel() -> XMLModel &;
    auto exeModel() -> ExeModel &;
    static auto linearPredictionModel() -> LinearPredictionModel &;
    auto jpegModel() -> JpegModel &;
    auto image24BitModel() -> Image24BitModel &;
    auto image8BitModel() -> Image8BitModel &;
    auto image4BitModel() -> Image4BitModel &;
    static auto image1BitModel() -> Image1BitModel &;
#ifdef USE_AUDIOMODEL
    auto audio8BitModel() -> Audio8BitModel &;
    auto audio16BitModel() -> Audio16BitModel &;
#endif //USE_AUDIOMODEL
};

#endif //PAQ8PX_MODELS_HPP
