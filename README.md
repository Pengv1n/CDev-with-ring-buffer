# CDev-with-ring-buffer

Модуль circ_cdev.ko собирается командой make в корне репозитория.
После установки модуля в ядро, создастся файл /dev/circ_cdev0.

При установке пакета write_1.0-0_all.deb, запуcтится сервис systemd, который раз в 30 сек будет записывать текущее время в файл символьного устройства /dev/circ_cdev0.

Скрипт read.sh выполняет команду cat для непрерывного чтения буфера символьного устройства.