#!/bin/bash
# Load modules
module load apptainer
source venv/bin/activate
INPUT_FILE="${1}"
CONFIG_FILE="${2}"
PARTITIONS="${3}"
# Validate input parameters
if [ -z "$INPUT_FILE" ] || [ -z "$CONFIG_FILE" ] || [ -z "$PARTITIONS" ]; then
    echo "Error: Missing required parameters. Usage: $0 <INPUT_FILE> <CONFIG_FILE> <PARTITIONS>"
    exit 1
fi
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: INPUT_FILE does not exist: $INPUT_FILE"
    exit 1
fi
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: CONFIG_FILE does not exist: $CONFIG_FILE"
    exit 1
fi
if ! [[ "$PARTITIONS" =~ ^[0-9]+$ ]] || [ "$PARTITIONS" -le 0 ]; then
    echo "Error: PARTITIONS must be a number greater than 0"
    exit 1
fi
# Convert to absolute paths
INPUT_FILE="$(cd "$(dirname "$INPUT_FILE")" && pwd)/$(basename "$INPUT_FILE")"
CONFIG_FILE="$(cd "$(dirname "$CONFIG_FILE")" && pwd)/$(basename "$CONFIG_FILE")"
EXPERIMENT_NAME="$(basename "$INPUT_FILE" .tiff)_$(basename "$CONFIG_FILE" .json)"
JOB_FILE="/dss/dsshome1/0D/di35pof/africapolis-workflow/jobs/${EXPERIMENT_NAME}.json"
cat > "$JOB_FILE" << EOF
{
    "gisInput":{
        "file":{
            "class": "File",
            "path": "$INPUT_FILE"
        }
    },
    "config":{
        "class":"File",
        "path": "$CONFIG_FILE"
    },
    "partitions": $PARTITIONS
}
EOF
WORKFLOW_FILE="/dss/dsshome1/0D/di35pof/africapolis-workflow/cwl/africapolis/AfricapolisWorkflow.cwl"
# Create output directory based on input file name and config file name
OUTPUT_DIR="/dss/dsshome1/0D/di35pof/africapolis-workflow/output/$EXPERIMENT_NAME"
mkdir -p "$OUTPUT_DIR"
cd "$OUTPUT_DIR" && toil-cwl-runner --singularity --batchSystem slurm  $WORKFLOW_FILE $JOB_FILE