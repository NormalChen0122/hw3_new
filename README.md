(1) how to setup and run my program <br>
1. 一開始先將uLCD display和B_L4S5I_IOT01A相接，角位如下<br>
serial rx接到D0<br>
serial tx接到D1<br>
reset pin接到D2<br>
5V接到5V<br>
GND接到GND<br>
(注意) 要記得這裡的rx, tx, reset不是看uLCD板子上的，而是線上面的<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/uLCD_pin.jpg)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/uLCD_pin_mbed.jpg)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/uLCD_pin_mbed2.jpg)<br>

2. 打開我的手機熱點，等待電腦連到手機的熱點<br>
如果不是要用我的手機熱點而是要用自己的熱點的話，需要做出以下動作:<br>
打開自己的手機熱點，確認電腦連線後，以及VMware選擇bridge的連線方法<br>
接下來在terminal上面打上```ip address```來查看自己的VMware ip address<br>
接下來將在model_deploy的資料夾底下的mbed_app.json裡的SSID and PASSWORD給換成自己的網路熱點設定<br>
接下來將在model_deploy的資料夾底下的main.cpp裡面的```const char* host = "192.168.58.120";```給換成自己的VMware ip address.<br>
接下來將在model_deploy的資料夾底下的wifi_mqtt/mqtt_client.py裡面的```host = "192.168.58.120"```給換成自己的VMware ip address.<br>

3. 接下來就可以將B_L4S5I_IOT01A和notebook給接起來<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/mbed_pc.jpg)<br>
4. 然後輸入此command，將code給燒入B_L4S5I_IOT01A(要注意command裡面的那個ee2405v3那個要看自己的mbed-os-build是在哪個資料夾底下，來去做改變)<br>
```sudo mbed compile --source . --source ~/ee2405v3/mbed-os-build/ -m B_L4S5I_IOT01A -t GCC_ARM --profile tflite.json -f```
5. 待燒入完成後，按下B_L4S5I_IOT01A的reset button(也就是在藍色按鈕旁邊的黑色按鈕)<br>
6. 直到uLCD上面寫說run python code，即可新開一個terminal，輸入下列command<br>
```sudo python3 wifi_mqtt/mqtt_client.py```
7. 並在原本的terminal輸入下列command(注意/dev/ttyACM0可能會更動)<br>
```sudo screen /dev/ttyACM0```
8. 如此程式及需要觀察的視窗都開啟完成，即可開始操作老師所想要我們達成的功能<br>
接下來是講解程式中如何操作，而操作結果的圖則是放在(2) what are the results<br>
9. 一開始會進入到gesture UI mode(LED1會亮)，有40, 50 ,60 ,70, 80五個角度可供選擇<br>
我們可以利用出拳來增加選擇的角度<br>
也可以利用畫圓或是畫slope來減少選擇的角度<br>
10. 按下user button就可以選定目前選擇到的角度，也就是uLCD上面紅色的角度，他會將選定的角度以WiFi/MQTT的方式傳給broker，而python會接收到，並且python會將其print出來<br>
11. 如此操作後就會進入到tilt angle detection mode(LED2會亮)，一開始會橘光(LED3)會亮5秒，讓我們有時間可以放好B_L4S5I_IOT01A，接下來橘光和藍光會交替閃爍，代表正在initializing the angle，當停止閃爍，並且亮藍光的時候，就代表initialize完成，如此uLCD上會顯示threshold angle(紅色)，和current angle(綠色)<br>
12. 接下來可以開始傾斜板子，當current angle大於threshold angle的時候，B_L4S5I_IOT01A會將超過的angle數值以WiFi/MQTT的方式傳給broker，而python會接收到，並將其print在screen上面，也會輸出目前超過幾次了<br>
13. 如果動作12的次數達到10次，python就會傳送RPC的指令，使得tilt angle detection mode停止，並且重新從動作9開始執行<br>

(2) what are the results<br>
一開始還沒執行python code的樣子<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/run_python_code.jpg)<br>
執行wifi_mqtt/mqtt_client.py之後的模樣，進入到gesture UI mode了，第二張照片是LED1亮的樣子<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/gesture_UI.jpg)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/LED1_lightOn.jpg)<br>
選擇角度，紅色為目前所選擇的角度，第二章照片是screen上面會做出的反應(將判斷出選擇的動作給print出來)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/gesture_UI_sel.jpg)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/gesture_UI_sel_screen.jpg)<br>
選定角度,接著進入到tilt angle detection mode，螢幕上python會輸出接收到的選定角度，而mbed會在screen輸出要我們把板子放好，，第二張照片是LED2亮的樣子<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/confirm_sel.jpg)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/LED2_lightOn.jpg)<br>
要initialize角度之前(亮橘光)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/pre_initilize.jpg)<br>
initialize完畢(亮藍光)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/initialize_over.jpg)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/initialize_over_screen.jpg)<br>
將threshold angle(紅色)和current angle(綠色)給秀在uLCD上<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/tilt_mode.jpg)<br>
將B_L4S5I_IOT01A給傾斜超過threshold angle，此超過的angle由python讀取到並且輸出<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/exceed_sel_ang.jpg)<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/exceed_sel_ang_10times_screen.jpg)<br>
當超過threshold angle的次數到達10次之後，回到gesture UI mode了<br>
![image](https://github.com/NormalChen0122/hw3_new/blob/master/hw3_picture/back_to_gesture_UI_mode.jpg)<br>

在最後面講一下是怎麼實做code的<br>
我以當初的mbed08的model_deploy為基礎開始架構其他環境<br>
把mbed09的RPC和mbed10的wifi/mqtt環境也都架構好<br>
接下來就是將gesture UI mode和tilt angle detection mode給分別寫成RPC function<br>
而運作時的迴圈都會用done_mode0和done_mode1來當作繼續運作或停止的flag<br>
所以也就可以想見，我要停止這兩個mode的function所要做的事情就是將done_mode1 = 1或是done_mode0 = 1<br>
然後還有一個比較關鍵的就是因為RPC的function的parameter已經是固定的，也就是只有in和out，並沒有我們mqtt會用到的client<br>
所以我是將其另成global<br>
至於python code，我設了兩個client，分別用來接收超過的角度，以及設定的角度<br>