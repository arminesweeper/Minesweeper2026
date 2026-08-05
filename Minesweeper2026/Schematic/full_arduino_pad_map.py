import re

path = r'e:\Robotics_Team\Minesweeper\Minesweeper\Minesweeper2026\Schematic\Robotics_Mine.kicad_pcb'
with open(path, encoding='utf-8', errors='ignore') as f:
    content = f.read()

idx = content.find('ARDUINO_ARDUINO_MEGA_2560')
start = content.rfind('(footprint', 0, idx)
depth = 0
i = start
in_str = False
while i < len(content):
    c = content[i]
    if c == '\"':
        in_str = not in_str
    elif not in_str:
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    i += 1
chunk = content[start:end]

# Parse each pad block for name and net
pads = []
for m in re.finditer(r'\(pad\s+\"([^\"]+)\"\s+thru_hole[\s\S]*?(?=\n\t\t\(pad |\n\t\t\(embedded|\n\t\))', chunk):
    block = m.group(0)
    name = m.group(1)
    net_m = re.search(r'\(net\s+(?:\"([^\"]+)\"|(\d+)|\"([^\"]+)\")\)', block)
    # KiCad 9 format: (net \"/PWM1\") or (net 4) weird
    net_m = re.search(r'\(net\s+\"([^\"]+)\"\)|\(net\s+(\d+)\)', block)
    if net_m:
        net = net_m.group(1) if net_m.group(1) else net_m.group(2)
    else:
        net = None
    pads.append((name, net))

print('=== FULL ARDUINO PAD MAP ===')
for name, net in pads:
    print(f'{name:10} -> {net}')

print('\n=== NAMED SIGNAL NETS ONLY ===')
for name, net in pads:
    if net and net not in ('P,GND',) and not net.isdigit() and not net.startswith('unconnected'):
        print(f'D{name}' if name.isdigit() else name, '->', net)

# Also check: nets named as numbers 4,5,6 - what are those?
print('\n=== Numeric nets (likely pin-named nets) ===')
for name, net in pads:
    if net and net.isdigit():
        print(f'pad {name} -> net \"{net}\"')
        
# Output:
# === FULL ARDUINO PAD MAP ===
# 3V3        -> None
# 4          -> 4
# 5          -> 5
# 5V_1       -> /5V_LOGIC
# 5V_2       -> /5V_LOGIC
# 5V_3       -> /5V_LOGIC
# 6          -> 6
# 7          -> 7
# 8          -> 8
# 9          -> 9
# 10         -> 10
# 11         -> 11
# 12         -> 12
# 13         -> 13
# 18         -> /B2
# 19         -> /A2
# 20         -> /B1
# 21         -> /A1
# 22         -> None
# 23         -> None
# 24         -> None
# 25         -> None
# 26         -> None
# 27         -> None
# 28         -> None
# 29         -> None
# 30         -> None
# 31         -> None
# 32         -> None
# 33         -> None
# 34         -> None
# 35         -> None
# 36         -> Net-(J13-Pin_1)
# 37         -> None
# 38         -> Net-(J14-Pin_1)
# 39         -> None
# 40         -> /DIR2
# 41         -> None
# 42         -> /DIR1
# 43         -> None
# 44         -> /PWM1
# 45         -> None
# 46         -> /PWM2
# 47         -> None
# 48         -> None
# 49         -> None
# 50         -> None
# 51         -> None
# 52         -> None
# 53         -> None
# AREF       -> None
# GND1       -> P,GND
# GND2       -> P,GND
# GND3       -> P,GND
# GND4       -> P,GND
# GND5       -> P,GND
# RESET      -> None
# SCL        -> /SCL
# SDA        -> /SDA

# === NAMED SIGNAL NETS ONLY ===
# 5V_1 -> /5V_LOGIC
# 5V_2 -> /5V_LOGIC
# 5V_3 -> /5V_LOGIC
# D18 -> /B2
# D19 -> /A2
# D20 -> /B1
# D21 -> /A1
# D36 -> Net-(J13-Pin_1)
# D38 -> Net-(J14-Pin_1)
# D40 -> /DIR2
# D42 -> /DIR1
# D44 -> /PWM1
# D46 -> /PWM2
# SCL -> /SCL
# SDA -> /SDA

# === Numeric nets (likely pin-named nets) ===
# pad 4 -> net "4"
# pad 5 -> net "5"
# pad 6 -> net "6"
# pad 7 -> net "7"
# pad 8 -> net "8"
# pad 9 -> net "9"
# pad 10 -> net "10"
# pad 11 -> net "11"
# pad 12 -> net "12"
# pad 13 -> net "13"