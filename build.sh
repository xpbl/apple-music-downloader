docker rm --force amwrapper
set -e
bash -c "cd wrapper && cmake . && make"
rm -f bin/rootfs/amwrapper
mv bin/amwrapper bin/rootfs/amwrapper
go build -o bin/amdownloader -buildvcs=false
cp Dockerfile ./bin
bash -c "cd bin && docker build --no-cache -t amwrapper -f Dockerfile ."