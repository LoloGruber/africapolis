cwlVersion: v1.2
class: CommandLineTool
baseCommand: [AfricapolisGraphConstruction]
hints:
  DockerRequirement:
    dockerPull: logru/africapolis:latest
requirements:
  InlineJavascriptRequirement: {}
  ResourceRequirement:
    coresMin: 1
    ramMin: 4096
inputs:
  primaryInput:
    type: File
    # format: GPKG
    inputBinding:
        prefix: -i
    doc: "Primary input, supplied as vector file object"
  additionalInput:
    type: File[]
    default: []
    # format: GPKG
    inputBinding:
        prefix: -a
    doc: "List of additional input vector files in proximity to the primary input, with their required secondary files (.dbf, .shx, .prj)"
  mode:
    type:
      type: enum
      symbols: [BUFFER_SWEEP, DELAUNAY]
    inputBinding:
        prefix: --mode
    doc: |
      Method used to connect neighbouring settlements.
      BUFFER_SWEEP sweeps a buffer over the settlements, connecting each of them to up to
      maxNeighborsPerNode others within the distance threshold.
      DELAUNAY triangulates the centroids of the settlements and keeps the edges within the
      distance threshold.
  distanceThreshold:
    type: float
    inputBinding:
        prefix: --distance-threshold
    doc: "Maximum distance in meters between two settlements sharing an edge"
  maxNeighborsPerNode:
    type: int?
    inputBinding:
        prefix: --max-neighbors-per-node
    doc: "Maximum number of edges per settlement. Required by the BUFFER_SWEEP mode, ignored by DELAUNAY"
  debug:
    type: boolean
    default: false
    inputBinding:
        prefix: --debug
    doc: "Enable debug logging"
outputs:
    graphBinary:
        type: File
        outputBinding:
            glob: "*.bin"
            outputEval: $(self[0])
