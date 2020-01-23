#ifndef PAQ8PX_FILENAME_HPP
#define PAQ8PX_FILENAME_HPP

#include "fileUtils.hpp"
#include "../String.hpp"

/**
 * A class to represent a filename
 */
class FileName : public String {
public:
    explicit FileName(const char *s = "");
    [[nodiscard]] auto lastSlashPos() const -> int;
    void keepFilename();
    void keepPath();

    /**
     * prepare path string for screen output
     */
    void replaceSlashes();
};


#endif //PAQ8PX_FILENAME_HPP
