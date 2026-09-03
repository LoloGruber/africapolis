class: CommandLineTool
cwlVersion: v1.2
baseCommand: [AfricapolisPolygonOutline]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
    coresMin: 1
    ramMin: 1000
inputs:
  vectorFile:
    type: File
    # format: GPKG
    inputBinding:
      prefix: -i
  mstFile:
    type: File
    # format: GPKG
    inputBinding:
      prefix: -m
  initialBuffer:
    type: float
    default: 100.0
    inputBinding:
      position: 2
      prefix: --initial-buffer
  buffer:
    type: float
    default: 30.0
    inputBinding:
      position: 3
      prefix: --buffer
outputs:
  outlineVectorFile:
    type: File
    # format: GPKG
    outputBinding:
      glob: "*.gpkg"
    doc: "Output vector file with polygon outlines"
