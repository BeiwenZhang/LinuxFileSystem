﻿# LinuxFileSystem
这是我的操作系统课程设计，请看readme-实验报告，里面包含对项目的功能介绍、使用方法等等
### 项目名称:FAT16文件系统管理程序
项目背景:
本项目旨在开发一个能在Linux环境下运行的文件系统管理程序，用于管理挂载在Linux系统上的FAT16格式U盘。该程序允许用户通过命令行界面进行文件和目录的基本操作，包括创建、删除、读取和更改目录。
#### 技术栈：
语言:C
系统:Linux Ubuntu 9.04
工具:GCC编译器，Eclipse SDK
主要职责:
文件系统访问:实现了对FAT16文件系统的底层访问功能，，包括读取和写入FAT表，扫描启动扇区和根目录项。
命令行交互:开发了一个简洁的命令行界面，使用户能够输入特定命令来操作文件系统，如1s`、`cd`、`cf`(创建文件)、`md`(创建目录)、`df`(删除文件)和`dd`(删除目录)
文件操作实现:编写了处理文件读写的核心功能，包括处理文件的创建和删除，文件内容的读取，以及目录的创建和更改。
#### 挑战与解决方案:
数据一致性:在对FAT表进行修改时，确保数据的一致性极其重要。通过实现双FAT表的同步更新机制，增强了文件系统的鲁棒性。
文件名处理:FAT16的文件名限制为8.3格式，需要将用户输入的文件名转换为FAT16兼容的格式。通过编写特定的字符串处理函数来解决这一问题。
