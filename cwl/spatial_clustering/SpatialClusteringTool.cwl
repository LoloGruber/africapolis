cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisSpatialClustering]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:1.1.0
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
    config:
        type: File
        inputBinding:
            prefix: -c
        doc: "Path to configuration for contraction task"
    graphBinary:
        type: File
        inputBinding:
            prefix: -g
        doc: "Binary file containing the graph structure of the components to be clustered"
    outputStem:
        type: string
        inputBinding:
            position: 2
            prefix: --outputStem 
        doc: "Output filename storing the merged polygons"     
outputs:
    standardOut:
        type: stdout
    errorOut:
        type: stderr
    clusteredOutput:
        type: ../types/Shapefile.yaml#Shapefile
        outputBinding:
            glob: "$(inputs.outputStem).*"
            outputEval:
                $include: ../utils/groupToShapefile.js
        doc: "Merged output file"
stdout: CLUSTER_$(inputs.graphBinary.basename)_stdout.log
stderr: CLUSTER_$(inputs.graphBinary.basename)_stderr.log