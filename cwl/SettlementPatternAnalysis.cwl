class: CommandLineTool
cwlVersion: v1.2
baseCommand: [python, SettlementPatternAnalysis.py]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis-python:latest
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
outputs:
  outputShapefile:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputBinding:
      glob: "$(inputs.shapefile.nameroot)*.shp"
    doc: "Output shapefile with polygon outlines"
stdout: OUTLINE_$(inputs.shapefile.nameroot)_stdout.log
stderr: OUTLINE_$(inputs.shapefile.nameroot)_stderr.log