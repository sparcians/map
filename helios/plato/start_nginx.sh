#!/bin/bash

# argument is the signal sent to a master process: stop, quit, reopen, reload 
# no argument starts the server

SCRIPT=`realpath $0`
SCRIPTPATH=`dirname $SCRIPT`

# load environment designed for nginx that supports plato
# TODO: find a way or a convention for detecting where the base conda environment is
# for now, assume where we installed miniforge3
MY_CONDA_ENV=nginx

if [[ "${CONDA_PREFIX##*/}" != $MY_CONDA_ENV ]]; then
  source $SCRIPTPATH/../miniforge3/etc/profile.d/conda.sh
  conda activate $MY_CONDA_ENV
fi

SIG=${1+"-s $1"}

nginx -c ${SCRIPTPATH}/server/nginx.conf -p ${SCRIPTPATH} $SIG
