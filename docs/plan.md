# USB Charging Station (DC Power v5 系統) 設計ドキュメント

## 1. プロジェクト概要

### 1.1 目的

DC 電源 v5(HLG-185H-15A → LiFePO4 12V 100Ah)の DC 出力を活用した、**棚置き型 USB 充電ステーション**を作る。3 ポートの USB 出力(PD/QC 対応)と、各ポートのリアルタイム情報を表示するローカル液晶を備える。

### 1.2 動機の優先順位

1. **実用性**: USB 充電ステーションとして日常的に使える(70%)
2. **可視化**: SW3518 から取得できる情報をローカル液晶で表示(20%)
3. **技術的好奇心**: SW35xx レジスタ叩き、PIO I²C、デュアルコア活用などの実装を楽しむ(10%)

### 1.3 スコープ外

- **Wi-Fi 連携、HA 統合、REST API**: ローカル完結とする
- **メインバッテリー SoC の表示**: HA タブレットで見る
- **タッチ操作 UI**: タッチパネル付き液晶だが、機能としては未使用
- **接続デバイスの SoC 取得**: USB PD では基本的に取得不可、推測ベースの「充電フェーズ」のみ表示
- **長期データ蓄積**: 直近 1 時間程度の表示用バッファのみ保持
- **OTA 更新**: USB 経由で書き込み

### 1.4 ESP32/RP2040 用途全体の割り当て

|用途|使用デバイス|電源|備考|
|---|---|---|---|
|**(1) 充電ステーション監視(本ドキュメント)**|**Waveshare RP2040-Zero**|BP5293-50 (12V→5V DC-DC) で独立給電|PIO I²C により 3 系統 I²C 構成、デュアルコアで処理分離|
|(2) LiFePo 監視(缶側)|ESP32-C3|USB 給電|BLE 構成|
|(3) Wi-Fi 時計(別プロジェクト)|ESP32-WROVER|単独電源|省電力要求、ディープスリープ前提|

---

## 2. 物理構成

### 2.1 配置

```

[煎餅缶] (床置き or 棚下、既存 v5 設計)

- HLG-185H-15A (PSU)
    
- LiFePO4 12V 100Ah
    
- ESP32-C3 (BLE 監視)
    
    ↓ DC 12V 配線
    

[充電ステーション] (棚上、本ドキュメントの対象)

- RP2040-Zero
- SW3518 ×3 (各々 USB-C rail と USB-A rail を持つ — 同時使用可、Vbus は 5V 縛り)
- HW-681 (LM2596 12V→5V + USB-A ×2)
- BP5293-50 (MCU 給電用 12V→5V DC-DC)
- ILI9341 2.4" TFT (タッチ機能は未使用)
- USB コネクタ最大 8 (SW3518 ×3 × 2 rail + HW-681 ×2)、UI が監視するのは SW3518 側のみ
- ユニバーサル基板実装

```

缶側と充電ステーションは **物理的・論理的に独立**。両者の間に通信はない。

### 2.2 電源系統

```

LiFePO4 (12.0〜14.4V) → 主ヒューズ → メイン系統2 (15A) → 充電ステーション (12V)
  │
  ├── SW3518 #1 ─┬─ USB-C rail (PD/QC ネゴ可)
  │              └─ USB-A rail (BC1.2 相当)
  ├── SW3518 #2 ─┬─ USB-C rail (PD/QC ネゴ可)   ※2 rail 同時使用時は Vbus 5V 縛り
  │              └─ USB-A rail (BC1.2 相当)
  ├── SW3518 #3 ─┬─ USB-C rail (PD/QC ネゴ可)
  │              └─ USB-A rail (BC1.2 相当)
  ├── HW-681 (LM2596) ── 5V ── USB-A ×2 出力 (BC1.2 相当の汎用充電)
  └── BP5293-50 (12V→5V) ── RP2040-Zero VBUS/5V ピン
                                   │
                                   └── 内蔵 LDO で 3.3V 生成
                                          ├── ILI9341 VCC
                                          └── I²C プルアップ 3.3V 側

```

**重要な前提**: v5 の 12V 系統は実際には **12.0〜14.4V で変動**する(HLG 充電中の最大値が 14.4V)。

ただし v5 の運用方針として **SOC 20〜30% で強制充電を発動**するため、実用的な入力電圧域は **13.0〜14.4V** に管理される。

- SW3518: データシート上 4.5〜24V 対応のため 12V 直結 OK
- HW-681 (LM2596): 入力 4.5〜40V 対応、出力 5V 最大 2A
- BP5293-50: 入力 7〜26V、出力 5V 1A、v5 の 12〜14.4V を完全カバー

#### 12V 系統の電流マージン試算

ピーク同時運用時の 12V 入力電流(効率を考慮):

- SW3518 ×3 が PD3.0 45W/ポートでフル稼働: 12V 換算で約 12A
- HW-681 が 2 ポート合計 ~2A 出力: 12V 換算で約 1A
- BP5293-50 + MCU/液晶 (~0.5W): 12V 換算で約 0.1A
- **合計ピーク: 約 13A**(メイン系統2 15A 定格の 87%)

実運用では 5 ポート同時最大は起きにくく、マージンは十分。

#### MCU 給電を独立 DC-DC で行う根拠

MCU + 液晶の 5V 給電は **BP5293-50 を専用に用意**し、SW3518 / HW-681 のいずれにも電気的に依存させない。理由:

- **SW3518 5V ランドからの取得を不採用**: SW3518 の 5V を流用すると、SW3518 が I²C で報告する Vbus / Iout の測定値に MCU 消費電流が干渉する懸念がある
- **HW-681 5V の分岐を不採用**: HW-681 は USB-A ×2 ポートを駆動するため、フル充電時に LM2596 の 2A 定格をタブレット側が食い潰し、5V ラインの垂下・ノイズで MCU リセットや表示異常を招くリスクが高い
- **独立 DC-DC は ¥240 の追加で最大の信頼性**: 12V から専用 5V を作るのが最も干渉が少なく、RP2040-Zero の標準給電経路 (VBUS → 内蔵 LDO → 3V3) を素直に使えるため USB 書き込み時の電源競合も発生しない

---

## 3. ハードウェア構成

### 3.1 主要部品

|部品|型番|数量|備考|
|---|---|---|---|
|MCU モジュール|RP2040-Zero 互換品|1|長辺 16 ピン使用、短辺 5 ピンは最終実装時に追加配線可|
|USB-PD 充電 IC|SW3518|3|モジュール形態|
|汎用 USB 充電モジュール|HW-681 (LM2596 + USB-A ×2)|1|12V→5V 降圧、USB-A ×2 出力|
|MCU 給電用 DC-DC|BP5293-50|1|秋月 111188、入力 7〜26V、出力 5V 1A、ROHM SIP3|
|TFT 液晶|ILI9341 2.4" 320×240 + XPT2046 タッチ|1|タッチは未配線|
|ユニバーサル基板|サイズ要検討|1||
|プルアップ抵抗|4.7kΩ|6|I²C 用 (3 系統)|
|USB ポート|USB-A or USB-C|3|SW3518 出力側|
|入出力パスコン|10μF 電解、0.1μF セラミック|適量|電源安定化|
|配線材、コネクタ等|||

### 3.2 GPIO 割当(RP2040-Zero 長辺 16 ピン構成)

開発時はブレッドボード使用のため、**長辺ピン (GP0〜GP15) のみで構成**する。短辺ピン (GP10〜GP13) は最終ユニバーサル基板実装時に余裕として残す。

```

[I²C0 - HW] (SW3518 #1) GP0: SDA0 GP1: SCL0

[I²C1 - HW] (SW3518 #2) GP2: SDA1 GP3: SCL1

[PIO I²C] (SW3518 #3、PIOステートマシンで実装) GP4: SDA GP5: SCL

[SPI0 - 液晶 ILI9341] GP6: SCK (SPI0 SCK) GP7: MOSI (SPI0 TX) GP8: CS GP14: DC GP15: BL (PWM)

[省略・直結] ILI9341 RST: 3.3V 直結(ソフトリセットコマンドで対応、ピン節約) ILI9341 MISO: 未配線(液晶への書き込み専用、タッチ不使用のため不要)

[内蔵] GP16: WS2812B RGB LED(基板裏面、ステータス表示用) USB端子: 書き込み・シリアルモニタ用 BOOT ボタン: 書き込みモード遷移用

[空き(将来拡張用)] GP9〜GP13(短辺ピン、最終実装時のみ使用可)

```

**RP2040 ピン配置の自由度**: HW I²C/SPI のピン割り当ては偶奇ペアの制約のみで、PIO I²C は完全自由。上記は SW3518 #1/#2/#3 の物理配線がしやすい順序で割り当てた案。

### 3.3 液晶のタッチ機能について

ILI9341 にはタッチパネル (XPT2046) が付属するが、本プロジェクトでは **タッチ機能は使用しない**。理由:

- 棚に置いた充電ステーションで触る動線が想定しにくい
- 常時表示で十分、画面切替不要

タッチ用の配線(CS、IRQ)は省略。

### 3.4 RP2040-Zero の実装方針

**開発時(Phase 1〜5)**: ブレッドボード使用。長辺 16 ピンにピンヘッダを実装し、必要に応じてジャンパーワイヤで配線。

**最終実装時(Phase 7)**: ユニバーサル基板にピンヘッダで実装。短辺ピンが必要な拡張があれば、その段階で配線追加。

**書き込み**: USB-C 経由でドラッグ&ドロップまたは PlatformIO upload。外部 USB-Serial 不要。

---

## 4. ソフトウェア構成

### 4.1 開発環境

- **フレームワーク**: PlatformIO + Arduino-Pico (Earle Philhower 版)
- **理由**:
    - dimitar 版 SW35xx ライブラリの I2CInterface 抽象がそのまま活用可能
    - TFT_eSPI が完全対応、描画パフォーマンス良好
    - PIO I²C は Arduino-Pico に組み込みサポートあり
    - デュアルコア API がシンプル
    - USB 経由の書き込み・シリアルモニタが標準

### 4.2 主要ライブラリ

|ライブラリ|提供元|用途|
|---|---|---|
|SW35xx_lib|github.com/dimitar-grigorov/SW35xx_lib|SW3518 制御|
|TFT_eSPI|bodmer/TFT_eSPI|ILI9341 液晶駆動|
|Arduino-Pico (Earle Philhower)|earlephilhower/arduino-pico|RP2040 用 Arduino コア、PIO I²C 含む|

### 4.4 デュアルコア活用方針

RP2040 のデュアルコアを活用し、**サンプリングと描画を物理コアで分離**する。

|コア|担当|処理|
|---|---|---|
|Core 0|データ取得|SW3518 ×3 ポーリング(1Hz)、リングバッファ更新、フェーズ判定|
|Core 1|描画|TFT_eSPI 描画、ステータス LED 更新|

コア間データ受け渡しは Arduino-Pico の mutex API または FIFO で行う。

---

## 5. I²C アドレス問題と解決方針

### 5.1 制約

- SW3518 の I²C アドレスは **0x3C 固定**(ストラップピンによる変更不可)
- 同一バスに 2 個以上ぶら下げるとアドレス衝突
- RP2040 のハードウェア I²C ペリフェラルは 2 個

### 5.2 採用案: HW I²C ×2 + PIO I²C ×1

RP2040 の PIO (Programmable I/O) ステートマシンを使い、3 系統目の I²C をハードウェアレベルで実装する。

**利点**:
- ビットバンギングではなくハードウェアステートマシンで動作
- **クロックストレッチ完全対応**(SW3518 がストレッチを使う場合でも安定)
- ピン配置が完全自由(任意の GPIO ペアで構成可)
- Arduino-Pico に組み込みサポートあり、自作不要

---

## 6. 機能仕様

### 6.1 ローカル液晶表示

**メイン画面(常時表示、320×240 横長 ROTATION 1)**

タイルは Port (= SW3518 IC 1 個) 単位で 3 つ並ぶ。各 Port のレイアウトは
`rail_mask` で動的切替する。

#### 単独 rail (USB-C or USB-A の片方だけが attach)

```
┌──── Port 0 ────┐
│  [Pill: PD3.0] │
│   18.45W       │   ← font4 (大)
│  9.00V  2.05A  │
│  🕗 00:23:14   │
│  [簡易SoC bar] │
│  ETA 1h 03m    │
│  4.21Wh        │
└────────────────┘
```

電圧・電流・経過時間・累積エネルギーまでフルに表示。これがほとんどの
時間帯の見え方になる。

#### 両 rail attach (`rail_mask == 0x3`、Vbus は 5V 縛り)

```
┌──── Port 1 ────┐
│ [Pill: Type-C 5V] │
│   7.50W           │   ← font4 (大、C 側)
│  [SoC bar (C)]    │
│  ETA 12m          │
│                   │   ← 余白 16 px
│ [Pill: Type-A 5V] │
│   4.00W           │   ← font4 (大、A 側)
│  [SoC bar (A)]    │
│  ETA 8m           │
└───────────────────┘
```

縦に 2 つ積み、`Type-C` / `Type-A` ラベル付きの pill で見分ける。
V/A の細字行・時計・セッション Wh はタイル高さに収まらないため省略
(単独 rail に戻れば再表示)。

**画面下部**: `Total %lu.%02luW` を全 Port 合計で表示。

**画面更新頻度**: 1 Hz。単独レイアウト中は差分更新 (変化した row のみ
描画)、両 rail のときは毎フレーム再描画 (rail mask 切替の境界以外は
SPI ~6 ms/秒 で十分軽い)。

### 6.2 履歴リングバッファ

RP2040 の内蔵 SRAM(264KB)に直近履歴を保持:

- **データ構造**: Port (= SW3518 IC) ごとにリングバッファ (3 本)。Vbus は 2 rail
  共通なので Port 単位、電流は rail 別 (`i_c_mA` / `i_a_mA`) に保持
- **`HistorySample` 構造 (12 B)**: `v_mV` / `i_c_mA` / `i_a_mA` / `proto` / `flags` / `reserved`
- **保持期間**: 直近 1 時間 × 1Hz = 3600 サンプル
- **合計サイズ**: 3 Port × 3600 × 12B ≒ **約 130 KB**

ILI9341 のフレームバッファ(部分更新で運用、フルバッファ不要)、Arduino-Pico ランタイム、各種ライブラリと合わせても 264KB に収まる。

**用途**:

1. 液晶の簡易 SoC バー描画 (peak_i_mA に対する現在 i_mA の比率)
2. 充電フェーズ判定 (直近数分の rail 別電流推移を見て CC/CV/完了を推測)
3. セッション統計 — rail 単位 (`SessionStats[kPorts][kRailsPerPort]`)。
   USB-A 側で扇風機を回しながら USB-C で PD ネゴ中の機器を抜き差し、と
   いう運用でも片方のセッションは継続する

### 6.3 充電フェーズ判定ロジック

リチウムイオン電池の典型的な充電パターンから推測する。判定は **rail 単位** に
行う (`analyze(history, Rail, reading)`) — USB-C 側がリチウム機器、USB-A 側が
LED ライト、のような混在運用で個別に Phase を出すため:

- 過去 60 秒の電流平均が定格 70% 以上で安定 → "CC 急速充電中"
- 過去 60 秒の電流が単調減少 → "CV 仕上げ充電中"
- 電流 < 100mA かつ電圧 > 4V → "充電ほぼ完了"
- 電流 < 50mA → "充電完了 / 待機"

**注意**: これは推測ベース。デバイスの実際の SoC とは厳密には対応しない。

### 6.4 ステータス LED (内蔵 WS2812B)

GP16 に接続された RGB LED でシステム状態を表示:

- 起動中: 青
- 正常動作: 緑(明るさ低め)
- I²C 通信エラー: 赤点滅
- 全ポート空き: 消灯または白(明るさ最低)

---

## 7. 開発フェーズ計画

### Phase 0: 環境構築

ソフト基盤とリモート開発インフラの整備。SW3518 到着前に着手可能な範囲を完了させる。

**ソフト・リモート開発インフラ(完了済み 2026-05-09)**
- PlatformIO プロジェクト雛形作成 (`platformio.ini`, `src/main.cpp`, `Makefile`)
- Arduino-Pico (Earle Philhower) で RP2040-Zero 内蔵 WS2812B (GP16) の R/G/B 1Hz Blink + Serial 出力を実装
- リモート開発インフラ構築: **Mac でビルド → Pi Zero W (USB 中継) → RP2040-Zero に書き込み**(詳細は [`docs/remote-dev-setup.md`](remote-dev-setup.md))
  - Mac は USB ケーブルで縛られず、Wi-Fi 経由で `make upload` / `make serial` できる
  - 初回のみ物理 BOOT ボタン必須、以降は 1200bps touch で自動 BOOTSEL 遷移
- 検証完了 (E1〜E6): ビルド → Pi 経由書き込み → シリアル出力受信 → ボタン無し再書き込み

**部品発注(進行中)**
- SW3518 モジュール、HW-681、BP5293-50、その他部品

**SW3518 / BP5293-50 / HW-681 到着後**
- BP5293-50 を 12V 入力で動作確認し、RP2040-Zero VBUS 給電で起動 → シリアル出力受信を再確認
- SW3518 ×3 と HW-681 を 12V 系統に並列接続して、各ポートの基本動作 (PD/QC ネゴ、USB-A 充電) を確認
- 結果に応じて §7 Phase 1 へ進む

#### Phase 0 で得られた知見(plan からの差分)

- **`build_flags` に `-DUSE_TINYUSB` を入れない**: Earle Philhower コアはデフォルトで USB CDC 対応、`USE_TINYUSB` を入れると CDC が enumerate されず picotool 含め接続不能になる
- **Waveshare RP2040-Zero の USB PID は起動中も BOOTSEL も `0x0003`**: 区別は `iProduct` 文字列(`"RP2 Boot"` / `"RP2040 Zero"`)、または `bInterfaceClass`(MSC+VendorSpec / CDC)
- **Raspberry Pi OS Bullseye / Pi Zero W (ARMv6) では `picotool` を apt で入れられない**: pico-sdk 2.0.0 + cmake からソースビルド(20〜30 分、一回限り)。`PICOTOOL_FLAT_INSTALL=1` で `/usr/local/picotool/picotool` に入るので `/usr/local/bin/picotool` へシンボリックリンク必要
- **Bullseye のデフォルト構成では `cdc_acm` カーネルモジュールが自動 load されない**: `/etc/modules-load.d/cdc_acm.conf` に `cdc_acm` を書く
- **udev rule は `ATTRS{idVendor}` ではなく `ENV{ID_VENDOR_ID}` で書く**: Bullseye の `tty` サブシステムでは `ATTRS` の親デバイス walk が効かなかった
- **socat の `crnl` オプションは削除**: 出力バッファリング遅延の原因になる

### Phase 1: 液晶単体動作

- RP2040-Zero + ILI9341 で Hello World
- TFT_eSPI のセットアップ(`User_Setup.h` 設定)
- バックライト PWM 制御
- 簡単なテキスト・図形描画

### Phase 2: SW3518 単体動作 (HW I²C)

- SW3518 #1 を HW I²C #0 (GP0/GP1) に接続
- dimitar 版 SW35xx_lib で電圧・電流・プロトコル取得確認
- 液晶に Port 1 の情報表示

ソフト側のドライバ層は実機到着前に先行実装済み (2026-05-11):

- dimitar 版 SW35xx_lib を `lib/SW35xx/` に vendoring (upstream 消失耐性)
- `Sw3518PortReader` (`src/sw3518_port_reader.{h,cpp}`) が `PortReader`
  契約を満たす形で実装、`make_port_reader(0)` で SW3518 #1 を返す
- `WireI2CWrapper` (`src/wire_i2c_wrapper.h`) が `TwoWire` を SW35xx_lib
  の `I2CInterface` に被せ、`begin()` で SDA/SCL/100kHz を一括設定
- 新 env `waveshare_rp2040_zero_hw` で `USE_MOCK_PORTS=0` ビルドが通る
  (`make build-hw` / `make upload-hw`)
- 実機到着後の作業は配線 + フラッシュ + シリアル目視のみ

### Phase 3: 2 ポート構成 (HW I²C ×2)

- SW3518 #2 を HW I²C #1 (GP2/GP3) に追加
- 2 ポート分の値を液晶に表示
- 表示レイアウトを 3 ポート用に拡張(#3 は未接続表示)

### Phase 4: 3 ポート構成 (HW I²C ×2 + PIO I²C ×1)

- SW3518 #3 を PIO I²C (GP4/GP5) に接続
- PioI2CWrapper.h を実装(dimitar 版 I2CInterface 適合)
- 3 ポート完全表示
- クロックストレッチ動作の検証

### Phase 5: リングバッファとフェーズ判定

- リングバッファ実装(Core 0、rail 別電流を 12 B/sample で保持)
- 1Hz サンプリング
- 簡易 SoC バー描画(peak_i_mA に対する現在 i_mA の比率、rail 単位)
- 充電フェーズ判定ロジック実装(rail 単位、CC/CV/NearDone/Done)
- セッション経過時間・積算 Wh の計算(rail 単位、両 rail 同時使用時は
  それぞれ独立のセッションを追跡)

### Phase 6: ステータス LED とエラーハンドリング

- 内蔵 WS2812B 制御
- I²C エラー時のリトライとリカバリ
- 電源異常時の挙動確認
- 長時間動作テスト(数日連続)

### Phase 7: 物理実装

- ユニバーサル基板レイアウト設計
- RP2040-Zero ピンヘッダ実装(長辺 + 必要なら短辺も)
- SW3518 ×3、HW-681、BP5293-50、ILI9341 配線
- BP5293-50 と HW-681 の独立 12V 分岐配線の確認
- ケース(棚上設置用)の検討・実装

---

## 8. 主要な設計判断

### 8.1 RP2040-Zero 採用の根拠

- **PIO I²C により 3 系統 I²C 構成がエレガントに実現可能**(クロックストレッチ完全対応)
- デュアルコアでサンプリングと描画を物理分離
- USB-C で書き込み・デバッグが完結、外部 USB-Serial 不要
- 小型(18×23.5mm)で筐体スペースを取らない
- 在庫品で追加コストなし
- 棚上設置で常時通電のため、RP2040 の弱点である省電力性能が問題にならない

### 8.2 Arduino-Pico (C++) 採用の根拠

- dimitar 版 SW35xx_lib の I2CInterface 抽象を活用、移植コスト最小
- TFT_eSPI による高速描画
- 開発を Claude Code に依頼する前提で、既存ライブラリ活用が最も品質安定
- コンパイル時型チェックにより実装ミスを早期検出

### 8.3 MCU 給電を独立 DC-DC (BP5293-50) で行う根拠

- **SW3518 5V 利用は不採用**: SW3518 の I²C 計測値 (Vbus / Iout) への干渉懸念があるため
- **HW-681 5V 共用は不採用**: USB-A ×2 ポートの充電負荷 (最大 2 台のタブレット) と容量を食い合うリスクがあるため
- **独立 DC-DC は ¥240 の追加で最大の信頼性**: 12V から専用 5V を作るのが最も干渉が少ない
- RP2040-Zero の標準給電経路 (VBUS → 内蔵 LDO → 3V3) を使うため、USB 書き込み時の電源競合が発生しない

### 8.4 タッチ機能未使用の根拠

- 棚置きの充電ステーションで触る動線が想定しにくい
- 常時表示で必要十分
- 配線・実装の複雑化を回避

### 8.5 Wi-Fi/HA 連携を採用しない根拠

- 「充電状況を見える化する」のがプロジェクトの本質
- HA 連携や長期データ蓄積は別途行えば良く、本プロジェクトの本質ではない
- ローカル完結により実装・運用がシンプル化
- Wi-Fi 認証情報管理、OTA セキュリティ等の懸念事項が消滅

---

## 9. 既知のリスク・要検証項目

### 9.3 PIO I²C のクロックストレッチ実機検証

Arduino-Pico の PIO I²C 実装はクロックストレッチ対応とされるが、SW3518 との実機組み合わせでの安定性は検証が必要。

**対策**: Phase 4 で重点的に検証。問題が発生した場合は I²C クロック周波数を 100kHz に下げる、あるいは PIO プログラムを調整。

### 9.4 RP2040-Zero ブレッドボード実装の制約

RP2040-Zero は短辺にもピンが出ているため、ブレッドボードに DIP 実装すると干渉する。

**対策**: 開発時は長辺 16 ピンのみピンヘッダを実装。短辺ピンは最終ユニバーサル基板実装時に必要に応じてはんだ付けで配線。

---

## 10. 参考資料

### 10.1 ライブラリ・データシート

- SW35xx 解析: hackaday.io/project/176558
- dimitar/SW35xx_lib: https://github.com/dimitar-grigorov/SW35xx_lib
- happyme531/h1_SW35xx (オリジナル): https://github.com/happyme531/h1_SW35xx
- Arduino-Pico (Earle Philhower): https://github.com/earlephilhower/arduino-pico
- TFT_eSPI: https://github.com/Bodmer/TFT_eSPI
- BP5293-50 データシート(MCU 給電用): https://akizukidenshi.com/goodsaffix/BP5293_DataSheet_J_VerC_161013.pdf
- SW3518 解説記事: https://unagidojyou.com/2022/11-29/arduino_read_sw3516_with_i2c/

### 10.2 関連ドキュメント

- 太陽光余剰電力活用 DC給電システム 設計サマリー

---

## 11. 次セッションへの引き継ぎ

### 11.1 すぐ着手できること

1. PlatformIO プロジェクト雛形の作成(Claude Code に依頼)
2. `platformio.ini` の設定(Arduino-Pico、ライブラリ依存定義)
3. TFT_eSPI の `User_Setup.h` 作成

### 11.2 SW3518 到着後の最初のステップ

Phase 0 で BP5293-50 による MCU 給電の動作確認、SW3518 ×3 と HW-681 の 12V 並列接続を確認し、Phase 2 から順次実装。

### 11.3 Claude Code への依頼想定

- PlatformIO プロジェクトの初期構造作成
- PioI2CWrapper.h の実装(dimitar 版 I2CInterface 適合)
- TFT_eSPI を使った液晶 UI 実装
- リングバッファクラスの実装
- ChargeAnalyzer の実装
- 各 Phase ごとの動作確認用スケッチ
- StatusLed クラスの実装

