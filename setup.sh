#!/bin/bash

set -e

echo "Price Driver Setup"
echo "=================="

# Create config directory
sudo mkdir -p /etc/price_driver

# Copy config file
echo "Installing config file..."
sudo cp config.csv /etc/price_driver/config.csv
sudo chmod 644 /etc/price_driver/config.csv

echo "Config installed at /etc/price_driver/config.csv"
echo ""

# Build the driver
echo "Building driver..."
make clean
make

echo ""
echo "Setup complete!"
echo ""
echo "To load the driver, run:"
echo "  sudo insmod price_driver.ko"
echo ""
echo "To unload the driver, run:"
echo "  sudo rmmod price_driver"
