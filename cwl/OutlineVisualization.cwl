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
  alpha:
    type: float
    default: 0.05
    inputBinding:
      position: 2
      prefix: --alpha
outputs:
  outputShapefile:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputBinding:
      glob: "$(inputs.shapefile.nameroot)*.shp"
    doc: "Output shapefile with polygon outlines"
  standardOut:
    type: stdout
  errorOut:
    type: stderr
stdout: OUTLINE_$(inputs.shapefile.nameroot)_stdout.log
stderr: OUTLINE_$(inputs.shapefile.nameroot)_stderr.log