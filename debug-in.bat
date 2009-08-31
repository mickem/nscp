dir /O:-D /A:-D /B stage\installer\*x64* > tmp.txt
Set /P _INST=<tmp.txt
echo Install: %_INST%
msiexec /l* installer.txt /i stage\installer\%_INST%