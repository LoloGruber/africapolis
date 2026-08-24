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
    type: File?
    secondaryFiles: [^.shx, ^.dbf, ^.prj, ^.cpg?, ^.qpj?]
    # format: SHP
    doc: "Input file to Africapolis workflow"
  vectorFile:
    type: File?
    # format: GPKG
    doc: "Input GeoPackage file to Africapolis workflow"
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
    # format: GPKG
    outputSource: mergeConcaveHulls/mergedOutput
  multi_polygons:
    type: File
    # format: GPKG
    outputSource: mergeMultiPolygons/mergedOutput
steps:
  split:
    run: fishnet/split.cwl
    in:
      shapefile: shapefile
      vectorFile: vectorFile
      depth: partitionDepth
    out: [split_files]
  filter:
    run: fishnet/filter.cwl
    in:
      vectorFile: split/split_files
      config: configFile
      skipFilter: 
        valueFrom: $(true)
    scatter: [vectorFile]
    out: [filteredVectorFile]
  graph_generation:
    run: graph_generation/GraphGeneration.cwl
    in: 
      vectorFiles: filter/filteredVectorFile
      filenamePrefix: 
        source: vectorFile
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
      files: filter/filteredVectorFile
    scatter: [workload]
    scatterMethod: dotproduct
    out: [clusteredOutput, clusterMSTs]
  visualization:
    run: OutlineVisualization.cwl
    in:
      vectorFile: clustering/clusteredOutput
      mstFile: clustering/clusterMSTs
    scatter: [vectorFile, mstFile]
    scatterMethod: dotproduct
    out: [outlineVectorFile]
  postFilter:
    run: fishnet/filter.cwl
    in:
      vectorFile: visualization/outlineVectorFile
      config: configFile
    scatter: [vectorFile]
    out: [filteredVectorFile]
  mergeMultiPolygons:
    run: fishnet/merge.cwl
    in:
      vectorFiles: clustering/clusteredOutput
      outputPath:
        source: vectorFile
        valueFrom: $("./"+self.nameroot+"_Africapolis_MultiPolygons")
    out: [mergedOutput]
  mergeConcaveHulls:
    run: fishnet/merge.cwl
    in:
      vectorFiles: postFilter/filteredVectorFile
      outputPath:
        source: vectorFile
        valueFrom: $("./"+self.nameroot+"_Africapolis")
    out: [mergedOutput]


  

