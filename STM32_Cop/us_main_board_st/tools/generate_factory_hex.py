#!/usr/bin/env python3
"""애플리케이션 HEX에 출하 기본 설정 레코드를 추가한다."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import zlib


SETTINGS_ADDRESS = 0x0800F800
SETTINGS_MAGIC = 0x4D424346
SETTINGS_VERSION = 6


def ihex_record(address: int, record_type: int, data: bytes) -> str:
    payload = bytes((len(data), (address >> 8) & 0xFF, address & 0xFF,
                     record_type)) + data
    checksum = (-sum(payload)) & 0xFF
    return ":" + (payload + bytes((checksum,))).hex().upper()


def default_settings_record() -> bytes:
    body = struct.pack(
        "<II8B4H2HB4B3s",
        SETTINGS_MAGIC,
        1,          # sequence
        SETTINGS_VERSION,
        1,          # Modbus address
        0,          # 9600 bps index
        1,          # Even parity
        10,         # 10 minutes
        0,          # RUN_TIME_MINUTES
        0,          # selected 28 kHz band
        0,          # power display=PERCENT
        280, 400, 680, 800,
        500,        # Sweep width Hz
        100,        # Sweep speed Hz
        0,          # RS485 termination disabled
        0, 0, 0, 0,  # 10 Hz fine remainder
        b"\x00\x00\x00",
    )
    if len(body) != 36:
        raise RuntimeError(f"설정 본문 크기 오류: {len(body)}")
    return body + struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF)


def generate(input_path: Path, output_path: Path) -> None:
    source_lines = [line.strip() for line in input_path.read_text(
        encoding="ascii").splitlines() if line.strip()]
    if not source_lines or source_lines[-1].upper() != ":00000001FF":
        raise RuntimeError("입력 HEX의 EOF record를 찾을 수 없습니다.")

    record = default_settings_record()
    lines = source_lines[:-1]
    lines.append(ihex_record(0, 0x04, bytes((0x08, 0x00))))
    offset = SETTINGS_ADDRESS & 0xFFFF
    for index in range(0, len(record), 16):
        lines.append(ihex_record(offset + index, 0x00, record[index:index + 16]))
    lines.append(":00000001FF")
    output_path.write_text("\n".join(lines) + "\n", encoding="ascii")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    generate(args.input, args.output)


if __name__ == "__main__":
    main()
