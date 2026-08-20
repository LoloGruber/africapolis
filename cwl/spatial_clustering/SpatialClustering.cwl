cwlVersion: v1.2
class: Workflow
requirements:
- class: SchemaDefRequirement
  types: 
    - $import: ../types/ComponentsOutput.yaml
    - $import: ../types/ClusterWorkload.yaml
inputs:
  config:
    type: File
    # format: JSON
    doc: "Path to configuration file for africapolis clustering step. Contains database credentials"
  workload: 
    type: ../types/ComponentsOutput.yaml#ComponentsOutput
    doc: "Object containing the json workload definition and the graph file"
  files: 
    type: File[]
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    doc: "List of shapefiles to be used for assigning the workload for the clustering"
outputs:
  clusteredOutput:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputSource: clustering/clusteredOutput
  clusterMSTs:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputSource: mstVisualization/mstShapefile
steps:
    prepare_cluster_workload:
        run:
            class: ExpressionTool
            inputs:
                workload:
                    type: ../types/ComponentsOutput.yaml#ComponentsOutput
                files: 
                    type: File[]
                    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
                    # format: SHP
                    doc: "List of shapefiles to be used for assigning the workload for the clustering step"
            outputs:
                clusterWorkload:
                    type: ../types/ClusterWorkload.yaml#ClusterWorkload
                    doc: "Parsed ClusterWorkload object"
            expression: |
                ${
                    let workloadJson = JSON.parse(inputs.workload.workloadJson.contents);
                    let fileNames = [...new Set(workloadJson.files.map(file => file.split("/").pop()))];
                    let files = fileNames.map(fileName => {
                        let fileObject = inputs.files.find(f => f.basename == fileName);
                        return fileObject;
                        });
                    let result = {
                        graphBinary: inputs.workload.graphBinary,
                        shpFiles: files
                    };
                    return {
                        clusterWorkload: result,
                    };
                }
        in:
            workload: workload
            files: files
        out: [clusterWorkload]
    mstVisualization:
      run: MSTVisualization.cwl
      in:
        clusterWorkload: prepare_cluster_workload/clusterWorkload
        geometryFiles:
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $(inputs.clusterWorkload.shpFiles)
        graphFile: 
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $(inputs.clusterWorkload.graphBinary)
        outputStem:
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $("Edges_"+ inputs.clusterWorkload.graphBinary.nameroot)
      out: [mstShapefile]
    clustering:
      run: SpatialClusteringTool.cwl
      in:
        clusterWorkload: prepare_cluster_workload/clusterWorkload
        config: config
        graphBinary:
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $(inputs.clusterWorkload.graphBinary)
        shpFiles: 
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $(inputs.clusterWorkload.shpFiles)
        outputStem:
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $("Clustered_"+ inputs.clusterWorkload.graphBinary.nameroot)
      out: [clusteredOutput]
