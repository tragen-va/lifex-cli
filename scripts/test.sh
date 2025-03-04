#!/bin/bash



cd ../
make

if [ $? != 0 ]; then
    echo "compilation failed"
    exit 1
fi





ip="10.0.0.226"
#ip="192.168.8.141"



echo -e "[ On / Off ]\n"

# on off
./lifx-cli --off --all
if [ $? != 0 ]; then
    echo "Test 0: fail"
else
    echo "Test 0: pass"
fi
./lifx-cli --on -a
if [ $? != 0 ]; then
    echo "Test 1: fail"
else
    echo "Test 1: pass"
fi
./lifx-cli -f -p $ip
if [ $? != 0 ]; then
    echo "Test 2: fail"
else
    echo "Test 2: pass"
fi
./lifx-cli --ip $ip -o
if [ $? != 0 ]; then
    echo "Test 3: fail"
else
    echo "Test 3: pass"
fi


./lifx-cli --off 843 --all
if [ $? == 0 ]; then
    echo "Test 4: fail"
else
    echo "Test 4: pass"
fi
./lifx-cli --on -a -p $ip
if [ $? == 0 ]; then
    echo "Test 5: fail"
else
    echo "Test 5: pass"
fi
./lifx-cli -f -p $ip 934
if [ $? == 0 ]; then
    echo "Test 6: fail"
else
    echo "Test 6: pass"
fi
./lifx-cli -ip $ip --o
if [ $? == 0 ]; then
    echo "Test 7: fail"
else
    echo "Test 7: pass"
fi


















echo -e "\n\n[ Single Colors ]\n"
# color
./lifx-cli --setColor [0x1a7a38] --all
if [ $? != 0 ]; then
    echo "Test 8: fail"
else
    echo "Test 8: pass"
fi
./lifx-cli -c [0xeb4034] --ip $ip 
if [ $? != 0 ]; then
    echo "Test 9: fail"
else
    echo "Test 9: pass"
fi
./lifx-cli -a -c [0x4034eb] --duration 100
if [ $? != 0 ]; then
    echo "Test 10: fail"
else
    echo "Test 10: pass"
fi


./lifx-cli -c 0x1a7a38 --all
if [ $? == 0 ]; then
    echo "Test 11: fail"
else
    echo "Test 11: pass"
fi
./lifx-cli -c [0xeb4034,0x5b18c2] --ip $ip 
if [ $? == 0 ]; then
    echo "Test 12: fail"
else
    echo "Test 12: pass"
fi
./lifx-cli -a -c [0x4034eb] -d 1000000000000
if [ $? == 0 ]; then
    echo "Test 13: fail"
else
    echo "Test 13: pass"
fi









echo -e "\n\n[ Warmth ]\n"
#warmth
./lifx-cli --warmth 9000 --all
if [ $? != 0 ]; then
    echo "Test 14: fail"
else
    echo "Test 14: pass"
fi
./lifx-cli -w 2700 -p $ip
if [ $? != 0 ]; then
    echo "Test 15: fail"
else
    echo "Test 15: pass"
fi
./lifx-cli -p $ip --warmth 2700 --duration 100
if [ $? != 0 ]; then
    echo "Test 16: fail"
else
    echo "Test 16: pass"
fi




./lifx-cli -p $ip -warmth 270 --duration 100
if [ $? != 0 ]; then
    echo "Test 17: fail"
else
    echo "Test 17: pass"
fi














echo -e "\n\n[ Brightness ]\n"
#brightness
./lifx-cli --all --brightness 10
if [ $? != 0 ]; then
    echo "Test 18: fail"
else
    echo "Test 18: pass"
fi
./lifx-cli -b 50 -p $ip 
if [ $? != 0 ]; then
    echo "Test 19: fail"
else
    echo "Test 19: pass"
fi
./lifx-cli --brightness 75 --all
if [ $? != 0 ]; then
    echo "Test 20: fail"
else
    echo "Test 20: pass"
fi
./lifx-cli --ip $ip -brightness 100     
if [ $? != 0 ]; then
    echo "Test 22: fail"
else
    echo "Test 22: pass"
fi



./lifx-cli --ip $ip --brightness 150           #bad
if [ $? != 0 ]; then
    echo "Test 23: pass"
else
    echo "Test 23: fail"
fi










echo -e "\n\n[ Multi Color ]\n"
#multiColor
./lifx-cli --ip $ip --setColorsR [0x46b34c,0xf09f3d,0x3debf0]
if [ $? != 0 ]; then
    echo "Test 24: fail"
else
    echo "Test 24: pass"
fi
./lifx-cli --ip $ip -s [0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0,0xe29627,0x159fd5,0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0,0xe29627,0x159fd5]
if [ $? != 0 ]; then
    echo "Test 25: fail"
else
    echo "Test 25: pass"
fi


./lifx-cli --ip $ip -r [0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0,0xe29627,0x159fd5,0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0,0xe29627,0x159fd5,0x46b34c]
if [ $? != 0 ]; then
    echo "Test 26: fail"
else
    echo "Test 26: pass"
fi








./lifx-cli --ip $ip --setColors [0x46b34c,0xf09f3d,0x3debf0]
if [ $? == 0 ]; then
    echo "Test 27: fail"
else
    echo "Test 27: pass"
fi

./lifx-cli --all --setColorsR [0x46b34c,0xf09f3d,0x3debf0]
if [ $? == 0 ]; then
    echo "Test 27: fail"
else
    echo "Test 27: pass"
fi

./lifx-cli --ip $ip -s [0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0,0xe29627,0x159fd5,0x228b22,0xe296,0x89cef9,0x485d18,0x290c22,0xef7ec0,0xe29627,0x159fd5]
if [ $? == 0 ]; then
    echo "Test 28: fail"
else
    echo "Test 28: pass"
fi













echo -e "\n\n[ List / Info ]"
# list/Info
./lifx-cli --ip $ip --info > /dev/null
if [ $? != 0 ]; then
    echo "Test 29: fail"
else
    echo "Test 29: pass"
fi

./lifx-cli -l > /dev/null
if [ $? != 0 ]; then
    echo "Test 30: fail"
else
    echo "Test 30: pass"
fi




./lifx-cli -p $ip -info 4354 > /dev/null
if [ $? == 0 ]; then
    echo "Test 31: fail"
else
    echo "Test 31: pass"
fi








# ./lifx --ip 192.168.8.141 -s [0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0,0xe29627,0x159fd5,0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0,0xe29627,0x159fd5]
