"""실제 파파고 API 호출 테스트: 번역 문구가 아닌 반환 형식을 검사합니다."""
import json
from pathlib import Path
import subprocess

CLI = Path(__file__).resolve().parents[1] / ".build/cpapago"


def run(*args):
    return subprocess.check_output([str(CLI), *args], text=True).strip()


def main():
    assert run("detect", "So far, so good!") == "en"
    assert run("detect", "안녕하세요. 만나서 반갑습니다.") == "ko"
    assert run("detect", "你好，很高兴认识你。") == "zh-cn"
    print("Single:", run("translate", "ko", "So far, so good!"))
    batch = json.loads(run("batch", "ko", "Morning, sir.", "Morning, ma'am.", "--source", "en", "--honorific"))
    assert len(batch) == 2 and all(isinstance(item, str) and item for item in batch)
    print("Honorific batch:", batch)
    verbose = json.loads(run("batch", "ko", "Morning!", "Morning, baby!", "--source", "en", "--verbose"))
    assert len(verbose) == 2 and all(isinstance(item.get("translatedText"), str) for item in verbose)
    print("Verbose batch:", [item["translatedText"] for item in verbose])
    assert run("translate", "ko", "你好，很高兴认识你。", "--source", "zh-cn")
    assert run("translate", "zh-tw", "Hello, nice to meet you.", "--source", "en")
    assert run("translate", "ko", "Hello & goodbye + 100% #test\nGood morning.", "--source", "en")
    print("Live detection/translation tests passed")


if __name__ == "__main__":
    main()
