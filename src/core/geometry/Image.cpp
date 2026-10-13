#include "geometry/Image.h"

#include <QtGui/QColor>
#include <QtGui/QImage>

namespace fez
{

bool hasTranslucentPixels(const QImage& image)
{
    if(!image.hasAlphaChannel())
        return false;

    for(int x = 0; x < image.width(); x++)
        for(int y = 0; y < image.height(); y++)
            if(image.pixelColor(x, y).alphaF() < 1.0f)
                return true;

    return false;
}

} // namespace fez
