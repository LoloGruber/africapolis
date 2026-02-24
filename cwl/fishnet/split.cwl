cwlVersion: v1.2
class: CommandLineTool
baseCommand: [FishnetShapefileSplitter]
hints:
  DockerRequirement:
    dockerPull: logru/fishnet-apps:1.1.0
requirements:
- class: SchemaDefRequirement
  types:
    - $import: ../types/GeoTIFF.yaml
    - $import: ../types/Shapefile.yaml
- class: InlineJavascriptRequirement
inputs:
    gisFile:
        type: 
        # - ../GIS.cwl#Shapefile
        - ../types/GeoTIFF.yaml#GeoTIFF
        inputBinding:
            position: 1
            prefix: --input 
            valueFrom: $(self.file)
    outputDir:
        type: string
        default: "./"
        inputBinding:
            position: 3
            prefix: -o
        doc: "Output directory"
    splits:
        type: int
        inputBinding:
            position: 2
            prefix: -s
    xOffset:
        type: int
        default: 0
        inputBinding:
            prefix: -x
        doc: "X offset for the naming of the output tiles"
    yOffset:
        type: int
        default: 0
        inputBinding:
            prefix: -y
        doc: "Y offset for the naming of the output tiles"

outputs:
    standardOut:
        type: stdout
    errorOut:
        type: stderr
    split_shapefiles:
        type: ../types/Shapefile.yaml#Shapefile[]
        outputBinding:
            glob: "$(inputs.gisFile.file.nameroot)*"
            outputEval:
                $include: ../utils/groupToShapefile.js
        doc: "Split output files"


stdout: SPLIT_$(inputs.gisFile.file.nameroot)_stdout.log
stderr: SPLIT_$(inputs.gisFile.file.nameroot)_stderr.log
    