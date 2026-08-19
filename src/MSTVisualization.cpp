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
#include <fishnet/PathHelper.h>
#include <fishnet/WGS84Ellipsoid.hpp>
#include <fishnet/MSTAlgorithm.hpp>


class MSTVisualization: public Task {
    std::vector<std::string> geometryFiles;
    std::filesystem::path graphFile;
    std::string outputStem;
    double bufferInMeters;

public:
    MSTVisualization(const std::vector<std::string> & geometryFiles, std::filesystem::path graphFile, double bufferInMeters, const std::string & outputStem):Task("MSTVisualization"), geometryFiles(geometryFiles), graphFile(std::move(graphFile)), outputStem(outputStem), bufferInMeters(bufferInMeters){}

    void run() {
        using Shapetype = fishnet::geometry::Polygon<double>;
        using SettlementType = SettlementShape<Shapetype>;
        using EdgeGeometryType = fishnet::geometry::SimplePolygon<double>;
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
        auto subgraphs = fishnet::graph::BFS::subgraphs(graph,[](){return fishnet::graph::GraphFactory::UndirectedGraph<SettlementType>();});
        auto p2pDistance = distanceFunctionForSpatialReference(spatialRef);
        auto edgeDistance = [&p2pDistance](const SettlementType & lhs, const SettlementType & rhs){return fishnet::geometry::shapeDistance(lhs,rhs,p2pDistance);};
        auto outputLayer = fishnet::VectorIO::empty<EdgeGeometryType>(spatialRef);
        for(const auto & subgraph: subgraphs){
            auto mst = fishnet::graph::MST::kruskal(subgraph,edgeDistance).value_or_throw("Failed to compute MST for subgraph");
            for(const auto & edge: mst.getEdges()){
                //TODO visualize edge buffered line with width = this->bufferInMeters
            }
        }
        auto outputExtension = std::filesystem::path(geometryFiles.front()).extension().string();
        fishnet::VectorIO::overwrite(outputLayer, fishnet::AbstractVectorFile(outputStem+"_edges"+outputExtension));
    }
};

int main(int argc, char *argv[]){
    CLI::App app{"MSTVisualization"};
    std::vector<std::string> geometryFiles;
    std::string graphFile;
    std::string outputStem;
    double bufferInMeters;
    app.add_option("-i,--inputs",geometryFiles,"Shapefiles storing the polygons with id")->required()->each(CLI::ExistingFile);
    app.add_option("-g,--graph",graphFile,"Input binary file storing the settlement graph adjacency")->required()->check(CLI::ExistingFile);
    app.add_option("-o", outputStem, "Output filename stem for storing the clustered shapefile");
    app.add_option("-b,--buffer", bufferInMeters, "Buffer size in meters")->default_val(30.0);
    CLI11_PARSE(app, argc, argv);
    MSTVisualization task(geometryFiles, fishnet::util::PathHelper::absoluteCanonical(graphFile), bufferInMeters, outputStem);
    task.run();
}