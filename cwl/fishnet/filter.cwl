cwlVersion: v1.2
class: CommandLineTool
baseCommand: [FishnetShapefilePreprocessor]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:1.0.0
requirements:
  - class: SchemaDefRequirement
    types:
    - $import: ../types/Shapefile.yaml
  - class: InlineJavascriptRequirement
inputs:
  gisFile:
    type: 
      # - ../GIS.cwl#GeoTIFF
      - ../types/Shapefile.yaml#Shapefile
    inputBinding:
      position: 1
      prefix: --input
      valueFrom: $(self.file)

  config:
    type: File
    doc: "Configuration file for filter process"
    inputBinding:
      prefix: --config
      position: 2
outputs:
  standardOut:
    type: stdout
  errorOut:
    type: stderr
  filtered_shapefile:
    type: ../types/Shapefile.yaml#Shapefile
    outputBinding:
      glob: "*_filtered.*"  # Gather all files associate with the shapefile
      outputEval:
        $include: ../utils/groupToShapefile.js 
stdout: FILTER_$(inputs.gisFile.file.nameroot)_stdout.log
stderr: FILTER_$(inputs.gisFile.file.nameroot)_stderr.log
