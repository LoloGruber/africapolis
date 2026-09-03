cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisGraphComponents]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  ResourceRequirement:
    coresMin: 1
    ramMin: 1000
inputs:
  config:
    type: File
    doc: "Path to configuration file for africapolis components step. Contains database credentials and parallelization target"
    inputBinding:
      prefix: -c
  graphBinaries:
    type: File[]
    inputBinding:
      prefix: -g
outputs:
  clusterWorkloadFiles:
    type: File[]
    outputBinding:
      glob: "*.json"
    doc: "Output file containing the associations of shapefiles to graph binaries of the graph"
  graphWorkloadFiles:
    type: File[]
    outputBinding:
      glob: "*.bin"
    doc: "Output file containing the binary representation of the graph components, to be used as input for the clustering step"