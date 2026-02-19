{
    "$graph": [
        {
            "class": "Workflow",
            "requirements": [
                {
                    "class": "ScatterFeatureRequirement"
                },
                {
                    "class": "StepInputExpressionRequirement"
                },
                {
                    "class": "InlineJavascriptRequirement"
                },
                {
                    "class": "SubworkflowFeatureRequirement"
                },
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "name": "#Shapefile.yaml/Shapefile",
                            "type": "record",
                            "fields": [
                                {
                                    "name": "#Shapefile.yaml/Shapefile/file",
                                    "type": "File",
                                    "secondaryFiles": [
                                        {
                                            "pattern": "^.shx",
                                            "required": null
                                        },
                                        {
                                            "pattern": "^.dbf",
                                            "required": null
                                        },
                                        {
                                            "pattern": "^.prj",
                                            "required": null
                                        },
                                        {
                                            "pattern": "^.cpg",
                                            "required": false
                                        },
                                        {
                                            "pattern": "^.qpj",
                                            "required": false
                                        }
                                    ],
                                    "doc": "The main Shapefile (.shp)"
                                }
                            ],
                            "inputBinding": {
                                "valueFrom": "$(self.file)"
                            }
                        },
                        {
                            "name": "#GeoTIFF.yaml/GeoTIFF",
                            "type": "record",
                            "fields": [
                                {
                                    "name": "#GeoTIFF.yaml/GeoTIFF/file",
                                    "type": "File"
                                }
                            ]
                        },
                        {
                            "name": "#GeoTIFF.yaml/Shapefile",
                            "type": "record",
                            "fields": [
                                {
                                    "name": "#GeoTIFF.yaml/Shapefile/file",
                                    "type": "File",
                                    "secondaryFiles": [
                                        {
                                            "pattern": "^.shx",
                                            "required": null
                                        },
                                        {
                                            "pattern": "^.dbf",
                                            "required": null
                                        },
                                        {
                                            "pattern": "^.prj",
                                            "required": null
                                        },
                                        {
                                            "pattern": "^.cpg",
                                            "required": false
                                        },
                                        {
                                            "pattern": "^.qpj",
                                            "required": false
                                        }
                                    ],
                                    "doc": "The main Shapefile (.shp)"
                                }
                            ]
                        },
                        {
                            "name": "#ClusterWorkload.yaml/ClusterWorkload",
                            "type": "record",
                            "fields": [
                                {
                                    "name": "#ClusterWorkload.yaml/ClusterWorkload/graphBinary",
                                    "type": "File"
                                },
                                {
                                    "name": "#ClusterWorkload.yaml/ClusterWorkload/shpFiles",
                                    "type": {
                                        "type": "array",
                                        "items": "#Shapefile.yaml/Shapefile"
                                    }
                                }
                            ]
                        },
                        {
                            "name": "#ComponentsOutput.yaml/ComponentsOutput",
                            "type": "record",
                            "fields": [
                                {
                                    "name": "#ComponentsOutput.yaml/ComponentsOutput/graphBinary",
                                    "type": "File"
                                },
                                {
                                    "name": "#ComponentsOutput.yaml/ComponentsOutput/workloadJson",
                                    "type": "File",
                                    "loadContents": true
                                }
                            ]
                        },
                        {
                            "name": "#GraphConstructionWorkload.yaml/GraphConstructionWorkload",
                            "type": "record",
                            "fields": [
                                {
                                    "name": "#GraphConstructionWorkload.yaml/GraphConstructionWorkload/primaryInput",
                                    "type": "#Shapefile.yaml/Shapefile"
                                },
                                {
                                    "name": "#GraphConstructionWorkload.yaml/GraphConstructionWorkload/additionalInput",
                                    "type": [
                                        "null",
                                        {
                                            "type": "array",
                                            "items": "#Shapefile.yaml/Shapefile"
                                        }
                                    ]
                                }
                            ]
                        }
                    ]
                }
            ],
            "inputs": [
                {
                    "type": "File",
                    "doc": "Configuration file for Africapolis workflow",
                    "id": "#main/config"
                },
                {
                    "type": [
                        "#GeoTIFF.yaml/GeoTIFF"
                    ],
                    "doc": "Input vector file to Africapolis workflow",
                    "id": "#main/gisInput"
                },
                {
                    "type": "int",
                    "default": 1,
                    "doc": "Number of partitions created on the input for parallel computation",
                    "id": "#main/partitions"
                }
            ],
            "steps": [
                {
                    "run": "#SpatialClustering.cwl",
                    "in": [
                        {
                            "source": "#main/config",
                            "id": "#main/clustering/config"
                        },
                        {
                            "source": "#main/filter/filtered_shapefile",
                            "id": "#main/clustering/files"
                        },
                        {
                            "source": "#main/graph_components/componentsOutput",
                            "id": "#main/clustering/workload"
                        }
                    ],
                    "scatter": [
                        "#main/clustering/workload"
                    ],
                    "scatterMethod": "dotproduct",
                    "out": [
                        "#main/clustering/clusteredOutput"
                    ],
                    "id": "#main/clustering"
                },
                {
                    "run": "#filter.cwl",
                    "in": [
                        {
                            "source": "#main/config",
                            "id": "#main/filter/config"
                        },
                        {
                            "source": "#main/split/split_shapefiles",
                            "id": "#main/filter/gisFile"
                        }
                    ],
                    "scatter": [
                        "#main/filter/gisFile"
                    ],
                    "out": [
                        "#main/filter/filtered_shapefile"
                    ],
                    "id": "#main/filter"
                },
                {
                    "run": "#GraphComponents.cwl",
                    "in": [
                        {
                            "source": "#main/config",
                            "id": "#main/graph_components/config"
                        },
                        {
                            "source": "#main/graph_generation/graphBinaries",
                            "id": "#main/graph_components/graphBinaries"
                        }
                    ],
                    "out": [
                        "#main/graph_components/componentsOutput"
                    ],
                    "id": "#main/graph_components"
                },
                {
                    "run": "#GraphGeneration.cwl",
                    "in": [
                        {
                            "source": "#main/config",
                            "id": "#main/graph_generation/config"
                        },
                        {
                            "source": "#main/gisInput",
                            "valueFrom": "$(self.file.nameroot)",
                            "id": "#main/graph_generation/filenamePrefix"
                        },
                        {
                            "source": "#main/filter/filtered_shapefile",
                            "id": "#main/graph_generation/shapefiles"
                        }
                    ],
                    "out": [
                        "#main/graph_generation/graphBinaries"
                    ],
                    "id": "#main/graph_generation"
                },
                {
                    "run": "#merge.cwl",
                    "in": [
                        {
                            "source": "#main/gisInput",
                            "id": "#main/merge/gisInput"
                        },
                        {
                            "source": "#main/gisInput",
                            "valueFrom": "$(\"./\"+self.file.nameroot+\"_Africapolis\")",
                            "id": "#main/merge/outputPath"
                        },
                        {
                            "source": "#main/clustering/clusteredOutput",
                            "id": "#main/merge/shpFiles"
                        }
                    ],
                    "out": [
                        "#main/merge/mergedOutput"
                    ],
                    "id": "#main/merge"
                },
                {
                    "run": "#split.cwl",
                    "in": [
                        {
                            "source": "#main/gisInput",
                            "id": "#main/split/gisFile"
                        },
                        {
                            "source": "#main/partitions",
                            "id": "#main/split/splits"
                        }
                    ],
                    "out": [
                        "#main/split/split_shapefiles"
                    ],
                    "id": "#main/split"
                },
                {
                    "run": "#OutlineVisualization.cwl",
                    "in": [
                        {
                            "source": "#main/merge/mergedOutput",
                            "id": "#main/visualize/gisFile"
                        }
                    ],
                    "out": [
                        "#main/visualize/outputShapefile"
                    ],
                    "id": "#main/visualize"
                }
            ],
            "id": "#main",
            "outputs": [
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "outputSource": "#main/visualize/outputShapefile",
                    "id": "#main/concave_hull"
                },
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "outputSource": "#main/merge/mergedOutput",
                    "id": "#main/multi_polygons"
                }
            ]
        },
        {
            "class": "CommandLineTool",
            "baseCommand": [
                "AfricapolisPolygonOutline"
            ],
            "hints": [
                {
                    "dockerPull": "logru/africapolis:1.0.0",
                    "class": "DockerRequirement"
                }
            ],
            "requirements": [
                {
                    "class": "InlineJavascriptRequirement"
                },
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        }
                    ]
                }
            ],
            "inputs": [
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "inputBinding": {
                        "position": 1,
                        "prefix": "-i",
                        "valueFrom": "$(self.file)"
                    },
                    "id": "#OutlineVisualization.cwl/gisFile"
                }
            ],
            "outputs": [
                {
                    "type": "File",
                    "id": "#OutlineVisualization.cwl/errorOut",
                    "outputBinding": {
                        "glob": "OUTLINE_$(inputs.gisFile.file.nameroot)_stderr.log"
                    }
                },
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "outputBinding": {
                        "glob": "$(inputs.gisFile.file.nameroot)*",
                        "outputEval": "${\nfunction groupShapefilesByNameroot(files) {\n    var grouped = {};\n    files.forEach(function(f) {\n      var nameroot = f.nameroot || f.basename.replace(/\\.[^/.]+$/, \"\");\n      if (!grouped[nameroot]) {\n        grouped[nameroot] = {};\n      }\n      if (f.basename.endsWith(\".shp\")) grouped[nameroot].shp = f;\n      if (f.basename.endsWith(\".shx\")) grouped[nameroot].shx = f;\n      if (f.basename.endsWith(\".dbf\")) grouped[nameroot].dbf = f;\n      if (f.basename.endsWith(\".prj\")) grouped[nameroot].prj = f;\n      if (f.basename.endsWith(\".cpg\")) grouped[nameroot].cpg = f;\n      if (f.basename.endsWith(\".qpj\")) grouped[nameroot].qpj = f;\n    });\n    return Object.values(grouped).filter(f => f && f.shp);;\n}\n\nfunction groupToShapefileObject(group) {\n    if (!group.shp) return null;\n    return {\n        \"file\":{\n            class: \"File\",\n            path: group.shp.path,\n            basename: group.shp.basename,\n            nameroot: group.shp.nameroot,\n            nameext: group.shp.nameext,\n            secondaryFiles: [group.shx, group.dbf, group.prj,group.cpg,group.qpj].filter(Boolean)\n        }\n    };\n}\n\nvar shapefiles = groupShapefilesByNameroot(self);\nif(shapefiles.length == 1)\n    return groupToShapefileObject(shapefiles.at(0));\nreturn shapefiles.map(groupToShapefileObject).filter(Boolean);\n}\n  "
                    },
                    "doc": "Output shapefile with polygon outlines",
                    "id": "#OutlineVisualization.cwl/outputShapefile"
                },
                {
                    "type": "File",
                    "id": "#OutlineVisualization.cwl/standardOut",
                    "outputBinding": {
                        "glob": "OUTLINE_$(inputs.gisFile.file.nameroot)_stdout.log"
                    }
                }
            ],
            "stdout": "OUTLINE_$(inputs.gisFile.file.nameroot)_stdout.log",
            "stderr": "OUTLINE_$(inputs.gisFile.file.nameroot)_stderr.log",
            "id": "#OutlineVisualization.cwl"
        },
        {
            "class": "CommandLineTool",
            "baseCommand": [
                "FishnetShapefilePreprocessor"
            ],
            "hints": [
                {
                    "dockerPull": "logru/africapolis:1.0.0",
                    "class": "DockerRequirement"
                }
            ],
            "requirements": [
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        }
                    ]
                },
                {
                    "class": "InlineJavascriptRequirement"
                }
            ],
            "inputs": [
                {
                    "type": "File",
                    "doc": "Configuration file for filter process",
                    "inputBinding": {
                        "prefix": "--config",
                        "position": 2
                    },
                    "id": "#filter.cwl/config"
                },
                {
                    "type": [
                        "#Shapefile.yaml/Shapefile"
                    ],
                    "inputBinding": {
                        "position": 1,
                        "prefix": "--input",
                        "valueFrom": "$(self.file)"
                    },
                    "id": "#filter.cwl/gisFile"
                }
            ],
            "outputs": [
                {
                    "type": "File",
                    "id": "#filter.cwl/errorOut",
                    "outputBinding": {
                        "glob": "FILTER_$(inputs.gisFile.file.nameroot)_stderr.log"
                    }
                },
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "outputBinding": {
                        "glob": "*_filtered.*",
                        "outputEval": "${\nfunction groupShapefilesByNameroot(files) {\n    var grouped = {};\n    files.forEach(function(f) {\n      var nameroot = f.nameroot || f.basename.replace(/\\.[^/.]+$/, \"\");\n      if (!grouped[nameroot]) {\n        grouped[nameroot] = {};\n      }\n      if (f.basename.endsWith(\".shp\")) grouped[nameroot].shp = f;\n      if (f.basename.endsWith(\".shx\")) grouped[nameroot].shx = f;\n      if (f.basename.endsWith(\".dbf\")) grouped[nameroot].dbf = f;\n      if (f.basename.endsWith(\".prj\")) grouped[nameroot].prj = f;\n      if (f.basename.endsWith(\".cpg\")) grouped[nameroot].cpg = f;\n      if (f.basename.endsWith(\".qpj\")) grouped[nameroot].qpj = f;\n    });\n    return Object.values(grouped).filter(f => f && f.shp);;\n}\n\nfunction groupToShapefileObject(group) {\n    if (!group.shp) return null;\n    return {\n        \"file\":{\n            class: \"File\",\n            path: group.shp.path,\n            basename: group.shp.basename,\n            nameroot: group.shp.nameroot,\n            nameext: group.shp.nameext,\n            secondaryFiles: [group.shx, group.dbf, group.prj,group.cpg,group.qpj].filter(Boolean)\n        }\n    };\n}\n\nvar shapefiles = groupShapefilesByNameroot(self);\nif(shapefiles.length == 1)\n    return groupToShapefileObject(shapefiles.at(0));\nreturn shapefiles.map(groupToShapefileObject).filter(Boolean);\n}\n  "
                    },
                    "id": "#filter.cwl/filtered_shapefile"
                },
                {
                    "type": "File",
                    "id": "#filter.cwl/standardOut",
                    "outputBinding": {
                        "glob": "FILTER_$(inputs.gisFile.file.nameroot)_stdout.log"
                    }
                }
            ],
            "stdout": "FILTER_$(inputs.gisFile.file.nameroot)_stdout.log",
            "stderr": "FILTER_$(inputs.gisFile.file.nameroot)_stderr.log",
            "id": "#filter.cwl"
        },
        {
            "class": "CommandLineTool",
            "baseCommand": [
                "FishnetShapefileMerger"
            ],
            "hints": [
                {
                    "dockerPull": "logru/africapolis:1.0.0",
                    "class": "DockerRequirement"
                }
            ],
            "requirements": [
                {
                    "class": "InlineJavascriptRequirement"
                },
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        }
                    ]
                }
            ],
            "inputs": [
                {
                    "type": "string",
                    "inputBinding": {
                        "position": 2,
                        "prefix": "-o",
                        "valueFrom": "$(self+\".shp\")"
                    },
                    "doc": "Output filename for result (Shapefile)",
                    "id": "#merge.cwl/outputPath"
                },
                {
                    "type": {
                        "type": "array",
                        "items": "#Shapefile.yaml/Shapefile",
                        "inputBinding": {
                            "valueFrom": "$(self.file)"
                        }
                    },
                    "inputBinding": {
                        "prefix": "-i"
                    },
                    "doc": "List of input shapefiles, with their required secondary files (.dbf, .shx, .prj)",
                    "id": "#merge.cwl/shpFiles"
                }
            ],
            "outputs": [
                {
                    "type": "File",
                    "id": "#merge.cwl/errorOut",
                    "outputBinding": {
                        "glob": "MERGE_stderr.log"
                    }
                },
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "outputBinding": {
                        "glob": "$(inputs.outputPath).*",
                        "outputEval": "${\nfunction groupShapefilesByNameroot(files) {\n    var grouped = {};\n    files.forEach(function(f) {\n      var nameroot = f.nameroot || f.basename.replace(/\\.[^/.]+$/, \"\");\n      if (!grouped[nameroot]) {\n        grouped[nameroot] = {};\n      }\n      if (f.basename.endsWith(\".shp\")) grouped[nameroot].shp = f;\n      if (f.basename.endsWith(\".shx\")) grouped[nameroot].shx = f;\n      if (f.basename.endsWith(\".dbf\")) grouped[nameroot].dbf = f;\n      if (f.basename.endsWith(\".prj\")) grouped[nameroot].prj = f;\n      if (f.basename.endsWith(\".cpg\")) grouped[nameroot].cpg = f;\n      if (f.basename.endsWith(\".qpj\")) grouped[nameroot].qpj = f;\n    });\n    return Object.values(grouped).filter(f => f && f.shp);;\n}\n\nfunction groupToShapefileObject(group) {\n    if (!group.shp) return null;\n    return {\n        \"file\":{\n            class: \"File\",\n            path: group.shp.path,\n            basename: group.shp.basename,\n            nameroot: group.shp.nameroot,\n            nameext: group.shp.nameext,\n            secondaryFiles: [group.shx, group.dbf, group.prj,group.cpg,group.qpj].filter(Boolean)\n        }\n    };\n}\n\nvar shapefiles = groupShapefilesByNameroot(self);\nif(shapefiles.length == 1)\n    return groupToShapefileObject(shapefiles.at(0));\nreturn shapefiles.map(groupToShapefileObject).filter(Boolean);\n}\n  "
                    },
                    "doc": "Merged output file",
                    "id": "#merge.cwl/mergedOutput"
                },
                {
                    "type": "File",
                    "id": "#merge.cwl/standardOut",
                    "outputBinding": {
                        "glob": "MERGE_stdout.log"
                    }
                }
            ],
            "stdout": "MERGE_stdout.log",
            "stderr": "MERGE_stderr.log",
            "id": "#merge.cwl"
        },
        {
            "class": "CommandLineTool",
            "baseCommand": [
                "FishnetShapefileSplitter"
            ],
            "hints": [
                {
                    "dockerPull": "logru/africapolis:1.0.0",
                    "class": "DockerRequirement"
                }
            ],
            "requirements": [
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#GeoTIFF.yaml/GeoTIFF"
                        },
                        {
                            "$import": "#GeoTIFF.yaml/Shapefile"
                        },
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        }
                    ]
                },
                {
                    "class": "InlineJavascriptRequirement"
                }
            ],
            "inputs": [
                {
                    "type": [
                        "#GeoTIFF.yaml/GeoTIFF"
                    ],
                    "inputBinding": {
                        "position": 1,
                        "prefix": "--input",
                        "valueFrom": "$(self.file)"
                    },
                    "id": "#split.cwl/gisFile"
                },
                {
                    "type": "string",
                    "default": "./",
                    "inputBinding": {
                        "position": 3,
                        "prefix": "-o"
                    },
                    "doc": "Output directory",
                    "id": "#split.cwl/outputDir"
                },
                {
                    "type": "int",
                    "inputBinding": {
                        "position": 2,
                        "prefix": "-s"
                    },
                    "id": "#split.cwl/splits"
                },
                {
                    "type": "int",
                    "default": 0,
                    "inputBinding": {
                        "prefix": "-x"
                    },
                    "doc": "X offset for the naming of the output tiles",
                    "id": "#split.cwl/xOffset"
                },
                {
                    "type": "int",
                    "default": 0,
                    "inputBinding": {
                        "prefix": "-y"
                    },
                    "doc": "Y offset for the naming of the output tiles",
                    "id": "#split.cwl/yOffset"
                }
            ],
            "outputs": [
                {
                    "type": "File",
                    "id": "#split.cwl/errorOut",
                    "outputBinding": {
                        "glob": "SPLIT_$(inputs.gisFile.file.nameroot)_stderr.log"
                    }
                },
                {
                    "type": {
                        "type": "array",
                        "items": "#Shapefile.yaml/Shapefile"
                    },
                    "outputBinding": {
                        "glob": "$(inputs.gisFile.file.nameroot)*",
                        "outputEval": "${\nfunction groupShapefilesByNameroot(files) {\n    var grouped = {};\n    files.forEach(function(f) {\n      var nameroot = f.nameroot || f.basename.replace(/\\.[^/.]+$/, \"\");\n      if (!grouped[nameroot]) {\n        grouped[nameroot] = {};\n      }\n      if (f.basename.endsWith(\".shp\")) grouped[nameroot].shp = f;\n      if (f.basename.endsWith(\".shx\")) grouped[nameroot].shx = f;\n      if (f.basename.endsWith(\".dbf\")) grouped[nameroot].dbf = f;\n      if (f.basename.endsWith(\".prj\")) grouped[nameroot].prj = f;\n      if (f.basename.endsWith(\".cpg\")) grouped[nameroot].cpg = f;\n      if (f.basename.endsWith(\".qpj\")) grouped[nameroot].qpj = f;\n    });\n    return Object.values(grouped).filter(f => f && f.shp);;\n}\n\nfunction groupToShapefileObject(group) {\n    if (!group.shp) return null;\n    return {\n        \"file\":{\n            class: \"File\",\n            path: group.shp.path,\n            basename: group.shp.basename,\n            nameroot: group.shp.nameroot,\n            nameext: group.shp.nameext,\n            secondaryFiles: [group.shx, group.dbf, group.prj,group.cpg,group.qpj].filter(Boolean)\n        }\n    };\n}\n\nvar shapefiles = groupShapefilesByNameroot(self);\nif(shapefiles.length == 1)\n    return groupToShapefileObject(shapefiles.at(0));\nreturn shapefiles.map(groupToShapefileObject).filter(Boolean);\n}\n  "
                    },
                    "doc": "Split output files",
                    "id": "#split.cwl/split_shapefiles"
                },
                {
                    "type": "File",
                    "id": "#split.cwl/standardOut",
                    "outputBinding": {
                        "glob": "SPLIT_$(inputs.gisFile.file.nameroot)_stdout.log"
                    }
                }
            ],
            "stdout": "SPLIT_$(inputs.gisFile.file.nameroot)_stdout.log",
            "stderr": "SPLIT_$(inputs.gisFile.file.nameroot)_stderr.log",
            "id": "#split.cwl"
        },
        {
            "class": "Workflow",
            "requirements": [
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#ComponentsOutput.yaml/ComponentsOutput"
                        }
                    ]
                },
                {
                    "class": "InlineJavascriptRequirement"
                }
            ],
            "inputs": [
                {
                    "type": "File",
                    "doc": "Path to configuration file for africapolis components step. Contains database credentials and parallelization target",
                    "id": "#GraphComponents.cwl/config"
                },
                {
                    "type": {
                        "type": "array",
                        "items": "File"
                    },
                    "id": "#GraphComponents.cwl/graphBinaries"
                }
            ],
            "outputs": [
                {
                    "type": {
                        "type": "array",
                        "items": "#ComponentsOutput.yaml/ComponentsOutput"
                    },
                    "outputSource": "#GraphComponents.cwl/post_components_output/componentsOutput",
                    "id": "#GraphComponents.cwl/componentsOutput"
                }
            ],
            "steps": [
                {
                    "run": "#GraphComponentsTool.cwl",
                    "in": [
                        {
                            "source": "#GraphComponents.cwl/config",
                            "id": "#GraphComponents.cwl/graph_components/config"
                        },
                        {
                            "source": "#GraphComponents.cwl/graphBinaries",
                            "id": "#GraphComponents.cwl/graph_components/graphBinaries"
                        }
                    ],
                    "out": [
                        "#GraphComponents.cwl/graph_components/clusterWorkloadFiles",
                        "#GraphComponents.cwl/graph_components/graphWorkloadFiles"
                    ],
                    "id": "#GraphComponents.cwl/graph_components"
                },
                {
                    "run": {
                        "class": "ExpressionTool",
                        "requirements": [
                            {
                                "class": "InlineJavascriptRequirement"
                            }
                        ],
                        "inputs": [
                            {
                                "type": {
                                    "type": "array",
                                    "items": "File"
                                },
                                "id": "#GraphComponents.cwl/post_components_output/run/clusterWorkloadFiles"
                            },
                            {
                                "type": {
                                    "type": "array",
                                    "items": "File"
                                },
                                "id": "#GraphComponents.cwl/post_components_output/run/graphWorkloadFiles"
                            }
                        ],
                        "outputs": [
                            {
                                "type": {
                                    "type": "array",
                                    "items": "#ComponentsOutput.yaml/ComponentsOutput"
                                },
                                "id": "#GraphComponents.cwl/post_components_output/run/componentsOutput"
                            }
                        ],
                        "expression": "${\n  function filesToMap(fileArray){\n    const result = {};\n    fileArray.forEach(f => {\n      result[f.nameroot] = f;\n    });\n    return result;\n  }\n  const clusterWorkloadFileMap = filesToMap(inputs.clusterWorkloadFiles);\n  const graphWorkloadFileMap = filesToMap(inputs.graphWorkloadFiles);\n  console.log(\"Graph workload files: \", graphWorkloadFileMap);\n  const componentsOutput = Object.keys(clusterWorkloadFileMap).map(basename => {\n    return {\n      \"workloadJson\": clusterWorkloadFileMap[basename],\n      \"graphBinary\": graphWorkloadFileMap[basename]\n    }\n  });\n  console.log(\"Components output: \", componentsOutput);\n  return {\"componentsOutput\": componentsOutput};\n}\n"
                    },
                    "in": [
                        {
                            "source": "#GraphComponents.cwl/graph_components/clusterWorkloadFiles",
                            "id": "#GraphComponents.cwl/post_components_output/clusterWorkloadFiles"
                        },
                        {
                            "source": "#GraphComponents.cwl/graph_components/graphWorkloadFiles",
                            "id": "#GraphComponents.cwl/post_components_output/graphWorkloadFiles"
                        }
                    ],
                    "out": [
                        "#GraphComponents.cwl/post_components_output/componentsOutput"
                    ],
                    "id": "#GraphComponents.cwl/post_components_output"
                }
            ],
            "id": "#GraphComponents.cwl"
        },
        {
            "class": "CommandLineTool",
            "baseCommand": [
                "AfricapolisGraphComponents"
            ],
            "hints": [
                {
                    "dockerPull": "logru/africapolis:1.0.0",
                    "class": "DockerRequirement"
                }
            ],
            "inputs": [
                {
                    "type": "File",
                    "doc": "Path to configuration file for africapolis components step. Contains database credentials and parallelization target",
                    "inputBinding": {
                        "prefix": "-c"
                    },
                    "id": "#GraphComponentsTool.cwl/config"
                },
                {
                    "type": {
                        "type": "array",
                        "items": "File"
                    },
                    "inputBinding": {
                        "prefix": "-g"
                    },
                    "id": "#GraphComponentsTool.cwl/graphBinaries"
                }
            ],
            "outputs": [
                {
                    "type": {
                        "type": "array",
                        "items": "File"
                    },
                    "outputBinding": {
                        "glob": "*.json"
                    },
                    "doc": "Output file containing the associations of shapefiles to graph binaries of the graph",
                    "id": "#GraphComponentsTool.cwl/clusterWorkloadFiles"
                },
                {
                    "type": "File",
                    "id": "#GraphComponentsTool.cwl/errorOut",
                    "outputBinding": {
                        "glob": "COMPONENTS_stderr.log"
                    }
                },
                {
                    "type": {
                        "type": "array",
                        "items": "File"
                    },
                    "outputBinding": {
                        "glob": "*.bin"
                    },
                    "doc": "Output file containing the binary representation of the graph components, to be used as input for the clustering step",
                    "id": "#GraphComponentsTool.cwl/graphWorkloadFiles"
                },
                {
                    "type": "File",
                    "id": "#GraphComponentsTool.cwl/standardOut",
                    "outputBinding": {
                        "glob": "COMPONENTS_stdout.log"
                    }
                }
            ],
            "stdout": "COMPONENTS_stdout.log",
            "stderr": "COMPONENTS_stderr.log",
            "id": "#GraphComponentsTool.cwl"
        },
        {
            "class": "Workflow",
            "requirements": [
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        },
                        {
                            "$import": "#GraphConstructionWorkload.yaml/GraphConstructionWorkload"
                        }
                    ]
                },
                {
                    "class": "StepInputExpressionRequirement"
                },
                {
                    "class": "ScatterFeatureRequirement"
                },
                {
                    "class": "InlineJavascriptRequirement"
                }
            ],
            "inputs": [
                {
                    "type": "File",
                    "id": "#GraphGeneration.cwl/config"
                },
                {
                    "type": [
                        "null",
                        "string"
                    ],
                    "doc": "Prefix used to identify the shapefiles. The prefix is used to extract the grid coordinates from the filenames.",
                    "id": "#GraphGeneration.cwl/filenamePrefix"
                },
                {
                    "type": {
                        "type": "array",
                        "items": "#Shapefile.yaml/Shapefile"
                    },
                    "doc": "List of shapefiles to be used for graph construction. Each polygon must be associated with a unique FISHNET ID.",
                    "id": "#GraphGeneration.cwl/shapefiles"
                }
            ],
            "outputs": [
                {
                    "type": {
                        "type": "array",
                        "items": "File"
                    },
                    "outputSource": "#GraphGeneration.cwl/generate_graph/graphBinary",
                    "id": "#GraphGeneration.cwl/graphBinaries"
                }
            ],
            "steps": [
                {
                    "run": "#GraphGenerationTool.cwl",
                    "in": [
                        {
                            "source": "#GraphGeneration.cwl/prepare_workload/graph_construction_workload",
                            "valueFrom": "$(inputs.graph_construction_workload.additionalInput)",
                            "id": "#GraphGeneration.cwl/generate_graph/additionalInput"
                        },
                        {
                            "source": "#GraphGeneration.cwl/config",
                            "id": "#GraphGeneration.cwl/generate_graph/config"
                        },
                        {
                            "source": "#GraphGeneration.cwl/prepare_workload/graph_construction_workload",
                            "id": "#GraphGeneration.cwl/generate_graph/graph_construction_workload"
                        },
                        {
                            "source": "#GraphGeneration.cwl/prepare_workload/graph_construction_workload",
                            "valueFrom": "$(inputs.graph_construction_workload.primaryInput)",
                            "id": "#GraphGeneration.cwl/generate_graph/primaryInput"
                        }
                    ],
                    "scatter": "#GraphGeneration.cwl/generate_graph/graph_construction_workload",
                    "scatterMethod": "dotproduct",
                    "out": [
                        "#GraphGeneration.cwl/generate_graph/graphBinary"
                    ],
                    "id": "#GraphGeneration.cwl/generate_graph"
                },
                {
                    "run": "#PrepareGraphConstruction.cwl",
                    "in": [
                        {
                            "source": "#GraphGeneration.cwl/filenamePrefix",
                            "id": "#GraphGeneration.cwl/prepare_workload/filenamePrefix"
                        },
                        {
                            "source": "#GraphGeneration.cwl/shapefiles",
                            "id": "#GraphGeneration.cwl/prepare_workload/shapefiles"
                        }
                    ],
                    "out": [
                        "#GraphGeneration.cwl/prepare_workload/graph_construction_workload"
                    ],
                    "id": "#GraphGeneration.cwl/prepare_workload"
                }
            ],
            "id": "#GraphGeneration.cwl"
        },
        {
            "class": "CommandLineTool",
            "baseCommand": [
                "AfricapolisGraphConstruction"
            ],
            "hints": [
                {
                    "dockerPull": "logru/africapolis:1.0.0",
                    "class": "DockerRequirement"
                }
            ],
            "requirements": [
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        }
                    ]
                },
                {
                    "class": "InlineJavascriptRequirement"
                }
            ],
            "inputs": [
                {
                    "type": {
                        "type": "array",
                        "items": "#Shapefile.yaml/Shapefile",
                        "inputBinding": {
                            "valueFrom": "$(self.file)"
                        }
                    },
                    "inputBinding": {
                        "prefix": "-a"
                    },
                    "doc": "List of additional input shapefiles in proximity to the primary input, with their required secondary files (.dbf, .shx, .prj)",
                    "id": "#GraphGenerationTool.cwl/additionalInput"
                },
                {
                    "type": "File",
                    "inputBinding": {
                        "prefix": "-c"
                    },
                    "doc": "Path to configuration for neighbours task. Contains graph database credentials, neighbouring criteria, ...",
                    "id": "#GraphGenerationTool.cwl/config"
                },
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "inputBinding": {
                        "prefix": "-i",
                        "valueFrom": "$(self.file)"
                    },
                    "doc": "Primary input, supplied as shapefile object",
                    "id": "#GraphGenerationTool.cwl/primaryInput"
                }
            ],
            "outputs": [
                {
                    "type": "File",
                    "id": "#GraphGenerationTool.cwl/errorOut",
                    "outputBinding": {
                        "glob": "GENERATE_GRAPH_$(inputs.primaryInput.file.nameroot)_stderr.log"
                    }
                },
                {
                    "type": "File",
                    "outputBinding": {
                        "glob": "*.bin",
                        "outputEval": "$(self[0])"
                    },
                    "id": "#GraphGenerationTool.cwl/graphBinary"
                },
                {
                    "type": "File",
                    "id": "#GraphGenerationTool.cwl/standardOut",
                    "outputBinding": {
                        "glob": "GENERATE_GRAPH_$(inputs.primaryInput.file.nameroot)_stdout.log"
                    }
                }
            ],
            "stdout": "GENERATE_GRAPH_$(inputs.primaryInput.file.nameroot)_stdout.log",
            "stderr": "GENERATE_GRAPH_$(inputs.primaryInput.file.nameroot)_stderr.log",
            "id": "#GraphGenerationTool.cwl"
        },
        {
            "class": "ExpressionTool",
            "requirements": [
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        },
                        {
                            "$import": "#GeoTIFF.yaml/GeoTIFF"
                        },
                        {
                            "$import": "#GeoTIFF.yaml/Shapefile"
                        },
                        {
                            "$import": "#GraphConstructionWorkload.yaml/GraphConstructionWorkload"
                        }
                    ]
                }
            ],
            "inputs": [
                {
                    "type": [
                        "null",
                        "string"
                    ],
                    "id": "#PrepareGraphConstruction.cwl/filenamePrefix"
                },
                {
                    "type": {
                        "type": "array",
                        "items": "#Shapefile.yaml/Shapefile"
                    },
                    "id": "#PrepareGraphConstruction.cwl/shapefiles"
                }
            ],
            "outputs": [
                {
                    "type": {
                        "type": "array",
                        "items": "#GraphConstructionWorkload.yaml/GraphConstructionWorkload"
                    },
                    "id": "#PrepareGraphConstruction.cwl/graph_construction_workload"
                }
            ],
            "expression": "${\nclass Coordinate{\n    constructor(x,y){\n        this.x = x;\n        this.y = y;\n    }\n    infinityDistance(other) {\n        return Math.max(Math.abs(this.x-other.x),Math.abs(this.y-other.y));\n    }\n\n    toString() {\n        return this.x+\",\"+this.y;\n    }\n};\n\nfunction stringToCoordinate(coordinateString){\n    return new Coordinate(...coordinateString.split(\",\").map(Number));\n}\n\nfunction shapefileObjectToCoordinate(filename,prefix){\n    const regex = new RegExp(`${prefix}_(\\\\d+)_(\\\\d+)_.*`);\n    const match = filename.match(regex);\n    if (match) {\n        return new Coordinate(parseInt(match[1]), parseInt(match[2]));\n    }\n    throw new Error(\"Could not parse grid coordinates from filename \\\"\"+filename+\"\\\" with prefix \\\"\"+prefix+\"\\\"\");\n}\nfunction neighbouringShapefiles(shapefiles, prefix){\n    const shapefileCoordinateMap = {};\n    shapefiles.forEach(s => {\n        const nameroot = s.file.nameroot;\n        const coordinate = shapefileObjectToCoordinate(nameroot,prefix);\n        shapefileCoordinateMap[coordinate] = s;\n    });\n    return Object.keys(shapefileCoordinateMap).map(coordinate => {\n        const primaryInput = shapefileCoordinateMap[coordinate];\n        const currentCoordinate = stringToCoordinate(coordinate);\n        const additionalInput = Object.keys(shapefileCoordinateMap).filter(other => {\n            const distance = stringToCoordinate(other).infinityDistance(currentCoordinate);\n            return distance > 0 && distance <=1;\n        }).map(c => shapefileCoordinateMap[c]);\n        if(additionalInput.length == 0){\n            return {\n                \"primaryInput\":primaryInput,\n                \"additionalInput\":null\n            };\n        }\n        else return {\n            \"primaryInput\":primaryInput,\n            \"additionalInput\":additionalInput\n        };\n    });\n}\n// TODO make it work even when no prefix is given\nconst result = neighbouringShapefiles(inputs.shapefiles,inputs.filenamePrefix);\nreturn {\"graph_construction_workload\":result};\n}",
            "id": "#PrepareGraphConstruction.cwl"
        },
        {
            "class": "Workflow",
            "requirements": [
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        },
                        {
                            "$import": "#ComponentsOutput.yaml/ComponentsOutput"
                        },
                        {
                            "$import": "#ClusterWorkload.yaml/ClusterWorkload"
                        }
                    ]
                }
            ],
            "inputs": [
                {
                    "type": "File",
                    "doc": "Path to configuration file for africapolis clustering step. Contains database credentials",
                    "id": "#SpatialClustering.cwl/config"
                },
                {
                    "type": {
                        "type": "array",
                        "items": "#Shapefile.yaml/Shapefile"
                    },
                    "doc": "List of shapefiles to be used for assigning the workload for the clustering",
                    "id": "#SpatialClustering.cwl/files"
                },
                {
                    "type": "#ComponentsOutput.yaml/ComponentsOutput",
                    "doc": "Object containing the json workload definition and the graph file",
                    "id": "#SpatialClustering.cwl/workload"
                }
            ],
            "outputs": [
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "outputSource": "#SpatialClustering.cwl/clustering/clusteredOutput",
                    "id": "#SpatialClustering.cwl/clusteredOutput"
                }
            ],
            "steps": [
                {
                    "run": "#SpatialClusteringTool.cwl",
                    "in": [
                        {
                            "source": "#SpatialClustering.cwl/prepare_cluster_workload/clusterWorkload",
                            "id": "#SpatialClustering.cwl/clustering/clusterWorkload"
                        },
                        {
                            "source": "#SpatialClustering.cwl/config",
                            "id": "#SpatialClustering.cwl/clustering/config"
                        },
                        {
                            "source": "#SpatialClustering.cwl/prepare_cluster_workload/clusterWorkload",
                            "valueFrom": "$(inputs.clusterWorkload.graphBinary)",
                            "id": "#SpatialClustering.cwl/clustering/graphBinary"
                        },
                        {
                            "source": "#SpatialClustering.cwl/prepare_cluster_workload/clusterWorkload",
                            "valueFrom": "$(\"Clustered_\"+ inputs.clusterWorkload.graphBinary.basename)",
                            "id": "#SpatialClustering.cwl/clustering/outputStem"
                        },
                        {
                            "source": "#SpatialClustering.cwl/prepare_cluster_workload/clusterWorkload",
                            "valueFrom": "$(inputs.clusterWorkload.shpFiles)",
                            "id": "#SpatialClustering.cwl/clustering/shpFiles"
                        }
                    ],
                    "out": [
                        "#SpatialClustering.cwl/clustering/clusteredOutput"
                    ],
                    "id": "#SpatialClustering.cwl/clustering"
                },
                {
                    "run": {
                        "class": "ExpressionTool",
                        "inputs": [
                            {
                                "type": {
                                    "type": "array",
                                    "items": "#Shapefile.yaml/Shapefile"
                                },
                                "doc": "List of shapefiles to be used for assigning the workload for the clustering step",
                                "id": "#SpatialClustering.cwl/prepare_cluster_workload/run/files"
                            },
                            {
                                "type": "#ComponentsOutput.yaml/ComponentsOutput",
                                "id": "#SpatialClustering.cwl/prepare_cluster_workload/run/workload"
                            }
                        ],
                        "outputs": [
                            {
                                "type": "#ClusterWorkload.yaml/ClusterWorkload",
                                "doc": "Parsed ClusterWorkload object",
                                "id": "#SpatialClustering.cwl/prepare_cluster_workload/run/clusterWorkload"
                            }
                        ],
                        "expression": "${\n    let workloadJson = JSON.parse(inputs.workload.workloadJson.contents);\n    let fileNames = [...new Set(workloadJson.files.map(file => file.split(\"/\").pop()))];\n    let files = fileNames.map(fileName => {\n        let fileObject = inputs.files.find(f => f.file.basename == fileName);\n        return fileObject;\n        });\n    let result = {\n        graphBinary: inputs.workload.graphBinary,\n        shpFiles: files\n    };\n    return {\n        clusterWorkload: result,\n    };\n}\n"
                    },
                    "in": [
                        {
                            "source": "#SpatialClustering.cwl/files",
                            "id": "#SpatialClustering.cwl/prepare_cluster_workload/files"
                        },
                        {
                            "source": "#SpatialClustering.cwl/workload",
                            "id": "#SpatialClustering.cwl/prepare_cluster_workload/workload"
                        }
                    ],
                    "out": [
                        "#SpatialClustering.cwl/prepare_cluster_workload/clusterWorkload"
                    ],
                    "id": "#SpatialClustering.cwl/prepare_cluster_workload"
                }
            ],
            "id": "#SpatialClustering.cwl"
        },
        {
            "class": "CommandLineTool",
            "baseCommand": [
                "AfricapolisSpatialClustering"
            ],
            "hints": [
                {
                    "dockerPull": "logru/africapolis:1.0.0",
                    "class": "DockerRequirement"
                }
            ],
            "requirements": [
                {
                    "class": "InlineJavascriptRequirement"
                },
                {
                    "class": "SchemaDefRequirement",
                    "types": [
                        {
                            "$import": "#Shapefile.yaml/Shapefile"
                        }
                    ]
                }
            ],
            "inputs": [
                {
                    "type": "File",
                    "inputBinding": {
                        "prefix": "-c"
                    },
                    "doc": "Path to configuration for contraction task",
                    "id": "#SpatialClusteringTool.cwl/config"
                },
                {
                    "type": "File",
                    "inputBinding": {
                        "prefix": "-g"
                    },
                    "doc": "Binary file containing the graph structure of the components to be clustered",
                    "id": "#SpatialClusteringTool.cwl/graphBinary"
                },
                {
                    "type": "string",
                    "inputBinding": {
                        "position": 2,
                        "prefix": "--outputStem"
                    },
                    "doc": "Output filename storing the merged polygons",
                    "id": "#SpatialClusteringTool.cwl/outputStem"
                },
                {
                    "type": {
                        "type": "array",
                        "items": "#Shapefile.yaml/Shapefile",
                        "inputBinding": {
                            "valueFrom": "$(self.file)"
                        }
                    },
                    "inputBinding": {
                        "prefix": "-i"
                    },
                    "doc": "List of input shapefiles, with their required secondary files (.dbf, .shx, .prj)",
                    "id": "#SpatialClusteringTool.cwl/shpFiles"
                }
            ],
            "outputs": [
                {
                    "type": "#Shapefile.yaml/Shapefile",
                    "outputBinding": {
                        "glob": "$(inputs.outputStem).*",
                        "outputEval": "${\nfunction groupShapefilesByNameroot(files) {\n    var grouped = {};\n    files.forEach(function(f) {\n      var nameroot = f.nameroot || f.basename.replace(/\\.[^/.]+$/, \"\");\n      if (!grouped[nameroot]) {\n        grouped[nameroot] = {};\n      }\n      if (f.basename.endsWith(\".shp\")) grouped[nameroot].shp = f;\n      if (f.basename.endsWith(\".shx\")) grouped[nameroot].shx = f;\n      if (f.basename.endsWith(\".dbf\")) grouped[nameroot].dbf = f;\n      if (f.basename.endsWith(\".prj\")) grouped[nameroot].prj = f;\n      if (f.basename.endsWith(\".cpg\")) grouped[nameroot].cpg = f;\n      if (f.basename.endsWith(\".qpj\")) grouped[nameroot].qpj = f;\n    });\n    return Object.values(grouped).filter(f => f && f.shp);;\n}\n\nfunction groupToShapefileObject(group) {\n    if (!group.shp) return null;\n    return {\n        \"file\":{\n            class: \"File\",\n            path: group.shp.path,\n            basename: group.shp.basename,\n            nameroot: group.shp.nameroot,\n            nameext: group.shp.nameext,\n            secondaryFiles: [group.shx, group.dbf, group.prj,group.cpg,group.qpj].filter(Boolean)\n        }\n    };\n}\n\nvar shapefiles = groupShapefilesByNameroot(self);\nif(shapefiles.length == 1)\n    return groupToShapefileObject(shapefiles.at(0));\nreturn shapefiles.map(groupToShapefileObject).filter(Boolean);\n}\n  "
                    },
                    "doc": "Merged output file",
                    "id": "#SpatialClusteringTool.cwl/clusteredOutput"
                },
                {
                    "type": "File",
                    "id": "#SpatialClusteringTool.cwl/errorOut",
                    "outputBinding": {
                        "glob": "CLUSTER_$(inputs.graphBinary.basename)_stderr.log"
                    }
                },
                {
                    "type": "File",
                    "id": "#SpatialClusteringTool.cwl/standardOut",
                    "outputBinding": {
                        "glob": "CLUSTER_$(inputs.graphBinary.basename)_stdout.log"
                    }
                }
            ],
            "stdout": "CLUSTER_$(inputs.graphBinary.basename)_stdout.log",
            "stderr": "CLUSTER_$(inputs.graphBinary.basename)_stderr.log",
            "id": "#SpatialClusteringTool.cwl"
        }
    ],
    "cwlVersion": "v1.2"
}
