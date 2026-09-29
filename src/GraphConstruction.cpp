#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <fishnet/Fishnet.hpp>
#include <fishnet/Task.hpp>
#include <fishnet/DistanceFunction.hpp>
#include <fishnet/DistancePredicate.hpp>
#include <fishnet/CompositePredicate.hpp>
#include <fishnet/BinaryFileAdjacency.hpp>
#include <fishnet/IGeometry.hpp>
#include <fishnet/SettlementShape.hpp>
#include <fishnet/PolygonNeighbours.hpp>
#include "BinarySettlementGraphAdjacency.hpp"
#include "SettlementLayerReader.hpp"
#include "EnumOption.hpp"

enum class GraphConstructionMode {
    BUFFER_SWEEP,
    DELAUNAY
};

struct GraphConstructionParameters {
    GraphConstructionMode mode;
    double distanceThreshold;
    std::optional<size_t> maxNeighboursPerNode; // BUFFER_SWEEP only
};

template<fishnet::geometry::Shape S>
class GraphConstruction : Task {
private:
    std::vector<SettlementShape<S>> settlements;
    GraphConstructionParameters parameters;
    DistanceFunction distanceFunction;
    std::filesystem::path graphBinaryOutputPath;
    std::unordered_map<FileReference, std::filesystem::path> fileRefMap;


    std::vector<std::pair<SettlementShape<S>,SettlementShape<S>>> compute_neighbours() {
        const auto distancePredicate = DistanceBiPredicate(distanceFunction,this->parameters.distanceThreshold);
        switch (parameters.mode) {
            case GraphConstructionMode::DELAUNAY:
                return fishnet::geometry::PolygonNeighbours::delaunay(this->settlements,distancePredicate);
            case GraphConstructionMode::BUFFER_SWEEP:
            {
                auto boundingBoxPolygonWrapper = [this](const SettlementShape<S> & settPolygon ){
                    /* Create scaled aaBB containing at least all points reachable from the polygon within the maximum edge distance*/
                    auto aaBB = fishnet::geometry::Rectangle<fishnet::math::DEFAULT_NUMERIC>(settPolygon);
                    double distanceMetersTopLeftBotLeft = this->distanceFunction({aaBB.left(),aaBB.top()},{aaBB.left(),aaBB.bottom()});
                    double scale = (this->parameters.distanceThreshold / distanceMetersTopLeftBotLeft) +1;
                    return fishnet::geometry::BoundingBoxWrapper(settPolygon,aaBB.scale(scale));
                };
                fishnet::util::AllOfPredicate<S,S> neighbouringPredicate;
                /* add all neighbouring predicates to composite predicate */
                neighbouringPredicate.add(distancePredicate);
                auto shortCircuitPredicate = [neighbouringPredicate= std::move(neighbouringPredicate)](const fishnet::geometry::BoundingBoxWrapper<SettlementShape<S>> & lhs, const fishnet::geometry::BoundingBoxWrapper<SettlementShape<S>> & rhs){
                    return lhs.getBoundingBox().overlap(rhs.getBoundingBox()) && neighbouringPredicate(lhs.getPolygon(),rhs.getPolygon());
                };
                const size_t MAX_NEIGHBORS_PER_NODE = Africapolis::requiredFor(parameters.maxNeighboursPerNode,"--max-neighbors-per-node",parameters.mode);
                return fishnet::geometry::PolygonNeighbours::sweepTemplate(this->settlements,shortCircuitPredicate,boundingBoxPolygonWrapper,MAX_NEIGHBORS_PER_NODE);
            }
            default:
                throw std::invalid_argument("Unknown graph construction mode. Aborting.");
        }
    }

public:
    GraphConstruction(const fishnet::AbstractVectorFile & primaryInput,
                    const fishnet::util::range_of<fishnet::AbstractVectorFile> auto & secondaryInputs,
                    GraphConstructionParameters parameters):Task("GraphConstruction"), parameters(std::move(parameters)) 
    {
        // Read primary input and get distance function
        auto reader = SettlementLayerReader<S>([this](const fishnet::VectorLayer<S> & layer, const fishnet::AbstractVectorFile & vectorFile){
            this->fileRefMap[HashingFileReferenceMapper{}(vectorFile)] = vectorFile.getPath();
            this->distanceFunction = distanceFunctionForSpatialReference(layer.getSpatialReference());
        });
        this->settlements = SettlementShape<S>::read(primaryInput, reader, HashingFileReferenceMapper{});
        this->graphBinaryOutputPath = std::to_string(HashingFileReferenceMapper{}(primaryInput).fileId) + "_graph.bin";
        // Read additional inputs with bounding box filter
        if(this->settlements.empty()){
            std::cerr << "Warning: No settlements read from primary input, returning empty graph" << std::endl;
        }else {
            auto distanceFromBoundingBoxFilter = DistancePredicate(this->distanceFunction, fishnet::geometry::minimalBoundingBox(this->settlements), this->parameters.distanceThreshold);
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

int main(int argc, char *argv[]){
    // Parse cmd arguments
    CLI::App app{"Africapolis Graph Construction"};
    std::string primaryInput;
    std::vector<std::string> additionalInputs;
    GraphConstructionParameters parameters;
    bool debug = false;
    app.add_option("-i,--input",primaryInput,"Primary input vector file storing the settlements")->required()->check(CLI::ExistingFile);
    app.add_option("-a,--additional_input",additionalInputs,"Additional input vector files storing the settlements")->each(CLI::ExistingFile);
    app.add_option("--mode",parameters.mode,"Method used to connect neighbouring settlements")
        ->required()
        ->transform(Africapolis::enumSymbols<GraphConstructionMode>());
    app.add_option("--distance-threshold",parameters.distanceThreshold,"Maximum distance in meters between two settlements sharing an edge")
        ->required()
        ->check(CLI::PositiveNumber);
    app.add_option("--max-neighbors-per-node",parameters.maxNeighboursPerNode,"Maximum number of edges per settlement. Required by the BUFFER_SWEEP mode")
        ->check(CLI::PositiveNumber);
    app.add_flag("--debug",debug,"Enable debug logging");
    CLI11_PARSE(app, argc, argv);
    if(debug){
        spdlog::set_level(spdlog::level::debug);
    }

    // Load shapes and settlement graph
    using ShapeType = fishnet::geometry::Polygon<double>;
    GraphConstruction<ShapeType> graphConstructor(
        fishnet::AbstractVectorFile(fishnet::util::PathHelper::absoluteCanonical(primaryInput)),
        additionalInputs | std::views::transform([](const std::string & str){ return fishnet::AbstractVectorFile(fishnet::util::PathHelper::absoluteCanonical(str)); }),
        std::move(parameters)
    );
    graphConstructor.run();
    return 0;
}
