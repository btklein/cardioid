#!/bin/bash 
nodes=1
ppn=1
let nmpi=$nodes*$ppn
tstamp=`date +%m_%d_%H_%M_%S`
#--------------------------------------
cat >batch.job <<EOF
#BSUB -o %J.out
#BSUB -e %J.err
#BSUB -R "span[ptile=${ppn}]"
#BSUB -R "select[ngpus=4] rusage[ngpus_shared=20]"
#BSUB -env "LSB_START_JOB_MPS=N"
#BSUB -n ${nmpi}
#BSUB -x
#BSUB -q excl
#BSUB -W 15
#---------------------------------------
ulimit -s 10240
#cat /proc/cpuinfo
export LD_LIBRARY_PATH=/home/ckim/project/CORAL/cardioid/serial_hpm/lib
./diffusion -x 62 -y 32 -z 32 -K simd_hoist_to_mthrd -N 100

export OMP_PLACES={0:1:8}
#export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=1
./diffusion -x 62 -y 32 -z 32 -K simd_mthrd_30disj -N 100

export OMP_PLACES={0:1:8}
export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=1
./diffusion -x 62 -y 32 -z 32 -K simd_mthrd_30disj -N 100
./diffusion -x 92 -y 32 -z 32 -K simd_mthrd_30disj -N 100
./diffusion -x 122 -y 32 -z 32 -K simd_mthrd_30disj -N 100

export OMP_PLACES={0:2:8}
export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=2
./diffusion -x 62 -y 62 -z 32 -K simd_mthrd_30disj -N 100
./diffusion -x 92 -y 62 -z 32 -K simd_mthrd_30disj -N 100
./diffusion -x 122 -y 62 -z 32 -K simd_mthrd_30disj -N 100

export OMP_PLACES={0:4:8}
export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=4
./diffusion -x 62 -y 62 -z 62 -K simd_mthrd_30disj -N 100
./diffusion -x 92 -y 62 -z 62 -K simd_mthrd_30disj -N 100
./diffusion -x 122 -y 62 -z 62 -K simd_mthrd_30disj -N 100

export OMP_PLACES={0:6:8}
export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=6
./diffusion -x 62 -y 92 -z 62 -K simd_mthrd_30disj -N 100
./diffusion -x 92 -y 92 -z 62 -K simd_mthrd_30disj -N 100
./diffusion -x 122 -y 92 -z 62 -K simd_mthrd_30disj -N 100

export OMP_PLACES={0:9:8}
export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=9
./diffusion -x 62 -y 92 -z 92 -K simd_mthrd_30disj -N 100
./diffusion -x 92 -y 92 -z 92 -K simd_mthrd_30disj -N 100
./diffusion -x 122 -y 92 -z 92 -K simd_mthrd_30disj -N 100

export OMP_PLACES={0:12:8}
export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=12
./diffusion -x 62 -y 122 -z 92 -K simd_mthrd_30disj -N 100
./diffusion -x 92 -y 122 -z 92 -K simd_mthrd_30disj -N 100
./diffusion -x 122 -y 122 -z 92 -K simd_mthrd_30disj -N 100

export OMP_PLACES={0:18:8}
export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=18
./diffusion -x 62 -y 182 -z 92 -K simd_mthrd_30disj -N 100
./diffusion -x 92 -y 182 -z 92 -K simd_mthrd_30disj -N 100
./diffusion -x 122 -y 182 -z 92 -K simd_mthrd_30disj -N 100

export OMP_PLACES={0:20:8}
export OMP_WAIT_POLICY=active
export OMP_NUM_THREADS=20
./diffusion -x 62 -y 152 -z 122 -K simd_mthrd_30disj -N 100
./diffusion -x 92 -y 152 -z 122 -K simd_mthrd_30disj -N 100
./diffusion -x 122 -y 122 -z 152 -K simd_mthrd_30disj -N 100



EOF
#---------------------------------------
bsub  <batch.job
