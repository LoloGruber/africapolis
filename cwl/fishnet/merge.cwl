cwlVersion: v1.2
class: CommandLineTool
baseCommand: [FishnetVectorFileMerger]
hints:
  DockerRequirement:
    dockerPull: logru/fishnet-apps:1.4.0
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
    coresMin: 1
    ramMin: 1000
inputs:
  vectorFiles:
    type: File[]
    # format: GPKG
    inputBinding:
        prefix: -i
    doc: "List of input vector files, with their required secondary files (.dbf, .shx, .prj)"
  outputPath:
    type: string
    inputBinding:
        position: 2
        prefix: -o 
        valueFrom: $(self+".gpkg")
    doc: "Output filename for result (Vectorfile)"  
outputs:
  mergedOutput:
    type: File
    # format: GPKG
    outputBinding:
        glob: "$(inputs.outputPath).gpkg"
    doc: "Merged output file"
stdout: MERGE_stdout.log
stderr: MERGE_stderr.log