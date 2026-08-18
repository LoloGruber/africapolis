#pragma once
#include "fishnet/GISFile.hpp"
#include <fishnet/GeometryObject.hpp>
#include <fishnet/VectorIO.hpp>

template<fishnet::geometry::GeometryObject G>
class ObservableVectorFileReader {
public:
    using geometry_type = G;
    using file_type = fishnet::AbstractVectorFile;

private:
    OGRSpatialReference spatialRef;
    fishnet::util::Consumer_t<fishnet::VectorLayer<G>> onSuccess;
public:
    ObservableVectorFileReader(fishnet::util::Consumer<fishnet::VectorLayer<G>> auto && onSuccess)
    : onSuccess(std::move(onSuccess)) {}

    fishnet::Either<fishnet::VectorLayer<G>,std::string> operator()(const file_type & file) const {
        auto layer = fishnet::VectorIO::tryRead<G>(static_cast<const fishnet::AbstractVectorFile&>(file));
        if(layer)
            onSuccess(layer.value());
        return layer;
    }

    OGRSpatialReference getSpatialReference() const noexcept {
        return spatialRef;
    }
};
