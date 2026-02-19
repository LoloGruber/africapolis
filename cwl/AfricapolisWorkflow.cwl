cwlVersion: v1.2
class: Workflow
requirements:
- class: ScatterFeatureRequirement
- class: StepInputExpressionRequirement
- class: InlineJavascriptRequirement
- class: SubworkflowFeatureRequirement
- class: SchemaDefRequirement
  types: 
    - $import: types/Shapefile.yaml
    - $import: types/GeoTIFF.yaml
    - $import: types/ClusterWorkload.yaml
    - $import: types/ComponentsOutput.yaml
    - $import: types/GraphConstructionWorkload.yaml
inputs:
  gisInput:
    type: 
      # - ../GIS.cwl#Shapefile
      - types/GeoTIFF.yaml#GeoTIFF
    doc: "Input vector file to Africapolis workflow"
  config:
    type: File
    doc: "Configuration file for Africapolis workflow"
  partitions:
    type: int
    default: 1
    doc: "Number of partitions created on the input for parallel computation"
outputs:
  concave_hull:
    type: types/Shapefile.yaml#Shapefile
    outputSource: visualize/outputShapefile
  multi_polygons:
    type: types/Shapefile.yaml#Shapefile
    outputSource: merge/mergedOutput
steps:
  split:
    run: fishnet/split.cwl
    in:
      gisFile: gisInput
      # Pass the relative path "fishnet", which will resolve to the directory created by the InitialWorkDirRequirement.
      splits: partitions
    out: [split_shapefiles]
  filter:
    run: fishnet/filter.cwl
    in:
      gisFile:
        source: split/split_shapefiles
      config: config
    scatter: [gisFile]
    out: [filtered_shapefile]
  graph_generation:
    run: graph_generation/GraphGeneration.cwl
    in: 
      shapefiles: filter/filtered_shapefile
      filenamePrefix: 
        source: gisInput
        valueFrom: $(self.file.nameroot)
      config: config
    out: [graphBinaries]
  graph_components:
    run: graph_components/GraphComponents.cwl
    in:
      graphBinaries: graph_generation/graphBinaries
      config: config
    out: [componentsOutput]
  clustering:
    run: spatial_clustering/SpatialClustering.cwl
    in: 
      workload: graph_components/componentsOutput
      config: config
      files: filter/filtered_shapefile
    scatter: [workload]
    scatterMethod: dotproduct
    out: [clusteredOutput]
  merge:
    run: fishnet/merge.cwl
    in:
      gisInput: gisInput
      shpFiles: clustering/clusteredOutput
      outputPath:
        source: gisInput
        valueFrom: $("./"+self.file.nameroot+"_Africapolis")
    out: [mergedOutput]
  visualize:
    run: OutlineVisualization.cwl
    in:
      gisFile: merge/mergedOutput
    out: [outputShapefile]


  

