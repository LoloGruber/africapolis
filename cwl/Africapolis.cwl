cwlVersion: v1.2
class: Workflow
requirements:
- class: ScatterFeatureRequirement
- class: StepInputExpressionRequirement
- class: InlineJavascriptRequirement
- class: SubworkflowFeatureRequirement
- class: SchemaDefRequirement
  types: 
    - $import: types/ClusterWorkload.yaml
    - $import: types/ComponentsOutput.yaml
    - $import: types/GraphConstructionWorkload.yaml
inputs:
  shapefile:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    doc: "Input file to Africapolis workflow"
  configFile:
    type: File
    # format: JSON
    doc: "Configuration file for Africapolis workflow"
  partitions:
    type: int
    default: 1
    doc: "Number of partitions created on the input for parallel computation"
outputs:
  concave_hull:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputSource: visualize/outputShapefile
  multi_polygons:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputSource: merge/mergedOutput
steps:
  split:
    run: fishnet/split.cwl
    in:
      shapefile: shapefile
      splits: partitions
    out: [split_shapefiles]
  filter:
    run: fishnet/filter.cwl
    in:
      shapefile: split/split_shapefiles
      config: configFile
    scatter: [shapefile]
    out: [filtered_shapefile]
  graph_generation:
    run: graph_generation/GraphGeneration.cwl
    in: 
      shapefiles: filter/filtered_shapefile
      filenamePrefix: 
        source: shapefile
        valueFrom: $(self.nameroot)
      config: configFile
    out: [graphBinaries]
  graph_components:
    run: graph_components/GraphComponents.cwl
    in:
      graphBinaries: graph_generation/graphBinaries
      config: configFile
    out: [componentsOutput]
  clustering:
    run: spatial_clustering/SpatialClustering.cwl
    in: 
      workload: graph_components/componentsOutput
      config: configFile
      files: filter/filtered_shapefile
    scatter: [workload]
    scatterMethod: dotproduct
    out: [clusteredOutput]
  merge:
    run: fishnet/merge.cwl
    in:
      shpFiles: clustering/clusteredOutput
      outputPath:
        source: shapefile
        valueFrom: $("./"+self.nameroot+"_Africapolis")
    out: [mergedOutput]
  visualize:
    run: OutlineVisualization.cwl
    in:
      shapefile: merge/mergedOutput
    out: [outputShapefile]


  

