#!/bin/sh
# Replays the scripted demonstration and stores the transcript in demo/session.log.
set -e
cd "$(dirname "$0")"
rm -rf scratch && mkdir -p scratch out
../cms --data scratch/demo.csv < steps/in.txt > out/session.log 2>&1
cp out/session.log session.log
echo "Wrote demo/session.log ($(wc -l < session.log) lines)"
