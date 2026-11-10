@echo off
REM 该行关闭了命令行的回显

REM 使用for命令遍历当前目录及其所有子目录中的文件
for /r %%f in (BF006三相380一控一.hex) do (
    REM 如果找到了一个文件，输出它的完整路径
    echo %%f
    REM 使用xcopy命令将文件复制到当前目录
    xcopy "%%f" ".\"
)

REM 输出完成消息
echo 完成。