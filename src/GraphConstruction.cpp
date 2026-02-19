#include <CLI/CLI.hpp>
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

using json = nlohmann::json;

template<fishnet::geometry::GeometryObject G>
class GraphConstructionShapefileReader {
public:
    using geometry_type = G;
    using file_type = fishnet::Shapefile;
private:
    DistanceFunction distanceFunction;
    std::unordered_map<FileReference, std::filesystem::path> fileRefMap;
    static inline HashingFileReferenceMapper fileRefMapper;
public:
    fishnet::Either<fishnet::VectorLayer<G>,std::string> operator()(const fishnet::Shapefile & shapefile) {
        auto layer = fishnet::VectorIO::tryRead(fishnet::ShapefileReader<G>{},shapefile);
        if(layer) {
            this->fileRefMap[fileRefMapper(shapefile)] = shapefile.getPath();
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

struct GraphConstructionConfig {
    constexpr static const char * BUFFER_DISTANCE_KEY = "buffer-distance-meters";
    constexpr static const char * MAX_NEIGHBORS_KEY = "max-neighbors-per-node";

    double bufferDistanceMeters;
    size_t maxNeighborsPerNode;

    GraphConstructionConfig(const json & config) {
        bufferDistanceMeters = config.at(BUFFER_DISTANCE_KEY).get<double>();
        maxNeighborsPerNode = config.at(MAX_NEIGHBORS_KEY).get<size_t>();
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

public:
    GraphConstruction(const fishnet::Shapefile & primaryInput,
                    const fishnet::util::range_of<fishnet::Shapefile> auto & secondaryInputs,
                    GraphConstructionConfig && config): config(std::move(config)) 
    {
        // Read primary input and get distance function
        auto reader = GraphConstructionShapefileReader<S>{};
        this->settlements = SettlementShape<S>::read(primaryInput, reader, HashingFileReferenceMapper{});
        this->distanceFunction = reader.getDistanceFunction();
        this->fileRefMap = reader.getFileReferenceMap();
        this->graphBinaryOutputPath = std::to_string(HashingFileReferenceMapper{}(primaryInput).fileId) + "_graph.bin";
        // Read additional inputs with bounding box filter
        if(this->settlements.empty()){
            std::cerr << "Warning: No settlements read from primary input, returning empty graph" << std::endl;
        }else {
            auto distanceFromBoundingBoxFilter = DistancePredicate(this->distanceFunction, fishnet::geometry::minimalBoundingBox(this->settlements), this->config.bufferDistanceMeters);
            auto additionalSettlements = SettlementShape<S>::template read<fishnet::Shapefile>(secondaryInputs, reader, HashingFileReferenceMapper{}, distanceFromBoundingBoxFilter);
            this->settlements.insert(this->settlements.end(), additionalSettlements.begin(), additionalSettlements.end());
        }
    }

    void run() override{
        auto graph = fishnet::graph::GraphFactory::UndirectedGraph<SettlementShape<S>>(
            WritingBinarySettlementGraphAdjacency<SettlementShape<S>>(
                this->graphBinaryOutputPath,
                std::move(this->fileRefMap),
                DefaultSettlementSerializer{},
                SettlementShapeDeserializer<S>{} // not used
            )
        );
        auto boundingBoxPolygonWrapper = [this](const SettlementShape<S> & settPolygon ){
            /* Create scaled aaBB containing at least all points reachable from the polygon within the maximum edge distance*/
            auto aaBB = fishnet::geometry::Rectangle<fishnet::math::DEFAULT_NUMERIC>(settPolygon);
            double distanceMetersTopLeftBotLeft = this->distanceFunction({aaBB.left(),aaBB.top()},{aaBB.left(),aaBB.bottom()});
            double scale = (this->config.bufferDistanceMeters / distanceMetersTopLeftBotLeft) +1;
            return fishnet::geometry::BoundingBoxWrapper(settPolygon,aaBB.scale(scale));
        };
        fishnet::util::AllOfPredicate<S,S> neighbouringPredicate;
        /* add all neighbouring predicates to composite predicate */
        neighbouringPredicate.add(DistanceBiPredicate(distanceFunction,config.bufferDistanceMeters));
        //std::ranges::for_each(config.initNeighbouringPredicates<S>(),[&neighbouringPredicate](const auto & predicate){neighbouringPredicate.add(predicate);});
        auto shortCircuitPredicate = [neighbouringPredicate= std::move(neighbouringPredicate)](const fishnet::geometry::BoundingBoxWrapper<SettlementShape<S>> & lhs, const fishnet::geometry::BoundingBoxWrapper<SettlementShape<S>> & rhs){
            return lhs.getBoundingBox().overlap(rhs.getBoundingBox()) && neighbouringPredicate(lhs.getPolygon(),rhs.getPolygon());
        };
        auto result = fishnet::geometry::findNeighbouringPolygonsTemplate(this->settlements,shortCircuitPredicate,boundingBoxPolygonWrapper,config.maxNeighborsPerNode);
        graph.addNodes(this->settlements);
        graph.addEdges(result);
    }
};

int main(int argc, char *argv[]){
    // Parse cmd arguments
    CLI::App app{"Africapolis Graph Construction"};
    std::string primaryInput;
    std::vector<std::string> additionalInputs;
    std::string configfile;
    app.add_option("-i,--input",primaryInput,"Primary input shapefile storing the settlements")->required()->check(CLI::ExistingFile);
    app.add_option("-a,--additional_input",additionalInputs,"Additional input shapefiles storing the settlements")->each([](const std::string & str){
        try{
            auto file = fishnet::Shapefile(str);
            if(not file.exists())
                throw std::invalid_argument("File "+ file.getPath().string() + " does not exist");
        }catch(std::invalid_argument & error){
            throw CLI::ValidationError(error.what());
        }
    });
    app.add_option("-c,--config", configfile, "Workflow configuration file path")->required();
    CLI11_PARSE(app, argc, argv);

    // Load shapes and settlement graph
    using ShapeType = fishnet::geometry::Polygon<double>;
    GraphConstructionConfig config(json::parse(std::ifstream(configfile)));
    GraphConstruction<ShapeType> graphConstructor(
        fishnet::Shapefile(fishnet::util::PathHelper::absoluteCanonical(primaryInput)),
        additionalInputs | std::views::transform([](const std::string & str){ return fishnet::Shapefile(fishnet::util::PathHelper::absoluteCanonical(str)); }),
        std::move(config)
    );
    graphConstructor.run();
    return 0;
}