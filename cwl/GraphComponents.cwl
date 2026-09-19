cwlVersion: v1.2
class: Workflow
requirements:
  - class: SchemaDefRequirement
    types: 
      - $import: ComponentsOutput.yaml
  - class: InlineJavascriptRequirement
inputs:
  graphBinaries:
    type: File[]
    # format: BIN
    doc: "List of graph binary files generated in the graph generation step"
  maxComponentsPerWorkload:
    type: int?
    doc: "Maximum number of graph components assigned to a single clustering task"
  debug:
    type: boolean
    default: false
    doc: "Enable debug logging"
outputs:
  componentsOutput:
    type: ComponentsOutput.yaml#ComponentsOutput[]
    outputSource: post_components_output/componentsOutput
steps:
  graph_components:
    run: GraphComponentsTool.cwl
    in:
      graphBinaries: graphBinaries
      maxComponentsPerWorkload: maxComponentsPerWorkload
      debug: debug
    out: [clusterWorkloadFiles,graphWorkloadFiles]
  post_components_output:
    run: 
      class: ExpressionTool
      requirements:
        - class: InlineJavascriptRequirement
      inputs:
        clusterWorkloadFiles: File[]
        graphWorkloadFiles: File[]
      outputs:
        componentsOutput:
          type: ComponentsOutput.yaml#ComponentsOutput[]
      expression: |
        ${
          function filesToMap(fileArray){
            const result = {};
            fileArray.forEach(f => {
              result[f.nameroot] = f;
            });
            return result;
          }
          const clusterWorkloadFileMap = filesToMap(inputs.clusterWorkloadFiles);
          const graphWorkloadFileMap = filesToMap(inputs.graphWorkloadFiles);
          console.log("Graph workload files: ", graphWorkloadFileMap);
          const componentsOutput = Object.keys(clusterWorkloadFileMap).map(basename => {
            return {
              "workloadJson": clusterWorkloadFileMap[basename],
              "graphBinary": graphWorkloadFileMap[basename]
            }
          });
          console.log("Components output: ", componentsOutput);
          return {"componentsOutput": componentsOutput};
        }
    in:
      clusterWorkloadFiles: graph_components/clusterWorkloadFiles
      graphWorkloadFiles: graph_components/graphWorkloadFiles
    out: [componentsOutput]
