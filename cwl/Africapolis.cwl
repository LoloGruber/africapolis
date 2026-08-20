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
  partitionDepth:
    type: int
    default: 1
    doc: "Number of partitions created on the input for parallel computation"
outputs:
  africapolis:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputSource: mergeConcaveHulls/mergedOutput
  multi_polygons:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputSource: mergeMultiPolygons/mergedOutput
  edges:
    type: File
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    outputSource: mergeEdges/mergedOutput
steps:
  split:
    run: fishnet/split.cwl
    in:
      shapefile: shapefile
      depth: partitionDepth
    out: [split_shapefiles]
  filter:
    run: fishnet/filter.cwl
    in:
      shapefile: split/split_shapefiles
      config: configFile
      skipFilter: 
        valueFrom: $(true)
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
    out: [clusteredOutput, clusterMSTs]
  visualization:
    run: OutlineVisualization.cwl
    in:
      shapefile: clustering/clusteredOutput
      mstFile: clustering/clusterMSTs
    scatter: [shapefile, mstFile]
    scatterMethod: dotproduct
    out: [outputShapefile]
  postFilter:
    run: fishnet/filter.cwl
    in:
      shapefile: visualization/outputShapefile
      config: configFile
    scatter: [shapefile]
    out: [filtered_shapefile]
  mergeMultiPolygons:
    run: fishnet/merge.cwl
    in:
      shpFiles: clustering/clusteredOutput
      outputPath:
        source: shapefile
        valueFrom: $("./"+self.nameroot+"_Africapolis_MultiPolygons")
    out: [mergedOutput]
  mergeEdges:
    run: fishnet/merge.cwl
    in:
      shpFiles: clustering/clusterMSTs
      outputPath:
        source: shapefile
        valueFrom: $("./"+self.nameroot+"_Africapolis_MST")
    out: [mergedOutput]
  mergeConcaveHulls:
    run: fishnet/merge.cwl
    in:
      shpFiles: postFilter/filtered_shapefile
      outputPath:
        source: shapefile
        valueFrom: $("./"+self.nameroot+"_Africapolis")
    out: [mergedOutput]


  

