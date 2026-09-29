cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisSpatialClustering]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
      coresMin: 1
      ramMin: 4096
inputs:
  vectorFiles:
    type: File[]
    # format: GPKG
    inputBinding:
        prefix: -i
    doc: "List of input vector files, with their required secondary files (.dbf, .shx, .prj)"
  graphBinary:
    type: File
    # format: BIN
    inputBinding:
        prefix: -g
    doc: "Binary file containing the graph structure of the components to be clustered"
  outputStem:
    type: string
    inputBinding:
        prefix: --outputStem 
    doc: "Output filename storing the merged polygons"     
  mode:
    type:
      type: enum
      symbols: [DBSCAN, BFS, DBSC]
    inputBinding:
        prefix: --mode
    doc: |
      Clustering algorithm applied to the settlement graph.
      BFS retrieves the connected components of the subgraph whose edges comply with the distance
      threshold.
      DBSCAN retrieves density based clusters, classifying settlements of clusters smaller than
      minClusterSize as noise.
      DBSC adopts DBSCAN and trims the settlement graph with a heuristic over the beta-order
      neighbourhood of each settlement, optionally combining distance with an attribute.
  distanceThreshold:
    type: float
    inputBinding:
        prefix: --distance-threshold
    doc: "Maximum distance in meters between two settlements of the same cluster"
  minClusterSize:
    type: int?
    inputBinding:
        prefix: --min-cluster-size
    doc: "Minimum number of settlements per cluster. Settlements of smaller clusters are classified as noise. Required by the DBSCAN and DBSC modes"
  beta:
    type: int?
    inputBinding:
        prefix: --beta
    doc: "Order of the neighbourhood considered when trimming the settlement graph. Required by the DBSC mode"
  t1:
    type: float?
    inputBinding:
        prefix: --t1
    doc: "Custom t1 threshold of the DBSC heuristic. Derived from the data when omitted"
  attributeMapper:
    type:
      - "null"
      - type: enum
        symbols: [AREA, NONE]
    inputBinding:
        prefix: --attribute-mapper
    doc: "Settlement attribute combined with the spatial distance by the DBSC mode. Defaults to NONE"
  debug:
    type: boolean
    default: false
    inputBinding:
        prefix: --debug
    doc: "Enable debug logging"
outputs:
  clusteredOutput:
    type: File
    # format: GPKG
    outputBinding:
        glob: "$(inputs.outputStem).gpkg"
    doc: "Clustering output vector file"
