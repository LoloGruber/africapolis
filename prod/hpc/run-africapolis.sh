#!/bin/bash
# Installation
REINSTALL=false
WORKFLOW_DIR="$HOME/africapolis-workflow"
while [[ $# -gt 0 ]]; do
    case $1 in
        --update)
            REINSTALL=true
            shift
            ;;
        *)
            break
            ;;
    esac
done

if [ ! -d "$WORKFLOW_DIR" ] || [ "$REINSTALL" = true ]; then
    rm -rf "$WORKFLOW_DIR"
    mkdir -p "$WORKFLOW_DIR"
    cd $TMPDIR
    rm -rf africapolis
    git clone https://github.com/LoloGruber/africapolis.git
    cp -r africapolis/cwl/ $WORKFLOW_DIR/cwl
    cp -r africapolis/data/input $WORKFLOW_DIR/input
    cp -r africapolis/data/cfg $WORKFLOW_DIR/cfg
    cd $WORKFLOW_DIR
    module load python 
    python -m venv venv
    source venv/bin/activate
    pip install toil[cwl]
    pip install nodeenv
    nodeenv -p
    echo "Installation complete. You can now run the workflow using the following command:"
    echo "$0 --input <INPUT_FILE> --config <CONFIG_FILE> --partitionDepth <DEPTH>"
    exit 0
fi
# Load modules
module load apptainer
module load gcc # required by nodejs
module load squashfs
source $WORKFLOW_DIR/venv/bin/activate
# Parse command line arguments
INPUT_FILE=""
CONFIG_FILE=""
PARTITIONS=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --input)
            INPUT_FILE="$2"
            shift 2
            ;;
        --config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        --partitionDepth)
            DEPTH="$2"
            shift 2
            ;;
        *)
            echo "Error: Unknown option $1"
            echo "Usage: $0 --input <INPUT_FILE> --config <CONFIG_FILE> --partitionDepth <DEPTH>"
            exit 1
            ;;
    esac
done
# Validate input parameters
if [ -z "$INPUT_FILE" ] || [ -z "$CONFIG_FILE" ] || [ -z "$DEPTH" ]; then
    echo "Error: Missing required parameters. Usage: $0 --input <INPUT_FILE> --config <CONFIG_FILE> --partitionDepth <DEPTH>"
    exit 1
fi
# Convert to absolute paths
INPUT_FILE="$(cd "$(dirname "$INPUT_FILE")" && pwd)/$(basename "$INPUT_FILE")"
CONFIG_FILE="$(cd "$(dirname "$CONFIG_FILE")" && pwd)/$(basename "$CONFIG_FILE")"
EXPERIMENT_NAME="$(basename "$INPUT_FILE" .gpkg)_$(basename "$CONFIG_FILE" .json)"
WORKFLOW_FILE="$WORKFLOW_DIR/cwl/Africapolis.cwl"
# Setup JobStore directory
JOB_STORE="$TMPDIR/toil"
mkdir -p "$JOB_STORE"
rm -rf "$JOB_STORE"
# Create output directory based on input file name and config file name
OUTPUT_DIR="$WORKFLOW_DIR/output/$EXPERIMENT_NAME"
mkdir -p "$OUTPUT_DIR"
cd "$OUTPUT_DIR"
# Run TOIL CWL workflow
SLURM_TIME=23:59:59
toil-cwl-runner --singularity --batchSystem slurm --jobStore $JOB_STORE --doubleMem True --retryCount 2 --slurmTime $SLURM_TIME --writeLogs $WORKFLOW_DIR/log $WORKFLOW_FILE --vectorFile "$INPUT_FILE" --configFile "$CONFIG_FILE" --partitionDepth "$DEPTH"
if [ $? -eq 0 ]; then
    echo "Workflow execution complete. Output files are located in $OUTPUT_DIR"
fi