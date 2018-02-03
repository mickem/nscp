mkdir tmp-protobuf
pushd tmp-protobuf/
wget https://protobuf.googlecode.com/files/protobuf-2.4.1.tar.bz2
tar jxvf protobuf-2.4.1.tar.bz2
cd protobuf-2.4.1
./configure --prefix /usr
make
sudo make install
cd python
sudo python setup.py install
popd