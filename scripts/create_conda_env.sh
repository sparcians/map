#!/bin/bash


if [[ $# -ne 2 ]]; then
    echo "Usage:"
    echo "   $0 <env name> <dev|run>"
    echo ""
    echo ""
    echo "Parameters:"
    echo "   <env name>  : name passed to '-n' of 'conda env create'"
    echo "                 must not already exist"
    echo "                 required"
    echo "   <dev|run>   : specify 'dev' or 'run' environment"
    echo "                 required"
    exit 1
fi

env_name="$1"


case "$2" in
    dev | run)
	devrun="$2"
	;;
    *)
	echo "::ERROR:: <dev|run> can only be 'dev' or 'run', not '$2'. Giving up." >&2
	exit 1
esac



TMPFILE=$(mktemp rendered_recipe.XXXXXX)

TMPENV="${TMPFILE/rendered_recipe/environment}"
# conda env create requires a known extension to differentiate between requirements.txt and enviornment.yaml
TMPENV="${TMPENV}.yml"


# script must run from root of the repo
script_bin=$(dirname $0)

set -xe

cd "$script_bin/.."



./scripts/render_recipe_for_platform.sh "$TMPFILE"


"$script_bin/rendered2env_yaml.sh" "$TMPFILE" "$devrun" > "$TMPENV"

#cleanup TMPFILE
rm "$TMPFILE"

# Create the environment!
(set -x; conda env create -f="$TMPENV" -n "$env_name")

# cleanup TMPENV (intentionally leaving it in case something goes wrong with creating environment
# user can manually rerun
rm "$TMPENV"
