#!/bin/bash


if ! type conda; then
    echo We need conda installed and available in the environment
    echo See https://docs.conda.io/en/latest/miniconda.html for installation
    echo instructions.
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "Usage:"
    echo "   $0 <rendered filename>"
    echo ""
    echo ""
    echo "Parameters:"
    echo "   <rendered filename>  : name passed to '--file' of 'conda render'"
    echo "                          required"
    exit 1
fi

rendered_output="$1"


missing_cmd=0
req_cmds=(jq yq)
for cmd in "${req_cmds[@]}"; do
    if ! type $cmd; then
	echo "Please 'conda install -c conda-forge $cmd'"
	missing_cmd=1
    fi
done

if [[ "$missing_cmd" != "0" ]]; then
    exit 1
fi

set -e
set -o pipefail

# ensure conda-build is installed in base environment
if conda info --json | jq --exit-status '.conda_build_version == "not installed"' > /dev/null; then
    echo "::INFO:: Installing conda-build in base environment"
    (set -x; conda install -n base -y conda-build)
fi


case "$(uname)" in
    Darwin)
	conda_platform='osx_64'
	;;
    Linux)
        conda_platform='linux_64'
	;;
    *)
	echo "::ERROR:: Unknown uname '$(uname)'"
	exit 1
	;;
esac

# script must run from root of the repo
script_bin=$(dirname $0)
cd "$script_bin/.."


# variant file names are subject to change when we rerender the CI
variant_file=".ci_support/${conda_platform}_.yaml"
if [[ ! -e "$variant_file" ]]; then
    echo "::ERROR:: variant file '$PWD/$variant_file' does not exist. Giving up."
    exit 1
fi



if [[ ! -d "conda.recipe" ]]; then
    echo "::ERROR:: I'm not where I'm supposed to be. Giving up."
    exit 1
fi

echo "Rendering recipe in $PWD/conda.recipe"

# SYSTEM_DEFAULTWORKINGDIRECTORY is defined in Azure Pipelines and isn't needed here
# when we're just rendering the recipe.  Define it so that we don't get a warning
# from 'conda render'
export SYSTEM_DEFAULTWORKINGDIRECTORY="$PWD"


# Render the recipe into a fully-resolved yaml file
(set -x; conda render -m "$variant_file" --file "$rendered_output" conda.recipe)

echo "::SUCCESS:: Rendered recipe written to '$rendered_output'"
