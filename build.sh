bash -c "cd wrapper && cmake . && make"
mv wrapper/bin/amwrapper bin/amwrapper 2>/dev/null
go build -o bin/amdownloader -buildvcs=false
cd bin