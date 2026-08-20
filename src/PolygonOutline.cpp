#include "fishnet/CollectionConcepts.hpp"
#include "fishnet/Feature.hpp"
#include "fishnet/OGRGeometryAdapter.hpp"
#include "fishnet/SimplePolygon.hpp"
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

using MSTEdge_t = fishnet::geometry::SimplePolygon<double>;
using SettlementShape_t = fishnet::geometry::Polygon<double>;
using ResultShape_t = fishnet::geometry::Polygon<double>;

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
        auto polygons = cluster | std::views::transform([](const auto & settlement){ return settlement.getGeometry(); });
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
        std::unordered_map<size_t, fishnet::Feature<MSTEdge_t>> edges;
        auto mstLayer = fishnet::VectorIO::read<MSTEdge_t>(mstFile);
        auto fromField = mstLayer.getSizeField(Africapolis::FROM_ID_FIELD).value_or_throw();
        auto toField = mstLayer.getSizeField(Africapolis::TO_ID_FIELD).value_or_throw();
        std::unordered_map<size_t, std::vector<MSTEdge_t>> nodesToFeature;
        for (auto && [idx,feature] : std::move(mstLayer).getFeatures() | std::views::enumerate) {
            size_t fromID = feature.getAttribute(fromField).value_or_throw();
            size_t toID = feature.getAttribute(toField).value_or_throw();
            nodesToFeature[fromID].push_back(feature.getGeometry());
            nodesToFeature[toID].push_back(feature.getGeometry());
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

        fishnet::Either<ResultShape_t, std::string> operator()(
            fishnet::util::forward_range_of<fishnet::Feature<SettlementShape_t>> auto && cluster,
            std::unordered_map<size_t, std::vector<MSTEdge_t>> const & idToMSTEdges,
            std::vector<size_t> const & mstNodeIDs,
            auto IDField) const
        {
            if (cluster.empty()) {
                return std::unexpected("Cannot visualize an empty cluster");
            }
            using GeometryPtr = fishnet::OGRGeometryAdapter::OGRUniquePtr<OGRGeometry>;
            std::vector<GeometryPtr> settlementPolygons; // stores transformed and buffered settlement polygons
            for (const auto & settlement : cluster) {
                auto ogrGeom = fishnet::OGRGeometryAdapter::toOGR(settlement.getGeometry());
                if(ogrGeom->transform(this->toMetric) != OGRERR_NONE){
                    return std::unexpected("Failed to transform settlement geometry to metric projection for settlement with ID: " + settlement.getAttribute(IDField).transform([](auto val){ return std::to_string(val); }).value_or("unknown"));
                }
                GeometryPtr buffered {ogrGeom->Buffer(initialBufferDistance)};
                if (buffered == nullptr) {
                    return std::unexpected("Buffering failed for settlement with ID: " + settlement.getAttribute(IDField).transform([](auto val){ return std::to_string(val); }).value_or("unknown"));
                }
                settlementPolygons.push_back(std::move(buffered));
            }
            OGRGeometryCollection bufferedCollection;
            for(const auto & geom: settlementPolygons){
                bufferedCollection.addGeometry(geom.get());
            }
            GeometryPtr merged {bufferedCollection.UnaryUnion()->Buffer(targetBufferDistance - initialBufferDistance)}; // erode the union of buffered settlements
            if(merged == nullptr){
                return std::unexpected("Failed to merge and erode settlement polygons for cluster");
            }
            if(merged->transform(this->toSrc) != OGRERR_NONE){
                return std::unexpected("Failed to transform merged settlement geometry back to source projection");
            }
            OGRGeometryCollection finalCollection;
            std::vector<GeometryPtr> mstEdges;
            for(auto nodeID: mstNodeIDs){
                auto it = idToMSTEdges.find(nodeID);
                if (it == idToMSTEdges.end()) {
                    continue; // No edges for this node
                }
                for(const auto & edge: it->second){
                    auto ogrEdge = fishnet::OGRGeometryAdapter::toOGR(edge);
                    mstEdges.push_back(std::move(ogrEdge));
                    finalCollection.addGeometry(mstEdges.back().get());
                }
            }
            finalCollection.addGeometry(merged.get());
            auto ogrResult = fishnet::OGRGeometryAdapter::fromOGR(*finalCollection.UnaryUnion()->toPolygon(),true);
            if(not ogrResult){
                return std::unexpected("Failed to convert final merged geometry to fishnet Polygon");
            }
            return ogrResult.value();
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
                    auto feature = fishnet::Feature<ResultShape_t>(settlement.getGeometry());
                    feature.copyAttributes(settlement);
                    outputLayer.addFeature(std::move(feature));
                }
                continue;
            }
            // Collect the MST node IDs for this cluster
            std::vector<size_t> clusterMSTNodeIDs;
            for(const auto & settlement: settlements) {
                auto settlementID = settlement.getAttribute(IDField).value_or_throw();
                clusterMSTNodeIDs.push_back(settlementID);
            }
            // Visualize the cluster using the already buffered MST edges and the buffering + eroding settlement polygons
            auto result = VisualizeCluster(initialBufferDistance, targetBufferDistance, createAzimuthalEquidistant(settlements), inputLayer.getSpatialReference())
                .operator()(settlements, idToMSTEdges, clusterMSTNodeIDs, IDField);
            if ( not result){
                spdlog::warn("Failed to visualize cluster {}: {}", clusterID, result.error());
                continue;
            }
            auto feature = fishnet::Feature<ResultShape_t>(result.value());
            feature.setAttribute(clusterField, clusterID);
            feature.setAttribute(IDField, geometryHasher(feature.getGeometry()));
            outputLayer.addFeature(std::move(feature));
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
    app.add_option("-i,--input", settlementFile, "Path to input shape file")->required()->check(CLI::ExistingFile);
    app.add_option("-m,--mst",mstFile, "Path to input MST shape file")->required()->check(CLI::ExistingFile); 
    app.add_option("--buffer", targetBufferDistance, "Buffer distance in meters for settlement polygons")->required()->check(CLI::PositiveNumber);
    app.add_option("--initial-buffer", initialBufferDistance, "Initial buffer distance in meters for settlement polygons before erosion to target buffer distance")->check(CLI::PositiveNumber)->default_val(100.0);
    CLI11_PARSE(app, argc, argv);
    SettlementVisualization outlineVisualization(initialBufferDistance, targetBufferDistance);
    outlineVisualization(settlementFile,mstFile);
    return 0; 
}
