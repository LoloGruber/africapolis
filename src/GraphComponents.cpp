#include <CLI/CLI.hpp>
#include <fishnet/Graph.hpp>
#include <fishnet/Task.hpp>
#include <fishnet/TaskConfig.hpp>
#include <fishnet/BidirectionalMap.hpp>
#include <unordered_set>
#include "BinarySettlementGraphAdjacency.hpp"

struct ClusterWorkload{
    std::vector<FileReference> files;
    std::vector<size_t> components;
};

struct ClusterWorkloadResult{
    std::vector<std::string> files;
    std::filesystem::path graphBinaryFile;
};

void to_json(nlohmann::json & j, const ClusterWorkloadResult & workloadResult) {
    j = nlohmann::json{
        {"files", workloadResult.files},
        {"graphBinaryFile", workloadResult.graphBinaryFile.string()}
    };
}

struct GraphComponentsConfig: TaskConfig{
    constexpr static const char * MAX_COMPONENTS_PER_WORKLOAD_KEY = "max-components-per-workload";
    size_t maxComponentsPerWorkload;

    GraphComponentsConfig(const nlohmann::json & configDescription): TaskConfig(configDescription){
        if(this->jsonDescription.contains(MAX_COMPONENTS_PER_WORKLOAD_KEY)){
            this->jsonDescription.at(MAX_COMPONENTS_PER_WORKLOAD_KEY).get_to(this->maxComponentsPerWorkload);
        }else {
            this->maxComponentsPerWorkload = 5000; // default value
        }
    }
};

class GraphComponents : public Task {
private:
    GraphComponentsConfig config;
    std::vector<std::filesystem::path> binGraphFiles;

    auto readInput() {
        auto fileIdToPathMap = std::unordered_map<FileReference, std::filesystem::path>{};
        auto graph = fishnet::graph::GraphFactory::UndirectedGraph<ProxySettlement>();
        for(const auto & binGraphFile : binGraphFiles) {
            auto subGraphAdjacency = ReadingBinarySettlementGraphAdjacency<ProxySettlement>(
                binGraphFile,
                ProxySettlementDeserializer{}
            );
            for(auto && [from,to]: subGraphAdjacency.getAdjacencyPairs()) {
                graph.addEdge(std::move(from), std::move(to));
            }
            graph.addNodes(std::move(subGraphAdjacency.nodes()));
            for(auto && [fileID, path] : subGraphAdjacency.getFileRefToPathMap()) {
                fileIdToPathMap.try_emplace(fileID, std::move(path));
            }
        }
        return std::make_pair(std::move(graph), std::move(fileIdToPathMap));
    }

    fishnet::util::BidirectionalMultiHashMap<FileReference, size_t> componentToFilesMap(const std::unordered_map<size_t, std::vector<ProxySettlement>> & componentToSettlements) {
        fishnet::util::BidirectionalMultiHashMap<FileReference, size_t> compToFilesMap;
        for (const auto& [componentID, settlements] : componentToSettlements) {
            auto fileRefsSet = fishnet::util::toUnorderedSet(settlements | std::views::transform([](const auto & settlement) { return settlement.file(); }));
            for(auto&& fileRef : fileRefsSet) {
                compToFilesMap.insert(std::move(fileRef), componentID);
            }
        }
        return compToFilesMap;
    }

    void splitAndInsertWorkload(std::vector<ClusterWorkload> & workloads, ClusterWorkload && workload) {
        if (workload.components.size() > config.maxComponentsPerWorkload) {
            size_t splitCount = (workload.components.size() + config.maxComponentsPerWorkload - 1) / config.maxComponentsPerWorkload;
            size_t componentsPerSplit = workload.components.size() / splitCount;
            auto it = workload.components.begin();
            for (size_t i = 0; i < splitCount; ++i) {
                size_t currentSplitSize = (i == splitCount - 1) ? workload.components.size() - (componentsPerSplit * i) : componentsPerSplit;
                std::vector<size_t> splitComponents(it, it + currentSplitSize);
                it += currentSplitSize;
                workloads.push_back(ClusterWorkload{
                    .files = workload.files,
                    .components = splitComponents
                });
            }
        } else {
            workloads.push_back(std::move(workload));
        }
    }

    void mergeSameFileSetWorkloads(std::vector<ClusterWorkload> & workloads) {
        auto fileSetHash = [](const std::unordered_set<FileReference> & fileSet) {
            size_t hash = 0;
            for (const auto & fileRef : fileSet) {
                hash ^= fileRef.fileId + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        };

        std::unordered_map<std::unordered_set<FileReference>, std::vector<size_t>, decltype(fileSetHash)> fileSetToComponentsMap;
        for (const auto & workload : workloads) {
            auto fileSet = fishnet::util::toUnorderedSet(workload.files);
            fileSetToComponentsMap[fileSet].insert(fileSetToComponentsMap[fileSet].end(), workload.components.begin(), workload.components.end());
        }
        workloads.clear();
        for (auto && [fileSet, components] : fileSetToComponentsMap) {
            workloads.push_back(ClusterWorkload{
                .files = fishnet::util::toVector(fileSet),
                .components = std::move(components)
            });
        }
    }

    ClusterWorkloadResult transformToWorkloadResult(const ClusterWorkload & workload, const std::unordered_map<FileReference, 
        std::filesystem::path> & fileIdToPathMap,
         const auto & graph,
         auto & components
    ) {
        std::filesystem::path outputGraphBinPath = std::filesystem::path("graph_component_" + std::to_string(workload.components.front()) + ".bin");
        std::unordered_map<FileReference, std::filesystem::path> fileIdToPathMapComponent;
        for (const auto & fileRef : workload.files) {
            fileIdToPathMapComponent[fileRef] = fishnet::util::PathHelper::absoluteCanonical(fileIdToPathMap.at(fileRef));
        }
        auto filePaths = fishnet::util::toVector(std::views::values(fileIdToPathMapComponent) | std::views::transform([](const auto & path) { return path.string(); }));
        // Build subgraph of component(s)
        auto subgraph = fishnet::graph::GraphFactory::UndirectedGraph(
            WritingBinarySettlementGraphAdjacency<ProxySettlement>(
            outputGraphBinPath,
            std::move(fileIdToPathMapComponent),
            DefaultSettlementSerializer{},
            ProxySettlementDeserializer{}
        ));
        for (const auto & component: workload.components) {
                subgraph.addNodes(std::move(components.at(component)));
        }
        for(const auto & node: subgraph.getNodes()) {
            for(const auto & neighbor : graph.getNeighbours(node)) {
                // check if neighbor not required in subgraph since we iterate over all nodes of a graph component 
                subgraph.addEdge(node, neighbor); 
            }
        }
        return ClusterWorkloadResult{.files = filePaths,.graphBinaryFile = std::move(outputGraphBinPath)};
    }

    void writeOutput(const fishnet::util::input_range_of<ClusterWorkloadResult> auto & workloadResults) {
        for (const auto & workload : workloadResults) {
            nlohmann::json outputJson = workload;
            std::ofstream outputFile(std::filesystem::path(workload.graphBinaryFile).replace_extension(".json"));
            outputFile << outputJson.dump(4);
            outputFile.close();
        }
    }

public: 
    GraphComponents(GraphComponentsConfig && config, std::vector<std::filesystem::path> binGraphFiles):Task("GraphComponents"), config(std::move(config)), binGraphFiles(std::move(binGraphFiles)){}

    void run() {
        spdlog::set_level(spdlog::level::debug);
        auto [graph, fileIdToPathMap] = readInput();
        spdlog::debug("Input read completed");
        const auto components = fishnet::graph::BFS::connectedComponents(graph).getAsMap();
        spdlog::debug("Connected components calculated");
        fishnet::util::BidirectionalMultiHashMap<FileReference, size_t> fileComponentMultiMap = componentToFilesMap(components);
        auto multiFileComponents = std::views::filter(fileComponentMultiMap.inverseKeySet(), [&fileComponentMultiMap](size_t component) {
            return fishnet::util::size(fileComponentMultiMap.get(component)) > 1;
        });
        std::vector<ClusterWorkload> workloads;
        // Insert multi-file component workloads, one workload per multi-file-component
        for(const auto & component:multiFileComponents) {
            workloads.push_back(ClusterWorkload{
                .files = fishnet::util::toVector(fileComponentMultiMap.getFrom(component)),
                .components = std::vector<size_t>{component}
            });
        }

        spdlog::debug("Multi-file components found: {}", workloads.size());
        mergeSameFileSetWorkloads(workloads);
        size_t multiFileWorkloadsCount = workloads.size();
        spdlog::debug("Multi-file component workloads created: {}", multiFileWorkloadsCount);
        // Insert single file component workloads, one workload processes all single-file-components of each file
        for(const auto & file:fileComponentMultiMap.keySet()) {
            auto componentsOfFile = fileComponentMultiMap.getTo(file);
            auto singleFileComponents = std::views::filter(componentsOfFile, [&](const auto & component) {
                return fishnet::util::size(fileComponentMultiMap.get(component)) == 1;
            });
            if (fishnet::util::isEmpty(singleFileComponents)) {
                continue;
            }
            splitAndInsertWorkload(workloads, ClusterWorkload {
                .files = {file},
                .components = fishnet::util::toVector(singleFileComponents)
            });
        }
        spdlog::debug("Single-file component workloads created: {}", workloads.size() - multiFileWorkloadsCount);
        // Transform workloads and export workload results
        writeOutput(std::views::transform(workloads, [&](const auto & workload) {
            return transformToWorkloadResult(workload, fileIdToPathMap, graph, components);
        }));
    }
};

int main(int argc, char * argv[]){
    spdlog::info("Starting GraphComponents binary");
    CLI::App app{"AfricapolisGraphComponents"};
    std::vector<std::string> binaryGraphFiles;
    std::string configFilename;
    app.add_option("-g,--graph-files", binaryGraphFiles, "Input binary graph files to partition into graph components")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-c,--config", configFilename, "Path to configuration file for graph components stage of Africapolis workflow")
        ->check(CLI::ExistingFile); // currently not required / used
    CLI11_PARSE(app, argc, argv);
    spdlog::info("Parsing command line arguments completed");
    std::vector<std::filesystem::path> binGraphPaths;
    for(auto && fileStr : binaryGraphFiles){
        binGraphPaths.push_back(std::filesystem::path(std::move(fileStr)));
    }
    GraphComponents task(
        GraphComponentsConfig(nlohmann::json::parse(std::ifstream(configFilename))),
        std::move(binGraphPaths)
    );
    task.run();
    return 0;
}