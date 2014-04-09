#!/bin/sh
# Copyright (C) 2012-2014 PIVA Software <www.pivasoftware.com>
# 	Author: MOHAMED Kallel <mohamed.kallel@pivasoftware.com>
# 	Author: AHMED Zribi <ahmed.zribi@pivasoftware.com>
# 	Author: ANIS ELLOUZE <anis.ellouze@pivasoftware.com>
# Copyright (C) 2011-2012 Luka Perkov <freecwmp@lukaperkov.net>

. /lib/functions.sh
. /usr/share/libubox/jshn.sh
. /usr/share/shflags/shflags.sh
. /usr/share/easycwmp/defaults

UCI_GET="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} get"
UCI_SET="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} set"
UCI_SHOW="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} show"
UCI_COMMIT="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} commit"
UCI_ADD="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} add"
UCI_DELETE="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} delete"
UCI_ADD_LIST="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} add_list"
UCI_DEL_LIST="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} del_list"
UCI_REVERT="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} revert"
UCI_CHANGES="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} changes"
UCI_BATCH="/sbin/uci -q ${UCI_CONFIG_DIR:+-c $UCI_CONFIG_DIR} batch"

DOWNLOAD_FILE="/tmp/easycwmp_download"
EASYCWMP_PROMPT="easycwmp>"
set_fault_tmp_file="/tmp/set_fault_tmp"
apply_service_tmp_file="/tmp/easycwmp_apply_service"
easycwmp_config_changed=""

get_functions=""
set_functions=""
add_object_functions=""
delete_object_functions=""

# Fault codes
E_REQUEST_DENIED="1"
E_INTERNAL_ERROR="2"
E_INVALID_ARGUMENTS="3"
E_RESOURCES_EXCEEDED="4"
E_INVALID_PARAMETER_NAME="5"
E_INVALID_PARAMETER_TYPE="6"
E_INVALID_PARAMETER_VALUE="7"
E_NON_WRITABLE_PARAMETER="8"
E_NOTIFICATION_REJECTED="9"
E_DOWNLOAD_FAILURE="10"
E_UPLOAD_FAILURE="11"
E_FILE_TRANSFER_AUTHENTICATION_FAILURE="12"
E_FILE_TRANSFER_UNSUPPORTED_PROTOCOL="13"
E_DOWNLOAD_FAIL_MULTICAST_GROUP="14"
E_DOWNLOAD_FAIL_CONTACT_SERVER="15"
E_DOWNLOAD_FAIL_ACCESS_FILE="16"
E_DOWNLOAD_FAIL_COMPLETE_DOWNLOAD="17"
E_DOWNLOAD_FAIL_FILE_CORRUPTED="18"
E_DOWNLOAD_FAIL_FILE_AUTHENTICATION="19"

# define a 'name' command-line string flag
DEFINE_boolean 'json' false 'send values using json' 'j'

FLAGS_HELP=`cat << EOF
USAGE: $0 [flags] command [parameter] [values]
command:
  get [value|notification|name]
  set [value|notification]
  apply [value|notification|object|service]
  add [object]
  delete [object]
  download
  factory_reset
  reboot
  inform [parameter|device_id]
  json_input
EOF`

FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

if [ ${FLAGS_help} -eq ${FLAGS_TRUE} ]; then
	exit 1
fi

__arg1=""; __arg2=""; __arg3=""; __arg4=""; __arg5="";

json_get_opt() {
	__arg1=""; __arg2=""; __arg3=""; __arg4=""; __arg5="";
	json_init
	json_load "$1"
	local command class
	json_get_var command command
	case "$command" in
		set|get|add|delete)
			json_get_var class class
			json_get_var __arg1 parameter
			json_get_var __arg2 argument
			if [ "$class" != "" ]; then
				action="$command""_$class"
			else
				action="$command""_value"
			fi
			;;
		download)
			action="download"
			json_get_var __arg1 url
			json_get_var __arg2 file_type
			json_get_var __arg3 file_size
			json_get_var __arg4 user_name
			json_get_var __arg5 password
			;;
		factory_reset|reboot)
			action="$command"
			;;
		inform)
			json_get_var class class
			if [ "$class" != "" ]; then
			action="inform_$class"
			else
				action="inform_parameter"
			fi
			;;
		apply)
			json_get_var class class
			json_get_var __arg1 argument
			if [ "$class" != "" ]; then
				action="apply_$class"
			else
				action="apply_value"
			fi
			;;
		end)
			action="end"
			echo "$EASYCWMP_PROMPT"
			;;
		exit)
			exit 0
			;;
	esac	
}

case "$1" in
	set|get|add|delete)
		if [ "$2" = "notification" -o "$2" = "value" -o "$2" = "name" -o "$2" = "object" ]; then
			__arg1="$3"
			__arg2="$4"
			action="$1_""$2"
		else
			__arg1="$2"
			__arg2="$3"
			action="$1""_value"
		fi
		;;
	download)
		action="download"
		__arg1="$2"
		__arg2="$3"
		__arg3="$4"
		__arg4="$5"
		__arg5="$6"
		;;
	factory_reset|reboot)
		action="$1"
		;;
	inform)
		if [ "$2" != "" ]; then
		action="inform_$2"
		else
			action="inform_parameter"
		fi
		;;
	apply)
		if [ "$2" != "" ]; then
			__arg1="$3"
			action="apply_$2"
		else
			__arg1="$2"
			action="apply_value"
		fi
		;;
	json_input)
		action="json_input"
		;;	
esac


if [ -z "$action" ]; then
	echo invalid action \'$1\'
	exit 1
fi

load_script() {
	. $1 
}

load_function() {
	get_functions="$get_functions get_$1"
	set_functions="$set_functions set_$1"
	add_object_functions="$add_object_functions add_object_$1"
	delete_object_functions="$delete_object_functions delete_object_$1"
	build_instances_$1  2> /dev/null
}

handle_scripts() {
	local section="$1"
	config_get prefix "$section" "prefix"
	config_list_foreach "$section" 'location' load_script
	config_list_foreach "$section" 'function' load_function
}

config_load easycwmp
config_foreach handle_scripts "scripts"

handle_action() {
	if [ "$action" = "get_value" ]; then
		local __param
		[ "$__arg1" = "" ] && __param="InternetGatewayDevice." || __param="$__arg1"
		easycwmp_execute_functions "$get_functions" "$__param"
		fault_code="$?"
		
		if [ "$fault_code" != "0" ]; then
			let fault_code=$fault_code+9000
			easycwmp_output "$__arg1" "" "" "" "$fault_code"
		fi
	fi
	
	if [ "$action" = "get_name" ]; then
		local __param="$__arg1"		
		[ "`echo $__arg2 | awk '{print tolower($0)}'`" = "false" ] &&  __arg2="0"
		[ "`echo $__arg2 | awk '{print tolower($0)}'`" = "true" ] &&  __arg2="1"
		if [ "$__arg2" != "0" -a "$__arg2" != "1" ]; then
			let fault_code=$E_INVALID_ARGUMENTS+9000
			easycwmp_output "$__arg1" "" "" "" "$fault_code"
			return
		fi
		if [ "$__arg1" = "InternetGatewayDevice." -a "$__arg2" = "0" ]; then
			easycwmp_output "InternetGatewayDevice." "" "0"
		fi
		if [ "$__arg1" = "" ]; then
			easycwmp_output "InternetGatewayDevice." "" "0"
			if [ "$__arg2" = "1" ]; then
				return
			fi
			__param="InternetGatewayDevice."
		fi
		easycwmp_execute_functions "$get_functions" "$__param" "$__arg2"
		fault_code="$?"
		
		if [ "$fault_code" != "0" ]; then
			let fault_code=$fault_code+9000
			easycwmp_output "$__arg1" "" "" "" "$fault_code"
		fi
	fi
	
	if [ "$action" = "get_notification" ]; then
		local __param
		[ "$__arg1" = "" ] && __param="InternetGatewayDevice." || __param="$__arg1"
		easycwmp_execute_functions "$get_functions" "$__param"
		fault_code="$?"
		if [ "$fault_code" != "0" ]; then
			let fault_code=$fault_code+9000
			easycwmp_output "$__arg1" "" "" "" "$fault_code"
		fi
	fi
	
	if [ "$action" = "set_value" ]; then

		local fault_code="0"
		[ "$__arg1" = "" ] && fault_code=$E_INVALID_PARAMETER_NAME
		if [ "$fault_code" = "0" ]; then
			easycwmp_execute_functions "$set_functions" "$__arg1" "$__arg2"
			fault_code="$?"
		fi
		if [ "$fault_code" != "0" ]; then
			let fault_code=$fault_code+9000
			easycwmp_set_parameter_fault "$__arg1" "$fault_code"
		fi
	fi
	
	if [ "$action" = "set_notification" ]; then
		local fault_code="0"
		[ "$__arg1" = "" ] && fault_code=$E_INVALID_PARAMETER_NAME
		if [ "$fault_code" = "0" ]; then
			easycwmp_execute_functions "$set_functions" "$__arg1" "$__arg2"
			fault_code="$?"
		fi
		if [ "$fault_code" != "0" ]; then
			let fault_code=$fault_code+9000
			easycwmp_set_parameter_fault "$__arg1" "$fault_code"
		fi
	fi
	
	if [ "$action" = "download" ]; then
# TODO: check firmaware size with falsh to be improved  
		dl_size=`df  |grep  /tmp | awk '{print $4;}'`
		dl_size_byte=$((${dl_size}*1024))
		if [ "$dl_size_byte" -lt "$__arg3" ]; then
			let fault_code=9000+$E_DOWNLOAD_FAILURE
			easycwmp_status_output "" "$fault_code"
		else 
			rm -f $DOWNLOAD_FILE 2> /dev/null
			local dw_url="$__arg1"
			[ "$__arg4" != "" -o "$__arg5" != "" ] && dw_url=`echo  "$__arg1" | sed -e "s@://@://$__arg4:$__arg5\@@g`
			wget -O $DOWNLOAD_FILE "$dw_url"
			fault_code="$?"
			if [ "$fault_code" != "0" ]; then
				let fault_code=9000+$E_DOWNLOAD_FAILURE
				easycwmp_status_output "" "$fault_code"
			else
				easycwmp_status_output "" "" "1"
			fi
		fi
	fi
	if [ "$action" = "apply_download" ]; then
		if [ "$__arg1" = "3 Vendor Configuration File" ]; then 
			/sbin/uci import < $DOWNLOAD_FILE  
			fault_code="$?"
			if [ "$fault_code" != "0" ]; then
				let fault_code=$E_DOWNLOAD_FAIL_FILE_CORRUPTED+9000
				easycwmp_status_output "" "$fault_code"
			else
				$UCI_COMMIT
				sync
				reboot
				easycwmp_status_output "" "" "1"
			fi
		elif [ "$__arg1" = "1 Firmware Upgrade Image" ]; then
			/sbin/sysupgrade $DOWNLOAD_FILE 
			fault_code="$?"
			if [ "$fault_code" != "0" ]; then
				let fault_code=$E_DOWNLOAD_FAIL_FILE_CORRUPTED+9000
				easycwmp_status_output "" "$fault_code"
			else
				easycwmp_status_output "" "" "1"
			fi
		else
			easycwmp_status_output "" "$(($E_INVALID_ARGUMENTS+9000))"
		fi
	fi
	if [ "$action" = "factory_reset" ]; then
		jffs2_mark_erase "rootfs_data"
		sync
		reboot
	fi
	
	if [ "$action" = "reboot" ]; then
		sync
		reboot
	fi
	
	if [ "$action" = "apply_notification" -o "$action" = "apply_value" ]; then
		if [ ! -f "$set_fault_tmp_file" ]; then
			$UCI_COMMIT
			if [ "$action" = "apply_value" ]; then
				$UCI_SET easycwmp.@acs[0].parameter_key="$__arg1"
				$UCI_COMMIT
				easycwmp_status_output "" "" "1"
			fi
			if [ "$action" = "apply_notification" ]; then
				easycwmp_status_output "" "" "0"
			fi
		else
			cat "$set_fault_tmp_file" 
			local cfg
			local cfg_reverts=`$UCI_CHANGES | cut -d'.' -f1 | sort -u`
			for cfg in $cfg_reverts; do
				$UCI_REVERT $cfg
			done
		fi
		rm -f "$set_fault_tmp_file"
	fi
	if [ "$action" = "apply_object" ]; then
		$UCI_SET easycwmp.@acs[0].parameter_key="$__arg1"
		$UCI_COMMIT
	fi
	if [ "$action" = "apply_service" ]; then
		if [ -f "$apply_service_tmp_file" ]; then
			chmod +x "$apply_service_tmp_file"
			/bin/sh "$apply_service_tmp_file"
			rm -f "$apply_service_tmp_file"
		fi
	fi
		
	if [ "$action" = "add_object" ]; then
		easycwmp_execute_functions "$add_object_functions" "$__arg1"
		fault_code="$?"
		if [ "$fault_code" != "0" ]; then
			let fault_code=$fault_code+9000
			easycwmp_status_output "" "$fault_code"
		fi
	fi
	
	if [ "$action" = "delete_object" ]; then
		easycwmp_execute_functions "$delete_object_functions" "$__arg1"
		fault_code="$?"
		if [ "$fault_code" != "0" ]; then
			let fault_code=$fault_code+9000
			easycwmp_status_output "" "$fault_code"
		fi
	fi

	if [ "$action" = "inform_parameter" ]; then
		easycwmp_get_inform_parameters
	fi
	
	if [ "$action" = "inform_device_id" ]; then
		easycwmp_get_inform_deviceid
	fi
	
	if [ "$action" = "json_input" ]; then
		echo "$EASYCWMP_PROMPT"
		while read CMD; do
			[ -z "$CMD" ] && continue
			json_get_opt "$CMD"
			handle_action
		done
		exit 0
	fi
}
handle_action 2>/dev/null
