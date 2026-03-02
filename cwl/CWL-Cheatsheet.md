# CWLTool
| Flag | Behaviour |
|---|---|
| `--leave-tmpdir` | Prevents deletion of temporary directories after execution
| `--tmpdir-prefix` `<Directory>` | Sets directory for temporary files
| `--log-dir` `<Directory>`| Sets directory for log files
| `--js-console` | Allows debugging js code with `console.log()`
| `--validate` | Just validates `.cwl` file, no job execution
| `--pack` | Bundles a workflow and its dependencies into a single file

For all command line options refer to the official [documentation](https://cwltool.readthedocs.io/en/latest/cli.html)
# Toil
| Flag | Behaviour |
|---|---|
| `--workDir` `<Directory>` | Sets directory for toil orchestration files
| `--batchSystem slurm` | Schedule jobs via slurm 


For all command line options refer to the official [documentation](https://toil.readthedocs.io/en/latest/running/cliOptions.html)