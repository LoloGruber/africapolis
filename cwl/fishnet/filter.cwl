cwlVersion: v1.2
class: CommandLineTool
baseCommand: [FishnetVectorFilePreprocessor]
hints:
  DockerRequirement:
    dockerPull: logru/fishnet-apps:1.4.0
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
    coresMin: 1
    ramMin: 250
inputs:
  vectorFile:
    type: File
    # format: GPKG
    inputBinding:
      prefix: --input
  config:
    type: File
    # format: JSON
    doc: "Configuration file for filter process"
    inputBinding:
      prefix: --config
  skipFilter:
    type: boolean
    default: false
    doc: "Skip the filtering process and return the input shapefile with Fishnet IDs as output"
    inputBinding:
      prefix: --no-filter
outputs:
  filteredVectorFile:
    type: File
    # format: GPKG
    outputBinding:
      glob: "*_filtered.gpkg"  # Gather all files associate with the vector file
stdout: FILTER_$(inputs.vectorFile.nameroot)_stdout.log
stderr: FILTER_$(inputs.vectorFile.nameroot)_stderr.log
