#pragma once
#include <fishnet/GISFile.hpp>
#include <fishnet/IGeometry.hpp>
#include <fishnet/OGRGeometryAdapter.hpp>
#include <fishnet/VectorIO.hpp>

/**
 * @brief Reads the settlement polygons of a vector file, observing every layer it reads
 *
 * A data source hands out OGR backed geometries, which are brought into S, the shape the workflow
 * computes on. Where S is the OGR backed geometry itself nothing is copied; where it is a fishnet
 * value type the layer is converted once per file, which pays off for the graph construction and
 * clustering stages, as those walk the segments of every settlement over and over.
 *
 * The callback runs on each layer that was read successfully, together with the file it came from,
 * which is what lets callers pick up the spatial reference or keep book on the input files without
 * reading them a second time.
 */
template<fishnet::geometry::Shape S>
class SettlementLayerReader {
public:
    using geometry_type = S;
    using file_type = fishnet::AbstractVectorFile;

private:
    /// the geometry a data source is read into, before the layer is brought into S
    using source_type = fishnet::geometry::OGRPolygonAdapter;

    fishnet::util::BiFunction_t<fishnet::VectorLayer<S>, file_type, void> onRead;

    static fishnet::VectorLayer<S> bringIntoShapeType(const fishnet::VectorLayer<source_type> & source) {
        if constexpr (std::same_as<S, source_type>) {
            return source;
        } else {
            static_assert(std::same_as<S, std::remove_cvref_t<decltype(std::declval<const source_type &>().toNative())>>,
                "The settlement shape is neither the geometry read from the data source nor the fishnet value type it converts into");
            return fishnet::VectorIO::nativeCopy(source);
        }
    }

public:
    explicit SettlementLayerReader(fishnet::util::BiFunction<fishnet::VectorLayer<S>, file_type, void> auto && onRead)
    : onRead(std::forward<decltype(onRead)>(onRead)) {}

    fishnet::Either<fishnet::VectorLayer<S>, std::string> operator()(const file_type & file) const {
        auto source = fishnet::VectorIO::tryRead<source_type>(file);
        if(not source)
            return std::unexpected(source.error());
        auto layer = bringIntoShapeType(source.value());
        onRead(layer, file);
        return layer;
    }
};
