#!/bin/sh

./sim -is 8192 -ds 8192 -bs 16 -a 1 -wb -wa trazas/spice.trace
./sim -is 16384 -ds 16384 -bs 16 -a 1 -wb -wa trazas/spice.trace
./sim -is 32768 -ds 32768 -bs 16 -a 1 -wb -wa trazas/spice.trace
./sim -is 65536 -ds 65536 -bs 16 -a 1 -wb -wa trazas/spice.trace
./sim -bs 16384 -bs 16 -a 1 -wb -wa trazas/spice.trace
./sim -bs 16384 -bs 32 -a 1 -wb -wa trazas/spice.trace
./sim -bs 16384 -bs 64 -a 1 -wb -wa trazas/spice.trace
./sim -is 32768 -ds 32768 -bs 128 -a 1 -wb -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 64 -a 2 -wb -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 64 -a 4 -wb -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 64 -a 8 -wb -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 64 -a 16 -wb -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 64 -a 128 -wb -wa trazas/spice.trace
./sim -is 1024 -ds 1024 -bs 64 -a 2 -wb -wa trazas/spice.trace
./sim -is 1024 -ds 1024 -bs 64 -a 8 -wb -wa trazas/spice.trace
./sim -is 1024 -ds 1024 -bs 64 -a 16 -wb -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 16 -a 1 -wt -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 32 -a 1 -wt -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 64 -a 2 -wt -wa trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 16 -a 1 -wb -nw trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 32 -a 1 -wb -nw trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 64 -a 2 -wb -nw trazas/spice.trace
./sim -is 8192 -ds 8192 -bs 16 -a 1 -wt -nw trazas/spice.trace # prueba
