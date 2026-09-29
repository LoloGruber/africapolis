cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisEdgeVisualization]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
    coresMin: 1
    ramMin: 4096
inputs:
  geometryFiles:
    type: File[]
    doc: "Input vector files storing the settlements of the graph"
    inputBinding:
      prefix: -i
  graphFile:
    type: File
    # format: BIN
    doc: "Input binary file storing the settlement graph adjacency"
    inputBinding:
      prefix: -g
  outputStem:
    type: string
    doc: "Output filename stem for storing the edges of the graph"
    inputBinding:
      prefix: -o
outputs:
  edgeShapefile:
    type: File
    outputBinding:
      glob: $(inputs.outputStem + "_edges.*")
    doc: "Vector file storing one polygon per edge of the settlement graph"
