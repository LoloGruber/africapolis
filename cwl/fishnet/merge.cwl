cwlVersion: v1.2
class: CommandLineTool
baseCommand: [FishnetShapefileMerger]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:1.0.0
requirements:
  - class: InlineJavascriptRequirement
  - class: SchemaDefRequirement
    types: 
      - $import: ../types/Shapefile.yaml
inputs:
    shpFiles:
        type: 
          type: array 
          items: ../types/Shapefile.yaml#Shapefile
          inputBinding: 
            valueFrom: $(self.file)
        inputBinding:
            prefix: -i
        doc: "List of input shapefiles, with their required secondary files (.dbf, .shx, .prj)"
    outputPath:
        type: string
        inputBinding:
            position: 2
            prefix: -o 
            valueFrom: $(self+".shp")
        doc: "Output filename for result (Shapefile)"  
outputs:
    standardOut:
        type: stdout
    errorOut:
        type: stderr
    mergedOutput:
        type: ../types/Shapefile.yaml#Shapefile
        outputBinding:
            glob: "$(inputs.outputPath).*"
            outputEval:
                $include: ../utils/groupToShapefile.js
        doc: "Merged output file"
stdout: MERGE_stdout.log
stderr: MERGE_stderr.log