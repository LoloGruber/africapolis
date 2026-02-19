#include <fishnet/Fishnet.hpp>
#include <CLI/CLI.hpp>
using MultiPolygon_t = fishnet::geometry::MultiPolygon<fishnet::geometry::Polygon<double>>;
using Polygon_t = fishnet::geometry::SimplePolygon<double>;
using Number_t= typename MultiPolygon_t::numeric_type;

#include <ogr_geometry.h>

auto concaveHull(const MultiPolygon_t & multiPolygon){
    OGRMultiPoint multiPoint;
    std::vector<std::unique_ptr<OGRPoint>> points;

    for(const auto & polygon : multiPolygon.getPolygons()) {
        for(const auto & point : polygon.getBoundary().getPoints()) {
            multiPoint.addGeometry(std::make_unique<OGRPoint>(point.x, point.y));
        }
    }
    double alpha = sqrt(multiPolygon.area() / multiPolygon.aaBB().area());
    OGRPolygon * concaveHull = multiPoint.ConcaveHull(alpha,true)->toPolygon();
    auto polygon = concaveHull== nullptr? std::nullopt : fishnet::OGRGeometryAdapter::fromOGR(*concaveHull);
    free(concaveHull);
    return polygon; 
}


template<fishnet::math::Number T>
struct DistanceFromReference{
    fishnet::geometry::Vec2D<T> referencePoint;
    constexpr DistanceFromReference(const fishnet::geometry::Vec2D<T> & referencePoint) noexcept : referencePoint(referencePoint) {}
    constexpr T operator()(const fishnet::geometry::Vec2D<T> & point) const noexcept {
        return point.distance(referencePoint);
    }
};

static const fishnet::geometry::Vec2D<Number_t> INFTY = fishnet::geometry::Vec2D<Number_t>(std::numeric_limits<Number_t>::max(), std::numeric_limits<Number_t>::max());

Polygon_t convexHull(const MultiPolygon_t & multiPolygon) {
    using PointType = fishnet::geometry::Vec2D<typename MultiPolygon_t::numeric_type>;
    auto nodes = multiPolygon.getPolygons() 
        | std::views::transform([](const auto & polygon) { return polygon.getBoundary().getPoints(); }) 
        | std::views::join;
    std::vector<PointType> points;
    for (const auto & point : nodes) {
        points.push_back(point);
    }
    auto n = (int) points.size();
    // Jarvis Walk Algo:
    int l = 0;
    for (int i = 1; i < n; i++)
        if (points[i].x < points[l].x)
            l = i;

    std::vector<PointType> hull;
    int p = l, q;
    do
    {
        hull.push_back(points[p]);
        q = (p+1)%n;
        for (int i = 0; i < n; i++){
           // If points[i] is more counterclockwise than current q, then update q
           if ((points[q]-points[p]).cross(points[i]-points[p]) > 0) {
                q = i;
            }
        }
        p = q;

    } while (p != l);
    return Polygon_t{fishnet::geometry::Ring<Number_t>{std::move(hull)}}; // Create a polygon from the hull points
}

Polygon_t toBoundaryPolygon(const MultiPolygon_t & multiPolygon) {
    using PointType = fishnet::geometry::Vec2D<typename MultiPolygon_t::numeric_type>;
    auto graph = fishnet::graph::GraphFactory::DirectedGraph<PointType>();
    auto segmentsView = multiPolygon.getPolygons()
            |std::views::transform([](const auto & polygon){return polygon.getBoundary().getSegments();}) 
            | std::views::join;
    std::vector<fishnet::geometry::Segment<Number_t>> segments;
    for (auto&& seg : segmentsView) {
        segments.push_back(std::move(seg)); 
    }
    for(const auto & polygon : multiPolygon.getPolygons()){
        for(const auto & current : polygon.getBoundary().getSegments()){
            if(not current.isValid()) continue; // skip invalid segments
            auto line = current.toLine();
            auto closestToP = INFTY;
            auto edgeOfClosestToP = current; // to be overwritten
            auto closestToQ = INFTY;
            auto edgeOfClosestToQ = current; // to be overwritten
            for(const auto & segment: segments){
                if(polygon.getBoundary().contains(segment) || not segment.isValid()) continue;
                auto intersection = segment.intersection(line);
                if(not intersection) continue; // skip if no intersection
                auto distanceToP = intersection.value().distance(current.p());
                auto distanceToQ = intersection.value().distance(current.q());
                if(distanceToP < distanceToQ && distanceToP < closestToP.distance(current.p())) {
                    closestToP = intersection.value();
                    edgeOfClosestToP = segment; // store the segment that is closest to p
                }
                if(distanceToQ < distanceToP && distanceToQ < closestToQ.distance(current.q())) {
                    closestToQ = intersection.value();
                    edgeOfClosestToQ = segment; // store the segment that is closest to q
                }
            }

            // for(const auto & rhs: multiPolygon.getPolygons()){
            //     if(lhs == rhs) continue; // skip self
            //     for(const auto & segment : rhs.getBoundary().getSegments()){
                    
            //     }
            // }


            if(closestToP != INFTY) {
                graph.addEdge(current.p(), closestToP); // add edge to the graph for the intersection point and the closest endpoint of the other segment
                graph.addEdge(closestToP, current.p()); // add edge to the graph for the intersection point and the closest endpoint of the other segment
                segments.push_back({closestToP, current.p()}); // add the intersection point to the segments
                if(not edgeOfClosestToP.isEndpoint(closestToP)){
                    graph.addEdge(edgeOfClosestToP.p(),closestToP);
                    graph.addEdge(closestToP, edgeOfClosestToP.p());
                }
            }
            if(closestToQ != INFTY && closestToQ != closestToP){
                graph.addEdge(current.q(), closestToQ); // add edge to the graph for the intersection point and the closest endpoint of the other segment
                graph.addEdge(closestToQ, current.q()); // add edge to the graph for the intersection point and the closest endpoint of the other segment
                segments.push_back({current.q(), closestToQ}); // add the intersection point to the segments
                if(not edgeOfClosestToQ.isEndpoint(closestToQ)){
                    graph.addEdge(edgeOfClosestToQ.p(),closestToQ);
                    graph.addEdge(closestToQ, edgeOfClosestToQ.q());
                }
            }
            graph.addEdge(current.p(), current.q()); 
        }
    }
    

    // for(const auto & segment : segments) {
    //     if(not segment.isValid()) continue; // skip invalid segments
    //     std::vector<PointType> segmentIntersections;
    //     auto line = segment.toLine();


    //     segmentIntersections.push_back(segment.p());
    //     for(const auto & other: segments){
    //         if(segment == other || not other.isValid()) continue; // skip self
    //         auto intersection = segment.intersection(other.toLine()); // find intersection and infinite line of other segment
    //         if(intersection) {
    //             auto intersectionPoint = intersection.value();
    //             segmentIntersections.push_back(intersectionPoint); // add intersection point to the current segment intersections
    //             auto endPointOnOtherSegment = other.p().distance(intersectionPoint) < other.q().distance(intersectionPoint) ? other.p() : other.q();
    //             graph.addEdge(intersectionPoint, endPointOnOtherSegment); // add edge to the graph for the intersection point and the closest endpoint of the other segment
    //             graph.addEdge(endPointOnOtherSegment, intersectionPoint); // add edge to the graph for the intersection point and the closest endpoint of the other segment
    //         }
    //     }
    //     segmentIntersections.push_back(segment.q());
    //     DistanceFromReference<Number_t> distanceFromReference(segment.p());
    //     // Sort the intersections by distance to the start point of the segment
    //     std::ranges::sort(segmentIntersections,std::less<Number_t>(),distanceFromReference);
    //     for(size_t i = 0; i < segmentIntersections.size() - 1; ++i) {
    //         graph.addEdge(segmentIntersections[i], segmentIntersections[i + 1]);
    //     }
    // }

    PointType topLeft = *std::ranges::max_element(graph.getNodes(),[](const auto & lhs, const auto & rhs) {
        return (lhs.y < rhs.y) || (lhs.y == rhs.y && lhs.x > rhs.x);
    });
    auto filteredNeighbours = graph.getNeighbours(topLeft) | std::views::filter([&topLeft](const auto & neighbour){
        return neighbour.x > topLeft.x; // filter out neighbours that are not to the right of the top left point
    });
    PointType p = topLeft;
    PointType q = *std::ranges::min_element(
        filteredNeighbours,
        [&topLeft](const auto & lhs, const auto & rhs) {
            return (lhs - topLeft).y < (rhs - topLeft).y; // find neighbour with smallest slope to the top left point 
        }
    );
    std::vector<PointType> outlinePoints;
    outlinePoints.push_back(p); // start with the top left point (first p)
    while(q != topLeft) {
        auto neighbours = graph.getNeighbours(q) | std::views::filter([p,q](const auto & r) {
            return r != p && r != q && (not (q-p).isParallel(r-p) || fishnet::geometry::Ray(p,q-p).contains(r) ); // filter out p and q and collinear points on the backward direction
        });
        for(const auto & neighbour : neighbours) {
            std::cout << neighbour.toString() << " ";
        }
        std::cout << std::endl;
        auto angleOffset = q.angle(p); // angle pq to x axis
        auto r = *std::ranges::max_element(neighbours, [&p,&angleOffset,&q](const auto& lhs, const auto& rhs) {
            auto angleL = lhs.angle(p) - angleOffset ; // Angle between pr and pq
            auto angleR = rhs.angle(p) - angleOffset; 
            // Prevent angles over 180 degrees to be a maximum value
            if (angleR >= fishnet::math::Radians::PI && angleL >= fishnet::math::Radians::PI){
                return angleL < angleR; 
            }
            if(angleL >= fishnet::math::Radians::PI){
                return true;
            }
            if (angleR >= fishnet::math::Radians::PI) {
                return false;
            }
            if(fishnet::math::areEqual(angleL.getAngleValue(), angleR.getAngleValue())){
                return lhs.distance(q) > rhs.distance(q); //make max element the closest point to q if angles are equal
            }
            return angleL < angleR;
        });
        if((q-p).cross(r-p) != 0){ // prevent insertion of collinear points
            outlinePoints.push_back(q);
        }
        p = q;
        q = r;
    }
    return Polygon_t{fishnet::geometry::Ring<Number_t>{std::move(outlinePoints)}}; // Create a polygon from the outline points
}

void toyExample() {
    using namespace fishnet::geometry;
    Polygon<double> r1 {Ring{Vec2DStd{0,0},Vec2DStd{0,2},Vec2DStd{2,2},Vec2DStd{2,0}}};
    Polygon<double> r2 {Ring{Vec2DStd{3,1},Vec2DStd{4,1},Vec2DStd{4,-1},Vec2DStd{3,-1}}};
    MultiPolygon<Polygon<double>> multiPolygon {r1,r2};
    auto boundaryPolygon = toBoundaryPolygon(multiPolygon);
    std::cout << "Boundary Polygon: " << boundaryPolygon.toString() << std::endl;
}

int main(int argc, char* argv[]) {
    using namespace fishnet::geometry;
    CLI::App app{"AfricapolisPolygonOutline"};
    std::string inputFilename;// = "/home/lolo/Desktop/uganda_lira_100m/Uganda_original_102023_Lira_Africapolis.shp";
    app.add_option("-i,--input", inputFilename, "Path to input shape file")->required()->check(CLI::ExistingFile);
    CLI11_PARSE(app, argc, argv);
    fishnet::Shapefile inputFile(inputFilename);
    auto inputLayer = fishnet::VectorIO::read<MultiPolygon_t>(inputFile);
    auto outputLayer = fishnet::VectorIO::emptyCopy<Polygon_t>(inputLayer);
    for(const auto & multiPolygonFeature: inputLayer.getFeatures()) {
        auto resultGeometry = concaveHull(multiPolygonFeature.getGeometry());
        if(not resultGeometry) {
            std::cout << "No concave hull found for feature: " << multiPolygonFeature.getGeometry() << std::endl;
            continue;
        }
        auto feature = fishnet::Feature<Polygon_t>(resultGeometry.value());
        feature.copyAttributes(multiPolygonFeature);
        outputLayer.addFeature(std::move(feature));
    }
    fishnet::Shapefile outputFile = {fishnet::util::PathHelper::appendToFilename(inputFile.getPath(), "_concave_hull").filename()};
    fishnet::VectorIO::overwrite(outputLayer, outputFile);
    return 0;
}