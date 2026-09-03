cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisSpatialClustering]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
      coresMin: 1
      ramMin: 4096
inputs:
  vectorFiles:
    type: File[]
    # format: GPKG
    inputBinding:
        prefix: -i
    doc: "List of input vector files, with their required secondary files (.dbf, .shx, .prj)"
  config:
    type: File
    # format: JSON
    inputBinding:
        prefix: -c
    doc: "Path to configuration for contraction task"
  graphBinary:
    type: File
    # format: BIN
    inputBinding:
        prefix: -g
    doc: "Binary file containing the graph structure of the components to be clustered"
  outputStem:
    type: string
    inputBinding:
        prefix: --outputStem 
    doc: "Output filename storing the merged polygons"     
outputs:
  clusteredOutput:
    type: File
    # format: GPKG
    outputBinding:
        glob: "$(inputs.outputStem).gpkg"
    doc: "Clustering output vector file"