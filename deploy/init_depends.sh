#/bin/bash
export DIR_API_DEFAULT=$(cd $(dirname $0); pwd)/api
[ ! -d $DIR_API_DEFAULT ] && DIR_API_DEFAULT=~/.local/api
echo "----------------------$0 DIR_API_DEFAULT = $DIR_API_DEFAULT init_depends.sh----------------------"

source $DIR_API_DEFAULT/use.sh

deploy_libcurl() {
	local third_party_path=$DIR_BIN/../third/
	log_msg "entering $third_party_path"
    do_or_die cd $third_party_path
    
	#fetching libcurl from github
	if [ ! -d ./appbuilding ]; then
        log_msg "fetching appbuilding from github now..."
        do_or_die git clone https://github.com/wells-xu/appbuilding.git
    else
        log_msg "appbuilding already exist...ready to pull the newest version"
        do_or_die cd ./appbuilding
        #do_or_die git pull
    fi
    
    #building libcurl output
    log_msg "entering $third_party_path/appbuilding/win/libcurl"
    do_or_die cd $third_party_path/appbuilding/win/libcurl
    bash build.sh $1 $2

    #deploy outputs
    say_is_dir_exist $dir_third_output_path
    say_is_dir_exist $dir_third_output_path/include
    say_is_dir_exist $dir_third_output_path/lib
    #c-ares
    do_or_die cp -rv $third_party_path/appbuilding/win/libcurl/c-ares/msvc/cares/include/* $dir_third_output_path/include
    do_or_die cp -rv $third_party_path/appbuilding/win/libcurl/c-ares/msvc/cares/lib/* $dir_third_output_path/lib
    #libcurl
    local curl_oname=$(ls $third_party_path/appbuilding/win/libcurl/curl/builds | head -n 1)
    log_ok $(ls $third_party_path/appbuilding/win/libcurl/curl/builds)
    log_ok $curl_oname
    say_is_dir_exist $third_party_path/appbuilding/win/libcurl/curl/builds/$curl_oname
    do_or_die cp -rv $third_party_path/appbuilding/win/libcurl/curl/builds/$curl_oname/include/* $dir_third_output_path/include
    do_or_die cp -rv $third_party_path/appbuilding/win/libcurl/curl/builds/$curl_oname/lib/* $dir_third_output_path/lib
}

deploy_winoah() {
    log_msg "entering $dir_third_path"
    do_or_die cd $dir_third_path

    #fetching winoah from github
    if [ ! -d ./winoah ]; then
        log_msg "fetching winoah from github now..."
        do_or_die git clone https://github.com/wells-xu/winoah.git
    else
        log_msg "winoah already exist..."
    fi

    log_msg "entering winoah: $dir_third_path/winoah"
    do_or_die cd ./winoah
    #do_or_die git pull

    say_is_file_exist ./build.bat
    do_or_die ./build.bat $1 $2

    do_or_die cp -rv ./build/output/baselog.dll $dir_third_output_path/lib
    do_or_die cp -rv ./build/output/libbase_static.lib $dir_third_output_path/lib
    do_or_die cp -rv ./base $dir_third_output_path/include
}

main() {
    log_msg "main starting at root path = $DIR_BIN"

    local config_flag=$1
    local machine_flag=$2

    if is_null $config_flag;then
        config_flag=debug
    fi
    if [ $config_flag != "debug" -a $config_flag != "release" ]; then
        log_fatal "MUST BUILD WITH CONFIGURATION TYPE debug or release"
    fi

    if is_null $machine_flag;then
        machine_flag=x86
    fi
    if [ $machine_flag != "x86" -a $machine_flag != "x64" ]; then
        log_fatal "MUST BUILD WITH MACHINE TYPE x86 or x64"
    fi

    log_msg "Your building mode is: [$config_flag : $machine_flag]"
    dir_third_path=$DIR_BIN/../third
    dir_third_output_path=$dir_third_path/output/${config_flag}_${machine_flag}
    do_or_die mkdir -p $dir_third_output_path
    do_or_die mkdir -p $dir_third_output_path/include
    do_or_die mkdir -p $dir_third_output_path/lib
    
	#deploy libcurl first
    #deploy_libcurl $config_flag $machine_flag

    #deploy winoah
    deploy_winoah $config_flag $machine_flag

    #just pause here with anykey will be quitting.
    read
}

main "$@"
