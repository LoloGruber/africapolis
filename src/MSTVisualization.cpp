#include <CLI/CLI.hpp>
#include <fishnet/Task.hpp>
#include <ogr_spatialref.h>
#include <spdlog/spdlog.h>
#include <fishnet/Graph.hpp>
#include <fishnet/PolygonDistance.hpp>
#include <fishnet/DistanceFunction.hpp>
#include <fishnet/Segment.hpp>
#include <fishnet/VectorIO.hpp>
#include "BinarySettlementGraphAdjacency.hpp"
#include "ObservableVectorFileReader.hpp"
#include "fishnet/CollectionConcepts.hpp"
#include <fishnet/PathHelper.h>
#include <fishnet/WGS84Ellipsoid.hpp>
#include <fishnet/MSTAlgorithm.hpp>
#include "AfricapolisConstants.hpp"


class MSTVisualization: public Task {
    std::vector<std::string> geometryFiles;
    std::filesystem::path graphFile;
    std::string outputStem;
    double bufferInMeters;

    using EdgeGeometryType = fishnet::geometry::SimplePolygon<double>;

    std::vector<EdgeGeometryType> bufferEdges(fishnet::util::forward_range_of<fishnet::geometry::Segment<double>> auto && edges, const OGRSpatialReference & spatialRef) const {
        // Compute the centroid of all edge endpoints to center the Azimuthal Equidistant projection
        double avgX = 0.0, avgY = 0.0;
        size_t count = 0;
        for (const auto & edge : edges) {
            avgX += edge.p().x + edge.q().x;
            avgY += edge.p().y + edge.q().y;
            count += 2;
        }
        double centerLon = count > 0 ? avgX / static_cast<double>(count) : 0.0;
        double centerLat = count > 0 ? avgY / static_cast<double>(count) : 0.0;

        // Create a single Azimuthal Equidistant projection centered on the dataset
        OGRSpatialReference metricRef;
        metricRef.SetAE(centerLat, centerLon, 0.0, 0.0);

        OGRCoordinateTransformation * toMetric = OGRCreateCoordinateTransformation(&spatialRef, &metricRef);
        OGRCoordinateTransformation * toOriginal = OGRCreateCoordinateTransformation(&metricRef, &spatialRef);

        if (toMetric == nullptr || toOriginal == nullptr) {
            OCTDestroyCoordinateTransformation(toMetric);
            OCTDestroyCoordinateTransformation(toOriginal);
            spdlog::warn("Failed to create coordinate transformations for buffering. Skipping all edges.");
            return {};
        }

        std::vector<EdgeGeometryType> bufferedEdges;
        for (const auto & edge: edges) {
            // Create OGRLineString from segment endpoints
            OGRLineString ogrLine;
            ogrLine.addPoint(edge.p().x, edge.p().y);
            ogrLine.addPoint(edge.q().x, edge.q().y);

            // Clone and transform to metric CRS
            OGRGeometry * cloned = ogrLine.clone();
            if (cloned == nullptr) {
                spdlog::warn("Failed to clone segment geometry. Skipping edge.");
                continue;
            }

            if (cloned->transform(toMetric) != OGRERR_NONE) {
                OGRGeometryFactory::destroyGeometry(cloned);
                spdlog::warn("Failed to transform segment to metric CRS. Skipping edge.");
                continue;
            }

            // Buffer in metric space
            OGRGeometry * buffered = cloned->Buffer(bufferInMeters, 30);
            OGRGeometryFactory::destroyGeometry(cloned);

            if (buffered == nullptr) {
                spdlog::warn("Buffer operation failed for segment. Skipping edge.");
                continue;
            }

            // Transform back to original CRS
            if (buffered->transform(toOriginal) != OGRERR_NONE) {
                OGRGeometryFactory::destroyGeometry(buffered);
                spdlog::warn("Failed to transform buffered segment back to original CRS. Skipping edge.");
                continue;
            }

            // Convert buffered OGR geometry to fishnet SimplePolygon
            const OGRPolygon * ogrPolygon = buffered->toPolygon();
            if (ogrPolygon == nullptr) {
                OGRGeometryFactory::destroyGeometry(buffered);
                spdlog::warn("Buffered geometry is not a polygon. Skipping edge.");
                continue;
            }

            auto ringOpt = fishnet::OGRGeometryAdapter::fromOGR(*ogrPolygon->getExteriorRing());
            OGRGeometryFactory::destroyGeometry(buffered);

            if (not ringOpt) {
                spdlog::warn("Failed to convert buffered exterior ring to fishnet Ring. Skipping edge.");
                continue;
            }

            bufferedEdges.emplace_back(ringOpt.value());
        }

        OCTDestroyCoordinateTransformation(toMetric);
        OCTDestroyCoordinateTransformation(toOriginal);

        return bufferedEdges;
    }

public:
    MSTVisualization(const std::vector<std::string> & geometryFiles, std::filesystem::path graphFile, double bufferInMeters, const std::string & outputStem):Task("MSTVisualization"), geometryFiles(geometryFiles), graphFile(std::move(graphFile)), outputStem(outputStem), bufferInMeters(bufferInMeters){}

    void run() {
        using Shapetype = fishnet::geometry::Polygon<double>;
        using SettlementType = SettlementShape<Shapetype>;
        OGRSpatialReference spatialRef;
        const auto reader = ObservableVectorFileReader<Shapetype>([&spatialRef](const fishnet::VectorLayer<Shapetype> & layer){spatialRef = layer.getSpatialReference();});
        auto nodes = SettlementType::read<fishnet::AbstractVectorFile>(
            geometryFiles | std::views::transform([](const std::string & file) { return fishnet::AbstractVectorFile(fishnet::util::PathHelper::absoluteCanonical(file)); }),
            reader,
            HashingFileReferenceMapper{});
        auto graph = fishnet::graph::GraphFactory::UndirectedGraph(
            ReadingBinarySettlementGraphAdjacency<SettlementType>(
                graphFile,
                SettlementShapeDeserializer<Shapetype>{std::move(nodes)}
            )
        );
        spdlog::debug("Computing subgraphs for the settlement graph with {} nodes and {} edges", fishnet::util::size(graph.getNodes()), fishnet::util::size(graph.getEdges()));
        auto subgraphs = fishnet::graph::BFS::subgraphs(graph,[](){return fishnet::graph::GraphFactory::UndirectedGraph<SettlementType>();});
        auto p2pDistance = distanceFunctionForSpatialReference(spatialRef);
        auto edgeDistance = [&p2pDistance](const SettlementType & lhs, const SettlementType & rhs){return fishnet::geometry::shapeDistance(lhs,rhs,p2pDistance);};

        struct EdgeKeys {
            size_t from;
            size_t to;
        };
        spdlog::debug("Computing MST for subgraphs");
        std::vector<std::pair<fishnet::geometry::Segment<double>, EdgeKeys>> edges;
        for(const auto & subgraph: subgraphs){
            auto mst = fishnet::graph::MST::kruskal(subgraph,edgeDistance).value_or_throw("Failed to compute MST for subgraph");
            for(const auto & edge: mst.getEdges()){
                auto [lhs, rhs] = fishnet::geometry::closestPoints(edge.getFrom(),edge.getTo());
                if(lhs == rhs)
                    continue;
                edges.emplace_back(fishnet::geometry::Segment<double>{lhs,rhs}, EdgeKeys{edge.getFrom().key(), edge.getTo().key()});
            }
        }
        spdlog::debug("Visualizing {} MST edges with a buffer of {} meters", edges.size(), bufferInMeters);
        auto outputLayer = fishnet::VectorIO::empty<EdgeGeometryType>(spatialRef);
        auto fromField = outputLayer.addSizeField(Africapolis::FROM_ID_FIELD).value_or_throw();
        auto toField = outputLayer.addSizeField(Africapolis::TO_ID_FIELD).value_or_throw();
        auto bufferedEdges = bufferEdges(edges | std::views::keys, spatialRef);
        for (auto && [idx,edgePolygon] : bufferedEdges | std::views::enumerate) {
            auto index = static_cast<size_t>(idx);
            fishnet::Feature<EdgeGeometryType> feature(edgePolygon);
            feature.addAttribute(fromField, edges[index].second.from);
            feature.addAttribute(toField, edges[index].second.to);
            outputLayer.addFeature(std::move(feature));
        }
        auto outputExtension = std::filesystem::path(geometryFiles.front()).extension().string();
        fishnet::VectorIO::overwrite(outputLayer, fishnet::AbstractVectorFile(outputStem+"_mst"+outputExtension));
    }
};

int main(int argc, char *argv[]){
    CLI::App app{"MSTVisualization"};
    std::vector<std::string> geometryFiles;
    std::string graphFile;
    std::string outputStem;
    double bufferInMeters;
    bool debug = false;
    app.add_option("-i,--inputs",geometryFiles,"Shapefiles storing the polygons with id")->required()->each(CLI::ExistingFile);
    app.add_option("-g,--graph",graphFile,"Input binary file storing the settlement graph adjacency")->required()->check(CLI::ExistingFile);
    app.add_option("-o", outputStem, "Output filename stem for storing the clustered shapefile");
    app.add_option("-b,--buffer", bufferInMeters, "Buffer size in meters")->default_val(30.0);
    app.add_option("--debug", debug, "Enable debug logging")->default_val(false);
    if(debug){
        spdlog::set_level(spdlog::level::debug);
    }
    CLI11_PARSE(app, argc, argv);
    MSTVisualization task(geometryFiles, fishnet::util::PathHelper::absoluteCanonical(graphFile), bufferInMeters, outputStem);
    task.run();
}