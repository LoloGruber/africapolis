cwlVersion: v1.2
class: CommandLineTool
baseCommand: [FishnetVectorFileSplitter]

hints:
  DockerRequirement:
    dockerPull: logru/fishnet-apps:1.4.0

requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
    coresMin: 1
    ramMin: 1000

inputs:
  shapefile:
    type: File?
    secondaryFiles:
      - "^.shx"
      - "^.dbf"
      - "^.prj"
      - "^.cpg?"
      - "^.qpj?"
    inputBinding:
      prefix: --input
    doc: "Input shapefile with required secondary files (.shx, .dbf, .prj)"

  vectorFile:
    type: File?
    # format: GPKG
    inputBinding:
      prefix: --input
    doc: "Input GeoPackage file (no secondary files required)"

  outputDir:
    type: string
    default: "./"
    inputBinding:
      prefix: -o
    doc: "Output directory"

  outputFormat:
    type: string
    default: "GEOPACKAGE"
    inputBinding:
      prefix: -f
    doc: "Output format (SHAPEFILE or GEOPACKAGE)"

  depth:
    type: int
    inputBinding:
      prefix: --depth

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
  split_files:
    type: File[]
    secondaryFiles:
      - "^.shx?"
      - "^.dbf?"
      - "^.prj?"
      - "^.cpg?"
      - "^.qpj?"
    outputBinding:
      glob:
        - "$((inputs.shapefile|| inputs.vectorFile).nameroot)*.shp"
        - "$((inputs.shapefile|| inputs.vectorFile).nameroot)*.gpkg"
    doc: "Split output spatial files (.shp or .gpkg)"

stdout: SPLIT_$((inputs.shapefile|| inputs.vectorFile).nameroot)_stdout.log
stderr: SPLIT_$((inputs.shapefile|| inputs.vectorFile).nameroot)_stderr.log