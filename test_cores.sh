#!/bin/bash
set -e

# Copy the source
cp bancada.c bancada_cores.c

# Patch bancada_cores.c to add 3 new colors and a dummy state array
# This simulates the memory footprint of syntax highlighting
sed -i 's/static XftColor  c_ink, c_fraco, c_sel, c_aberto;/static XftColor  c_ink, c_fraco, c_sel, c_aberto, c_kw, c_str, c_com;/g' bancada_cores.c

# Add a dummy state array allocation to simulate lexer state (1 int per line)
sed -i '/linhas = realloc/a \
    static int *estados = NULL;\
    estados = realloc(estados, (size_t) cap_linhas * sizeof *estados);' bancada_cores.c

# Compile both
gcc -O2 -Wall -o bancada_old bancada.c -lX11 -lXft $(pkg-config --cflags xft)
gcc -O2 -Wall -o bancada_cores bancada_cores.c -lX11 -lXft $(pkg-config --cflags xft)

# Measure memory
echo "Starting bancada_old..."
./bancada_old bancada.c &
PID_OLD=$!
sleep 2
PSS_OLD=$(grep -i pss /proc/$PID_OLD/smaps_rollup | awk '{print $2}')
kill $PID_OLD

echo "Starting bancada_cores..."
./bancada_cores bancada_cores.c &
PID_CORES=$!
sleep 2
PSS_CORES=$(grep -i pss /proc/$PID_CORES/smaps_rollup | awk '{print $2}')
kill $PID_CORES

echo "PSS Old: ${PSS_OLD} kB"
echo "PSS Cores: ${PSS_CORES} kB"
DIFF=$((PSS_CORES - PSS_OLD))
echo "Difference: ${DIFF} kB"

