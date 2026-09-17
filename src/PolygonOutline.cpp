#include "fishnet/CollectionConcepts.hpp"
#include "fishnet/Feature.hpp"
#include "fishnet/OGRGeometryAdapter.hpp"
#include "fishnet/Vec2D.hpp"
#include "fishnet/VectorLayer.hpp"
#include <algorithm>
#include <expected>
#include <fishnet/Fishnet.hpp>
#include <ogr_spatialref.h>
#include <spdlog/spdlog.h>
#include <fishnet/Task.hpp>
#include <CLI/CLI.hpp>
#include <ogr_geometry.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "AfricapolisConstants.hpp"

/*
 * Every geometry of this stage is read from a data source, handed to OGR for buffering,
 * transformation and unioning, and written straight back out. None of it is walked segment by
 * segment, so the OGR backed adapters are used throughout and nothing is converted either way.
 */
using MSTEdge_t = fishnet::geometry::OGRPolygonAdapter;
using SettlementShape_t = fishnet::geometry::OGRPolygonAdapter;
using ResultShape_t = fishnet::geometry::OGRPolygonAdapter;

class SettlementVisualization: public Task{
private:
    double initialBufferDistance;
    double targetBufferDistance;

    /**
     * @brief Creates an Azimuthal Equidistant OGRSpatialReference centered on the given lon/lat.
     * @param lon Center longitude (degrees)
     * @param lat Center latitude (degrees)
     * @return OGRSpatialReference with Azimuthal Equidistant projection
     */
    static OGRSpatialReference createAzimuthalEquidistant(fishnet::util::forward_range_of<fishnet::Feature<SettlementShape_t>> auto && cluster) {
        auto polygons = cluster | std::views::transform([](const auto & settlement) -> const SettlementShape_t & { return settlement.getGeometry(); });
        double total_area = std::ranges::fold_left(polygons, 0.0, [](double current, const auto & polygon){ return current + polygon.area(); });
        auto centroid = std::ranges::fold_left(polygons, fishnet::geometry::Vec2DReal(), [total_area](const auto & current, const auto & polygon){ return current + polygon.centroid() * (polygon.area()/ total_area); });
        OGRSpatialReference sr;
        sr.SetAE(centroid.y, centroid.x, 0.0, 0.0);
        return sr;
    }

    std::unordered_map<size_t,std::vector<fishnet::Feature<SettlementShape_t>>> clusterSettlements(fishnet::VectorLayer<SettlementShape_t> && settlements) const {
        std::unordered_map<size_t,std::vector<fishnet::Feature<SettlementShape_t>>> clusters;
        auto clusterIDField = settlements.getSizeField(Africapolis::CLUSTER_ID_FIELD).value_or_throw();
        for(auto && settlement : settlements.getFeatures()){
            size_t clusterID = settlement.getAttribute(clusterIDField).value_or_throw();
            clusters[clusterID].emplace_back(std::move(settlement));
        }
        return clusters;
    }
    std::unordered_map<size_t, std::vector<MSTEdge_t>> mstEdges(const fishnet::AbstractVectorFile & mstFile) const {
        auto mstLayer = fishnet::VectorIO::read<MSTEdge_t>(mstFile);
        auto fromField = mstLayer.getSizeField(Africapolis::FROM_ID_FIELD).value_or_throw();
        auto toField = mstLayer.getSizeField(Africapolis::TO_ID_FIELD).value_or_throw();
        std::unordered_map<size_t, std::vector<MSTEdge_t>> nodesToFeature;
        for (auto && [idx,feature] : std::move(mstLayer).getFeatures() | std::views::enumerate) {
            size_t fromID = feature.getAttribute(fromField).value_or_throw();
            size_t toID = feature.getAttribute(toField).value_or_throw();
            nodesToFeature[fromID].push_back(feature.getGeometry());
            // an edge is incident to two settlements, so the second insertion is the last use of
            // the feature and takes its geometry instead of cloning it again
            nodesToFeature[toID].push_back(std::move(feature).getGeometry());
        }
        return nodesToFeature;
    }

    struct VisualizeCluster {
        double initialBufferDistance;
        double targetBufferDistance;
        OGRSpatialReference metric;
        OGRCoordinateTransformation * toMetric; 
        OGRCoordinateTransformation * toSrc; 

        VisualizeCluster(double initialBufferDistance, double targetBufferDistance, OGRSpatialReference && metric, OGRSpatialReference const & src)
            : initialBufferDistance(initialBufferDistance), targetBufferDistance(targetBufferDistance), metric(std::move(metric)) 
        {
            this->toMetric = OGRCreateCoordinateTransformation(&src, &this->metric);
            this->toSrc = OGRCreateCoordinateTransformation(&this->metric, &src);
        }

        ~VisualizeCluster() {
            OCTDestroyCoordinateTransformation(toMetric);
            OCTDestroyCoordinateTransformation(toSrc);
        }

        fishnet::Either<std::vector<ResultShape_t>, std::string> operator()(
            fishnet::util::forward_range_of<fishnet::Feature<SettlementShape_t>> auto && cluster,
            std::unordered_map<size_t, std::vector<MSTEdge_t>> const & idToMSTEdges,
            auto IDField) const
        {
            if (cluster.empty()) {
                return std::unexpected("Cannot visualize an empty cluster");
            }
            // Collect the MST node IDs for this cluster
            std::vector<size_t> mstNodeIDs;
            for(const auto & settlement: cluster) {
                auto settlementID = settlement.getAttribute(IDField).value_or_throw();
                mstNodeIDs.push_back(settlementID);
            }
            using GeometryPtr = fishnet::geometry::OGRUniquePtr<OGRGeometry>;
            OGRGeometryCollection bufferedCollection;
            for (const auto & settlement : cluster) {
                // the geometry belongs to the input layer and is kept in canonical form by its
                // adapter, so it is copied before being reprojected rather than transformed in place
                GeometryPtr ogrGeom {settlement.getGeometry().raw()->clone()};
                if(ogrGeom->transform(this->toMetric) != OGRERR_NONE){
                    return std::unexpected("Failed to transform settlement geometry to metric projection for settlement with ID: " + settlement.getAttribute(IDField).transform([](auto val){ return std::to_string(val); }).value_or("unknown"));
                }
                GeometryPtr buffered {ogrGeom->Buffer(initialBufferDistance)};
                if (buffered == nullptr) {
                    return std::unexpected("Buffering failed for settlement with ID: " + settlement.getAttribute(IDField).transform([](auto val){ return std::to_string(val); }).value_or("unknown"));
                }
                // the buffered geometry is needed nowhere else, so the collection takes it over
                // rather than copying it and leaving us to keep the original alive
                bufferedCollection.addGeometryDirectly(buffered.release());
            }
            GeometryPtr merged {bufferedCollection.UnaryUnion()->Buffer(targetBufferDistance - initialBufferDistance)}; // erode the union of buffered settlements
            if(merged == nullptr){
                return std::unexpected("Failed to merge and erode settlement polygons for cluster");
            }
            if(merged->transform(this->toSrc) != OGRERR_NONE){
                return std::unexpected("Failed to transform merged settlement geometry back to source projection");
            }
            OGRGeometryCollection finalCollection;
            for(auto nodeID: mstNodeIDs){
                auto it = idToMSTEdges.find(nodeID);
                if (it == idToMSTEdges.end()) {
                    continue; // No edges for this node
                }
                for(const auto & edge: it->second){
                    // the edge is already an OGR geometry and addGeometry copies it into the
                    // collection, so there is nothing to build and nothing to keep alive here
                    finalCollection.addGeometry(edge.raw());
                }
            }
            finalCollection.addGeometryDirectly(merged.release());
            GeometryPtr finalUnion {finalCollection.UnaryUnion()};
            if(finalUnion == nullptr){
                spdlog::debug("Failed to merge geometry collection: {}", finalCollection.exportToWkt());
                return std::unexpected("Failed to merge final settlement and MST geometries");
            }
            // UnaryUnion can leave behind self-touching spikes (bowtie pinch points) from
            // floating point noise where buffered settlements meet MST edge geometries.
            // MakeValid repairs these, so that what ends up in the output file is a valid polygon.
            GeometryPtr repairedFinalUnion {finalUnion->MakeValid()};
            if(repairedFinalUnion != nullptr){
                finalUnion = std::move(repairedFinalUnion);
            }
            // The union is a polygon or a multi-polygon of them, and either is handed straight to
            // the adapters: the geometry is taken over as it is instead of being read out point by
            // point and validated all over again.
            fishnet::geometry::OGRGeometryAdapter unionGeometry {std::move(finalUnion)};
            std::vector<ResultShape_t> resultPolygons;
            if (unionGeometry.isPolygon()) {
                resultPolygons.push_back(std::move(unionGeometry).toPolygon().value());
            } else if (unionGeometry.isMultiPolygon()) {
                auto parts = std::move(unionGeometry).toMultiPolygon().value();
                for (auto && part : parts.getPolygons()) {
                    resultPolygons.push_back(std::move(part));
                }
            } else {
                return std::unexpected("Final merged geometry is neither a polygon nor a multipolygon");
            }
            return resultPolygons;
        }
    };

public: 
    SettlementVisualization(double initialBufferDistance, double targetBufferDistance)
        : Task("SettlementVisualization"), initialBufferDistance(initialBufferDistance), targetBufferDistance(targetBufferDistance) {}

    void operator()(const fishnet::AbstractVectorFile & settlementFile, const fishnet::AbstractVectorFile & mstFile) const {
        auto idToMSTEdges = mstEdges(mstFile);
        auto inputLayer = fishnet::VectorIO::read<SettlementShape_t>(settlementFile);
        auto outputLayer = fishnet::VectorIO::emptyCopy<ResultShape_t>(inputLayer);
        auto clusteredSettlements = clusterSettlements(std::move(inputLayer));
        auto IDField = outputLayer.getSizeField(Task::FISHNET_ID_FIELD).value_or_throw();
        auto clusterField = outputLayer.getSizeField(Africapolis::CLUSTER_ID_FIELD).value_or_throw();
        auto geometryHasher = std::hash<ResultShape_t>();
        for (auto && [clusterID, settlements] : clusteredSettlements) {
            if(clusterID == Africapolis::NOISE_CLUSTER_ID){
                for (auto && settlement : settlements) {
                    // the settlement is written out unchanged and not used afterwards, so its
                    // geometry is taken rather than cloned; its attributes are a separate member
                    auto feature = fishnet::Feature<ResultShape_t>(std::move(settlement).getGeometry());
                    feature.copyAttributes(settlement);
                    outputLayer.addFeature(std::move(feature));
                }
                continue;
            }
            OGRSpatialReference projectedRef = inputLayer.getSpatialReference().IsProjected() ? inputLayer.getSpatialReference() : createAzimuthalEquidistant(settlements);
            VisualizeCluster(initialBufferDistance, targetBufferDistance, std::move(projectedRef), inputLayer.getSpatialReference())
                .operator()(settlements, idToMSTEdges, IDField)
                .if_value_or_error(
                    [&](auto && result){
                        for(auto && polygon: result){
                            auto feature = fishnet::Feature<ResultShape_t>(std::move(polygon));
                            feature.setAttribute(clusterField, clusterID);
                            feature.setAttribute(IDField, geometryHasher(feature.getGeometry()));
                            outputLayer.addFeature(std::move(feature));
                        }
                    },
                    [clusterID](auto && error){
                        spdlog::warn("Failed to visualize cluster {}: {}", clusterID, error);
                    }
                );
        }
        auto outputPath = fishnet::util::PathHelper::appendToFilename(settlementFile.getPath(), "_concave_hull").filename();
        fishnet::VectorIO::overwrite(outputLayer, fishnet::AbstractVectorFile(outputPath));
    }
};


int main(int argc, char* argv[]) {
    using namespace fishnet::geometry;
    CLI::App app{"AfricapolisSettlementOutline"};
    std::string settlementFile;
    std::string mstFile;
    double initialBufferDistance;
    double targetBufferDistance;
    bool debug = false;
    app.add_option("-i,--input", settlementFile, "Path to input shape file")->required()->check(CLI::ExistingFile);
    app.add_option("-m,--mst",mstFile, "Path to input MST shape file")->required()->check(CLI::ExistingFile); 
    app.add_option("--buffer", targetBufferDistance, "Buffer distance in meters for settlement polygons")->required()->check(CLI::PositiveNumber);
    app.add_option("--initial-buffer", initialBufferDistance, "Initial buffer distance in meters for settlement polygons before erosion to target buffer distance")->check(CLI::PositiveNumber)->default_val(100.0);
    app.add_flag("--debug",debug, "Enable debug logging")->default_val(false);
    CLI11_PARSE(app, argc, argv);
    if(debug){
        spdlog::set_level(spdlog::level::debug);
    }
    SettlementVisualization outlineVisualization(initialBufferDistance, targetBufferDistance);
    outlineVisualization(settlementFile,mstFile);
    return 0; 
}
