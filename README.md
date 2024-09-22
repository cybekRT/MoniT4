# MoniT4
Based on project: https://github.com/Xinyuan-LilyGO/LilyGo-Display-IDF  
Designed for: https://www.lilygo.cc/products/t4-s3

## Purpose
This project makes a small PC monitor from your LilyGo T4-S3.  
Allows to display your temperatures, CPU, RAM and SWAP usage, disk usage and ping to different addresses.

Provided client.sh script is sample data provider that sends temperature, resource usage and pings to specified IP address.

Available resources to monitor are configurable, so different PCs with different number of partitions and temperature sensors can configure their dashboard.

Firmware supports multiple connected clients, changed by swiping on the touch screen.  
Wifi configuration is saved to flash after successful connection.