mkdir tmp-cryptopp
pushd tmp-cryptopp/
wget http://www.cryptopp.com/cryptopp562.zip
unzip cryptopp562.zip
make
sudo make install
popd