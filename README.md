# 1. Upload Files
- Upload CWL directory containing the main workflow file `AfricapolisWorkflow.cwl` in the same directory structure is in the repository (packed/single file version does not work with `toil`)
- Upload input files or pull from STAC
- Upload config file(s)
- Store files in [corresponding directory](directory_structure.png)
# 2. Install Toil
- Create python venv
```
module load python
python -m venv ~/africapolis-workflow/venv
```
- Source venv
``` 
source ~/africapolis-workflow/venv/bin/activate
```
- Install via pip
```
pip install toil[cwl]
```
# 3. Execute Workflow
Use run script which creates an output directory for each experiment (input + config combination) 
```
./run.sh input/Corvara_IT.tiff cfg/dbscan200.json 1
```
or use `toil-cwl-runner` directly (current directory will be output directory)
```
module load apptainer
source ~/africapolis-workflow/venv/bin/activate
toil-cwl-runner --singularity --batchSystem slurm  ~/africapolis-workflow/cwl/africapolis/AfricapolisWorkflow.cwl ~/africapolis-workflow/jobs/<JobFile>.json
```