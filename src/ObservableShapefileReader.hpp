#include <fishnet/GeometryObject.hpp>
#include <fishnet/VectorIO.hpp>

template<fishnet::geometry::GeometryObject G>
class ObservableShapefileReader {
public:
    using geometry_type = G;
    using file_type = fishnet::Shapefile;

private:
    OGRSpatialReference spatialRef;
    fishnet::util::Consumer_t<fishnet::VectorLayer<G>> onSuccess;
public:
    ObservableShapefileReader(fishnet::util::Consumer<fishnet::VectorLayer<G>> auto && onSuccess)
    : onSuccess(std::move(onSuccess)) {}

    fishnet::Either<fishnet::VectorLayer<G>,std::string> operator()(const fishnet::Shapefile & shapefile) const {
        auto layer = fishnet::VectorIO::tryRead(fishnet::ShapefileReader<G>{},shapefile);
        if(layer)
            onSuccess(layer.value());
        return layer;
    }

    OGRSpatialReference getSpatialReference() const noexcept {
        return spatialRef;
    }
};