cwlVersion: v1.2
class: Workflow
requirements:
- class: InlineJavascriptRequirement
- class: StepInputExpressionRequirement
- class: SubworkflowFeatureRequirement

inputs:
    shapefile:
        type: File
        secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
        doc: "Input shapefile to visualize the edges of the graph"
    graphConstructionMode:
        type:
          type: enum
          symbols: [BUFFER_SWEEP, DELAUNAY]
        default: DELAUNAY
        doc: "Method used to connect neighbouring settlements"
    graphDistanceThreshold:
        type: float
        default: 200.0
        doc: "Maximum distance in meters between two settlements sharing an edge"
    maxNeighborsPerNode:
        type: int?
        doc: "Maximum number of edges per settlement. Required by the BUFFER_SWEEP mode"
    debug:
        type: boolean
        default: false
        doc: "Enable debug logging"
outputs:
    edges_shapefile:
        type: File
        outputSource: edge_visualization/edgeShapefile
steps:
    graph_generation:
        run: GraphConstructionTool.cwl
        in: 
            primaryInput: shapefile
            mode: graphConstructionMode
            distanceThreshold: graphDistanceThreshold
            maxNeighborsPerNode: maxNeighborsPerNode
            debug: debug
        out: [graphBinary]
    edge_visualization:
        run: EdgeVisualization.cwl
        in:
            geometryFiles:
                source: shapefile
                valueFrom: $([self])
            graphFile: graph_generation/graphBinary
            outputStem:
                source: shapefile
                valueFrom: $(self.nameroot)
        out: [edgeShapefile]
