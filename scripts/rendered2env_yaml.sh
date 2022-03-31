#!/bin/bash


script_bin=$(dirname $0)

if [[ $# -ne 2 ]]; then
    echo "Usage:"   >&2
    echo "   $0 <rendered conda.recipe> <dev|run>"  >&2
    echo ""  >&2
    echo ""  >&2
    echo "Parameters:"  >&2
    echo "   <rendered conda.recipe>  : output of $script_bin/render_recipe_for_platform.sh"  >&2
    echo "   <dev|run>                : specify 'dev' or 'run' environment"  >&2
    exit 1
fi

recipe="$1"

if [[ ! -f "$recipe" ]]; then
    echo "::ERROR:: Unable to find recipe '$recipe'. Giving up." >&2
    exit 1
fi

# NOTE: this depends on having the Argos 'run' requirements
# duplicated in 'build' because they get rendered with 
    #run:
    #    - python_abi 3.7.* *_cp37m
    #
# but the build requirements are rendered with
#
    #build:
    #    - python_abi 3.7 1_cp37m
    #
# and if we flatten those into the same list for 'conda env create'
# conda complains that it doesn't know how to merge 
# '*_cp37m' with '1_cp37m'
# That is probably an edge case that conda doesn't hit
# when people aren't flattening all of the requirements into


case "$2" in
    dev)
	sections='.requirements.build, .requirements.host'
	;;
    run)
	sections='.requirements.run'
	;;
    *)
	echo "::ERROR:: <dev|run> can only be 'dev' or 'run', not '$2'. Giving up." >&2
	exit 1
esac





# Convert rendered recipe into an environment.yaml file
rendered2env_yaml() {
    echo "channels:"
    echo "  - conda-forge"
    echo "dependencies:"
    #shyaml get-values requirements.build requirements.host requirements.run | \
    cat "$recipe" | \
	yq -y "[$sections] | flatten" | \
	sed '
            # strip the leading dash and space
	    s/^- //;
	    # convert whitespace that does not have
	    # an operator following it to "="
	    s/ \([^<>=!]\)/=\1/g;
	    # remove remaining whitespace
	    s/ //g;
            # put leading dash and space back
	    s/^/  - /' | sort
}

rendered2env_yaml "$1"
