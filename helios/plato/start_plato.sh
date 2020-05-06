#!/bin/bash

# load environment designed for plato
# TODO: find a way or a convention for detecting where the base conda environment is
# for now, assume where we installed miniforge3

SCRIPT=`realpath $0`
SCRIPTPATH=`dirname $SCRIPT`
MY_CONDA_ENV=plato
if [[ "${CONDA_PREFIX##*/}" != $MY_CONDA_ENV ]]; then
  source $SCRIPTPATH/../miniforge3/etc/profile.d/conda.sh
  conda activate $MY_CONDA_ENV
fi

cd ${SCRIPTPATH}/django

export LOG_FILE=uvicorn.log
export PYTHONUNBUFFERED=1
export PYTHONPATH=$PWD/../py
export PYTHONOPTIMIZE=1

uvicorn server.asgi:application --host 0 --port 8002 --workers 1 --loop uvloop --ws websockets --reload
