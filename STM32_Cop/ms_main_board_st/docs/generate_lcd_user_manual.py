"""
Generate the final LCD-panel user manual for CSF JET Multi Megasonic.

Outputs:
- MANUAL_CSF_JET_LCD_Panel_v1.md
- MANUAL_CSF_JET_LCD_Panel_v1.docx
- user-manual figures used by the DOCX
"""
from __future__ import annotations

import os
from pathlib import Path

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Inches, Pt, RGBColor
from PIL import Image, ImageDraw, ImageFont


DOCS = Path(__file__).resolve().parent
MD_OUT = DOCS / "MANUAL_CSF_JET_LCD_Panel_v1.md"
DOCX_OUT = DOCS / "MANUAL_CSF_JET_LCD_Panel_v1.docx"


def load_font(name: str, size: int):
    for path in [
        f"C:/Windows/Fonts/{name}.ttf",
        f"C:/Windows/Fonts/{name}.TTF",
    ]:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


FONT_TITLE = load_font("malgunbd", 30)
FONT_HEAD = load_font("malgunbd", 22)
FONT = load_font("malgun", 18)
FONT_SMALL = load_font("malgun", 15)
FONT_LCD = load_font("consola", 22)


def draw_center(draw: ImageDraw.ImageDraw, box, text: str, font, fill=(0, 0, 0), spacing=4):
    bbox = draw.multiline_textbbox((0, 0), text, font=font, spacing=spacing, align="center")
    x = box[0] + ((box[2] - box[0]) - (bbox[2] - bbox[0])) / 2
    y = box[1] + ((box[3] - box[1]) - (bbox[3] - bbox[1])) / 2
    draw.multiline_text((x, y), text, font=font, fill=fill, align="center", spacing=spacing)


def arrow(draw: ImageDraw.ImageDraw, start, end, fill=(35, 85, 135), width=4):
    draw.line((start, end), fill=fill, width=width)
    x1, y1 = start
    x2, y2 = end
    if abs(x2 - x1) >= abs(y2 - y1):
        s = 13 if x2 >= x1 else -13
        draw.polygon([(x2, y2), (x2 - s, y2 - 8), (x2 - s, y2 + 8)], fill=fill)
    else:
        s = 13 if y2 >= y1 else -13
        draw.polygon([(x2, y2), (x2 - 8, y2 - s), (x2 + 8, y2 - s)], fill=fill)


def new_canvas(filename: str, title: str, size=(1200, 720)):
    path = DOCS / filename
    img = Image.new("RGB", size, "white")
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle((10, 10, size[0] - 10, size[1] - 10), radius=20, outline=(30, 75, 120), width=3)
    draw.text((35, 28), title, font=FONT_TITLE, fill=(25, 65, 110))
    return img, draw, path


def make_panel_figure():
    img, d, path = new_canvas("lcd_user_panel_layout.png", "LCD 조작 패널 구성", (1200, 760))
    d.rounded_rectangle((90, 115, 1110, 650), radius=30, fill=(245, 248, 252), outline=(80, 110, 150), width=4)

    # LCD
    d.rounded_rectangle((300, 170, 900, 330), radius=15, fill=(212, 238, 220), outline=(40, 100, 60), width=4)
    d.rectangle((335, 205, 865, 295), fill=(28, 55, 38))
    d.text((380, 218), "0.00W     0000kHz", font=FONT_LCD, fill=(180, 255, 180))
    d.text((380, 255), "NORMAL STOP PWR1", font=FONT_LCD, fill=(180, 255, 180))
    d.text((520, 135), "LCD1602 표시창", font=FONT_HEAD, fill=(25, 65, 110))

    # LEDs
    led_labels = ["NORMAL", "H/L SET", "8 POWER", "REMOTE", "EXT", "TX/RX"]
    for i, label in enumerate(led_labels):
        x = 150 + i * 170
        d.ellipse((x, 390, x + 38, 428), fill=(80, 180, 95), outline=(30, 90, 40), width=2)
        d.text((x - 28, 440), label, font=FONT_SMALL, fill=(40, 40, 40))

    # Buttons
    buttons = [("START/STOP", "RUN/STOP"), ("MODE", "항목/필드"), ("UP", "증가"), ("DOWN", "감소"), ("SET", "저장")]
    for i, (name, desc) in enumerate(buttons):
        x = 135 + i * 210
        d.rounded_rectangle((x, 535, x + 150, 610), radius=18, fill=(232, 242, 252), outline=(30, 80, 130), width=3)
        draw_center(d, (x, 535, x + 150, 585), name, FONT_SMALL, fill=(20, 55, 95))
        draw_center(d, (x, 580, x + 150, 610), desc, FONT_SMALL, fill=(80, 80, 80))

    img.save(path)
    return path


def make_menu_flow_figure():
    img, d, path = new_canvas("lcd_user_menu_flow.png", "LCD 메뉴 흐름도", (1200, 820))
    boxes = {
        "select": (70, 130, 360, 250, "선택 화면\nRUN/STOP, MODE,\nFREQ CH, PWR 선택"),
        "setting": (455, 130, 745, 250, "SETTING MODE\n1.FREQ SET\n2.POWER SET\n3.EXT:RS485"),
        "freq": (70, 390, 360, 560, "FREQ SET\nCH → FREQ → BIT\nSET 저장\nSTART 3초: AUTO"),
        "power": (455, 390, 745, 560, "POWER SET\nCH → LOW → HIGH → DEF\nSET 저장"),
        "rs485": (840, 390, 1130, 560, "EXT:RS485\nBAUD → ADDR → TERM → PARITY\nSET 저장"),
    }
    for box in boxes.values():
        x1, y1, x2, y2, text = box
        d.rounded_rectangle((x1, y1, x2, y2), radius=18, fill=(240, 248, 255), outline=(35, 85, 135), width=3)
        draw_center(d, (x1, y1, x2, y2), text, FONT, fill=(30, 30, 30))
    arrow(d, (360, 190), (455, 190))
    d.text((375, 155), "MODE 길게", font=FONT_SMALL, fill=(35, 85, 135))
    arrow(d, (600, 250), (210, 390))
    arrow(d, (600, 250), (600, 390))
    arrow(d, (600, 250), (985, 390))
    d.text((370, 310), "UP/DOWN 선택 후 SET", font=FONT, fill=(35, 85, 135))
    d.text((85, 640), "공통: MODE 짧게=필드 이동 / UP·DOWN=값 변경 / SET=저장 / SET 길게=선택 화면 복귀", font=FONT, fill=(50, 50, 50))
    img.save(path)
    return path


def make_lcd_examples_figure():
    img, d, path = new_canvas("lcd_user_screen_examples.png", "LCD 화면 예시", (1200, 780))
    examples = [
        ("기본 정지 화면", "0.00W     0000kHz", "NORMAL STOP PWR1"),
        ("FREQ 채널 선택", "01CH      0400kHz", "CH01 E0000 A0000"),
        ("FREQ 수동 설정", "START(3SEC):AUTO", "FREQ:0406kHz"),
        ("POWER 설정", "P-SET P1", "DEF:0.50W"),
        ("RS485 설정", "EXT:RS485 SET", "BAUD:9600"),
        ("알람 화면", "   ALARM STOP", "ERR1 LOW ALARM"),
    ]
    for i, (title, line1, line2) in enumerate(examples):
        col = i % 2
        row = i // 2
        x = 90 + col * 560
        y = 120 + row * 195
        d.text((x, y), title, font=FONT_HEAD, fill=(25, 65, 110))
        d.rounded_rectangle((x, y + 38, x + 470, y + 155), radius=12, fill=(28, 55, 38), outline=(60, 130, 80), width=4)
        d.text((x + 35, y + 58), line1, font=FONT_LCD, fill=(180, 255, 180))
        d.text((x + 35, y + 103), line2, font=FONT_LCD, fill=(180, 255, 180))
    img.save(path)
    return path


def make_remote_ext_figure():
    img, d, path = new_canvas("lcd_user_remote_ext.png", "REMOTE / EXT 운전 개념", (1200, 720))
    d.rounded_rectangle((90, 150, 395, 330), radius=18, fill=(255, 246, 222), outline=(150, 105, 30), width=3)
    draw_center(d, (90, 150, 395, 330), "REMOTE 운전\n외부 접점 ON/OFF\nREMOTE 입력 SHORT=RUN", FONT)
    d.rounded_rectangle((805, 150, 1110, 330), radius=18, fill=(226, 239, 218), outline=(50, 110, 50), width=3)
    draw_center(d, (805, 150, 1110, 330), "EXT 운전\nRS-485 Modbus RTU\n마스터에서 읽기/쓰기", FONT)
    d.rounded_rectangle((465, 230, 735, 450), radius=18, fill=(232, 242, 252), outline=(35, 85, 135), width=3)
    draw_center(d, (465, 230, 735, 450), "CSF JET\nMulti Megasonic\nGenerator", FONT_HEAD, fill=(25, 65, 110))
    arrow(d, (395, 240), (465, 300))
    arrow(d, (805, 240), (735, 300))
    d.text((130, 390), "REMOTE: 현장 접점 제어가 필요한 경우 사용", font=FONT, fill=(60, 60, 60))
    d.text((130, 430), "EXT: PLC/PC에서 RS-485 통신으로 상태 확인 및 설정 제어", font=FONT, fill=(60, 60, 60))
    d.text((130, 500), "주의: 통신 설정(주소/속도/패리티)은 패널의 EXT:RS485 메뉴에서 저장", font=FONT, fill=(180, 45, 45))
    img.save(path)
    return path


def make_wiring_figure():
    img, d, path = new_canvas("lcd_user_wiring.png", "후면 연결 개념", (1200, 760))
    parts = [
        (80, 150, 330, 290, "전원 입력\nAC 전원/접지"),
        (475, 150, 725, 290, "출력 커넥터\nNozzle / PZT"),
        (870, 150, 1120, 290, "Control I/O\nREMOTE/SENSOR/ALARM"),
        (250, 440, 520, 590, "RS-485\nA/B + GND"),
        (680, 440, 950, 590, "보호 접지\nPE 확인"),
    ]
    for x1, y1, x2, y2, text in parts:
        d.rounded_rectangle((x1, y1, x2, y2), radius=16, fill=(245, 248, 252), outline=(35, 85, 135), width=3)
        draw_center(d, (x1, y1, x2, y2), text, FONT)
    arrow(d, (330, 220), (475, 220))
    arrow(d, (725, 220), (870, 220))
    arrow(d, (520, 515), (680, 515))
    d.text((110, 660), "설치 전 출력선/센서선/접지/RS-485 A,B 극성을 반드시 확인한다.", font=FONT, fill=(180, 45, 45))
    img.save(path)
    return path


FIGURES = {
    "panel": make_panel_figure(),
    "flow": make_menu_flow_figure(),
    "screens": make_lcd_examples_figure(),
    "remote_ext": make_remote_ext_figure(),
    "wiring": make_wiring_figure(),
}


MANUAL_MD = r"""# CSF JET Multi Megasonic Generator

## LCD 패널 사용자 운전 매뉴얼

- 문서 버전: v3.0
- 개정일: 2026-05-19
- 적용 장비: CSF JET Multi Megasonic Generator (LCD 패널형)
- 적용 펌웨어: STM32G474 LCD 메뉴 기반 빌드

---

## < OPERATING SUMMARY >

| 구분 | 조작 |
| ---- | ---- |
| RUN/STOP | 선택 화면에서 `START/STOP` |
| 선택 항목 이동 | 선택 화면에서 `MODE` 짧게 |
| 설정 메뉴 진입 | 선택 화면에서 `MODE` 길게 |
| 값 증가/감소 | `UP` / `DOWN` |
| 연속 증가/감소 | `UP` / `DOWN` 길게 |
| 저장 | `SET` 짧게 |
| 선택 화면 복귀 | 설정 화면에서 `SET` 약 2초 |
| FREQ Auto Tuning | FREQ SET 화면에서 `START/STOP` 약 3초 |
| 알람 해제 | 알람 표시 중 원인 확인 후 `SET` |

비프음 기준:

- 일반 버튼: 짧게 1회
- 길게 누름 인식: 조금 길게 1회
- 저장 성공: 2회
- 저장 실패: 3회
- 알람: 긴 1회

---

## 1. Specification

| No. | Item | Specification |
| --- | ---- | ------------- |
| 1 | Model Name | CSF JET Multi Megasonic Generator |
| 2 | Display | LCD1602, 2 lines x 16 characters |
| 3 | Button | START/STOP, MODE, UP, DOWN, SET |
| 4 | Status LED | NORMAL, H/L SET, 8 POWER, REMOTE, EXT, TX/RX |
| 5 | Operating Frequency | 10CH, 400kHz ~ 2200kHz |
| 6 | Frequency Setting | Manual setting + Auto Tuning |
| 7 | LC Relay Setting | 16 steps, BIT 0000 ~ 1111 |
| 8 | Power Profile | 8 POWER channels, LOW/HIGH/DEF |
| 9 | Output Range | 0.20W ~ 5.00W setting range |
| 10 | Communication | RS-485 Half-Duplex, Modbus RTU |
| 11 | Default Communication | Address 1, 9600bps, None-8-1 |
| 12 | Setting Storage | Internal FLASH, power-off retention |

Product composition:

- Generator: 1 EA
- High-frequency output cable: 1 EA
- Control connector / harness: according to shipment option
- RS-485 communication line: according to installation option

---

## 2. Caution on Product Use

### 2.1 Caution on Installation

1. Install the generator on a stable support where vibration does not affect the unit.
2. Do not install in an atmosphere containing acid, alkali, conductive dust, oil mist, or corrosive gas.
3. Keep at least 5cm clearance from walls or other equipment for ventilation.
4. Do not install where ambient temperature exceeds the product operating condition.
5. Use the assigned nozzle/transducer and matched output cable.
6. Do not change cable type or length without technical confirmation.
7. Always connect protective earth before applying power.
8. Do not stack generators directly. Use shelves that allow air flow from front, rear, left and right.
9. Before handling wiring, turn off the power and disconnect the plug.

### 2.2 Caution on Use

1. Do not operate at high output with the output connector disconnected.
2. Do not repeatedly restart after an alarm without checking the cause.
3. If SENSOR is open, the unit enters a stop/alarm condition and output is inhibited.
4. In REMOTE operation, keep ON/OFF intervals long enough for stable operation.
5. When changing FREQ or POWER setting, start from low output and increase step by step.
6. When using RS-485, match address, baudrate and parity between master and generator.

---

## 3. Name and Function of Each Part

### 3.1 Operation Panel

![LCD 조작 패널 구성](./lcd_user_panel_layout.png)

| No. | Part | Function |
| --- | ---- | -------- |
| 1 | Power Switch | Turns generator power ON/OFF |
| 2 | LCD Display | Shows power, frequency, mode, menu, alarm and communication setting |
| 3 | NORMAL LED | Lights when NORMAL mode is selected |
| 4 | H/L SET LED | Reserved for high/low alarm setting indication |
| 5 | 8 POWER LED | Lights when 8 POWER item or power setting is selected |
| 6 | REMOTE LED | Lights when REMOTE mode is selected; blinks during operation |
| 7 | EXT LED | Lights when EXT communication mode is selected |
| 8 | TX/RX LED | Communication activity indication |
| 9 | START/STOP Button | Starts/stops oscillation; in FREQ SET long press starts Auto Tuning |
| 10 | MODE Button | Selects item/field; long press enters setting menu or returns to upper menu |
| 11 | UP Button | Increases item/value; long press repeats quickly |
| 12 | DOWN Button | Decreases item/value; long press repeats quickly |
| 13 | SET Button | Saves selected value; long press returns to select screen from setting mode |

### 3.2 LCD Display

The LCD has two lines. The first line usually shows output/frequency or the selected setting value. The second line shows mode, status, selected channel or current edit field.

![LCD 화면 예시](./lcd_user_screen_examples.png)

Common examples:

```text
0.00W     0000kHz
NORMAL STOP PWR1
```

```text
01CH      0400kHz
CH01 E0000 A0000
```

```text
EXT:RS485 SET
BAUD:9600
```

### 3.3 Back of Generator

![후면 연결 개념](./lcd_user_wiring.png)

| Part | Function |
| ---- | -------- |
| Power Connector | Connects input power and protective earth |
| Output Connector | Connects nozzle/transducer output cable |
| Control Connector | SENSOR, REMOTE, BCD selection, alarm relay contacts |
| RS-485 Connector | A/B communication line and signal GND |
| Fuse Holder | Protection fuse according to product rating |

---

## 4. Connecting Method

### 4.1 Connect Generator and Nozzle

1. Turn off generator power.
2. Connect the output connector to the assigned nozzle/transducer line.
3. Check connector lock and cable strain relief.
4. Do not interchange output lines between units unless confirmed.

### 4.2 Connect Power Cord

1. Check input power rating.
2. Connect protective earth first.
3. Connect power cord firmly.
4. Do not turn on power before output and SENSOR wiring are checked.

### 4.3 Connect Control Connector

The control connector includes remote operation input, SENSOR input and alarm relay contacts.

SENSOR input:

- Normal condition: SHORT
- Open during stop: Err7 SENSOR OFF
- Open during run: Err8 SENSOR RUN
- If SENSOR is not used, it must be shorted according to shipment wiring.

REMOTE input:

- Used for external contact ON/OFF operation.
- In REMOTE mode, output follows the remote contact state.
- Use an independent dry contact for each generator.

Alarm relay:

- GO relay indicates normal output range.
- LOW/HIGH/TRANSDUCER alarm relays indicate each alarm condition.
- OPERATE relay is a fail-safe integrated status output.

### 4.4 Connect RS-485

1. Connect A/B lines with correct polarity.
2. Connect Signal GND when possible.
3. Use bus topology.
4. Apply 120ohm termination at both ends of the bus if required.
5. Set each generator address differently.

---

## 5. Operation Ready

Before operating:

1. Supply DI water or process fluid to the nozzle if required by the process.
2. Confirm nozzle/transducer mounting condition.
3. Confirm output cable connection.
4. Confirm SENSOR normal state.
5. Confirm mode: NORMAL / REMOTE / EXT.
6. Confirm selected FREQ channel and POWER channel.
7. Start at low output after installation or nozzle change.

---

## 6. Operating Method

### 6.1 Menu Structure

![LCD 메뉴 흐름도](./lcd_user_menu_flow.png)

```text
SELECT 화면
  MODE 짧게: MODE -> FREQ CH -> 8 POWER 항목 순환
  MODE 길게: SETTING MODE 진입

SETTING MODE
  1.FREQ SET
  2.POWER SET
  3.EXT:RS485
```

### 6.2 NORMAL Operation

NORMAL mode is used when the operator controls the generator from the front panel.

Basic procedure:

1. Turn on power.
2. Confirm NORMAL LED.
3. Select required FREQ channel and POWER channel if needed.
4. Press `START/STOP`.
5. Confirm RUN display and output value.
6. Press `START/STOP` again to stop.

Changing selected mode/channel on SELECT screen:

1. Press `MODE` shortly to enter select item mode.
2. Press `MODE` shortly to choose item:
   - MODE
   - FREQ CH
   - 8 POWER
3. Use `UP/DOWN` to change the selected value.
4. Press `SET` to save.

### 6.3 FREQ Channel Selection

The generator has 10 FREQ channels.

| Panel CH | Default Frequency | Internal Meaning |
| -------- | ----------------- | ---------------- |
| CH01 | 400kHz | Frequency profile 1 |
| CH02 | 600kHz | Frequency profile 2 |
| CH03 | 800kHz | Frequency profile 3 |
| CH04 | 1000kHz | Frequency profile 4 |
| CH05 | 1200kHz | Frequency profile 5 |
| CH06 | 1400kHz | Frequency profile 6 |
| CH07 | 1600kHz | Frequency profile 7 |
| CH08 | 1800kHz | Frequency profile 8 |
| CH09 | 2000kHz | Frequency profile 9 |
| CH10 | 2200kHz | Frequency profile 10 |

To select a channel:

1. On SELECT screen, press `MODE` until FREQ CH item appears.
2. Use `UP/DOWN` to select CH01~CH10.
3. Press `SET`.
4. Save success is indicated by two beeps.

### 6.4 FREQ SET: Manual Frequency and BIT Setting

Use this menu when changing the saved frequency or LC relay BIT for each FREQ channel.

Procedure:

1. From SELECT screen, press `MODE` long to enter SETTING MODE.
2. Select `1.FREQ SET`.
3. Press `SET`.
4. Press `MODE` shortly to move field:
   - CH
   - FREQ
   - BIT
5. Use `UP/DOWN` to change the blinking field.
6. Press `SET` to save the current CH frequency and BIT.
7. Save success is indicated by two beeps.

BIT display:

```text
BIT:0010 03/16
```

- BIT is the LC relay combination.
- 0000~1111 corresponds to 16 relay combinations.
- E means expected relay bit for the channel.
- A means currently applied relay bit.

### 6.5 Auto Tuning

Auto Tuning searches suitable frequency and LC relay setting for the selected FREQ channel.

Procedure:

1. Enter `FREQ SET`.
2. Select the target CH.
3. Press `START/STOP` for about 3 seconds.
4. During tuning, LCD shows scan progress.
5. If tuning succeeds, LCD shows `AUTO TUNE DONE`.
6. Press `SET` to save if needed.

Notes:

- Perform Auto Tuning with the actual nozzle/transducer connected.
- Start from low power.
- If `NO DETECTED` or `TUNE TIMEOUT` appears, check nozzle, cable, load and process condition.

### 6.6 POWER SET

POWER SET stores 8 power profiles. Each profile has LOW, HIGH and DEF values.

| Field | Meaning | Range |
| ----- | ------- | ----- |
| CH | Power profile number | PWR1~PWR8 |
| LOW | Low alarm threshold | 0.10W~0.95W |
| HIGH | High alarm threshold | 0.20W~5.00W |
| DEF | Default output | 0.20W~5.00W |

Procedure:

1. From SELECT screen, press `MODE` long.
2. Select `2.POWER SET`.
3. Press `SET`.
4. Press `MODE` shortly to move field:
   - CH
   - LOW
   - HIGH
   - DEF
5. Use `UP/DOWN` to adjust the blinking value.
6. Press `SET` to save.

Protection:

- If output is lower than LOW threshold for the alarm delay, Err1 occurs.
- If output is higher than HIGH threshold for the alarm delay, Err2 occurs.
- The generator stops output when alarm occurs.

### 6.7 REMOTE Operation

![REMOTE / EXT 운전 개념](./lcd_user_remote_ext.png)

REMOTE mode is used when an external contact controls RUN/STOP.

Setup:

1. Stop oscillation.
2. Turn off power.
3. Connect REMOTE contact line to control connector.
4. Turn on power.
5. Select REMOTE mode on SELECT screen:
   - Press `MODE` shortly to select MODE item.
   - Use `UP/DOWN` to select REMOTE.
   - Press `SET` to save.

Operation:

- REMOTE contact SHORT: output ON
- REMOTE contact OPEN: output OFF
- REMOTE LED indicates REMOTE mode.
- Keep ON/OFF intervals long enough for stable operation.

Notes:

- Use independent contacts for multiple generators.
- Do not wire multiple generators in parallel from one dry contact unless the interface is designed for it.

### 6.8 EXT Operation

EXT mode is used when a PLC/PC controls or monitors the generator through RS-485 Modbus RTU.

Setup from panel:

1. Enter SELECT screen.
2. Select MODE item.
3. Use `UP/DOWN` to select EXT.
4. Press `SET` to save.
5. Enter SETTING MODE.
6. Select `3.EXT:RS485`.
7. Set BAUD, ADDR, TERM and PARITY.
8. Press `SET` to save each setting.

EXT:RS485 setting fields:

| Field | Range / Option | Description |
| ----- | -------------- | ----------- |
| BAUD | 9600 / 19200 / 38400 / 115200 | Communication speed |
| ADDR | 1~247 | Modbus slave address |
| TERM | OFF / ON | Termination setting record, hardware switching reserved |
| PARITY | NONE-8-1 / EVEN-8-1 | UART parity from panel |

PC/PLC initial communication:

- Baud: 9600
- Data: 8
- Stop: 1
- Parity: None
- Slave ID: 1
- Function test: FC03 read

---

## 7. Cable Wiring Diagram

### 7.1 SENSOR

SENSOR is a safety input. If the SENSOR loop opens, oscillation is stopped or inhibited.

| State | Meaning |
| ----- | ------- |
| SHORT | Normal |
| OPEN while stopped | Err7 SENSOR OFF |
| OPEN while running | Err8 SENSOR RUN |

### 7.2 REMOTE

REMOTE input is used for external RUN/STOP operation.

| REMOTE input | Operation |
| ------------ | --------- |
| OPEN | STOP |
| SHORT | RUN |

### 7.3 ALARM Output

| Output | Meaning |
| ------ | ------- |
| GO | Output is within normal range |
| LOW ALARM | Err1 occurred |
| HIGH ALARM | Err2 occurred |
| TRANSDUCER ALARM | Err5 occurred |
| OPERATE | Integrated operation/alarm contact |

### 7.4 8 POWER External Selection

8 POWER channel can be selected by BCD input when the external input option is used.

| BCD3 | BCD2 | BCD1 | Selected Power |
| ---- | ---- | ---- | -------------- |
| 0 | 0 | 0 | PWR1 |
| 0 | 0 | 1 | PWR2 |
| 0 | 1 | 0 | PWR3 |
| 0 | 1 | 1 | PWR4 |
| 1 | 0 | 0 | PWR5 |
| 1 | 0 | 1 | PWR6 |
| 1 | 1 | 0 | PWR7 |
| 1 | 1 | 1 | PWR8 |

---

## 8. ERROR Display

When an alarm occurs, the generator stops output and the LCD shows the alarm.

```text
   ALARM STOP
ERR1 LOW ALARM
```

| Code | Display | Meaning | Output |
| ---- | ------- | ------- | ------ |
| Err1 | LOW ALARM | Output is lower than LOW threshold | Stop |
| Err2 | HIGH ALARM | Output is higher than HIGH threshold | Stop |
| Err5 | TRANS ALM | Transducer/load alarm | Stop |
| Err6 | SET ERROR | Invalid setting attempted | Conditional |
| Err7 | SNS OFF | SENSOR open while stopped | Stop |
| Err8 | SNS RUN | SENSOR open while running | Stop |

Alarm recovery:

1. Stop operation.
2. Check the cause.
3. Correct wiring, load or setting.
4. Press `SET` on alarm screen to clear recoverable alarm.
5. For SENSOR alarm, turn power OFF, restore SENSOR normal state, then turn power ON.

---

## 9. Communication Specification

### 9.1 RS-485 Specification

| Item | Specification |
| ---- | ------------- |
| Electrical Interface | RS-485 Half-Duplex |
| Protocol | Modbus RTU |
| Default Baudrate | 9600bps |
| Supported Baudrate | 9600 / 19200 / 38400 / 115200 |
| Data | 8 bit |
| Parity | None / Even from panel, Odd available by register |
| Stop Bit | 1 bit |
| CRC | Modbus CRC-16 |
| Frame End | 3.5 character silent interval |

### 9.2 Communication Setting from Panel

1. `MODE` long → SETTING MODE.
2. Select `3.EXT:RS485`.
3. `SET` to enter.
4. `MODE` shortly to select BAUD / ADDR / TERM / PARITY.
5. `UP/DOWN` to change value.
6. `SET` to save.

### 9.3 Main Modbus Registers

| Address | R/W | Name | Unit / Value |
| ------- | --- | ---- | ------------ |
| 0x0000 | R/W | RUN control | 0=STOP, 1=RUN |
| 0x0001 | R/W | Frequency set | x0.1kHz |
| 0x0002 | R/W | Duty set | x0.1% |
| 0x0003 | R | Actual frequency | x0.1kHz |
| 0x0004 | R | Actual duty | x0.1% |
| 0x0005 | R | Status flags | bit field |
| 0x0006 | R/W | Operating mode | 0=NORMAL, 1=REMOTE, 2=EXT |
| 0x0010 | R/W | Slave address | 1~247 |
| 0x0011 | R/W | Baud index | 0=9600, 1=19200, 2=38400, 3=115200 |
| 0x0012 | R/W | Parity | 0=None, 1=Even, 2=Odd |
| 0x00FF | W | Software reset | write 0x1234 |

Supported function codes:

| Code | Function |
| ---- | -------- |
| 0x03 | Read Holding Registers |
| 0x06 | Write Single Register |
| 0x10 | Write Multiple Registers |

Communication check:

1. Check A/B polarity.
2. Check common GND.
3. Check slave address.
4. Check baudrate and parity.
5. Start with FC03 read test.
6. After baud/parity change, set the master to the same value.

---

## 10. Setting Storage

The generator stores user settings in internal FLASH.

Stored items:

- Operating mode: NORMAL / REMOTE / EXT
- Selected FREQ channel
- Selected POWER channel
- FREQ channel profiles: frequency and BIT
- POWER profiles: LOW / HIGH / DEF
- RS-485 settings: address, baudrate, termination setting, parity

Storage behavior:

- Press `SET` to save current setting.
- Save success: two beeps.
- Save failure: three beeps.
- Saved values are restored after power OFF/ON.

---

## 11. Troubleshooting

| Symptom | Possible Cause | Check / Measure |
| ------- | -------------- | --------------- |
| No output | STOP state, alarm, SENSOR open | Check LCD, clear alarm, check SENSOR |
| Output stops soon | LOW/HIGH alarm | Check load and POWER LOW/HIGH setting |
| Err7 appears at power ON | SENSOR line open | Turn power OFF and check SENSOR short |
| Frequency is not expected | Wrong FREQ CH or unsaved setting | Select FREQ CH and press SET |
| BIT is not applied | Wrong FREQ profile or no save | Check E/A display and press SET |
| RS-485 no response | A/B polarity, address, baud, parity mismatch | Check wiring and EXT:RS485 setting |
| Saved value not restored | SET not pressed or save failed | Save again and confirm two beeps |

---

## 12. Maintenance

### 12.1 Daily Check

- LCD display normal.
- LED indication normal.
- Output cable firmly connected.
- No alarm record.
- No abnormal noise, heat or smell.

### 12.2 Weekly Check

- Output repeatability.
- RS-485 communication response.
- Remote contact operation.
- Cooling and ventilation.

### 12.3 Monthly Check

- Output cable insulation and connector condition.
- Nozzle/transducer mounting.
- Alarm threshold validity.
- Backup of important settings.

---

## 13. A/S

### 13.1 Warranty Period

Warranty period and service condition follow the sales agreement or product warranty document.

### 13.2 Warranty Exception

Warranty may not apply to:

- Damage caused by careless handling.
- Damage caused by incorrect wiring or installation.
- Unauthorized repair or modification.
- Fire, flood, earthquake or other natural disasters.
- Consumables such as fuse and packing.

### 13.3 Service Request Information

When requesting service, provide:

- Model name.
- Serial number.
- Firmware version if available.
- Alarm code and operating condition.
- Wiring photo and installation condition.
- Description of symptom and reproduction steps.

---

## 14. Revision History

| Version | Date | Description |
| ------- | ---- | ----------- |
| v3.0 | 2026-05-19 | Rewritten as final LCD-panel user manual. Added operation procedures, menu setting, REMOTE/EXT usage, LCD button diagrams and user-oriented troubleshooting. |
"""


def write_markdown():
    MD_OUT.write_text(MANUAL_MD, encoding="utf-8")


def add_docx_table(doc: Document, headers, rows):
    table = doc.add_table(rows=1 + len(rows), cols=len(headers))
    table.style = "Light Grid Accent 1"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, header in enumerate(headers):
        cell = table.rows[0].cells[i]
        cell.text = str(header)
        for p in cell.paragraphs:
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            for r in p.runs:
                r.bold = True
                r.font.name = "맑은 고딕"
                r.font.size = Pt(9)
    for ri, row in enumerate(rows):
        for ci, value in enumerate(row):
            cell = table.rows[ri + 1].cells[ci]
            cell.text = str(value)
            for p in cell.paragraphs:
                for r in p.runs:
                    r.font.name = "맑은 고딕"
                    r.font.size = Pt(9)
    doc.add_paragraph()
    return table


def add_docx_image(doc: Document, path: Path, caption: str, width=6.4):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.add_run().add_picture(str(path), width=Inches(width))
    cp = doc.add_paragraph(caption)
    cp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    if cp.runs:
        cp.runs[0].italic = True
        cp.runs[0].font.size = Pt(9)
        cp.runs[0].font.color.rgb = RGBColor(0x66, 0x66, 0x66)


def add_lcd_box(doc: Document, line1: str, line2: str, title: str | None = None):
    if title:
        doc.add_paragraph(title, style="List Bullet")
    table = doc.add_table(rows=2, cols=1)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    for row, text in enumerate([line1, line2]):
        cell = table.rows[row].cells[0]
        cell.text = text
        for p in cell.paragraphs:
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            for r in p.runs:
                r.font.name = "Consolas"
                r.font.size = Pt(11)
    doc.add_paragraph()


def write_docx():
    doc = Document()
    section = doc.sections[0]
    section.top_margin = Inches(0.6)
    section.bottom_margin = Inches(0.6)
    section.left_margin = Inches(0.65)
    section.right_margin = Inches(0.65)

    normal = doc.styles["Normal"]
    normal.font.name = "맑은 고딕"
    normal.font.size = Pt(10)
    normal.paragraph_format.space_after = Pt(4)

    for level in range(1, 4):
        style = doc.styles[f"Heading {level}"]
        style.font.name = "맑은 고딕"
        style.font.color.rgb = RGBColor(0x1A, 0x3C, 0x6E)

    # Cover
    doc.add_paragraph()
    title = doc.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = title.add_run("Instruction Manual")
    run.bold = True
    run.font.size = Pt(22)
    run.font.color.rgb = RGBColor(0x1A, 0x3C, 0x6E)
    subtitle = doc.add_paragraph()
    subtitle.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = subtitle.add_run("----- CSF JET Multi Megasonic Generator -----")
    run.bold = True
    run.font.size = Pt(16)
    doc.add_paragraph()
    add_docx_image(doc, FIGURES["panel"], "[그림 1] LCD 조작 패널 구성", width=5.8)
    info = doc.add_paragraph()
    info.alignment = WD_ALIGN_PARAGRAPH.CENTER
    info.add_run("LCD 패널 사용자 운전 매뉴얼\n문서 버전 v3.0 / 2026-05-19").font.size = Pt(11)
    doc.add_page_break()

    doc.add_heading("< OPERATING SUMMARY >", level=1)
    add_docx_table(doc, ["구분", "조작"], [
        ["RUN/STOP", "선택 화면에서 START/STOP"],
        ["설정 메뉴 진입", "선택 화면에서 MODE 길게"],
        ["값 변경", "UP/DOWN, 길게 누르면 빠른 반복"],
        ["저장", "SET 짧게, 성공 시 2회 비프"],
        ["선택 화면 복귀", "설정 화면에서 SET 약 2초"],
        ["Auto Tuning", "FREQ SET에서 START/STOP 약 3초"],
    ])

    doc.add_heading("1. Specification", level=1)
    add_docx_table(doc, ["No.", "Item", "Specification"], [
        [1, "Model Name", "CSF JET Multi Megasonic Generator"],
        [2, "Display", "LCD1602, 2 lines x 16 characters"],
        [3, "Button", "START/STOP, MODE, UP, DOWN, SET"],
        [4, "Operating Frequency", "10CH, 400kHz ~ 2200kHz"],
        [5, "Power Profile", "8 POWER channels, LOW/HIGH/DEF"],
        [6, "Communication", "RS-485 Half-Duplex, Modbus RTU"],
        [7, "Storage", "Internal FLASH, power-off retention"],
    ])

    doc.add_heading("2. Caution on Product Use", level=1)
    doc.add_heading("2.1 Caution on Installation", level=2)
    for text in [
        "Install on a stable support and secure ventilation space.",
        "Do not install in corrosive gas, conductive dust, oil mist or high-temperature place.",
        "Connect protective earth before applying power.",
        "Use assigned nozzle/transducer and matched output cable.",
        "Do not change cable type or length without technical confirmation.",
    ]:
        doc.add_paragraph(text, style="List Number")
    doc.add_heading("2.2 Caution on Use", level=2)
    for text in [
        "Do not operate with output connector disconnected.",
        "If alarm occurs, check cause before restarting.",
        "SENSOR must be normal before operation.",
        "Start from low output after installation or nozzle change.",
        "For RS-485, match address, baudrate and parity with the master.",
    ]:
        doc.add_paragraph(text, style="List Number")

    doc.add_heading("3. Name and Function of Each Part", level=1)
    doc.add_heading("3.1 Operation Panel", level=2)
    add_docx_image(doc, FIGURES["panel"], "[그림 2] 버튼, LCD, LED 구성", width=6.5)
    add_docx_table(doc, ["Part", "Function"], [
        ["LCD Display", "Shows output, frequency, mode, setting menu and alarm."],
        ["START/STOP", "Starts/stops oscillation. In FREQ SET, long press starts Auto Tuning."],
        ["MODE", "Selects item/field. Long press enters setting menu or returns to upper menu."],
        ["UP/DOWN", "Changes item/value. Long press repeats quickly."],
        ["SET", "Saves value. Long press returns to select screen from setting mode."],
        ["NORMAL/REMOTE/EXT LED", "Shows selected operating mode."],
        ["TX/RX LED", "Communication activity indication."],
    ])
    doc.add_heading("3.2 LCD Display Examples", level=2)
    add_docx_image(doc, FIGURES["screens"], "[그림 3] LCD 화면 예시", width=6.5)

    doc.add_heading("4. Connecting Method", level=1)
    add_docx_image(doc, FIGURES["wiring"], "[그림 4] 후면 연결 개념", width=6.5)
    add_docx_table(doc, ["Connection", "Check Point"], [
        ["Power", "Input power rating and protective earth."],
        ["Output", "Assigned nozzle/transducer and cable connection."],
        ["SENSOR", "Normal state is SHORT. OPEN causes Err7/Err8."],
        ["REMOTE", "Dry contact input for external RUN/STOP."],
        ["RS-485", "A/B polarity, Signal GND, address, baudrate and parity."],
        ["Alarm Relay", "GO, LOW, HIGH, TRANSDUCER, OPERATE contacts."],
    ])

    doc.add_heading("5. Operation Ready", level=1)
    for text in [
        "Supply DI water or process fluid to the nozzle if required.",
        "Confirm nozzle/transducer mounting condition.",
        "Confirm output cable and SENSOR wiring.",
        "Confirm selected mode, FREQ channel and POWER channel.",
        "Start at low output after installation or nozzle change.",
    ]:
        doc.add_paragraph(text, style="List Number")

    doc.add_heading("6. Operating Method", level=1)
    add_docx_image(doc, FIGURES["flow"], "[그림 5] LCD 메뉴 흐름도", width=6.5)
    doc.add_heading("6.1 NORMAL Operation", level=2)
    for text in [
        "Turn on power and confirm NORMAL LED.",
        "Select required FREQ channel and POWER channel.",
        "Press START/STOP to run.",
        "Confirm RUN display and output value.",
        "Press START/STOP again to stop.",
    ]:
        doc.add_paragraph(text, style="List Number")
    doc.add_heading("6.2 FREQ SET", level=2)
    for text in [
        "MODE long -> SETTING MODE.",
        "Select 1.FREQ SET and press SET.",
        "MODE short moves CH -> FREQ -> BIT.",
        "UP/DOWN changes the blinking field.",
        "SET saves the current channel frequency and BIT.",
        "START/STOP 3 seconds starts Auto Tuning.",
    ]:
        doc.add_paragraph(text, style="List Number")
    add_lcd_box(doc, "START(3SEC):AUTO", "FREQ:0406kHz", "FREQ value edit example")
    add_lcd_box(doc, "START(3SEC):AUTO", "BIT:0010 03/16", "BIT edit example")
    doc.add_heading("6.3 POWER SET", level=2)
    add_docx_table(doc, ["Field", "Meaning", "Range"], [
        ["CH", "Power profile", "PWR1~PWR8"],
        ["LOW", "Low alarm threshold", "0.10W~0.95W"],
        ["HIGH", "High alarm threshold", "0.20W~5.00W"],
        ["DEF", "Default output", "0.20W~5.00W"],
    ])
    doc.add_heading("6.4 REMOTE Operation", level=2)
    add_docx_image(doc, FIGURES["remote_ext"], "[그림 6] REMOTE / EXT 운전 개념", width=6.4)
    for text in [
        "Stop oscillation and connect REMOTE contact line.",
        "Select REMOTE mode from SELECT screen and press SET.",
        "REMOTE contact SHORT starts output.",
        "REMOTE contact OPEN stops output.",
        "Use independent contact for each generator.",
    ]:
        doc.add_paragraph(text, style="List Number")
    doc.add_heading("6.5 EXT Operation", level=2)
    for text in [
        "Select EXT mode from SELECT screen and press SET.",
        "Enter SETTING MODE -> 3.EXT:RS485.",
        "Set BAUD, ADDR, TERM and PARITY.",
        "Press SET to save each setting.",
        "Use RS-485 Modbus RTU master to monitor or control.",
    ]:
        doc.add_paragraph(text, style="List Number")

    doc.add_heading("7. Cable Wiring Diagram", level=1)
    add_docx_table(doc, ["Signal", "Normal State", "Description"], [
        ["SENSOR", "SHORT", "OPEN causes Err7/Err8 and inhibits output."],
        ["REMOTE", "OPEN=STOP, SHORT=RUN", "Used in REMOTE operation."],
        ["GO", "SHORT in normal range", "Output level normal indication."],
        ["LOW/HIGH/TRANSDUCER", "SHORT at alarm", "Individual alarm contacts."],
        ["OPERATE", "SHORT normal, OPEN alarm/power-off", "Fail-safe integrated output."],
    ])

    doc.add_heading("8. ERROR Display", level=1)
    add_lcd_box(doc, "   ALARM STOP", "ERR1 LOW ALARM", "Alarm display example")
    add_docx_table(doc, ["Code", "Meaning", "Output", "Recovery"], [
        ["Err1", "LOW ALARM", "Stop", "Check load/LOW setting, then SET"],
        ["Err2", "HIGH ALARM", "Stop", "Check load/HIGH setting, then SET"],
        ["Err5", "TRANSDUCER ALARM", "Stop", "Check nozzle/cable/load, then SET or restart"],
        ["Err6", "SETTING ERROR", "Conditional", "Input valid value"],
        ["Err7", "SENSOR OFF", "Stop", "Power OFF, restore SENSOR, power ON"],
        ["Err8", "SENSOR RUN", "Stop", "Power OFF, restore SENSOR, power ON"],
    ])

    doc.add_heading("9. Communication Specification", level=1)
    add_docx_table(doc, ["Item", "Specification"], [
        ["Interface", "RS-485 Half-Duplex"],
        ["Protocol", "Modbus RTU"],
        ["Default", "Address 1, 9600bps, None-8-1"],
        ["Supported Baudrate", "9600 / 19200 / 38400 / 115200"],
        ["Panel Setting", "SETTING MODE -> 3.EXT:RS485"],
        ["Function Code", "0x03 / 0x06 / 0x10"],
    ])
    add_docx_table(doc, ["Address", "R/W", "Name", "Value"], [
        ["0x0000", "R/W", "RUN control", "0=STOP, 1=RUN"],
        ["0x0001", "R/W", "Frequency", "x0.1kHz"],
        ["0x0002", "R/W", "Duty", "x0.1%"],
        ["0x0006", "R/W", "Mode", "0=NORMAL,1=REMOTE,2=EXT"],
        ["0x0010", "R/W", "Slave address", "1~247"],
        ["0x0011", "R/W", "Baud index", "0~3"],
        ["0x0012", "R/W", "Parity", "0=None,1=Even,2=Odd"],
    ])

    doc.add_heading("10. Setting Storage", level=1)
    for text in [
        "Press SET to save current setting.",
        "Two beeps indicate successful save.",
        "Three beeps indicate failed save.",
        "Saved values are restored after power OFF/ON.",
        "Mode, FREQ, POWER and RS-485 settings are stored.",
    ]:
        doc.add_paragraph(text, style="List Bullet")

    doc.add_heading("11. Troubleshooting", level=1)
    add_docx_table(doc, ["Symptom", "Possible Cause", "Measure"], [
        ["No output", "STOP/alarm/SENSOR open", "Check LCD, alarm and SENSOR."],
        ["Output stops", "LOW/HIGH alarm", "Check POWER LOW/HIGH setting."],
        ["Err7 at power ON", "SENSOR open", "Power OFF and restore SENSOR short."],
        ["Wrong frequency", "Wrong FREQ CH or no save", "Select CH and press SET."],
        ["RS-485 no response", "A/B/address/baud/parity mismatch", "Check wiring and EXT:RS485."],
        ["Setting not restored", "SET not pressed or save failed", "Save again and confirm two beeps."],
    ])

    doc.add_heading("12. Maintenance", level=1)
    add_docx_table(doc, ["Period", "Check Item"], [
        ["Daily", "Cable/connector, LCD/LED, alarm history."],
        ["Weekly", "Output repeatability, RS-485 response, REMOTE contact."],
        ["Monthly", "Transducer/cable insulation, alarm threshold, setting backup."],
    ])

    doc.add_heading("13. A/S", level=1)
    for text in [
        "Warranty period and service condition follow the product warranty document.",
        "Provide model name, serial number, alarm code, symptom and installation photos when requesting service.",
        "Warranty may not apply to unauthorized modification, incorrect wiring, natural disaster or consumables.",
    ]:
        doc.add_paragraph(text, style="List Bullet")

    doc.add_heading("14. Revision History", level=1)
    add_docx_table(doc, ["Version", "Date", "Description"], [
        ["v3.0", "2026-05-19", "Final LCD-panel user manual with menu setting, REMOTE/EXT operation and figures."],
    ])

    doc.save(DOCX_OUT)


if __name__ == "__main__":
    write_markdown()
    write_docx()
    print(MD_OUT)
    print(DOCX_OUT)
