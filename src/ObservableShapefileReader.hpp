#pragma once
#include <fishnet/GeometryObject.hpp>
#include <fishnet/VectorIO.hpp>
#include <filesystem>

template<fishnet::VectorGISFile F, fishnet::geometry::GeometryObject G>
class GenericVectorReader {
public:
    using geometry_type = G;
    using file_type = F;

private:
    OGRSpatialReference spatialRef;
    fishnet::util::Consumer_t<fishnet::VectorLayer<G>> onSuccess;
public:
    GenericVectorReader(fishnet::util::Consumer<fishnet::VectorLayer<G>> auto && onSuccess)
    : onSuccess(std::move(onSuccess)) {}

    fishnet::Either<fishnet::VectorLayer<G>,std::string> operator()(const F & file) const {
        auto layer = fishnet::VectorIO::tryRead<G>(static_cast<const fishnet::AbstractVectorFile&>(file));
        if(layer)
            onSuccess(layer.value());
        return layer;
    }

    OGRSpatialReference getSpatialReference() const noexcept {
        return spatialRef;
    }
};

template<typename Func>
auto withVectorFileType(const std::string & path, Func && func) {
    auto ext = std::filesystem::path(path).extension().string();
    if(ext == ".gpkg") {
        return func.template operator()<fishnet::GeoPackage>();
    }
    return func.template operator()<fishnet::Shapefile>();
}
