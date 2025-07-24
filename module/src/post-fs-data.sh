#!/system/bin/sh


MODDIR=${0%/*}
if [ "$ZYGISK_ENABLED" ]; then
  exit 0
fi

cd "$MODDIR"

if [ "$(which magisk)" ]; then
  for file in ../*; do
    if [ -d "$file" ] && [ -d "$file/zygisk" ] && ! [ -f "$file/disable" ]; then
      if [ -f "$file/post-fs-data.sh" ]; then
        cd "$file"
        log -p i -t "zygisk-sh" "Manually trigger post-fs-data.sh for $file"
        sh "$(realpath ./post-fs-data.sh)"
        cd "$MODDIR"
      fi
    fi
  done
fi

cd "$MODDIR" || exit

chmod +x $MODDIR/bin -R



create_sys_perm() {
  mkdir -p $1
  chmod 555 $1
  chcon u:object_r:system_file:s0 $1
}

#export TMP_PATH=/apex/com.android.syzuel
#create_sys_perm $TMP_PATH
#mount -t tmpfs tmpfs $TMP_PATH
#
#if [ -f $MODDIR/lib/arm64-v8a/libzygisk.so ];then
#  create_sys_perm $TMP_PATH/lib64
#  cp $MODDIR/lib/arm64-v8a/libzygisk.so $TMP_PATH/lib64/libsyzuel.so
#  chcon u:object_r:system_file:s0 $TMP_PATH/lib64/libsyzuel.so
#fi


#export LIB_ZYGISK_SO_PATH=$TMP_PATH/lib64/libsyzuel.so
export LIB_ZYGISK_SO_PATH=/data/adb/modules/zygiskADI/lib/arm64-v8a/libzygisk.so

#  must usr &
#  must usr &
./bin/zygiskd daemon &
#./bin/adi -m -c   $MODDIR/zygisk3.json &
./bin/adi  -m  -p 1 --exec /system/bin/app_process64  --injectSoPath $LIB_ZYGISK_SO_PATH --injectFunSym entry --injectFunArg d63138f231 --monitorCount 10&

