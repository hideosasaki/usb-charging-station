# リモート開発環境セットアップ手順

Mac から Raspberry Pi Zero W 経由で RP2040-Zero に書き込み・シリアル接続するための手順。

## 構成

```
[Mac] ──Wi-Fi/SSH──> [Raspberry Pi Zero W] ──USB──> [RP2040-Zero]
       (build)         (flash + serial bridge)         (target)
```

- **Mac**: PlatformIO ビルドのみ。`.uf2` を生成し scp で Pi へ転送。
- **Pi Zero W**: `picotool` で書き込み、`socat` で `/dev/rp2040-cdc` を TCP 5333 にブリッジ。
- **書き込みフロー**: Mac で `make upload` → scp → Pi 上で 1200bps touch → `picotool load`。

## 前提

- Mac から Pi へ SSH 鍵認証で接続できる (パスワードレス)
- Pi で Raspberry Pi OS (Bullseye / Bookworm) が動作
- リポジトリ直下に `.env` を作成 (`.env.example` をコピー) し、`PI_HOST` に SSH エイリアス名を設定
- `~/.ssh/config` に `PI_HOST` と一致する `Host` エントリを定義 (`.ssh-config.example` 参照)

以降のドキュメント中の `$PI_HOST` は、各自の `.env` で設定したホスト名に読み替える。

## Pi 側セットアップ

リポジトリの `scripts/pi-setup.sh` を Pi にコピーして実行する。

```bash
# Mac から
scp scripts/pi-setup.sh $PI_HOST:/tmp/
ssh $PI_HOST "bash /tmp/pi-setup.sh"
```

スクリプトが行うこと:
- `picotool socat python3-serial usbutils` などの apt インストール
- `/etc/udev/rules.d/99-rp2040.rules` 配備 (BOOTSEL: `/dev/rp2040-bootsel`、CDC: `/dev/rp2040-cdc` シンボリックリンク)
- `/etc/systemd/system/rp2040-serial.service` (socat で TCP 5333 公開)
- `/etc/sudoers.d/rp2040-serial` (パスワードレスで service stop/start)
- `~/bin/rp2040-load.sh` (1200bps touch + picotool load)

セットアップ後、グループ反映のため一度 logout してから再ログイン。

### picotool が apt で入らない場合

ARMv6 (Pi Zero W 無印) でも Bookworm 公式の `picotool` armhf バイナリは動くはず。動かない場合は `pico-sdk` を入れた上でソースビルド:

```bash
sudo apt install -y cmake build-essential libusb-1.0-0-dev pico-sdk
git clone https://github.com/raspberrypi/picotool ~/picotool
cd ~/picotool && mkdir build && cd build
cmake -DPICO_SDK_PATH=/usr/share/pico-sdk ..
make -j2     # Pi Zero W では 10〜15 分
sudo make install
```

## Mac 側セットアップ

`.ssh-config.example` の中身を `~/.ssh/config` に追記。

```bash
cat .ssh-config.example >> ~/.ssh/config
```

スクリプトに実行権限:
```bash
chmod +x scripts/*.sh
```

## 使い方

### 初回のみ (BOOTSEL 物理エントリ)

RP2040-Zero の BOOT ボタンを押しながら Pi の USB ポートに挿す。`lsusb` で `2e8a:0003` が見えれば OK。

```bash
ssh $PI_HOST "lsusb -d 2e8a:0003"
```

### 書き込み

Mac で:
```bash
make upload
```

これは以下を実行する:
1. `pio run` でビルド (`firmware.uf2` 生成)
2. `scp` で Pi に転送
3. `ssh` 越しに `~/bin/rp2040-load.sh` を実行 → 1200bps touch → `picotool load -f -x -v`

### シリアル接続

Mac で:
```bash
make serial
```

`nc $PI_HOST 5333` で Pi 上の socat が `/dev/rp2040-cdc` をブリッジ。Ctrl-C で抜ける。

### 書き込み + シリアル

```bash
make flash
```

## トラブルシューティング

| 症状 | 対処 |
|---|---|
| `firmware not found` | `make build` (= `pio run`) 先に実行 |
| `picotool: no devices found` | RP2040 が BOOTSEL に入っていない。物理 BOOT ボタン押しながら抜き差し |
| `Resource busy` (1200bps touch 時) | `sudo systemctl stop rp2040-serial.service` を Pi で手動実行 |
| `nc: connection refused` | Pi 上で `systemctl status rp2040-serial.service` 確認、`/dev/rp2040-cdc` が存在するか確認 |
| 1200bps touch が効かない | スケッチが Serial を初期化していない / 暴走中 → 物理 BOOT ボタン使用 |
| Mac から Pi に SSH できない | ホスト名の名前解決確認 (mDNS / DNS / hosts)、`ssh -v` でデバッグ |

## アーキテクチャメモ

- **なぜ socat の `fork` か**: クライアント (Mac) 切断時に親 listener を維持するため
- **なぜ `crnl` か**: Arduino-Pico の `Serial.println` は `\r\n` を出すので nc 側で改行が二重にならない
- **なぜ `ControlMaster auto` か**: scp + ssh を連続で叩くため、TCP ハンドシェイクの再利用で Pi Zero W (CPU 弱) の負荷削減
- **なぜ TCP 5333 か**: 任意。`PI_PORT` 環境変数で変更可
