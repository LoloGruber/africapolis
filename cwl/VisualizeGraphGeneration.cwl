cwlVersion: v1.2
class: Workflow
requirements:
- class: InlineJavascriptRequirement
- class: SubworkflowFeatureRequirement

inputs:
    shapefile:
        type: File
        secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
        doc: "Input shapefile to visualize the edges of the graph"
    configFile:
        type: File
        doc: "Configuration file for Africapolis workflow"
outputs:
    edges_shapefile:
        type: File
        secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
        outputSource: edge_visualization/edgeShapefile
steps:
    graph_generation:
        run: graph_generation/GraphGenerationTool.cwl
        in: 
            primaryInput: [shapefile]
            config: configFile
        out: [graphBinary]
    edge_visualization:
        run: EdgeVisualization.cwl
        in:
            geometryFile: shapefile
            graphFile: graph_generation/graphBinary 
        out: [edgeShapefile]
