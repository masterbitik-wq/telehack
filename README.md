# telehack
# Telnet Figlet Client

Программа подключается к telnet-сервису `telehack.com`, отправляет команду `figlet` с указанным шрифтом и текстом, получает ASCII-арт и выводит его в консоль.

## Сборка
make
Или вручную:
gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -o figlet_client Client.c -lws2_32

## Использование
./figlet_client <шрифт> <текст>

## Примеры:
./figlet_client standard "Hello, World!"
./figlet_client big "ASCII"
./figlet_client banner "Test"
