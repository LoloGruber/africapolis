cwlVersion: v1.2
class: Workflow
requirements:
  - class: SchemaDefRequirement
    types: 
      - $import: ../types/ComponentsOutput.yaml
  - class: InlineJavascriptRequirement
inputs:
  config:
    type: File
    doc: "Path to configuration file for africapolis components step. Contains database credentials and parallelization target"
  graphBinaries:
    type: File[]
outputs:
  componentsOutput:
    type: ../types/ComponentsOutput.yaml#ComponentsOutput[]
    outputSource: post_components_output/componentsOutput
steps:
  graph_components:
    run: GraphComponentsTool.cwl
    in:
      config: config
      graphBinaries: graphBinaries
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
          type: ../types/ComponentsOutput.yaml#ComponentsOutput[]
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