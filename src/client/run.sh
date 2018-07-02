#!/bin/bash

server=10.11.1.192:50000
runtime=10
ware=400
cli=400
io=8
core=16
tech=ilock
out=

function check() {
  out=$1
  if [ -d ${out} ]; then
    for i in `seq 1 128`; do
      nout=${out}-${i}
      if [ ! -d ${nout} ]; then 
        out=$nout
        break;
      fi
    done;
  fi
  mkdir -p $out;
  echo "mkdir ${out}";
}

function start() {
  mkdir -p ${out}
  
  for c in `seq 20 20 400`; do
    let pc=c/5;
    ssh tzhu@90s191 "./ilock/src/client/TpccClient -c ${pc} -s  81:160:400 -a ${server} -t ${runtime}" > $out/${pc}-91.txt &
    ssh tzhu@90s193 "./ilock/src/client/TpccClient -c ${pc} -s 161:240:400 -a ${server} -t ${runtime}" > $out/${pc}-93.txt &
    ssh tzhu@90s194 "./ilock/src/client/TpccClient -c ${pc} -s 241:320:400 -a ${server} -t ${runtime}" > $out/${pc}-94.txt &
    ssh tzhu@90s196 "./ilock/src/client/TpccClient -c ${pc} -s 321:400:400 -a ${server} -t ${runtime}" > $out/${pc}-96.txt &
    ./TpccClient -c ${pc} -s    1:80:400 -a ${server} -t ${runtime}  > $out/${pc}-90.txt  -l
    cp $out/${pc}-90.txt $out/${c}.txt
    sleep 5s;
  done;
}

function start2() {
  mkdir -p ${out}
  
  for c in `seq 200 100 400`; do
    let pc=c/4;
    let wn=ware/4;
    ssh tzhu@90s191 "./ilock/src/client/TpccClient -c ${pc} -s   $[wn+1]:$[wn*2]:${ware} -a ${server} -t ${runtime}" > $out/${c}-91.txt &
    ssh tzhu@90s193 "./ilock/src/client/TpccClient -c ${pc} -s $[wn*2+1]:$[wn*3]:${ware} -a ${server} -t ${runtime}" > $out/${c}-93.txt &
    ssh tzhu@90s194 "./ilock/src/client/TpccClient -c ${pc} -s $[wn*3+1]:$[wn*4]:${ware} -a ${server} -t ${runtime}" > $out/${c}-94.txt &
    ./TpccClient -c ${pc} -s    1:${wn}:${ware} -a ${server} -t ${runtime}  > $out/${c}-90.txt  -l
    cp $out/${c}-90.txt $out/${c}.txt
    sleep 5s;
  done;
}

function exp0() {
  basepath="e0/${tech}" 
  for ware in `seq 50 50 400`; do
    check ${basepath}/ware-${ware}
    for cli in `seq 50 50 400`; do
      let pc=cli/4;
      let wn=ware/4;
      echo "running ${cli} clients visit ${ware} warehouses";
      savepath=$out/${cli}
      ssh tzhu@90s191 "./ilock/src/client/TpccClient -c ${pc} -s   $[wn+1]:$[wn*2]:${ware} -a ${server} -t ${runtime}" > $savepath-91.txt &
      ssh tzhu@90s193 "./ilock/src/client/TpccClient -c ${pc} -s $[wn*2+1]:$[wn*3]:${ware} -a ${server} -t ${runtime}" > $savepath-93.txt &
      ssh tzhu@90s194 "./ilock/src/client/TpccClient -c ${pc} -s $[wn*3+1]:$[wn*4]:${ware} -a ${server} -t ${runtime}" > $savepath-94.txt &
      ./TpccClient -c ${pc} -s           1:${wn}:${ware} -a ${server} -t ${runtime}  > $savepath-90.txt -l
      cp $savepath-90.txt $savepath.txt;
      sleep 5s;
    done;
  done;
}


function exp1() {
  basepath="e1/${tech}" 
  for ware in `seq 100 100 400`; do
    check ${basepath}/ware-${ware}
    for cli in `seq 50 50 400`; do
      let pc=cli/4;
      let wn=ware/4;
      echo "running ${cli} clients visit ${ware} warehouses";
      savepath=$out/${cli}
      ssh tzhu@90s191 "./ilock/src/client/TpccClient -c ${pc} -s   $[wn+1]:$[wn*2]:${ware} -a ${server} -t ${runtime}" > $savepath-91.txt &
      ssh tzhu@90s193 "./ilock/src/client/TpccClient -c ${pc} -s $[wn*2+1]:$[wn*3]:${ware} -a ${server} -t ${runtime}" > $savepath-93.txt &
      ssh tzhu@90s194 "./ilock/src/client/TpccClient -c ${pc} -s $[wn*3+1]:$[wn*4]:${ware} -a ${server} -t ${runtime}" > $savepath-94.txt &
      ./TpccClient -c ${pc} -s           1:${wn}:${ware} -a ${server} -t ${runtime}  > $savepath-90.txt -l
      cp $savepath-90.txt $savepath.txt;
      sleep 5s;
    done;
  done;
}

function exp2() {
  basepath="e2/${tech}" 
  for ware in `seq 50 50 400`; do
    check ${basepath}/ware-${ware}
    for cli in `seq 100 100 400`; do
      let pc=cli/4;
      let wn=ware/4;
      echo "running ${cli} clients visit ${ware} warehouses";
      savepath=$out/${cli}
      ssh tzhu@90s191 "./ilock/src/client/TpccClient -c ${pc} -s   $[wn+1]:$[wn*2]:${ware} -a ${server} -t ${runtime}" > $savepath-91.txt &
      ssh tzhu@90s193 "./ilock/src/client/TpccClient -c ${pc} -s $[wn*2+1]:$[wn*3]:${ware} -a ${server} -t ${runtime}" > $savepath-93.txt &
      ssh tzhu@90s194 "./ilock/src/client/TpccClient -c ${pc} -s $[wn*3+1]:$[wn*4]:${ware} -a ${server} -t ${runtime}" > $savepath-94.txt &
      ./TpccClient -c ${pc} -s           1:${wn}:${ware} -a ${server} -t ${runtime}  > $savepath-90.txt -l
      cp $savepath-90.txt $savepath.txt;
      sleep 5s;
    done;
  done;
}

function exp3() {
  basepath="me1/${tech}"
  read_count=10;
  write_count=0;
  for theta in 0.8 0.9; do
   # for row_number in 10000 100000 1000000; do
   for row_number in 10000 100000 1000000 10000000; do
      check ${basepath}/theta-${theta}/tablesize-${row_number}
      for cli in 20 40 50 100 150 200 250 300 350 400; do
        let pc=cli/4;
        echo "running ${cli} clients visit ${row_number} rows, theta: ${theta}";
        savepath=$out/${cli}
        ssh tzhu@90s191 "./ilock/src/client/TpccClient -c ${pc} -p ${read_count}:${write_count}:${row_number}:${theta} -a ${server} -t ${runtime} -m" > $savepath-91.txt &
        ssh tzhu@90s193 "./ilock/src/client/TpccClient -c ${pc} -p ${read_count}:${write_count}:${row_number}:${theta} -a ${server} -t ${runtime} -m" > $savepath-93.txt &
        ssh tzhu@90s194 "./ilock/src/client/TpccClient -c ${pc} -p ${read_count}:${write_count}:${row_number}:${theta} -a ${server} -t ${runtime} -m" > $savepath-94.txt &
        ./TpccClient -c ${pc} -p ${read_count}:${write_count}:${row_number}:${theta} -a ${server} -t ${runtime} -m  > $savepath-90.txt -l
        cp $savepath-90.txt $savepath.txt;
        sleep 5s;
      done;
    done;
 done;
}

function ext() {
  base="e3/ilock"
  rm all;
  for c in `seq 2 2 16`; do    
    for f in `seq 50 50 400`; do      
      grep COMMIT core-${c}/${f}.txt | awk '{print $7}' >> tmp;  
    done;  
    cat tmp | paste -s >> all;   
    rm tmp; 
  done;
}

function dump() {
  for c in `seq 20 20 400`; do
    grep "global.cpp:58" ${out}/${c}.txt | awk '{print $7}' | paste -s >> all ;
  done;
  cat all
  rm all
}

$1
