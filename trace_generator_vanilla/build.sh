#!/bin/bash
echo "$(whoami)"

echo -e "${RED}Building Trace Generator...${DEF}"

[ "$UID" -eq 0 ] || exec sudo -E "$0" "$@"
if [ -z "${PIN_ROOT}" ]; then
  echo "Please set PIN_ROOT before setting up the trace generator."
  exit 1
fi

echo -e "${RED}Building Simpoint...${DEF}"
cd Simpoint3.2
make Simpoint
cd ..

echo -e "${RED}Building Tracer Tool...${DEF}"
cd ./src
echo -e "${RED} make gettrace.test${DEF}"
make gettrace.test
sudo rm -rf trace.out 