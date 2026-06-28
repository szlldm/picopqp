#!/bin/bash
APPS=(
	"bootloader_latch"
	"erase_fw"
	"ping"
	"pqp_info"
	"reboot"
	"run_fw"
	"set_routing_table"
	"transfer_test"
	"tty"
	"upload_fw"
)

mkdir -p build
for app in ${APPS[@]}; do
	echo "Build ${app} ..."
	gcc "${app}.c" gnd_app_base.c -lm -pthread -lpthread -lrt -O3 -o "./build/${app}"
done

