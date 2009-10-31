rem dir /O:-D /A:-D /B stage\installer\*x64* > tmp.txt
echo dir /O:-D /A:-D /B stage\%2installer\*%1* 
dir /O:-D /A:-D /B stage\%2installer\*%1* > tmp.txt
Set /P _INST=<tmp.txt
echo Install: %_INST%
msiexec /l* installer.txt /i stage\%2installer\%_INST%