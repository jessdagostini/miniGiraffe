patch -p < gbwt-patch.diff
patch -p < gbwtgraph-patch.diff
CURR_DIR=`pwd`
cd $CURR_DIR/deps/sdsl-lite
bash install.sh .
cd $CURR_DIR/deps/libhandlegraph
mkdir build
cd build/
cmake ..
make DESTDIR=.
make DESTDIR=. install
cd $CURR_DIR/deps/gbwt
bash install.sh .
cd $CURR_DIR/deps/gbwtgraph
CXXFLAGS="-I$CURR_DIR/deps/gbwt/include -I$CURR_DIR/deps/libhandlegraph/build/usr/local/include/" LDFLAGS="-L$CURR_DIR/deps/gbwt/lib -L$CURR_DIR/deps/libhandlegraph/build/usr/local/lib/" bash install.sh .
