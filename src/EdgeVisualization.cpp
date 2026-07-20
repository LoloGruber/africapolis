#include <CLI/CLI.hpp>
#include <fishnet/Task.hpp>
#include <ogr_spatialref.h>
#include <spdlog/spdlog.h>
#include <fishnet/Graph.hpp>
#include <fishnet/PolygonDistance.hpp>
#include <fishnet/Segment.hpp>
#include <fishnet/VectorIO.hpp>
#include "BinarySettlementGraphAdjacency.hpp"
#include "ObservableShapefileReader.hpp"


class EdgeVisualization: public Task {
    std::filesystem::path geometryFile;
    std::filesystem::path graphFile;

    static std::optional<fishnet::geometry::SimplePolygon<double>> visualizeEdge(const fishnet::geometry::IPolygon auto & from, const fishnet::geometry::IPolygon auto & to) noexcept{
        auto [l,r] = fishnet::geometry::closestPoints(from,to);
        fishnet::geometry::Segment<double> best {l,r};
        if(not best.isValid())
            return std::nullopt; // dont create edge when polygons touch each other (0-length segment)
        double WIDTH = 0.000001;
        auto orthogonalToSegment = best.direction().orthogonal().normalize(); // orthogonal direction vector 
        auto center = best.p() + best.direction().normalize() * (best.length() / 2); // middle point of the segment
        auto p1 = (best.p() + (orthogonalToSegment * WIDTH / 2));
        auto p2 = (best.p() - (orthogonalToSegment * WIDTH / 2));
        auto p3 = (best.q() + (orthogonalToSegment * WIDTH / 2));
        auto p4 = (best.q() - (orthogonalToSegment * WIDTH / 2));

        /*Comparator to sort the points clockwise*/
        auto cmpClockwise = [&center](fishnet::geometry::Vec2DReal const& u, fishnet::geometry::Vec2DReal const& w) {
            return u.angle(center).getAngleValue() > w.angle(center).getAngleValue();
        };
        std::vector<fishnet::geometry::Vec2D<double>> vectors = {p1, p2, p3, p4};
        std::sort(vectors.begin(), vectors.end(), cmpClockwise);
        try{
            return std::make_optional<fishnet::geometry::SimplePolygon<double>>(vectors);
        }catch(fishnet::geometry::InvalidGeometryException & exc){
            return std::nullopt;
        }
    }


public:
    EdgeVisualization(std::filesystem::path geometryFile, std::filesystem::path graphFile):Task("EdgeVisualization"), geometryFile(std::move(geometryFile)), graphFile(std::move(graphFile)){}

    void run() {
        using Shapetype = fishnet::geometry::Polygon<double>;
        using SettlementType = SettlementShape<Shapetype>;
        using EdgeGeometryType = fishnet::geometry::SimplePolygon<double>;
        OGRSpatialReference spatialRef;
        const auto reader = ObservableShapefileReader<Shapetype>([&spatialRef](const fishnet::VectorLayer<Shapetype> & layer){spatialRef = layer.getSpatialReference();});
        auto nodes = SettlementType::read<fishnet::Shapefile>(fishnet::Shapefile(geometryFile), reader, HashingFileReferenceMapper{});
        auto graph = fishnet::graph::GraphFactory::UndirectedGraph(
            ReadingBinarySettlementGraphAdjacency<SettlementType>(
                graphFile,
                SettlementShapeDeserializer<Shapetype>{std::move(nodes)}
            )
        );
        auto outputLayer = fishnet::VectorIO::empty<EdgeGeometryType>(spatialRef);
        for(const auto & edge: graph.getEdges()){
            auto optEdgePolygon = visualizeEdge(edge.getFrom(), edge.getTo());
            if(optEdgePolygon.has_value()){
                outputLayer.addFeature(fishnet::Feature<EdgeGeometryType>(optEdgePolygon.value()));
            }else{
                spdlog::warn("Failed to visualize edge between settlements '{}' and '{}'. Skipping edge.", edge.getFrom().key(), edge.getTo().key());
            }
        }
        fishnet::VectorIO::overwrite(outputLayer,fishnet::Shapefile(geometryFile.stem().string()+"_edges.shp"));
    }
};

int main(int argc, char *argv[]){
    CLI::App app{"EdgeVisualization"};
    std::string geometryFile;
    std::string graphFile;
    app.add_option("-i,--input",geometryFile,"Input shapefile storing the settlements")->required()->check(CLI::ExistingFile);
    app.add_option("-g,--graph",graphFile,"Input binary file storing the settlement graph adjacency")->required()->check(CLI::ExistingFile);
    CLI11_PARSE(app, argc, argv);
    EdgeVisualization task(fishnet::util::PathHelper::absoluteCanonical(geometryFile), fishnet::util::PathHelper::absoluteCanonical(graphFile));
    task.run();
}