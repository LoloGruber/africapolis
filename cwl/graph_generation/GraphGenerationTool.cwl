cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisGraphConstruction]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:1.1.0
requirements:
    - class: SchemaDefRequirement
      types: 
        - $import: ../types/Shapefile.yaml
    - class: InlineJavascriptRequirement
inputs:
  primaryInput:
    type: ../types/Shapefile.yaml#Shapefile
    inputBinding:
        prefix: -i
        valueFrom: $(self.file)
    doc: "Primary input, supplied as shapefile object"
  additionalInput:
    type: 
      type: array
      items: ../types/Shapefile.yaml#Shapefile
      inputBinding:
        valueFrom: $(self.file)
    inputBinding:
        prefix: -a
    doc: "List of additional input shapefiles in proximity to the primary input, with their required secondary files (.dbf, .shx, .prj)"
  config:
    type: File
    inputBinding:
        prefix: -c
    doc: "Path to configuration for neighbours task. Contains graph database credentials, neighbouring criteria, ..."
outputs:
    standardOut:
        type: stdout
    errorOut:
        type: stderr
    graphBinary:
        type: File
        outputBinding:
            glob: "*.bin"
            outputEval: $(self[0])
            
stdout: GENERATE_GRAPH_$(inputs.primaryInput.file.nameroot)_stdout.log
stderr: GENERATE_GRAPH_$(inputs.primaryInput.file.nameroot)_stderr.log