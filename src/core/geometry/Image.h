#pragma once

class QImage;

namespace fez
{

// True when the image has an alpha channel and at least one pixel with alpha below 1 (pixelColor(x, y).alphaF()).
// Images without alpha channel and null images give false.
bool hasTranslucentPixels(const QImage& image);

} // namespace fez
