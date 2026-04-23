cwlVersion: v1.2
class: CommandLineTool
baseCommand: [FishnetShapefilePreprocessor]
hints:
  DockerRequirement:
    dockerPull: logru/fishnet-apps:1.2.1
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
    coresMin: 1
    ramMin: 250
inputs:
  shapefile:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    inputBinding:
      prefix: --input
  config:
    type: File
    # format: JSON
    doc: "Configuration file for filter process"
    inputBinding:
      prefix: --config
outputs:
  filtered_shapefile:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputBinding:
      glob: "*_filtered.shp"  # Gather all files associate with the shapefile
stdout: FILTER_$(inputs.shapefile.nameroot)_stdout.log
stderr: FILTER_$(inputs.shapefile.nameroot)_stderr.log
