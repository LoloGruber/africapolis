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
      ramMin: 1000
inputs:
  shpFiles:
    type: File[]
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    inputBinding:
        prefix: -i
    doc: "List of input shapefiles, with their required secondary files (.dbf, .shx, .prj)"
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
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputBinding:
        glob: "$(inputs.outputStem).shp"
    doc: "Clustering output shapefile"
stdout: CLUSTER_$(inputs.graphBinary.basename)_stdout.log
stderr: CLUSTER_$(inputs.graphBinary.basename)_stderr.log