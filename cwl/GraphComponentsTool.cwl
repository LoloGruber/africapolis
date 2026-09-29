cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisGraphComponents]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  ResourceRequirement:
    coresMin: 1
    ramMin: 4096
inputs:
  graphBinaries:
    type: File[]
    inputBinding:
      prefix: -g
  maxComponentsPerWorkload:
    type: int?
    inputBinding:
      prefix: --max-components-per-workload
    doc: "Maximum number of graph components assigned to a single clustering task. Defaults to 5000 when omitted"
  debug:
    type: boolean
    default: false
    inputBinding:
      prefix: --debug
    doc: "Enable debug logging"
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
