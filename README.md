tel2com
====

SSH または telnet とシリアルポートを中継するプログラムです。  
昔のパソコン通信の BBS ホストプログラムに SSH または telnet で接続したり、RS-232C で制御する機器を SSH または telnet を使用して LAN やインターネットから制御できます。  
SSH は SSH2 のみ対応しています。認証方式はパスフレーズ認証、公開鍵認証に対応しています。また、ユーザー名とパスワードを BBS プログラム側に渡す方法も選択可能です。  
公開鍵認証で使用可能な鍵の種類は RSA/ECDSA/ED25519 です。  
Raspberry Pi, Nano Pi, BeagbleBone Black, PINE64 等の Linux が動作するシングルボードコンピュータでも使用可能です。  
ただし、シングルボードコンピュータで RS-232C を使用する場合、UART の信号レベルは 3.3V ですので、レベル変換 IC 等を使用して接続する必要があります。  
お勧めはしませんが USB シリアルポート変換ケーブルを使用して中継する事も可能です。  

## 対応機器・デストリビューション
PC (Ubuntu 20, CentOS 8)  
Raspberry Pi (Raspbian Buster)  
NanoPi NEO/NEO2 (Armbian Buster)  
BeagleBone Black (Debian Buster)  
PINE64 (Armbian Buster)  
上記は動作を確認した機器・デストリビューションですが、言語は C で、基本的なライブラリしか使用していませんので、libssh がインストールできる環境であれば問題なくビルド、実行できると思います。  
libssh は 0.8.4 より前のバージョンでは脆弱性が発見されているようです。脆弱性は他にもあるようですので、可能な限り新しいバージョンを使用してください。  

## ビルド方法
libssh-dev または libssh-devel をインストールしてください。  
$ sudo apt install libssh-dev  
$ sudo yum install libssh-devel  
libssh は [LGPLv2.1](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html) です。  
各機器のディレクトリに移動し、make を実行してください。  

PC (Ubuntu, CentOS 等)の場合  
$ cd PC  
$ make  

PC 版の Linux では gcc や make がインストールされていない場合があります。  
その場合、
$ sudo apt install gcc make  
$ sudo yum install gcc make  
等を実行してください。  

Raspberry Pi の場合  
$ cd RaspberryPi  
$ make  

NanoPi の場合  
$ cd NanoPi  
$ make  

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

SSH については認証方法の各項目の設定が必要です。  
パスフレーズ認証の場合、ユーザー名を user= に、  
$ tel2com -p パスフレーズ  
を実行して表示される文字列を pass= に設定してください。  
公開鍵認証の場合、公開鍵のファイル名を pub_key= に設定してください。  
BBS 認証の場合、BBS の ID やパスワード入力時に表示される文字列を bbs_id=, bbs_pass= に設定してください。  

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
