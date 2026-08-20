#include <fishnet/Fishnet.hpp>
#include <spdlog/spdlog.h>
#include <fishnet/Task.hpp>
#include <CLI/CLI.hpp>
#include <ogr_geometry.h>

using MultiPolygon_t = fishnet::geometry::MultiPolygon<fishnet::geometry::Polygon<double>>;
using Polygon_t = fishnet::geometry::SimplePolygon<double>;
using Number_t= typename MultiPolygon_t::numeric_type;

class OutlineVisualization: public Task{
private:
    double min_alpha;
    double buffer_distance;

    /**
     * @brief Extracts the exterior ring of a Polygon as a fishnet SimplePolygon.
     *        If the geometry is a MultiPolygon, falls back to its convex hull.
     * @param geom OGRGeometry pointer (may be null)
     * @return Optional fishnet SimplePolygon
     */
    static std::optional<Polygon_t> extractPolygon(const OGRGeometry * geom) {
        if (geom == nullptr) return std::nullopt;
        if (geom->toPolygon() != nullptr) {
            return fishnet::OGRGeometryAdapter::fromOGR(*geom->toPolygon()->getExteriorRing());
        }
        if (geom->toMultiPolygon() != nullptr) {
            OGRGeometry * hull = geom->ConvexHull();
            std::optional<Polygon_t> result;
            if (hull != nullptr && hull->toPolygon() != nullptr) {
                result = fishnet::OGRGeometryAdapter::fromOGR(*hull->toPolygon()->getExteriorRing());
            }
            OGRGeometryFactory::destroyGeometry(hull);
            return result;
        }
        return std::nullopt;
    }

    /**
     * @brief Creates an Azimuthal Equidistant OGRSpatialReference centered on the given lon/lat.
     * @param lon Center longitude (degrees)
     * @param lat Center latitude (degrees)
     * @return OGRSpatialReference with Azimuthal Equidistant projection
     */
    static OGRSpatialReference createAzimuthalEquidistant(double lon, double lat) {
        OGRSpatialReference sr;
        sr.SetAE(lat, lon, 0.0, 0.0);
        return sr;
    }

    /**
     * @brief Buffer an OGRGeometry by a distance in meters using azimuthal equidistant reprojection.
     *        Reprojects to a local azimuthal CRS, buffers, then reprojects back to the original CRS.
     * @param geom OGRGeometry to buffer (in WGS84 / degrees)
     * @param distanceMeters Buffer distance in meters
     * @param lon Center longitude for the projection
     * @param lat Center latitude for the projection
     * @return A new OGRGeometry pointer (caller must destroy), or nullptr on failure.
     */
    static OGRGeometry * bufferInMeters(const OGRGeometry * geom, double distanceMeters, double lon, double lat) {
        if (geom == nullptr) return nullptr;

        OGRSpatialReference wgs84;
        wgs84.importFromEPSG(4326);

        OGRSpatialReference azEq = createAzimuthalEquidistant(lon, lat);

        OGRCoordinateTransformation * toAzEq = OGRCreateCoordinateTransformation(&wgs84, &azEq);
        OGRCoordinateTransformation * toWgs84 = OGRCreateCoordinateTransformation(&azEq, &wgs84);

        if (toAzEq == nullptr || toWgs84 == nullptr) {
            OCTDestroyCoordinateTransformation(toAzEq);
            OCTDestroyCoordinateTransformation(toWgs84);
            return nullptr;
        }

        OGRGeometry * cloned = geom->clone();
        if (cloned == nullptr) {
            OCTDestroyCoordinateTransformation(toAzEq);
            OCTDestroyCoordinateTransformation(toWgs84);
            return nullptr;
        }

        if (cloned->transform(toAzEq) != OGRERR_NONE) {
            OGRGeometryFactory::destroyGeometry(cloned);
            OCTDestroyCoordinateTransformation(toAzEq);
            OCTDestroyCoordinateTransformation(toWgs84);
            return nullptr;
        }

        OGRGeometry * buffered = cloned->Buffer(distanceMeters, 30);
        OGRGeometryFactory::destroyGeometry(cloned);
        OCTDestroyCoordinateTransformation(toAzEq);

        if (buffered == nullptr) {
            OCTDestroyCoordinateTransformation(toWgs84);
            return nullptr;
        }

        if (buffered->transform(toWgs84) != OGRERR_NONE) {
            OGRGeometryFactory::destroyGeometry(buffered);
            OCTDestroyCoordinateTransformation(toWgs84);
            return nullptr;
        }

        OCTDestroyCoordinateTransformation(toWgs84);
        return buffered;
    }

public: 
    OutlineVisualization(double min_alpha, double buffer_distance)
        : Task("OutlineVisualization"), min_alpha(min_alpha), buffer_distance(buffer_distance) {}

    void operator()(const std::filesystem::path & inputFilename) const {
        fishnet::AbstractVectorFile inputFile(inputFilename);
        auto inputLayer = fishnet::VectorIO::read<MultiPolygon_t>(inputFile);
        auto outputLayer = fishnet::VectorIO::emptyCopy<Polygon_t>(inputLayer);
        auto field = outputLayer.addDoubleField("Alpha").value_or_throw();
        for(const auto & multiPolygonFeature: inputLayer.getFeatures()) {
            const auto & geometry = multiPolygonFeature.getGeometry();

            // Pass single-polygon features through unchanged
            if (fishnet::util::size(geometry.getPolygons()) < 2) {
                auto singlePoly = geometry.getPolygons().front();
                auto feature = fishnet::Feature<Polygon_t>(singlePoly);
                feature.copyAttributes(multiPolygonFeature);
                feature.setAttribute(field, 0.0);
                outputLayer.addFeature(std::move(feature));
                continue;
            }

            // Step c: Concave hull of the settlement (computed in OGR space)
            OGRMultiPoint multiPoint;
            for(const auto & polygon : geometry.getPolygons()) {
                for(const auto & point : polygon.getBoundary().getPoints()) {
                    multiPoint.addGeometry(std::make_unique<OGRPoint>(point.x, point.y));
                }
            }
            double computed_alpha = sqrt(1.0 / static_cast<double>(fishnet::util::size(geometry.getPolygons())));
            double alpha = std::max(min_alpha, computed_alpha);

            OGRGeometry* hullPtr = multiPoint.ConcaveHull(alpha, true);
            if (hullPtr == nullptr || hullPtr->toPolygon() == nullptr) {
                OGRGeometry* fallback = multiPoint.ConvexHull();
                if (hullPtr) { OGRGeometryFactory::destroyGeometry(hullPtr); }
                hullPtr = fallback;
            }
            if (hullPtr == nullptr) {
                spdlog::warn("No concave hull found for feature {}", geometry.toString());
                continue;
            }

            // Compute centroid for azimuthal equidistant projection center
            auto centroid = geometry.centroid();
            double lon = centroid.x;
            double lat = centroid.y;

            // Step d: Buffer the building polygons by the configured distance in meters
            auto ogrMulti = fishnet::OGRGeometryAdapter::toOGR(geometry);
            OGRGeometry * bufferedPtr = bufferInMeters(ogrMulti.get(), buffer_distance, lon, lat);
            if (bufferedPtr == nullptr) {
                spdlog::warn("Buffering failed for feature {}; using concave hull only", geometry.toString());
                auto resultGeom = extractPolygon(hullPtr);
                OGRGeometryFactory::destroyGeometry(hullPtr);
                if (not resultGeom) continue;
                auto feature = fishnet::Feature<Polygon_t>(resultGeom.value());
                feature.copyAttributes(multiPolygonFeature);
                feature.setAttribute(field, alpha);
                outputLayer.addFeature(std::move(feature));
                continue;
            }

            // Step e: Merge settlement polygon (concave hull) and buffered building polygons (OGR space)
            OGRGeometry * mergedPtr = hullPtr->Union(bufferedPtr);
            OGRGeometryFactory::destroyGeometry(hullPtr);
            OGRGeometryFactory::destroyGeometry(bufferedPtr);

            auto mergedGeometry = extractPolygon(mergedPtr);
            OGRGeometryFactory::destroyGeometry(mergedPtr);

            if (not mergedGeometry) {
                spdlog::warn("Union failed for feature {}; skipping", geometry.toString());
                continue;
            }

            auto feature = fishnet::Feature<Polygon_t>(mergedGeometry.value());
            feature.copyAttributes(multiPolygonFeature);
            feature.setAttribute(field, alpha);
            outputLayer.addFeature(std::move(feature));
        }
        auto outputPath = fishnet::util::PathHelper::appendToFilename(inputFile.getPath(), "_concave_hull").filename();
        fishnet::VectorIO::overwrite(outputLayer, fishnet::AbstractVectorFile(outputPath));
    }
};


int main(int argc, char* argv[]) {
    using namespace fishnet::geometry;
    CLI::App app{"AfricapolisPolygonOutline"};
    std::string inputFilename;
    double min_alpha=.3;
    double buffer_distance = 30.0;
    app.add_option("-i,--input", inputFilename, "Path to input shape file")->required()->check(CLI::ExistingFile);
    app.add_option("--alpha",min_alpha, "Alpha parameter for concave hull")->required()->check(CLI::Range(0.0,1.0));
    app.add_option("--buffer", buffer_distance, "Buffer distance in meters for building polygons [default: 30.0]")->check(CLI::PositiveNumber);
    CLI11_PARSE(app, argc, argv);
    OutlineVisualization outlineVisualization(min_alpha, buffer_distance);
    outlineVisualization(inputFilename);
    return 0;
}
