cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisMSTVisualization]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
      coresMin: 1
      ramMin: 1000
inputs:
    geometryFiles:
        type: File[]
        secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
        doc: "Input shapefiles to visualize the edges of the graph"
        inputBinding:
            prefix: "-i"
    graphFile:
        type: File
        doc: "Input graph binary file"
        inputBinding:
            prefix: "-g"
    outputStem:
        type: string
        doc: "Output stem for the output shapefile"
        inputBinding:
            prefix: "-o"
outputs:
    mstShapefile:
        type: File
        secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
        outputBinding:
            glob: $(inputs.outputStem + "_mst.shp")
stdout: $(inputs.outputStem + "_mst.log")
stderr: $(inputs.outputStem + "_mst.err")