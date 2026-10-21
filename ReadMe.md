# LenovoEduCloudPWSet

## 关于本项目

联想智能云教室控制台管理员密码破解工具，支持读取，强制重置控制台管理员密码

## 用法

### 命令行版本

```
强制重置密码
LenovoEduCloudPWSet -e -p <password> [-k <pubkey.bin>] [-o <wfpwd.bin>]
读取密码
LenovoEduCloudPWSet -d [-i <wfpwd.bin>] [-k <prikey.bin>] [-o <wfpwd.bin>]
读取管理员用户名
LenovoEduCloudPWSet -u [-c <BSSvrCfg.ini>]
```
不指定 `-k` / `-i` / `-o` / `-c` 时，使用当前目录默认文件。

### GUI 版本

双击运行 `LenovoEduCloudPWSet_GUI.exe` 或者 `LenovoEduCloudPWSet_GUI64.exe`：

1. 选择或手动输入联想云教室控制台安装路径
2. 点击“重置密码”，输入新密码
3. 点击“读取密码”，查看当前密码
4. 点击“读取用户名”，查看管理员账号

所有文件均使用所选目录下的默认文件名，无需额外配置。

## 开源说明

在修改和由本仓库代码衍生的代码中需要说明“基于 TimeControl 开发”。

本项目基于 `MIT` 许可证开源，详情请参阅 [License](License) 文件。 

您可以在遵守许可证的前提下自由使用、修改和分发本软件。