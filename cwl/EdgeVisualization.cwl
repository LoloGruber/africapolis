cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisEdgeVisualization]
inputs:
    geometryFile:
        type: File
        secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
        doc: "Input shapefile to visualize the edges of the graph"
        inputBinding:
            prefix: "-i"
    graphFile:
        type: File
        doc: "Input graph binary file"
        inputBinding:
            prefix: "-g"
outputs:
    edgeShapefile:
        type: File
        secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
        outputBinding:
            glob: $(inputs.geometryFile.nameroot + "_edges.shp")