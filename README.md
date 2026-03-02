# Africapolis Workflow
The *Africapolis* Workflow is completely orchestrated with the [Common Workflow Language (CWL)](https://www.commonwl.org/user_guide/), with each stage being a command line program, composed based on [*Fishnet* framework](https://gitlab2.informatik.uni-wuerzburg.de/descartes/sos/fishnet).
# Development
The required binaries of *Africapolis* can be manually install on the system. This can be achieved with the [install](install.sh) script. Make sure that the install prefix location (*$INSTALL_PREFIX*) is referenced in *PATH* (e.g. *usr/local/bin*). 
```shell
./install.sh
```
Additionally, a [CWL Runner](https://www.commonwl.org/implementations/) must be installed to execute the workflow. The reference executor [cwltool](https://cwltool.readthedocs.io/en/latest/cli.html#cwltool) is recommended and can be installed as follows:
```shell
sudo apt-get install cwltool
```
### Running the Workflow
```
cwltool Africapolis.cwl --shapefile <File.shp> --configFile <Config.Json> --partitions <UnsignedInt>
``` 
# Deployment
## Docker
A custom docker image containing the binaries and the GDAL library is built using the [Dockerfile](Dockerfile). Use the [CWL Workflow File](cwl/Africapolis.cwl) to run the workflow.
## HPC
Use the [Africapolis Shellscript](prod/hpc/run-africapolis.sh) to install and run the workflow on a HPC system. You can also do the following steps manually:
### 1. Upload Files
- Upload CWL directory containing the main workflow file `Africapolis.cwl` in the same directory structure as in the repository (packed/single file version does not work with `toil`)
- Upload input files or pull from STAC
- Upload config file(s)
- Store files in [corresponding directory](directory_structure.png)
### 2. Install Toil
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
### 3. Execute Workflow
Execute via the `toil-cwl-runner` directly (current directory will be output directory)
```
module load apptainer
source ~/africapolis-workflow/venv/bin/activate
toil-cwl-runner --singularity --batchSystem slurm  ~/africapolis-workflow/cwl/AfricapolisWorkflow.cwl --shapefile <File.shp> --configFile <Config.Json> --partitions <UnsignedInt>
```