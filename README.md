 AVRマイコンでUSB Power Deliveryする

## 概要

USB PDコントローラーを使わず、AVRのペリフェラル（CCL, SPI, USART, AC, DAC）とMOSFETだけでUSB PDをやってようと始めたプロジェクト。

このツイートのリプライツリーを見ると考えてることがわかるかも。[https://twitter.com/minagi_yu/status/1478704467101257728](https://twitter.com/minagi_yu/status/1478704467101257728)

## 構成

- CCL

受信側ではBMC((Biphase Mark Code)をデコードするために使用。送信側ではCCラインを駆動するためのトーテムポールのMOSFETの下側のゲートの出力に使用。

- SPI

スレーブに設定して、CCLでデコードしたCCラインのデータを受信する。

- USART

ハード回路でBMCにエンコードしないので、ボーレートを倍の周波数としてあらかじめBMCにしたデータを送信する。

- AC

CCラインの電圧レベルは約1.2Vのため、直接デジタル入力できないため、しきい値の変換器として使用。

- DAC

CC出力の電圧レベルも1.2Vなので送信用MOSFETのコレクタ電源として使用。

- TCB0

300kbpsのBMCのデコードするときの遅延時間を設けるために使用。

## 進捗

- 送信側の処理を無効にして、作った[USB PDアナライザー](https://github.com/minagi-yu/USB_PD_Analyzer)でPDのパケットを見れた
- 送受信を決め打ちで設定して、Appleの充電器から9Vを取り出せた
- receiveブランチで送信側のプロトコルスタックを作成中 **NOW**
  - SPIのバイトオーダーが逆のほうが作りやすかったので変更
  -  まずは[パソコン側のソフト](https://github.com/minagi-yu/PdPacketDecodeTest/)でSOPとかCRCとかの検証
