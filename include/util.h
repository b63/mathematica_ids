#ifndef UTIL_H
#define UTIL_H

#include <vector>
#include <memory>

struct Image {
    int *data;
    size_t w;
    size_t h;

    inline Image()
    : data{nullptr}, w {0}, h{0}
    {}

    inline Image(int *data, size_t w, size_t h)
        : data (data), w (w), h (h) 
        {}

    inline ~Image() {
        if (data) delete[] data;
    }
};

#endif
