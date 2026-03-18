# Price Feed Driver

Linux kernel character device driver for generating price feed quotes with CSV-based ticker configuration.

## Prerequisites

- Linux kernel headers installed
- GCC compiler
- Make utility
- Root/sudo access for module installation

## Installation

### 1. Clone repository

```bash
git clone <repository-url>
cd price_driver
```

### 2. Install kernel headers (if not already installed)

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install linux-headers-$(uname -r) build-essential
```

**CentOS/RHEL:**
```bash
sudo yum install kernel-devel-$(uname -r) gcc
```

### 3. Create config directory

```bash
sudo mkdir -p /etc/price_driver
sudo cp config.csv /etc/price_driver/config.csv
```

### 4. Install udev rules

```bash
sudo cp 99-price_feed.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### 5. Build the module

```bash
make
```

### 6. Load the module

**Default parameters:**
```bash
sudo insmod price_driver.ko
```

**With custom parameters:**
```bash
sudo insmod price_driver.ko \
  buffer_size=8192 \
  max_tickers=512 \
  max_delta_percent=10 \
  generation_timeout_ms=200 \
  config_path="/etc/price_driver/config.csv"
```

## Usage

### Read price data

```bash
cat /dev/price_feed
```

The device will output binary price data. Each price entry is 12 bytes:
- 8 bytes: ticker symbol (null-padded)
- 4 bytes: price as uint32_t

### Continuous monitoring

```bash
while true; do
  cat /dev/price_feed | hexdump -C
  sleep 1
done
```

### Check module parameters

```bash
cat /sys/module/price_driver/parameters/buffer_size
cat /sys/module/price_driver/parameters/max_tickers
cat /sys/module/price_driver/parameters/max_delta_percent
cat /sys/module/price_driver/parameters/generation_timeout_ms
cat /sys/module/price_driver/parameters/config_path
```

### Modify parameters at runtime

```bash
echo 200 | sudo tee /sys/module/price_driver/parameters/generation_timeout_ms
echo 15 | sudo tee /sys/module/price_driver/parameters/max_delta_percent
```

## Configuration

### CSV Format

Edit `/etc/price_driver/config.csv`:


```csv
AAPL,200,100,150
GOOGL,3000,2000,2500
MSFT,400,200,300
TSLA,1000,500,750
AMZN,4000,2000,3000
```

header is prohibited but would look like this
```
TICKER,UPPER_BOUND,LOWER_BOUND,INITIAL_PRICE
```

## Uninstall

### Remove module

```bash
sudo rmmod price_driver
```

### Clean build files

```bash
make clean
```

### Remove config and rules

```bash
sudo rm /etc/udev/rules.d/99-price_feed.rules
sudo rm -rf /etc/price_driver
```

## Module Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `buffer_size` | 4096 | Internal buffer size in bytes |
| `max_tickers` | 256 | Maximum number of supported tickers |
| `ticker_len` | 8 | Length of ticker symbol strings |
| `price_entry_size` | 12 | Size of each price entry in bytes |
| `config_path` | /etc/price_driver/config.csv | Path to CSV configuration file |
| `max_line_len` | 256 | Maximum CSV line length |
| `max_delta_percent` | 5 | Maximum price change percentage per read |
| `generation_timeout_ms` | 100 | Delay between reads in milliseconds |

## Troubleshooting

### Device not found

Check if module is loaded:
```bash
lsmod | grep price_driver
```

Check device creation:
```bash
ls -la /dev/price_feed
```

Check kernel logs:
```bash
sudo dmesg | tail -20
```

### Permission denied reading device

Verify udev rules are applied:
```bash
ls -la /etc/udev/rules.d/99-price_feed.rules
ls -la /dev/price_feed
```

If permissions are wrong (not 0666), reload rules:
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Config file not found

Ensure config directory and file exist:
```bash
ls -la /etc/price_driver/config.csv
```

Check dmesg for specific error:
```bash
sudo dmesg | grep price_feed
```

## Development

Edit `price_driver.c` and rebuild:
```bash
make clean
make
sudo rmmod price_driver
sudo insmod price_driver.ko
```

## License

GPL v2
