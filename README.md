# quark
IOT设备基础SDK
集成设备注册，长连接（MQTT），语音等功能
## 编译
### 初始化第三方依赖
执行下面的命令首先把依赖的第三方库解压
```bash
bash env.init.sh
```
### 安装系统依赖库（UBUNTU）
```
sudo apt-get install cmake libssl-dev build-essential
```
### 配置cmake
```
cmake .
```
### 编译
```
make -j4
```
## 测试