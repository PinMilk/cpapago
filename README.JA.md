# cpapago

[한국어](README.md) · [English](README.en.md) · [日本語](README.JA.md)

[`nodepapago`](https://github.com/PinMilk/nodepapago)の翻訳・言語検出機能を**C11**に移植した非公式のPapagoライブラリです。
macOS/Linux向けに**C APIとCLI**を提供し、HTTP通信にはlibcurl、並列処理にはPOSIXスレッドを使用します。

> 非公式のWeb APIを使用するため、Papagoのサービス変更によって動作しなくなる場合があります。
> 翻訳文はモデルによって変わります。入力はPapagoのサーバーに送信されるため、機密情報を入力しないでください。

## 目次

- [クイックスタート](#クイックスタート)
- [ビルド環境](#ビルド環境)
- [CLIの使い方](#cliの使い方)
- [Cライブラリの使い方](#cライブラリの使い方)
- [言語コード](#言語コード)
- [テスト](#テスト)
- [パフォーマンス](#パフォーマンス)
- [制限とセキュリティ](#制限とセキュリティ)
- [プロジェクト構成](#プロジェクト構成)

## クイックスタート

先に[必要なツール](#ビルド環境)をインストールしてください。

```bash
git clone https://github.com/PinMilk/cpapago.git
cd cpapago
make

# 原文の言語を自動検出して韓国語に翻訳
./.build/cpapago translate ko 'So far, so good!'

# 言語検出: en
./.build/cpapago detect 'So far, so bueno!'
```

すでにリポジトリがある場合はcloneを省略します。以降のコマンドは**プロジェクトのルート**で実行してください。

| ビルド結果 | 用途 |
| --- | --- |
| `.build/cpapago` | コマンドラインプログラム |
| `.build/libcpapago.a` | 静的ライブラリ |
| `.build/basic` | 単一テキスト翻訳の実行例 |

## ビルド環境

必須：**C11コンパイラー、Make、libcurlの開発用ファイル、`curl-config`**。
テストにはPython 3、メモリ検査にはsanitizer対応のClangが必要です。

### 依存関係のインストール

**macOS** — Xcode Command Line Toolsをインストールします。インストール済みなら省略してください。

```bash
xcode-select --install
```

**Ubuntu / Debian**:

```bash
sudo apt update
sudo apt install build-essential libcurl4-openssl-dev python3
```

**Fedora**:

```bash
sudo dnf install gcc make libcurl-devel python3
```

ほかのLinuxディストリビューションでは、相当するパッケージをインストールしてください。
WindowsではWSLのLinux環境でビルドできます。**ネイティブのMSVCビルドには対応していません**。

### ビルド設定

```bash
make                               # 標準ビルド
make CC=clang                      # コンパイラーを指定
make -C /path/to/cpapago            # 別のディレクトリからビルド
```

`/path/to/cpapago`は実際のプロジェクトパスに置き換えてください。MakefileはPATH上の`curl-config`からヘッダー・リンクオプションを取得します。
macOSでHomebrew版libcurlを使用する場合：

```bash
brew install curl
make CURL_CONFIG="$(brew --prefix curl)/bin/curl-config"
```

別のインストール先は`make CURL_CONFIG=/path/to/curl-config`で指定できます。
ビルド後にコンパイラー・libcurl・コンパイルオプションを変更した場合は、`make clean`の後に再ビルドしてください。

## CLIの使い方

```text
cpapago detect TEXT
cpapago translate TARGET TEXT [OPTIONS]
cpapago batch TARGET TEXT... [OPTIONS]
```

### 使用例

```bash
# 原文の言語を指定し、敬語での翻訳を要求
./.build/cpapago translate ko 'Morning, sir.' --source en --honorific

# 入力順に文字列配列を出力
./.build/cpapago batch ko 'Morning, sir.' "Morning, ma'am." --source en --honorific

# APIレスポンス全体をJSONオブジェクトの配列で出力
./.build/cpapago batch ko 'Morning!' 'Morning, baby!' --source en --verbose

# 中国語コードの大文字・小文字変換はライブラリが処理
./.build/cpapago translate ko '你好，很高兴认识你。' --source zh-cn
```

| オプション | 説明 | 既定値 |
| --- | --- | --- |
| `--source CODE` | 原文の言語。省略または`detect`で自動検出 | 自動検出 |
| `--honorific` | 敬語での翻訳を要求。対応状況は言語の組み合わせによる | 無効 |
| `--verbose` | 翻訳文の代わりにAPIレスポンス全体のJSONを出力 | 無効 |
| `--timeout MS` | **HTTPリクエストごと**のタイムアウト（ミリ秒）。正の整数 | `30000` |
| `--parallel N` | 複数翻訳のワーカー数。`1`～`16` | `4` |
| `--base-url URL` | APIサーバーの変更。ローカルHTTPテストなどに使用 | `https://papago.naver.com` |
| `--` | 以降の引数をオプションではなくテキストとして扱う | — |

言語検出は小文字の言語コード、単一翻訳は文字列、複数翻訳はJSON配列を出力します。
`--verbose`の場合、単一翻訳はJSONオブジェクト、複数翻訳はオブジェクトの配列です。
エラーは標準エラー出力に表示します。終了コードは成功`0`、リクエスト失敗`1`、使用方法の誤り`2`です。

## Cライブラリの使い方

公開型・関数とメモリ所有権の規則は[`include/cpapago.h`](include/cpapago.h)を参照してください。

| 関数 | 成功時の出力 | 解放方法 |
| --- | --- | --- |
| `cpapago_detect()` | 小文字の言語コード | `free(output)` |
| `cpapago_translate()` | 翻訳文またはJSON全体の文字列 | `free(output)` |
| `cpapago_translate_many()` | 入力順を維持した結果文字列の配列 | `cpapago_free_many(outputs, count)` |

戻り値が`CPAPAGO_OK`であることを確認してから出力ポインターを使用してください。

### 単一翻訳

次の内容を`myapp.c`として保存してください。

```c
#include "cpapago.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    cpapago_parameter parameter = {
        .source = NULL,  /* 自動検出 */
        .target = "ko",
        .text = "So far, so good!"
    };
    cpapago_options options = {0};
    cpapago_error error = {0};
    char *output = NULL;

    if (cpapago_init() != CPAPAGO_OK) return 1;
    cpapago_status status = cpapago_translate(&parameter, &options, &output, &error);
    if (status == CPAPAGO_OK) puts(output);
    else fprintf(stderr, "%s\n", error.message);

    free(output);
    cpapago_cleanup();
    return status == CPAPAGO_OK ? 0 : 1;
}
```

ライブラリをビルドした後、コンパイル・リンクします。

```bash
cc -std=c11 -Iinclude myapp.c .build/libcpapago.a $(curl-config --libs) -pthread -o .build/myapp
./.build/myapp
```

別のlibcurlでビルドした場合は、リンクにも**同じ`curl-config`**を使用してください。

### 複数翻訳

次のコードは`cpapago_init()`の後、`cpapago_cleanup()`の前に使用します。

```c
cpapago_parameter parameters[] = {
    {"en", "ko", "Morning, sir."},
    {"en", "ko", "Morning, ma'am."}
};
cpapago_options options = {.honorific = true};
cpapago_error error = {0};
char **outputs = NULL;

if (cpapago_translate_many(parameters, 2, &options, &outputs, &error) == CPAPAGO_OK) {
    puts(outputs[0]);
    puts(outputs[1]);
} else {
    fprintf(stderr, "%s\n", error.message);
}
cpapago_free_many(outputs, 2);
```

### Cのオプション

`cpapago_options`に`NULL`またはゼロ初期化した値（`{0}`）を渡すと既定値を使用します。

| フィールド | 説明 | 既定値 |
| --- | --- | --- |
| `honorific` | 敬語での翻訳を要求 | `false` |
| `verbose` | 翻訳文の代わりにJSON全体を文字列で返す | `false` |
| `timeout_ms` | HTTPリクエストごとのタイムアウト。`0`は既定値 | `30000` |
| `parallelism` | 複数翻訳のワーカー数。`0`は既定値、最大`16` | `4` |
| `base_url` | APIサーバーのアドレス。`NULL`は既定値 | `https://papago.naver.com` |

### メモリとエラー処理

- 入力は**ヌル終端のUTF-8文字列**です。呼び出しが完了するまで文字列・オプションを変更・解放しないでください。
- `cpapago_init()`はリクエスト用スレッドの作成前に一度、`cpapago_cleanup()`はすべての処理が完了した後に呼び出します。
- 成功時の出力は上の表に従って解放してください。失敗時の出力ポインターは`NULL`です。
- `cpapago_error`には状態・HTTPコード・説明が入ります。不要な場合は`NULL`を渡せます。
- 複数翻訳は同期処理です。ワーカーの完了を待って戻り、1件でも失敗すると部分結果をすべて解放します。
  件数`0`は成功し、出力は`NULL`です。
- Cの`verbose`出力はJavaScriptオブジェクトではなく**JSON文字列**です。必要に応じて別途解析してください。

## 言語コード

| コード | 言語 | コード | 言語 |
| --- | --- | --- | --- |
| `ko` | 韓国語 | `en` | 英語 |
| `ja` | 日本語 | `zh-cn` | 中国語（簡体字） |
| `zh-tw` | 中国語（繁体字） | `hi` | ヒンディー語 |
| `es` | スペイン語 | `fr` | フランス語 |
| `de` | ドイツ語 | `pt` | ポルトガル語 |
| `vi` | ベトナム語 | `id` | インドネシア語 |
| `fa` | ペルシア語 | `ar` | アラビア語 |
| `mm` | ビルマ語 | `th` | タイ語 |
| `ru` | ロシア語 | `it` | イタリア語 |

翻訳できる方向はPapagoが対応する言語の組み合わせに依存します。
中国語コードはリクエスト時に`zh-CN`・`zh-TW`へ変換し、検出結果は小文字で返します。

## テスト

| コマンド | 検証内容 | 外部APIの呼び出し |
| --- | --- | --- |
| `make test` | JSON・引数の検証、ローカルHTTPのエンコード・並列処理・エラー・タイムアウト | なし |
| `make test-sanitize` | 同じテストをClang ASan/UBSanで実行 | なし |
| `make test-live` | 実際の言語検出・翻訳・敬語・JSONレスポンス全体 | あり |

実APIテストにはネットワーク接続が必要です。翻訳文の完全一致ではなく、主に戻り値の形式を検証します。

## パフォーマンス

フォーム・JSON処理の一時割り当てを減らし、自動検出・複数翻訳ではワーカーごとにHTTP接続を再利用します。
原文の言語が分かっている場合は、`--source`またはCの`source`パラメーターを指定すると検出リクエストを省略できます。
測定条件・結果・再現方法は[PERFORMANCE.md](PERFORMANCE.md)（韓国語）を参照してください。
実際の翻訳時間はネットワークとPapagoの応答時間によって変わります。

## 制限とセキュリティ

- TLS証明書の検証を維持し、自動リダイレクトは行いません。
- JSON文法・UTF-8・Unicodeエスケープを検証します。出力内のヌル文字、64段階を超えるネスト、
  8 MiBを超えるHTTPレスポンスは拒否します。
- タイムアウトはリクエスト単位です。自動検出では検出と翻訳を別々に要求するため、全体の所要時間は長くなる場合があります。
- 現在のAPIで使われていない旧HMAC-MD5認証コードは含みません。

## プロジェクト構成

```text
include/cpapago.h  公開型・関数
src/cpapago.c     HTTP・自動検出・並列翻訳
src/json.c        JSON検証・文字列抽出
src/main.c        CLI
examples/basic.c  単一翻訳の使用例
tests/            単体・ローカルHTTP・実APIテスト
Makefile          ビルド・テスト
```

## ライセンス

MIT。元のnodepapagoの著作権表示は[`LICENSE`](LICENSE)に維持しています。
