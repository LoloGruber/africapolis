#include <CLI/CLI.hpp>
#include <ranges>
#include <spdlog/spdlog.h>
#include <fishnet/Fishnet.hpp>
#include <fishnet/DBSC.hpp>
#include <fishnet/DistanceFunction.hpp>
#include <fishnet/DistancePredicate.hpp>
#include <fishnet/SettlementShape.hpp>
#include <fishnet/IDReduceFunction.hpp>
#include <fishnet/Task.hpp>
#include <fishnet/FunctionalConcepts.hpp>
#include <fishnet/PathHelper.h>
#include "BinarySettlementGraphAdjacency.hpp"
#include "SettlementLayerReader.hpp"
#include "AfricapolisConstants.hpp"
#include "EnumOption.hpp"


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

struct ClusteringParameters {
    ClusterMode mode;
    double distanceThreshold;
    std::optional<size_t> minClusterSize;   // DBSCAN and DBSC
    std::optional<size_t> beta;             // DBSC only
    std::optional<double> t1;               // DBSC only
    DBSCAttributeFunction attributeFunction = DBSCAttributeFunction::NONE; // DBSC only

    template<fishnet::graph::Graph G> requires(fishnet::geometry::Shape<typename G::node_type>)
    fishnet::ClusterAlgorithm_t<G> getSpatialClusterAlgorithm(DistanceFunction && distanceFunction) const {
        using T = typename G::node_type;
        switch(this->mode){
            case ClusterMode::DBSCAN:
                {
                    // the algorithm outlives this call, so the distance function is copied into it
                    return fishnet::DBSCAN<T>(this->distanceThreshold, Africapolis::requiredFor(this->minClusterSize,"--min-cluster-size",this->mode), [distanceFunction](const T & lhs, const T & rhs){
                        return fishnet::geometry::shapeDistance(lhs,rhs,distanceFunction);
                    });
                }
            case ClusterMode::BFS: 
                return fishnet::BFSClustering<T>(DistanceBiPredicate(std::move(distanceFunction), this->distanceThreshold));
            case ClusterMode::DBSC:
                return fishnet::DBSCBuilder<T>()
                    .setEps(this->distanceThreshold)
                    .setBeta(Africapolis::requiredFor(this->beta,"--beta",this->mode))
                    .setMinPts(Africapolis::requiredFor(this->minClusterSize,"--min-cluster-size",this->mode))
                    .setDistanceFunction([distanceFunction](const T & lhs, const T & rhs){
                        return fishnet::geometry::shapeDistance(lhs,rhs,distanceFunction);
                    })
                    .setAttributeExtractor(attributeMapper<T>(this->attributeFunction))
                    .setT1(this->t1.value_or(NAN))
                    .build();
        }
        throw std::runtime_error("Unsupported clustering mode");
    }
};

class SpatialClustering : public Task {
private: 
    ClusteringParameters parameters;
    std::vector<std::string> inputFilenames;
    std::filesystem::path graphFile;
    std::string outputStem;
public:

    SpatialClustering(
        ClusteringParameters parameters,
        std::vector<std::string> && inputFilenames,
        const std::filesystem::path & graphFile,
        std::string && outputStem
    ):Task("Clustering"), parameters(std::move(parameters)), inputFilenames(std::move(inputFilenames)), graphFile(graphFile), outputStem(std::move(outputStem)){}
    
    void run() {
        // Load shapes and settlement graph
        using ShapeType = fishnet::geometry::Polygon<double>;
        using SettlementType = SettlementShape<ShapeType>;
        auto vectorFiles = inputFilenames | std::views::transform([](const std::string & str){ return fishnet::AbstractVectorFile(str); });
        OGRSpatialReference spatialRef;
        auto onReadStoreSpatialRef = [&spatialRef](const fishnet::VectorLayer<ShapeType> & layer, const fishnet::AbstractVectorFile & vectorFile){
            if(spatialRef.IsEmpty()){
                spatialRef = layer.getSpatialReference();
            }
        };
        SettlementLayerReader<ShapeType> reader(onReadStoreSpatialRef);
        auto settlements = SettlementType::read<fishnet::AbstractVectorFile>(vectorFiles, reader,HashingFileReferenceMapper{});
        auto adj = ReadingBinarySettlementGraphAdjacency<SettlementType>(
            this->graphFile,
            SettlementShapeDeserializer<ShapeType>{std::move(settlements)}
        );
        auto graph = fishnet::graph::GraphFactory::UndirectedGraph(std::move(adj));

        // Run clustering
        auto clusterAlgorithm = parameters.getSpatialClusterAlgorithm<decltype(graph)>(distanceFunctionForSpatialReference(spatialRef));
        auto result = clusterAlgorithm(graph);

        // Store result
        auto outputLayer = fishnet::VectorIO::empty<ShapeType>(spatialRef);
        auto idField = outputLayer.addSizeField(Task::FISHNET_ID_FIELD).value_or_throw();
        auto clusterField = outputLayer.addSizeField(Africapolis::CLUSTER_ID_FIELD).value_or_throw();
        for(auto && cluster : result.clusters){
            size_t clusterID = std::ranges::fold_left(cluster, 0, [](size_t current, const auto & settlement){ return current + settlement.key(); });
            for(auto && settlement : cluster){
                auto feature = fishnet::Feature<ShapeType>(settlement.geometry());
                feature.setAttribute(idField, settlement.key());
                feature.setAttribute(clusterField, clusterID);
                outputLayer.addFeature(std::move(feature));
            }
        }
        for(auto && noise : result.noise){
            fishnet::Feature<ShapeType> feature(noise.geometry());
            feature.setAttribute(idField, noise.key());
            feature.setAttribute(clusterField, Africapolis::NOISE_CLUSTER_ID);
            outputLayer.addFeature(std::move(feature));
        }
        auto extension = (*std::ranges::begin(vectorFiles)).getPath().extension().string();
        auto outputPath = fishnet::util::PathHelper::absoluteCanonical(this->outputStem + extension);
        fishnet::VectorIO::overwrite(outputLayer, fishnet::AbstractVectorFile(outputPath));
    }
};

int main(int argc, char *argv[]){
    // Parse cmd arguments
    CLI::App app{"Africapolis Spatial Clustering"};
    std::vector<std::string> inputfiles;
    std::string graphFile;
    std::string outputStem;
    ClusteringParameters parameters;
    bool debug = false;
    app.add_option("-i,--inputs",inputfiles,"Input vector files storing the polygons with id for clustering")->required()->each(CLI::ExistingFile);
    app.add_option("-g,--graph",graphFile,"Graph file")->required()->check(CLI::ExistingFile);
    app.add_option("--outputStem", outputStem, "Output filename stem for storing the clustered vector file");
    app.add_option("--mode",parameters.mode,"Clustering algorithm applied to the settlement graph")
        ->required()
        ->transform(Africapolis::enumSymbols<ClusterMode>());
    app.add_option("--distance-threshold",parameters.distanceThreshold,"Maximum distance in meters between two settlements of the same cluster")
        ->required()
        ->check(CLI::PositiveNumber);
    app.add_option("--min-cluster-size",parameters.minClusterSize,"Minimum number of settlements per cluster. Settlements of smaller clusters are classified as noise. Required by the DBSCAN and DBSC modes")
        ->check(CLI::PositiveNumber);
    app.add_option("--beta",parameters.beta,"Order of the neighbourhood considered when trimming the settlement graph. Required by the DBSC mode")
        ->check(CLI::PositiveNumber);
    app.add_option("--t1",parameters.t1,"Custom t1 threshold of the DBSC heuristic. Derived from the data when omitted")
        ->check(CLI::PositiveNumber);
    app.add_option("--attribute-mapper",parameters.attributeFunction,"Settlement attribute combined with the spatial distance by the DBSC mode")
        ->transform(Africapolis::enumSymbols<DBSCAttributeFunction>());
    app.add_flag("--debug",debug,"Enable debug logging");
    CLI11_PARSE(app, argc, argv); 
    if(debug){
        spdlog::set_level(spdlog::level::debug);
    }
    SpatialClustering clusteringTask(
        std::move(parameters),
        std::move(inputfiles), 
        std::filesystem::path(graphFile),
        std::move(outputStem)
    );
    clusteringTask.run();
    return 0;
}
