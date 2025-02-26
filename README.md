# LIFX CLI

*A command-line tool for controlling LIFX smart lights on a Linux system.*

![LIFX CLI](https://yourimageurl.com/banner.png)

## 🚀 Features

- ✅ Turn LIFX lights on/off
- 🎨 Set colors (single or multi-zone)
- 🌞 Adjust brightness and warmth
- 💾 Load and save scenes
- 📋 List and query devices
- 🌍 Apply commands to all devices or specific IPs

---

## 📥 Installation

### Prerequisites
- 🐧 Linux operating system
- 🛠 g++ compiler
- 🌐 LIFX LAN connectivity

### 🔧 Build Instructions
```sh
# Clone the repository
git clone https://github.com/yourusername/lifx-cli.git
cd lifx-cli

# Compile the program
make
```

---

## 📌 Usage

```sh
lifx [OPTIONS] COMMAND [ARGS]
```

### 🎛 Commands

#### 💡 Device Control:
- `--on, -o`  → Turn a light **on**
- `--off, -f` → Turn a light **off**
- `--setColor {hex}, -c` → Set a single color
- `--setColors {hex,hex}, -s` → Set multi-zone colors
- `--setColorsR {hex,hex}, -r` → Assign colors randomly across zones
- `--brightness {percent}, -b` → Set brightness (0-100%)
- `--warmth {kelvin}, -w` → Adjust warmth in Kelvin
- `--loadScene {sceneName}, -v` → Load a saved scene
- `--saveScene {sceneName}, -x` → Save a scene

#### 🔍 Device Query:
- `--list, -l` → List all LIFX devices
- `--info, -i` → Show device details
- `--listScene, -e` → List saved scenes

#### 🎯 Target Selection:
- `--ip {ip}, -p` → Target a device by IP
- `--all, -a` → Apply command to all devices
- `--duration {ms}, -d` → Set transition duration

---

## 📝 Examples

```sh
# Turn on a specific light
lifx --on --ip 192.168.1.100

# Set a light to green
lifx --setColor [0x00FF00] --ip 192.168.1.100

# Set multi-zone colors randomly
lifx --setColorsR [0xFF0000,0x00FF00,0x0000FF] --ip 192.168.1.100

# List all LIFX devices on the network
lifx --list
```

---

## Limitations
Due to not owning all of lifx producs / different versions, some features implemented have not been fully tested


---

## 🤝 Contributing
Pull requests are welcome! Please open an issue first to discuss any major changes.

---

## 📜 License
This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.


















