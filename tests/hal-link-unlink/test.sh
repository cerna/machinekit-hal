#!/usr/bin/env bash

realtime start
python hallink.py

realtime stop

exit $?
