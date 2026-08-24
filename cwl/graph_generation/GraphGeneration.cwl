cwlVersion: v1.2
class: Workflow
requirements:
  - class: SchemaDefRequirement
    types: 
      - $import: ../types/GraphConstructionWorkload.yaml
  - class: StepInputExpressionRequirement
  - class: ScatterFeatureRequirement
  - class: InlineJavascriptRequirement
inputs:
  vectorFiles:
    type: File[]
    # format: GPKG
    doc: "List of vector files to be used for graph construction. Each polygon must be associated with a unique FISHNET ID."
  filenamePrefix:
    type: string?
    doc: "Prefix used to identify the vector files. The prefix is used to extract the grid coordinates from the filenames."
  config:
    type: File
    # format: JSON
outputs: 
  graphBinaries:
    type: File[]
    # format: BIN
    outputSource: generate_graph/graphBinary
steps:
  prepare_workload:
    run: PrepareGraphConstruction.cwl
    in:
      vectorFiles: vectorFiles
      filenamePrefix: filenamePrefix
    out: [graph_construction_workload]
  generate_graph:
    run: GraphGenerationTool.cwl
    in:
      graph_construction_workload: prepare_workload/graph_construction_workload
      primaryInput: 
        source: prepare_workload/graph_construction_workload
        valueFrom: $(inputs.graph_construction_workload.primaryInput)
      additionalInput:
        source: prepare_workload/graph_construction_workload
        valueFrom: $(inputs.graph_construction_workload.additionalInput)
      config: config
    scatter: graph_construction_workload
    scatterMethod: dotproduct
    out: [graphBinary]