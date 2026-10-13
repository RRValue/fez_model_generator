#pragma once

#include "model/Geometry.h"

#include "writer/Writer.h"

#include <QtCore/QString>

class GeometryWriter : public Writer
{
public:
    using Writer::Writer;

    void writeObj(const Geometry& geometry);
};
