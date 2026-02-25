#include <fishnet/Fishnet.hpp>
#include <CLI/CLI.hpp>
#include <optional>
#include <ogr_geometry.h>
using MultiPolygon_t = fishnet::geometry::MultiPolygon<fishnet::geometry::Polygon<double>>;
using Polygon_t = fishnet::geometry::SimplePolygon<double>;
using Number_t= typename MultiPolygon_t::numeric_type;


auto concaveHull(const MultiPolygon_t & multiPolygon,double min_alpha){
    OGRMultiPoint multiPoint;
    for(const auto & polygon : multiPolygon.getPolygons()) {
        for(const auto & point : polygon.getBoundary().getPoints()) {
            multiPoint.addGeometry(std::make_unique<OGRPoint>(point.x, point.y));
        }
    }
    /*  alpha near 0 (sparse buildings relative to bounding box) causes ConcaveHull to
        return nullptr or a MultiPolygon that cannot be cast to OGRPolygon.
        Clamp to a minimum so the hull stays computable. */

    double computed_alpha = sqrt(1.0 / static_cast<double>(fishnet::util::size(multiPolygon.getPolygons())));
    double alpha = std::max(min_alpha, computed_alpha);

    OGRGeometry* hullGeometryPtr = multiPoint.ConcaveHull(alpha, true);
    // If ConcaveHull failed or returned a non-polygon geometry, try the convex hull as a fallback
    if (hullGeometryPtr == nullptr || hullGeometryPtr->toPolygon() == nullptr) {
        OGRGeometry* fallback = multiPoint.ConvexHull();
        if (hullGeometryPtr) { OGRGeometryFactory::destroyGeometry(hullGeometryPtr); }
        hullGeometryPtr = fallback;
    }
    auto polygon = hullGeometryPtr == nullptr || hullGeometryPtr->toPolygon() == nullptr
        ? std::nullopt
        : fishnet::OGRGeometryAdapter::fromOGR(*hullGeometryPtr->toPolygon()->getExteriorRing());
    OGRGeometryFactory::destroyGeometry(hullGeometryPtr);
    return std::make_pair(polygon,computed_alpha);
}

int main(int argc, char* argv[]) {
    using namespace fishnet::geometry;
    CLI::App app{"AfricapolisPolygonOutline"};
    std::string inputFilename;// = "/home/lolo/Desktop/uganda_lira_100m/Uganda_original_102023_Lira_Africapolis.shp";
    double min_alpha=.3;
    app.add_option("-i,--input", inputFilename, "Path to input shape file")->required()->check(CLI::ExistingFile);
    app.add_option("--alpha",min_alpha, "Alpha parameter for concave hull")->required()->check(CLI::Range(0.0,1.0));
    CLI11_PARSE(app, argc, argv);
    fishnet::Shapefile inputFile(inputFilename);
    auto inputLayer = fishnet::VectorIO::read<MultiPolygon_t>(inputFile);
    auto outputLayer = fishnet::VectorIO::emptyCopy<Polygon_t>(inputLayer);
    auto field = outputLayer.addDoubleField("Alpha").value_or_throw();
    for(const auto & multiPolygonFeature: inputLayer.getFeatures()) {
        auto [resultGeometry, alpha_value] = concaveHull(multiPolygonFeature.getGeometry(),min_alpha);
        if(not resultGeometry) {
            std::cout << "No concave hull found for feature: " << multiPolygonFeature.getGeometry() << std::endl;
            continue;
        }
        auto feature = fishnet::Feature<Polygon_t>(resultGeometry.value());
        feature.copyAttributes(multiPolygonFeature);
        feature.setAttribute(field, alpha_value);
        outputLayer.addFeature(std::move(feature));
    }
    fishnet::Shapefile outputFile = {fishnet::util::PathHelper::appendToFilename(inputFile.getPath(), "_concave_hull").filename()};
    fishnet::VectorIO::overwrite(outputLayer, outputFile);
    return 0;
}