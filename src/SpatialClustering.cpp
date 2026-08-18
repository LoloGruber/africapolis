#include <CLI/CLI.hpp>
#include <magic_enum.hpp>
#include <fishnet/Fishnet.hpp>
#include <fishnet/DBSC.hpp>
#include <fishnet/TaskConfig.hpp>
#include <fishnet/DistanceFunction.hpp>
#include <fishnet/DistancePredicate.hpp>
#include <fishnet/SettlementShape.hpp>
#include <fishnet/IDReduceFunction.hpp>
#include <fishnet/Task.hpp>
#include <fishnet/FunctionalConcepts.hpp>
#include <fishnet/PathHelper.h>
#include "BinarySettlementGraphAdjacency.hpp"
#include "ObservableVectorFileReader.hpp"


enum class ClusterMode {
    DBSCAN,
    BFS,
    DBSC
};

enum class DBSCAttributeFunction{
    AREA,
    NONE
};

template<typename T>
static fishnet::util::UnaryFunction_t<T, double> attributeMapper(DBSCAttributeFunction attributeFunction){
    switch(attributeFunction){
        case DBSCAttributeFunction::AREA:
            return [](const T & node){ return node.area(); };
        case DBSCAttributeFunction::NONE:
            return [](const T & node){ return 0.0; };
    }
    throw std::runtime_error("Unsupported attribute function");
}

struct ClusteringConfig:TaskConfig {
    constexpr static const char * CLUSTER_KEY = "clustering";
    constexpr static const char * CLUSTER_MODE_KEY = "mode";
    constexpr static const char * CLUSTER_ARGS_KEY = "args";

    ClusterMode clusterMode;
    json clusterArgs;
    ClusteringConfig(const json & config):TaskConfig(config){
        auto clusterConfig = this->jsonDescription.at(CLUSTER_KEY);
        this->clusterMode = magic_enum::enum_cast<ClusterMode>(clusterConfig.at(CLUSTER_MODE_KEY).get<std::string>()).value();
        this->clusterArgs = clusterConfig.at(CLUSTER_ARGS_KEY);
    }

    template<fishnet::graph::Graph G> requires(fishnet::geometry::Shape<typename G::node_type>)
    fishnet::ClusterAlgorithm_t<G> getSpatialClusterAlgorithm(DistanceFunction && distanceFunction) const {
        using T = typename G::node_type;
        switch(this->clusterMode){
            case ClusterMode::DBSCAN:
                {
                    double eps = clusterArgs.at("distance-threshold").get<double>();
                    size_t minPts = clusterArgs.at("min-cluster-size").get<size_t>(); 
                    return fishnet::DBSCAN<T>(eps, minPts, [&distanceFunction](const typename G::node_type & lhs, const typename G::node_type & rhs){
                        return fishnet::geometry::shapeDistance(lhs,rhs,distanceFunction);
                    });
                }
            case ClusterMode::BFS: 
                {
                    double distanceThreshold = clusterArgs.at("distance-threshold").get<double>();
                    return fishnet::BFSClustering<T>(DistanceBiPredicate(std::move(distanceFunction), distanceThreshold));
                }
            case ClusterMode::DBSC:
                {   
                    double customT1 = clusterArgs.contains("t1") ? clusterArgs.at("t1").get<double>() : NAN;
                    DBSCAttributeFunction attributeFunction = clusterArgs.contains("attribute-mapper") ? magic_enum::enum_cast<DBSCAttributeFunction>(clusterArgs.at("attribute-mapper").get<std::string>()).value_or(DBSCAttributeFunction::NONE) : DBSCAttributeFunction::NONE;
                    return fishnet::DBSCBuilder<T>()
                        .setEps(clusterArgs.at("distance-threshold").get<double>())
                        .setBeta(clusterArgs.at("beta").get<size_t>())
                        .setMinPts(clusterArgs.at("min-cluster-size").get<size_t>())
                        .setDistanceFunction([distanceFunction](const typename G::node_type & lhs, const typename G::node_type & rhs){
                            return fishnet::geometry::shapeDistance(lhs,rhs,distanceFunction);
                        })
                        .setAttributeExtractor(attributeMapper<T>(attributeFunction))
                        .setT1(customT1)
                        .build();
                }
        }
        throw std::runtime_error("Unsupported clustering mode");
    }
};

static std::string getOutputFilename(const std::string & inputFilename, const std::string & suffix) {
    auto path = std::filesystem::path(inputFilename);
    auto ext = path.extension().string();
    return fishnet::util::PathHelper::absoluteCanonical(path.stem().string() + suffix + ext).string();
}

class SpatialClustering : public Task {
private: 
    ClusteringConfig config;
    std::vector<std::string> inputFilenames;
    std::filesystem::path graphFile;
    std::string outputStem;
    std::string outputExtension;
public:

    SpatialClustering(
        const ClusteringConfig & config,
        std::vector<std::string> && inputFilenames,
        const std::filesystem::path & graphFile,
        std::string && outputStem
    ):Task("Clustering"), config(config), inputFilenames(std::move(inputFilenames)), graphFile(graphFile), outputStem(std::move(outputStem)){
        // Derive output extension from first input file
        if(not this->inputFilenames.empty()){
            this->outputExtension = std::filesystem::path(this->inputFilenames.front()).extension().string();
        } else {
            this->outputExtension = ".shp"; // default fallback
        }
    }
    

    void run() {
        // Load shapes and settlement graph
        using ShapeType = fishnet::geometry::Polygon<double>;
        using SettlementType = SettlementShape<ShapeType>;
        auto vectorFiles = inputFilenames | std::views::transform([](const std::string & str){ return fishnet::AbstractVectorFile(str); });
        OGRSpatialReference spatialRef;
        auto onReadStoreSpatialRef = [&spatialRef](const fishnet::VectorLayer<ShapeType> & layer){
            if(spatialRef.IsEmpty()){
                spatialRef = layer.getSpatialReference();
            }
        };
        ObservableVectorFileReader<ShapeType> reader(onReadStoreSpatialRef);
        auto settlements = SettlementType::read<fishnet::AbstractVectorFile>(vectorFiles, reader,HashingFileReferenceMapper{});
        auto adj = ReadingBinarySettlementGraphAdjacency<SettlementType>(
            this->graphFile,
            SettlementShapeDeserializer<ShapeType>{std::move(settlements)}
        );
        auto graph = fishnet::graph::GraphFactory::UndirectedGraph(std::move(adj));

        // Run clustering
        auto clusterAlgorithm = config.getSpatialClusterAlgorithm<decltype(graph)>(distanceFunctionForSpatialReference(spatialRef));
        auto result = clusterAlgorithm(graph);

        // Store result
        using OutputShapeType = fishnet::geometry::MultiPolygon<ShapeType>;
        auto outputLayer = fishnet::VectorIO::empty<OutputShapeType>(spatialRef);
        auto idField = outputLayer.addSizeField(Task::FISHNET_ID_FIELD).value_or_throw();
        auto mergeFunction = IDReduceFunction();
        for(auto && cluster : result.clusters){
            auto settlementMultiPolygon = mergeFunction(cluster);
            auto id = settlementMultiPolygon.key();
            fishnet::Feature<OutputShapeType> feature(settlementMultiPolygon.geometry());
            feature.setAttribute(idField, size_t(id));
            outputLayer.addFeature(std::move(feature));
        }
        for(auto && noise : result.noise){
            fishnet::Feature<OutputShapeType> feature(OutputShapeType(noise.geometry()));
            feature.setAttribute(idField, size_t(9999999999999));
            outputLayer.addFeature(std::move(feature));
        }
        auto outputPath = fishnet::util::PathHelper::absoluteCanonical(this->outputStem + this->outputExtension);
        fishnet::VectorIO::overwrite(outputLayer, fishnet::AbstractVectorFile(outputPath));
    }


};

static bool isVectorFile(const std::string & path) {
    auto ext = std::filesystem::path(path).extension().string();
    return ext == ".shp" || ext == ".gpkg";
}

int main(int argc, char *argv[]){
    // Parse cmd arguments
    CLI::App app{"Fishnet Clustering Algorithm"};
    std::vector<std::string> inputfiles;
    std::string graphFile;
    std::string configfile;
    std::string outputStem;
    app.add_option("-i,--inputs",inputfiles,"Input vector files storing the polygons with id for clustering")->required()->each([](const std::string & str){
        if(not isVectorFile(str))
            throw CLI::ValidationError("File "+ str + " is not a supported vector file (.shp or .gpkg)");
        if(not std::filesystem::exists(str))
            throw CLI::ValidationError("File "+ str + " does not exist");
    });
    app.add_option("-c,--config", configfile, "Workflow configuration file path")->required()->check(CLI::ExistingFile);
    app.add_option("-g, --graph",graphFile,"Graph file")->required()->check(CLI::ExistingFile);
    app.add_option("--outputStem", outputStem, "Output filename stem for storing the clustered vector file");
    CLI11_PARSE(app, argc, argv); 
    SpatialClustering clusteringTask(
        ClusteringConfig(nlohmann::json::parse(std::ifstream(configfile))),
        std::move(inputfiles), 
        std::filesystem::path(graphFile),
        std::move(outputStem)
    );
    clusteringTask.run();
    return 0;
}
