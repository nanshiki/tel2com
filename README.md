tel2com
====

TCP/IP とシリアルポートを中継するプログラムです。  
昔のパソコン通信の BBS ホストプログラムや RS-232C で制御する機器を telnet を使用して LAN やインターネットから制御できます。  
Raspberry Pi, Nano Pi, BeagbleBone Black 等の Linux が動作するシングルボードコンピュータでも使用可能です。  
ただし、シングルボードコンピュータで RS-232C を使用する場合、UART の信号レベルは 3.3V ですので、レベル変換 IC 等を使用して接続する必要があります。  

## 対応機器・デストリビューション
PC (Ubuntu 18, CentOS 7)  
Raspberry Pi (Raspbian)  
NanoPi NEO/NEO2 (Ubuntu)  
BeagleBone Black (Angstrom)  
PINE64 (Debian)  
言語は C で、基本的なライブラリしか使用していませんので、最近の Linux であれば問題なくビルド、実行できると思います。  

## ビルド方法
各機器のディレクトリに移動し、make を実行してください。  

PC (Ubuntu, CentOS 等)の場合  
$ cd PC  
$ make  

PC 版の Ubuntu では gcc や make がインストールされていない場合があります。  
$ sudo apt install gcc make  
を実行してください。  

Raspberry Pi の場合  
$ cd RaspberryPi  
$ make  

NanoPi の場合  
$ cd NanoPi  
$ make  

Raspberry Pi, Nano Pi の場合、WriningPi のライブラリをインストールしておく必要があります。  

BeagleBone Black の場合  
$ cd BeagleBone  
$ make  

PINE64 の場合  
$ cd PINE64  
$ make  

## 実行
設定は各機器向けのディレクトリの tel2com.ini に記述してあります。  
PC の場合は  
[COM]  
name=ttyS0  
を使用するシリアルポートに変更してください。  

ケーブルや RS-232C のレベル変換方法等については下記を参照して下さい。  
[BBS ホストプログラムとの接続方法](https://www.nanshiki.co.jp/software/t2c_connect.html)

ケーブル接続後、各機器のディレクトリに生成された tel2com を実行します。  
待ち受けする TCP/IP のポート番号が 1024 以下の場合 sudo 等を使用して root 権限で起動する必要があります。  
PC の場合、ファイアウォールで待ち受けする TCP/IP のポート番号が閉じている場合がありますのでご注意下さい。  
シリアルポートは dialout グループになっていると思われますので、一般ユーザーで起動する場合は dialout グループに追加するか sudo 等を使用して root 権限で起動してください。  

## ライセンス

[MIT ライセンス](https://github.com/tcnksm/tool/blob/master/LICENCE)

サポートは有償となります。  
複数回線対応等のカスタマイズは可能です。  

## 著作者

[有限会社軟式](https://www.nanshiki.co.jp)
