class: CommandLineTool
cwlVersion: v1.2
baseCommand: [AfricapolisPolygonOutline]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:1.0.0
requirements:
  - class: InlineJavascriptRequirement
  - class: SchemaDefRequirement
    types:
      - $import: types/Shapefile.yaml
inputs:
  gisFile:
    type: types/Shapefile.yaml#Shapefile
    inputBinding:
      position: 1
      prefix: -i
      valueFrom: $(self.file)
outputs:
  outputShapefile:
    type: types/Shapefile.yaml#Shapefile
    outputBinding:
      glob: "$(inputs.gisFile.file.nameroot)*"
      outputEval: 
        $include: utils/groupToShapefile.js
    doc: "Output shapefile with polygon outlines"
  standardOut:
    type: stdout
  errorOut:
    type: stderr
stdout: OUTLINE_$(inputs.gisFile.file.nameroot)_stdout.log
stderr: OUTLINE_$(inputs.gisFile.file.nameroot)_stderr.log