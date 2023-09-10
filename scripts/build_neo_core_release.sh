#!/usr/bin/bash
# (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

echo "Compiling NEO production packages..."
cores=$(nproc)
cores=$((cores - 2))
echo "Using $cores CPU cores"
cd ..
root="./bin/production"
rm -rf $root
mkdir $root
cmake -S ./ -B ./$root # configure
cmake --build ./$root --config=Release --target neo --target=neo -j $cores # build neo
rm -rf tmp
mkdir tmp
mv ./$root/neo ./tmp/neo
rm -rf $root
mkdir $root
mv ./tmp/neo ./$root/neo
rm -rf tmp
cd $root || exit
zip -r ./neo.zip ./*
echo "Done compiling NEO production packages."
