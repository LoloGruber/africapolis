#include <CLI/CLI.hpp>
#include <magic_enum.hpp>
#include <nlohmann/json.hpp> 
#include <fishnet/Fishnet.hpp>
#include <fishnet/Task.hpp>
#include <fishnet/DistanceFunction.hpp>
#include <fishnet/DistancePredicate.hpp>
#include <fishnet/CompositePredicate.hpp>
#include <fishnet/BinaryFileAdjacency.hpp>
#include <fishnet/SettlementShape.hpp>
#include <fishnet/PolygonNeighbours.hpp>
#include "BinarySettlementGraphAdjacency.hpp"
#include "fishnet/FunctionalConcepts.hpp"
#include "fishnet/ShapeGeometry.hpp"

using json = nlohmann::json;

template<fishnet::geometry::GeometryObject G>
class GraphConstructionVectorReader {
public:
    using geometry_type = G;
    using file_type = fishnet::AbstractVectorFile;
private:
    DistanceFunction distanceFunction;
    std::unordered_map<FileReference, std::filesystem::path> fileRefMap;
    static inline HashingFileReferenceMapper fileRefMapper;
public:
    fishnet::Either<fishnet::VectorLayer<G>,std::string> operator()(const fishnet::AbstractVectorFile & vectorFile) {
        auto layer = fishnet::VectorIO::tryRead<G>(vectorFile);
        if(layer) {
            this->fileRefMap[fileRefMapper(vectorFile)] = vectorFile.getPath();
            this->distanceFunction = distanceFunctionForSpatialReference(layer->getSpatialReference());
        }
        return layer;
    }

    DistanceFunction getDistanceFunction() const noexcept {
        return distanceFunction;
    }

    const std::unordered_map<FileReference, std::filesystem::path> & getFileReferenceMap() const noexcept {
        return fileRefMap;
    }
};

enum class GraphConstructionMode {
    BUFFER_SWEEP,
    DELAUNAY
};

struct GraphConstructionConfig {
    constexpr static const char * GRAPH_CONSTRUCTION_KEY = "graph-construction";
    constexpr static const char * GRAPH_CONSTRUCTION_MODE = "mode";
    constexpr static const char * GRAPH_CONSTRUCTION_ARGS = "args";
    constexpr static const char * DISTANCE_THRESHOLD_KEY = "distance-threshold";
    constexpr static const char * MAX_NEIGHBORS_KEY = "max-neighbors-per-node";

    GraphConstructionMode mode;
    json args;
    double distanceThreshold;

    GraphConstructionConfig(const json & config) {
        auto graphConstructionConfig = config.at(GRAPH_CONSTRUCTION_KEY);
        this->mode = magic_enum::enum_cast<GraphConstructionMode>(graphConstructionConfig.at(GRAPH_CONSTRUCTION_MODE).get<std::string>()).value();
        this->args = graphConstructionConfig.at(GRAPH_CONSTRUCTION_ARGS);
        this->distanceThreshold = this->args.at(DISTANCE_THRESHOLD_KEY).get<double>();
    }
};

template<fishnet::geometry::Shape S>
class GraphConstruction : Task {
private:
    std::vector<SettlementShape<S>> settlements;
    GraphConstructionConfig config;
    DistanceFunction distanceFunction;
    std::filesystem::path graphBinaryOutputPath;
    std::unordered_map<FileReference, std::filesystem::path> fileRefMap;


    std::vector<std::pair<SettlementShape<S>,SettlementShape<S>>> compute_neighbours() {
        const auto distancePredicate = DistanceBiPredicate(distanceFunction,this->config.distanceThreshold);
        switch (config.mode) {
            case GraphConstructionMode::DELAUNAY:
                return fishnet::geometry::PolygonNeighbours::delaunay(this->settlements,distancePredicate);
            case GraphConstructionMode::BUFFER_SWEEP:
            {
                auto boundingBoxPolygonWrapper = [this](const SettlementShape<S> & settPolygon ){
                    /* Create scaled aaBB containing at least all points reachable from the polygon within the maximum edge distance*/
                    auto aaBB = fishnet::geometry::Rectangle<fishnet::math::DEFAULT_NUMERIC>(settPolygon);
                    double distanceMetersTopLeftBotLeft = this->distanceFunction({aaBB.left(),aaBB.top()},{aaBB.left(),aaBB.bottom()});
                    double scale = (this->config.distanceThreshold / distanceMetersTopLeftBotLeft) +1;
                    return fishnet::geometry::BoundingBoxWrapper(settPolygon,aaBB.scale(scale));
                };
                fishnet::util::AllOfPredicate<S,S> neighbouringPredicate;
                /* add all neighbouring predicates to composite predicate */
                neighbouringPredicate.add(distancePredicate);
                //std::ranges::for_each(config.initNeighbouringPredicates<S>(),[&neighbouringPredicate](const auto & predicate){neighbouringPredicate.add(predicate);});
                auto shortCircuitPredicate = [neighbouringPredicate= std::move(neighbouringPredicate)](const fishnet::geometry::BoundingBoxWrapper<SettlementShape<S>> & lhs, const fishnet::geometry::BoundingBoxWrapper<SettlementShape<S>> & rhs){
                    return lhs.getBoundingBox().overlap(rhs.getBoundingBox()) && neighbouringPredicate(lhs.getPolygon(),rhs.getPolygon());
                };
                const size_t MAX_NEIGHBORS_PER_NODE = config.args.at(GraphConstructionConfig::MAX_NEIGHBORS_KEY).get<size_t>();
                return fishnet::geometry::PolygonNeighbours::sweepTemplate(this->settlements,shortCircuitPredicate,boundingBoxPolygonWrapper,MAX_NEIGHBORS_PER_NODE);
            }
            default:
                throw std::invalid_argument("Unknown graph construction mode. Aborting.");
        }
    }

public:
    GraphConstruction(const fishnet::AbstractVectorFile & primaryInput,
                    const fishnet::util::range_of<fishnet::AbstractVectorFile> auto & secondaryInputs,
                    GraphConstructionConfig && config):Task("GraphConstruction"), config(std::move(config)) 
    {
        // Read primary input and get distance function
        auto reader = GraphConstructionVectorReader<S>{};
        this->settlements = SettlementShape<S>::read(primaryInput, reader, HashingFileReferenceMapper{});
        this->distanceFunction = reader.getDistanceFunction();
        this->fileRefMap = reader.getFileReferenceMap();
        this->graphBinaryOutputPath = std::to_string(HashingFileReferenceMapper{}(primaryInput).fileId) + "_graph.bin";
        // Read additional inputs with bounding box filter
        if(this->settlements.empty()){
            std::cerr << "Warning: No settlements read from primary input, returning empty graph" << std::endl;
        }else {
            auto distanceFromBoundingBoxFilter = DistancePredicate(this->distanceFunction, fishnet::geometry::minimalBoundingBox(this->settlements), this->config.distanceThreshold);
            auto additionalSettlements = SettlementShape<S>::template read<fishnet::AbstractVectorFile>(secondaryInputs, reader, HashingFileReferenceMapper{}, distanceFromBoundingBoxFilter);
            this->settlements.insert(this->settlements.end(), additionalSettlements.begin(), additionalSettlements.end());
        }
    }

    void run() {
        auto graph = fishnet::graph::GraphFactory::UndirectedGraph(
            WritingBinarySettlementGraphAdjacency<SettlementShape<S>>(
                this->graphBinaryOutputPath,
                std::move(this->fileRefMap),
                DefaultSettlementSerializer{},
                SettlementShapeDeserializer<S>{} // not used
            )
        );
        auto result = this->compute_neighbours();
        graph.addNodes(this->settlements);
        graph.addEdges(result);
    }
};

static bool isVectorFile(const std::string & path) {
    auto ext = std::filesystem::path(path).extension().string();
    return ext == ".shp" || ext == ".gpkg";
}

int main(int argc, char *argv[]){
    // Parse cmd arguments
    CLI::App app{"Africapolis Graph Construction"};
    std::string primaryInput;
    std::vector<std::string> additionalInputs;
    std::string configfile;
    app.add_option("-i,--input",primaryInput,"Primary input vector file storing the settlements")->required()->check(CLI::ExistingFile);
    app.add_option("-a,--additional_input",additionalInputs,"Additional input vector files storing the settlements")->each([](const std::string & str){
        if(not isVectorFile(str))
            throw CLI::ValidationError("File "+ str + " is not a supported vector file (.shp or .gpkg)");
        if(not std::filesystem::exists(str))
            throw CLI::ValidationError("File "+ str + " does not exist");
    });
    app.add_option("-c,--config", configfile, "Workflow configuration file path")->required();
    CLI11_PARSE(app, argc, argv);

    // Load shapes and settlement graph
    using ShapeType = fishnet::geometry::Polygon<double>;
    GraphConstructionConfig config(json::parse(std::ifstream(configfile)));
    GraphConstruction<ShapeType> graphConstructor(
        fishnet::AbstractVectorFile(fishnet::util::PathHelper::absoluteCanonical(primaryInput)),
        additionalInputs | std::views::transform([](const std::string & str){ return fishnet::AbstractVectorFile(fishnet::util::PathHelper::absoluteCanonical(str)); }),
        std::move(config)
    );
    graphConstructor.run();
    return 0;
}
