cwlVersion: v1.2
class: Workflow
requirements:
- class: SchemaDefRequirement
  types: 
    - $import: ComponentsOutput.yaml
    - $import: ClusterWorkload.yaml
inputs:
  workload: 
    type: ComponentsOutput.yaml#ComponentsOutput
    doc: "Object containing the json workload definition and the graph file"
  files: 
    type: File[]
    # format: GPKG
    doc: "List of vector files to be used for assigning the workload for the clustering"
  mode:
    type:
      type: enum
      symbols: [DBSCAN, BFS, DBSC]
    doc: "Clustering algorithm applied to the settlement graph"
  distanceThreshold:
    type: float
    doc: "Maximum distance in meters between two settlements of the same cluster"
  minClusterSize:
    type: int?
    doc: "Minimum number of settlements per cluster. Required by the DBSCAN and DBSC modes"
  beta:
    type: int?
    doc: "Order of the neighbourhood considered when trimming the settlement graph. Required by the DBSC mode"
  t1:
    type: float?
    doc: "Custom t1 threshold of the DBSC heuristic"
  attributeMapper:
    type:
      - "null"
      - type: enum
        symbols: [AREA, NONE]
    doc: "Settlement attribute combined with the spatial distance by the DBSC mode"
  mstBuffer:
    type: float?
    doc: "Width in meters of the buffer applied to the MST edges"
  debug:
    type: boolean
    default: false
    doc: "Enable debug logging"
outputs:
  clusteredOutput:
    type: File
    # format: GPKG
    outputSource: clustering/clusteredOutput
  clusterMSTs:
    type: File
    # format: GPKG
    outputSource: mstVisualization/mstShapefile
steps:
    prepare_cluster_workload:
        run:
            class: ExpressionTool
            inputs:
                workload:
                    type: ComponentsOutput.yaml#ComponentsOutput
                files: 
                    type: File[]
                    # format: GPKG
                    doc: "List of vector files to be used for assigning the workload for the clustering step"
            outputs:
                clusterWorkload:
                    type: ClusterWorkload.yaml#ClusterWorkload
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
                        vectorFiles: files
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
          valueFrom: $(inputs.clusterWorkload.vectorFiles)
        graphFile: 
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $(inputs.clusterWorkload.graphBinary)
        outputStem:
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $("Edges_"+ inputs.clusterWorkload.graphBinary.nameroot)
        buffer: mstBuffer
        debug: debug
      out: [mstShapefile]
    clustering:
      run: SpatialClusteringTool.cwl
      in:
        clusterWorkload: prepare_cluster_workload/clusterWorkload
        graphBinary:
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $(inputs.clusterWorkload.graphBinary)
        vectorFiles: 
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $(inputs.clusterWorkload.vectorFiles)
        outputStem:
          source: prepare_cluster_workload/clusterWorkload
          valueFrom: $("Clustered_"+ inputs.clusterWorkload.graphBinary.nameroot)
        mode: mode
        distanceThreshold: distanceThreshold
        minClusterSize: minClusterSize
        beta: beta
        t1: t1
        attributeMapper: attributeMapper
        debug: debug
      out: [clusteredOutput]
