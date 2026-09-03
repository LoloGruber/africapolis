cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisGraphConstruction]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
    coresMin: 1
    ramMin: 4096
inputs:
  primaryInput:
    type: File
    # # format: GPKG
    inputBinding:
        prefix: -i
    doc: "Primary input, supplied as vector file object"
  additionalInput:
    type: File[]
    default: []
    # format: GPKG
    inputBinding:
        prefix: -a
    doc: "List of additional input vector files in proximity to the primary input, with their required secondary files (.dbf, .shx, .prj)"
  config:
    type: File
    inputBinding:
        prefix: -c
    doc: "Path to configuration for neighbours task. Contains graph database credentials, neighbouring criteria, ..."
outputs:
    graphBinary:
        type: File
        outputBinding:
            glob: "*.bin"
            outputEval: $(self[0])