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
  shapefile:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    inputBinding:
      prefix: -i
  mstFile:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
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
  outputShapefile:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputBinding:
      glob: "*.shp"
    doc: "Output shapefile with polygon outlines"
stdout: OUTLINE_$(inputs.shapefile.nameroot)_stdout.log
stderr: OUTLINE_$(inputs.shapefile.nameroot)_stderr.log
