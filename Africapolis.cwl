cwlVersion: v1.2
class: Workflow
requirements:
- class: ScatterFeatureRequirement
- class: StepInputExpressionRequirement
- class: InlineJavascriptRequirement
- class: SubworkflowFeatureRequirement
- class: SchemaDefRequirement
  types: 
    - $import: cwl/ClusterWorkload.yaml
    - $import: cwl/ComponentsOutput.yaml
    - $import: cwl/GraphConstructionWorkload.yaml
inputs:
  # --- Input data ---------------------------------------------------------------------------------
  shapefile:
    type: File?
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    doc: "Input file to Africapolis workflow"
  vectorFile:
    type: File?
    # format: GPKG
    doc: "Input GeoPackage file to Africapolis workflow"

  # --- Split, run by FishnetSplit.cwl -------------------------------------------------------------
  partitionDepth:
    type: int
    default: 1
    doc: |
      Depth of the quadrant splitting applied to the input, creating 4^partitionDepth tiles which
      the following steps process in parallel.

  # --- Graph construction, run by GraphConstructionTool.cwl ---------------------------------------
  graphConstructionMode:
    type:
      type: enum
      symbols: [BUFFER_SWEEP, DELAUNAY]
    default: DELAUNAY
    doc: |
      Method used to connect neighbouring settlements.
      BUFFER_SWEEP sweeps a buffer over the settlements, connecting each of them to up to
      maxNeighborsPerNode others within graphDistanceThreshold.
      DELAUNAY triangulates the centroids of the settlements and keeps the edges within
      graphDistanceThreshold.
  graphDistanceThreshold:
    type: float
    default: 200.0
    doc: "Maximum distance in meters between two settlements sharing an edge"
  maxNeighborsPerNode:
    type: int?
    doc: "Maximum number of edges per settlement. Required by the BUFFER_SWEEP mode, ignored by DELAUNAY"

  # --- Graph components, run by GraphComponentsTool.cwl -------------------------------------------
  maxComponentsPerWorkload:
    type: int?
    doc: "Maximum number of graph components assigned to a single clustering task. Defaults to 5000"

  # --- Clustering, run by SpatialClusteringTool.cwl -----------------------------------------------
  clusteringMode:
    type:
      type: enum
      symbols: [DBSCAN, BFS, DBSC]
    default: DBSCAN
    doc: |
      Clustering algorithm applied to the settlement graph.
      BFS retrieves the connected components of the subgraph whose edges comply with
      clusterDistanceThreshold.
      DBSCAN retrieves density based clusters, classifying settlements of clusters smaller than
      minClusterSize as noise.
      DBSC adopts DBSCAN and trims the settlement graph with a heuristic over the beta-order
      neighbourhood of each settlement, optionally combining distance with an attribute.
  clusterDistanceThreshold:
    type: float
    default: 200.0
    doc: "Maximum distance in meters between two settlements of the same cluster"
  minClusterSize:
    type: int?
    default: 3
    doc: "Minimum number of settlements per cluster. Settlements of smaller clusters are classified as noise. Required by the DBSCAN and DBSC modes"
  dbscBeta:
    type: int?
    doc: "Order of the neighbourhood considered when trimming the settlement graph. Required by the DBSC mode"
  dbscT1:
    type: float?
    doc: "Custom t1 threshold of the DBSC heuristic. Derived from the data when omitted"
  dbscAttributeMapper:
    type:
      - "null"
      - type: enum
        symbols: [AREA, NONE]
    doc: "Settlement attribute combined with the spatial distance by the DBSC mode. Defaults to NONE"

  # --- MST visualization, run by MSTVisualization.cwl ---------------------------------------------
  mstBuffer:
    type: float?
    doc: "Width in meters of the buffer applied to the MST edges connecting the settlements of a cluster. Defaults to 30.0"

  # --- Outline visualization, run by PolygonOutline.cwl -------------------------------------------
  initialBuffer:
    type: float?
    default: 100.0
    doc: "Width in meters of the buffer grown around each settlement before it is eroded back to outlineBuffer"
  outlineBuffer:
    type: float
    default: 30.0
    doc: "Width in meters of the buffer remaining around the settlements of a cluster after erosion"

  # --- Post filter, run by FishnetFilter.cwl ------------------------------------------------------
  minArea:
    type: float?
    doc: "Drop resulting urban areas covering less than this area in square meters. No area filter is applied when omitted"
  dropContainedPolygons:
    type: boolean
    default: false
    doc: "Drop resulting urban areas which are fully contained in another one"

  # --- Diagnostics --------------------------------------------------------------------------------
  debug:
    type: boolean
    default: false
    doc: "Enable debug logging in every step of the workflow"
outputs:
  africapolis:
    type: File
    # format: GPKG
    outputSource: mergeConcaveHulls/mergedOutput
  multi_polygons:
    type: File
    # format: GPKG
    outputSource: mergeMultiPolygons/mergedOutput
steps:
  split:
    run: cwl/FishnetSplit.cwl
    in:
      shapefile: shapefile
      vectorFile: vectorFile
      depth: partitionDepth
    out: [split_files]
  filter:
    run: cwl/FishnetFilter.cwl
    in:
      vectorFile: split/split_files
      skipFilter: 
        valueFrom: $(true)
    scatter: [vectorFile]
    out: [filteredVectorFile]
  graph_generation:
    run: cwl/GraphGeneration.cwl
    in: 
      vectorFiles: filter/filteredVectorFile
      filenamePrefix: 
        source: vectorFile
        valueFrom: $(self.nameroot)
      mode: graphConstructionMode
      distanceThreshold: graphDistanceThreshold
      maxNeighborsPerNode: maxNeighborsPerNode
      debug: debug
    out: [graphBinaries]
  graph_components:
    run: cwl/GraphComponents.cwl
    in:
      graphBinaries: graph_generation/graphBinaries
      maxComponentsPerWorkload: maxComponentsPerWorkload
      debug: debug
    out: [componentsOutput]
  clustering:
    run: cwl/SpatialClustering.cwl
    in: 
      workload: graph_components/componentsOutput
      files: filter/filteredVectorFile
      mode: clusteringMode
      distanceThreshold: clusterDistanceThreshold
      minClusterSize: minClusterSize
      beta: dbscBeta
      t1: dbscT1
      attributeMapper: dbscAttributeMapper
      mstBuffer: mstBuffer
      debug: debug
    scatter: [workload]
    scatterMethod: dotproduct
    out: [clusteredOutput, clusterMSTs]
  visualization:
    run: cwl/PolygonOutline.cwl
    in:
      vectorFile: clustering/clusteredOutput
      mstFile: clustering/clusterMSTs
      initialBuffer: initialBuffer
      buffer: outlineBuffer
      debug: debug
    scatter: [vectorFile, mstFile]
    scatterMethod: dotproduct
    out: [outlineVectorFile]
  postFilter:
    run: cwl/FishnetFilter.cwl
    in:
      vectorFile: visualization/outlineVectorFile
      minArea: minArea
      dropContainedPolygons: dropContainedPolygons
    scatter: [vectorFile]
    out: [filteredVectorFile]
  mergeMultiPolygons:
    run: cwl/FishnetMerge.cwl
    in:
      vectorFiles: clustering/clusteredOutput
      outputPath:
        source: vectorFile
        valueFrom: $("./"+self.nameroot+"_Africapolis_MultiPolygons")
    out: [mergedOutput]
  mergeConcaveHulls:
    run: cwl/FishnetMerge.cwl
    in:
      vectorFiles: postFilter/filteredVectorFile
      outputPath:
        source: vectorFile
        valueFrom: $("./"+self.nameroot+"_Africapolis")
    out: [mergedOutput]
