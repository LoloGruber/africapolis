# pragma once
#include <fishnet/Fishnet.hpp>
#include <fishnet/SettlementShape.hpp>
#include <fishnet/BinaryAdjacency.hpp>
#include <fishnet/FileReference.hpp>

struct HashingFileReferenceMapper {
    FileReference operator()(const fishnet::AbstractVectorFile & vectorFile) const noexcept {
        return FileReference::hash(vectorFile.getPath().stem());
    }
};

template<typename S>
concept ISettlement = requires(S s){
    {s.key()} -> std::convertible_to<size_t>;
    {s.file()} -> std::convertible_to<FileReference>;
};

struct ProxySettlement {
    size_t id;
    FileReference fileRef;

    ProxySettlement(size_t id, const FileReference & fileRef) : id(id), fileRef(fileRef) {}

    size_t key() const noexcept {
        return id;
    }

    FileReference file() const noexcept {
        return fileRef;
    }

    bool operator==(const ProxySettlement & other) const noexcept {
        return id == other.id;
    }
};

namespace std {
    template<>
    struct hash<ProxySettlement> {
        size_t operator()(const ProxySettlement & settlement) const noexcept {
            return settlement.key();
        }
    };
}

static_assert(ISettlement<ProxySettlement>);

template<typename F, typename T>
concept Serializer = fishnet::util::UnaryFunction<F, T, std::vector<uint8_t>>;

template<typename F, typename T>
concept Deserializer = fishnet::util::UnaryFunction<F, std::vector<uint8_t>, T>;

struct DefaultSettlementSerializer {
    static std::vector<uint8_t> operator()(const ISettlement auto & settlement){
        std::vector<uint8_t> buffer;
        // Serialize ID
        size_t id = settlement.key();
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&id), reinterpret_cast<const uint8_t*>(&id) + sizeof(size_t));
        // Serialize FileReference
        FileReference fileRef = settlement.file();
        size_t fileId = fileRef.fileId;
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&fileId), reinterpret_cast<const uint8_t*>(&fileId) + sizeof(size_t));
        return buffer;
    }
};

struct ProxySettlementDeserializer {
    static ProxySettlement operator()(const std::vector<uint8_t> & buffer){
        if (buffer.size() != 2 * sizeof(size_t)) {
            throw std::runtime_error("Invalid buffer size");
        }

        // Deserialize ID
        size_t id;
        std::memcpy(&id, buffer.data(), sizeof(size_t));

        // Deserialize FileReference
        size_t fileId;
        std::memcpy(&fileId, buffer.data() + sizeof(size_t), sizeof(size_t));

        return ProxySettlement(id, FileReference(fileId));
    }
};

template<fishnet::geometry::Shape S>
struct SettlementShapeDeserializer {
    static ProxySettlementDeserializer proxyDeserializer;
    std::unordered_map<size_t, S> idToGeometryMap;

    SettlementShapeDeserializer(std::vector<SettlementShape<S>> && settlements) {
        for (auto && settlement : settlements) {
            auto key = settlement.key();
            idToGeometryMap.insert({key, std::move(settlement.geometry())});
        }
    }

    SettlementShapeDeserializer() = default;

    SettlementShape<S> operator()(const std::vector<uint8_t> & buffer){
        auto proxy = proxyDeserializer(buffer);
        // Create and return the SettlementShape
        auto geometry = std::move(idToGeometryMap.at(proxy.key()));
        idToGeometryMap.erase(proxy.key());
        return SettlementShape<S>(proxy.key(), proxy.file(), std::move(geometry));
    }
};

template<ISettlement Settlement>
class BinarySettlementGraphAdjacency : public fishnet::graph::BinaryAdjacency<fishnet::graph::AdjacencyMap<Settlement>> {
private:
    using Base = fishnet::graph::BinaryAdjacency<fishnet::graph::AdjacencyMap<Settlement>>;
    std::unordered_map<FileReference, std::filesystem::path> fileRefToPathMap;
public:
    BinarySettlementGraphAdjacency( 
        std::unordered_map<FileReference, std::filesystem::path> && fileRefToPathMap,
        Serializer<Settlement> auto && serializer,
        Deserializer<Settlement> auto && deserializer
    ):Base(fishnet::graph::AdjacencyMap<Settlement>(),std::forward<decltype(serializer)>(serializer),std::forward<decltype(deserializer)>(deserializer)),
        fileRefToPathMap(std::move(fileRefToPathMap)){}

    BinarySettlementGraphAdjacency(
        const std::vector<uint8_t> & data,
        Serializer<Settlement> auto && serializer,
        Deserializer<Settlement> auto && deserializer
    ):Base(fishnet::graph::AdjacencyMap<Settlement>(),std::forward<decltype(serializer)>(serializer),std::forward<decltype(deserializer)>(deserializer))
    {
            deserialize(data);
    }

    const std::unordered_map<FileReference, std::filesystem::path> & getFileRefToPathMap() const {
        return fileRefToPathMap;
    }

    std::unordered_map<FileReference, std::filesystem::path> & getFileRefToPathMap() {
        return fileRefToPathMap;
    }

    std::vector<uint8_t> serialize() const noexcept {
        std::vector<uint8_t> result;
        
        // Serialize fileRefToPathMap
        size_t mapSize = fileRefToPathMap.size();
        result.insert(result.end(), reinterpret_cast<const uint8_t*>(&mapSize), reinterpret_cast<const uint8_t*>(&mapSize) + sizeof(mapSize));
        
        for (const auto& [fileRef, path] : fileRefToPathMap) {
            size_t fileId = fileRef.fileId;
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&fileId), reinterpret_cast<const uint8_t*>(&fileId) + sizeof(fileId));
            
            std::string pathStr = fishnet::util::PathHelper::absoluteCanonical(path).string();
            size_t pathSize = pathStr.size();
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&pathSize), reinterpret_cast<const uint8_t*>(&pathSize) + sizeof(pathSize));
            result.insert(result.end(), pathStr.begin(), pathStr.end());
        }

        // Serialize the graph data
        auto data = Base::get();
        result.insert(result.end(), data.begin(), data.end());
        
        return result;
    }

    void deserialize(const std::vector<uint8_t> & data) {
        if(data.empty()){
            return;
        }

        size_t offset = 0;
        auto ensure_bytes = [&](size_t need){
            if(offset + need > data.size()){
                throw std::runtime_error("Binary data truncated");
            }
        };

        // Deserialize fileRefToPathMap
        ensure_bytes(sizeof(size_t));
        size_t mapSize;
        std::memcpy(&mapSize, data.data() + offset, sizeof(size_t));
        offset += sizeof(size_t);

        for (size_t i = 0; i < mapSize; ++i) {
            ensure_bytes(sizeof(size_t));
            size_t fileId;
            std::memcpy(&fileId, data.data() + offset, sizeof(size_t));
            offset += sizeof(size_t);

            ensure_bytes(sizeof(size_t));
            size_t pathSize;
            std::memcpy(&pathSize, data.data() + offset, sizeof(size_t));
            offset += sizeof(size_t);

            ensure_bytes(pathSize);
            std::string pathStr(reinterpret_cast<const char*>(data.data() + offset), pathSize);
            offset += pathSize;

            fileRefToPathMap[FileReference(fileId)] = std::filesystem::path(pathStr);
        }

        // Load the remaining binary data into the graph
        std::vector<uint8_t> graphData(data.begin() + offset, data.end());
        Base::load(graphData);
    }
};

template<ISettlement Settlement>
class WritingBinarySettlementGraphAdjacency: public BinarySettlementGraphAdjacency<Settlement> {
private: 
    std::filesystem::path outputPath;

    void write() const {
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for writing binary settlement graph adjacency");
        }
        // Serialize the graph data
        auto data = this->serialize();
        file.write(reinterpret_cast<const char*>(data.data()), data.size());

        file.close();
    }

public:
    WritingBinarySettlementGraphAdjacency(
        std::filesystem::path outputPath,
        std::unordered_map<FileReference,std::filesystem::path> fileRefToPathMap,
        Serializer<Settlement> auto && serializer,
        Deserializer<Settlement> auto && deserializer
    ):BinarySettlementGraphAdjacency<Settlement>(std::move(fileRefToPathMap),std::forward<decltype(serializer)>(serializer),std::forward<decltype(deserializer)>(deserializer)),
        outputPath(std::move(outputPath)){}

    ~WritingBinarySettlementGraphAdjacency() {
        write();
    }
};

template<ISettlement Settlement>
class ReadingBinarySettlementGraphAdjacency: public BinarySettlementGraphAdjacency<Settlement> {
private:
    std::vector<uint8_t> read(const std::filesystem::path & inputPath) const {
        std::ifstream file(inputPath, std::ios::binary);
        if (!file.is_open() || std::filesystem::is_empty(inputPath)) {
            throw std::runtime_error("Failed to open file for reading binary settlement graph adjacency or file is empty");
        }
        // Read the remaining binary data
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        return data;
    }

public:
    ReadingBinarySettlementGraphAdjacency(const std::filesystem::path & inputPath,
        Deserializer<Settlement> auto && deserializer
    ):BinarySettlementGraphAdjacency<Settlement>(read(inputPath), DefaultSettlementSerializer{}, std::forward<decltype(deserializer)>(deserializer)){}
};