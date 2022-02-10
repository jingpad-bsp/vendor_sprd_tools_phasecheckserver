#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <grp.h>
#include <jni.h>
#ifndef CHANNEL_SOCKET
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#endif
#include <utils/Log.h>
#include <utils/String16.h>
#include <android/log.h>
#include <string.h>
#include "NativeService.h"
#include "engphasecheck.h"
#include <cutils/properties.h>
#include <pthread.h>

#ifdef CHANNEL_SOCKET
#include <cutils/sockets.h>
#endif

#define PHASE_CHECK_MISCDATA "miscdata"
#define CHARGE_SWITCH_FILE "/sys/class/power_supply/battery/stop_charge"
#define CHARGE_SWITCH_FILE_k414 "/sys/class/power_supply/battery/charger.0/stop_charge"
#define JEITA_CONTROL_FILE "/sys/class/power_supply/battery/charger.0/jeita_control"
/* SPRD 940291 - [true demo version] Control charge region @{ */
#define CHARGE_STATUS_FILE "/sys/class/power_supply/usb/online"
#define CHARGE_LEVEL_FILE "/sys/class/power_supply/battery/capacity"
/* }@ */
#define MIPI_SWITCH_FILE  "/sys/class/modem/debug-log/channel"
/* SPRD 815541 - Coulometer Power Test @{ */
#define CC_TEST_CMD_FILE  "/sys/class/power_supply/sprdfgu/cc_test_cmd"
#define CC_TEST_RESULT_FILE  "/sys/class/power_supply/sprdfgu/cc_test_result"
#define CC_TEST_VOL_FILE  "/sys/class/power_supply/sprdfgu/fgu_vol"
/* }@ */
#define PHASECHECK_SERVICE "phasecheck_srv"
#define MAX_BUF_LEN 4096
#define PHASE_CHECK_MISCDATA_PATH "ro.vendor.product.partitionpath"
/* SPRD 911107 - write /sys/class/misc/sprd_7sreset/hard_mode no effective @{ */
#define RESET_MODE_FILE  "/sys/class/misc/sprd_7sreset/hard_mode"
/* }@ */
/* SPRD 1016593 - Coulometer Power Test for kernel 4.14 @{ */
#define CC_TEST_ENERGY_FILE  "/sys/class/power_supply/sc27xx-fgu/energy_now"
#define CC_TEST_VOLTAGE_FILE  "/sys/class/power_supply/battery/voltage_now"
/* }@ */
#define DELTANV_BIN_FILE  "/dev/block/platform/soc/soc:ap-ahb/20600000.sdio/by-name/l_deltanv"
#define DELTA_FILESIZE 2048
//gnss-test for Bug 913791
#define GNSS_CTRL_BACKLIGHT "/sys/class/backlight/sprd_backlight/brightness"
#define CABC_PATH "/sys/module/dpu_r4p0/parameters/cabc_disable"
#define CPU_DEBUG_PATH "/proc/sprd_hang_debug/wdt_disable"

typedef unsigned char BOOLEAN;
typedef unsigned char uint8;
typedef unsigned short uint16;  //NOLINT
typedef unsigned int uint32;

typedef signed char int8;
typedef signed short int16;  //NOLINT
typedef signed int int32;

namespace android
{
    //static struct sigaction oldact;
    static pthread_key_t sigbuskey;
    static SP09_PHASE_CHECK_T Phase;
    static SP15_PHASE_CHECK_T phase_check_sp15;

    int NativeService::Instance()
    {
#ifdef CHANNEL_SOCKET
        return 0;
#else
        ENG_LOG("phasecheck_sprd new ..NativeService Instantiate\n");
        int ret = defaultServiceManager()->addService(
                String16("phasechecknative"), new NativeService());
        ENG_LOG("phasecheck_sprd new ..NatveService ret = %d\n", ret);
        return ret;
#endif
    }

    NativeService::NativeService()
    {
        ENG_LOG("phasecheck_sprd NativeService create\n");
        //m_NextConnId = 1;
        pthread_key_create(&sigbuskey,NULL);
    }

    NativeService::~NativeService()
    {
        pthread_key_delete(sigbuskey);
        ENG_LOG("phasecheck_sprd NativeService destory\n");
    }

    int execute_shell_cmd(char *cmd)
    {
        ENG_LOG("shell cmd is: %s",cmd);
        FILE *fstream = NULL;
        char buff[1024];
        memset(buff,0,sizeof(buff));
        if(NULL==(fstream=popen(cmd,"r")))
        {
            fprintf(stderr,"execute shell command failed: %s",strerror(errno));
            return 0;
        }

        while(NULL!=fgets(buff, sizeof(buff), fstream))
        {
            printf("execute shell cmd result is:%s",buff);
        }
        pclose(fstream);
        return 1;
    }

    char * get_sn1()
    {
        int readnum;
        unsigned int magic=0;
        FILE *fd = NULL;
        int len;
        char buf[PROPERTY_VALUE_MAX];
        len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
        char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
        ENG_LOG("phasecheck_sprd : phasecheck Path:%s\n", phasecheckPath);
        fd = fopen(phasecheckPath, "r");
        ENG_LOG("phasecheck_sprd huasong: %s open Ok phasecheckPath = point 1 \n", phasecheckPath);
        if(NULL != fd){
            readnum = fread(&magic, sizeof(unsigned int), 1, fd);
            fclose(fd);
            ENG_LOG("mmitest magic: 0x%x!;sizeof(phase_check_sp15)= %d, %d IN", magic, sizeof(phase_check_sp15), __LINE__);
        }else{
            ENG_LOG("fail to open miscdata! %d IN", __LINE__);
        }
        if(magic == SP09_SPPH_MAGIC_NUMBER || magic == SP05_SPPH_MAGIC_NUMBER){
            ENG_LOG("phasecheck_sprd: %s open Ok phasecheckPath point 1 \n",__FUNCTION__);
            int ifd = open(phasecheckPath, O_RDWR);
            if (ifd >= 0) {
                ENG_LOG("phasecheck_sprd: %s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                len = read(ifd,&Phase,sizeof(SP09_PHASE_CHECK_T));
                close(ifd);
                if (len <= 0){
                    ENG_LOG("phasecheck_sprd: %s read fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                    return NULL;
                }
            } else {
                ENG_LOG("phasecheck_sprd: %s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return NULL;
            }
            ENG_LOG("phasecheck_sprd SN1 %s'", Phase.SN1);
            ENG_LOG("phasecheck_sprd SN2 %s'", Phase.SN2);

            return Phase.SN1;
        }else if(magic == SP15_SPPH_MAGIC_NUMBER){
            fd = fopen(phasecheckPath, "r");
            if(NULL != fd){
                readnum = fread(&phase_check_sp15, sizeof(SP15_PHASE_CHECK_T), 1, fd);
                fclose(fd);
            }else{
                ENG_LOG("phasecheck_sprd: %s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return NULL;
            }
            return phase_check_sp15.SN1;
        }
        return NULL;
    }

    char * get_sn2()
    {
        int readnum;
        unsigned int magic=0;
        FILE *fd = NULL;
        int len;
        char buf[PROPERTY_VALUE_MAX];
        len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
        char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
        ENG_LOG("phasecheck_sprd : phasecheck Path:%s\n", phasecheckPath);
        fd = fopen(phasecheckPath, "r");
        if(NULL != fd){
            readnum = fread(&magic, sizeof(unsigned int), 1, fd);
            fclose(fd);
            ENG_LOG("mmitest magic: 0x%x!;sizeof(phase_check_sp15)= %d, %d IN", magic, sizeof(phase_check_sp15), __LINE__);
        }else{
            ENG_LOG("fail to open miscdata! %d IN", __LINE__);
        }
        if(magic == SP09_SPPH_MAGIC_NUMBER || magic == SP05_SPPH_MAGIC_NUMBER){
            ENG_LOG("phasecheck_sprd: %s open Ok phasecheckPath point 1 \n",__FUNCTION__);
            int ifd = open(phasecheckPath,O_RDWR);
            if (ifd >= 0) {
                ENG_LOG("phasecheck_sprd: %s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                len = read(ifd,&Phase,sizeof(SP09_PHASE_CHECK_T));
                close(ifd);
                if (len <= 0){
                    ENG_LOG("phasecheck_sprd: %s read fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                    return NULL;
                }
            } else {
                ENG_LOG("phasecheck_sprd: %s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return NULL;
            }
            ENG_LOG("phasecheck_sprd SN1 %s'", Phase.SN1);
            ENG_LOG("phasecheck_sprd SN2 %s'", Phase.SN2);

            return Phase.SN2;
        }else if(magic == SP15_SPPH_MAGIC_NUMBER){
            fd = fopen(phasecheckPath, "r");
            if(NULL != fd){
                readnum = fread(&phase_check_sp15, sizeof(SP15_PHASE_CHECK_T), 1, fd);
                fclose(fd);
            }else{
                ENG_LOG("phasecheck_sprd: %s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return NULL;
            }
            return phase_check_sp15.SN2;
        }
        return NULL;
    }
    char *str_cat(char *ret, const char *str1, const char *str2, bool first){
        int len1 = 0;
        int len2 = 0;
        for (len1 = 0; *(str1+len1) != '\0'; len1++){}
        for (len2 = 0; *(str2+len2) != '\0'; len2++){}
        ENG_LOG("phasecheck_sprd str1 len: %d'", len1);
        ENG_LOG("phasecheck_sprd str2 len: %d'", len2);
        int i;
        if(first) {
            for (i=0; i<len1; i++){
                *(ret+i) = *(str1+i);
            }
        }
        *(ret+len1) = '|';
        for (i=0; i<len2; i++){
            *(ret+len1+i+1) = *(str2+i);
        }
        *(ret+len1+len2+1) = '\0';

        return ret;
    }
    char* get_phasecheck(char *ret, int* testSign, int* item)
    {
        int readnum;
        unsigned int magic=0;
        FILE *fd = NULL;
        int len;
        char buf[PROPERTY_VALUE_MAX];
        len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
        char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
        ENG_LOG("phasecheck_sprd : phasecheck Path:%s\n", phasecheckPath);
        fd = fopen(phasecheckPath, "r");
        if(NULL != fd){
            readnum = fread(&magic, sizeof(unsigned int), 1, fd);
            fclose(fd);
            ENG_LOG("mmitest magic: 0x%x!;sizeof(phase_check_sp15)= %d, %d IN", magic, sizeof(phase_check_sp15), __LINE__);
        }else{
            ENG_LOG("fail to open miscdata! %d IN", __LINE__);
        }
        if(magic == SP09_SPPH_MAGIC_NUMBER || magic == SP05_SPPH_MAGIC_NUMBER){
            ENG_LOG("phasecheck_sprd: %s magic number in [SP09,SP05] \n",__FUNCTION__);
            int ifd = open(phasecheckPath, O_RDWR);
            if (ifd >= 0) {
                ENG_LOG("phasecheck_sprd: %s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                len = read(ifd,&Phase,sizeof(SP09_PHASE_CHECK_T));
                close(ifd);
                if (len <= 0){
                    ENG_LOG("phasecheck_sprd: %s read fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                    return NULL;
                }
            } else {
                ENG_LOG("phasecheck_sprd: %s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return NULL;
            }
            if(Phase.StationNum <= 0) return NULL;
            if(Phase.StationNum == 1) return Phase.StationName[0];
            unsigned int i = 0;
            while(i < Phase.StationNum) {
                ENG_LOG("phasecheck_sprd StationName[%d]: %s'",i, Phase.StationName[i]);
                if(i == 0) {
                    str_cat(ret, Phase.StationName[i], Phase.StationName[i+1], true);
                    ENG_LOG("phasecheck_sprd i = 0 result: %s'", ret);
                } else if(i != 1) {
                    str_cat(ret, ret, Phase.StationName[i], false);
                    ENG_LOG("phasecheck_sprd result: %s'", ret);
                }
                i++;
            }

            ENG_LOG("phasecheck_sprd iTestSign %x'", Phase.iTestSign);
            ENG_LOG("phasecheck_sprd iItem %x'", Phase.iItem);
            *testSign = (int)Phase.iTestSign;
            *item = (int)Phase.iItem;

        }else if(magic == SP15_SPPH_MAGIC_NUMBER){
            ENG_LOG("phasecheck_sprd: %s magic number is SP15 \n",__FUNCTION__);
            fd = fopen(phasecheckPath, "r");
            if(NULL != fd){
                readnum = fread(&phase_check_sp15, sizeof(SP15_PHASE_CHECK_T), 1, fd);
                fclose(fd);
            }else{
                ENG_LOG("phasecheck_sprd: %s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return NULL;
            }
            if(phase_check_sp15.StationNum <= 0) return NULL;
            if(phase_check_sp15.StationNum == 1) return phase_check_sp15.StationName[0];
            int i = 0;
            while(i < phase_check_sp15.StationNum) {
                ENG_LOG("phasecheck_sprd StationName[%d]: %s'",i, phase_check_sp15.StationName[i]);
                if(i == 0) {
                    str_cat(ret, phase_check_sp15.StationName[i], phase_check_sp15.StationName[i+1], true);
                    ENG_LOG("phasecheck_sprd i = 0 result: %s'", ret);
                } else if(i != 1) {
                    str_cat(ret, ret, phase_check_sp15.StationName[i], false);
                    ENG_LOG("phasecheck_sprd result: %s'", ret);
                }
                i++;
            }

            ENG_LOG("phasecheck_sprd iTestSign %x'", phase_check_sp15.iTestSign);
            ENG_LOG("phasecheck_sprd iItem %x'", phase_check_sp15.iItem);
            *testSign = (int)phase_check_sp15.iTestSign;
            *item = (int)phase_check_sp15.iItem;
        }

        return ret;
    }

    jint eng_writephasecheck(jint type, jint station, jint value)
    {
        int ret=0;
        int len;

        ENG_LOG("%s open Ok phasecheckPath point 1 \n",__FUNCTION__);
        char buf[PROPERTY_VALUE_MAX];
        len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
        char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
        ENG_LOG("phasecheck_sprd : phasecheck Path:%s\n", phasecheckPath);
        int fd = open(phasecheckPath, O_RDWR);
        if (fd >= 0)
        {
            ENG_LOG("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            len = read(fd,&Phase,sizeof(SP09_PHASE_CHECK_T));
            close(fd);
            if (len <= 0){
                ENG_LOG("%s read fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return -1;
            }
        } else {
            ENG_LOG("%s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            return -1;
        }

        fd = open(phasecheckPath, O_RDWR);
        if (fd >= 0){
            if(type == SIGN_TYPE)
            {
                if(value)
                {
                    Phase.iTestSign |= (unsigned short)(1<<station);
                } else {
                    Phase.iTestSign &= (unsigned short)(~(1<<station));
                }

                len = write(fd,&Phase,sizeof(SP09_PHASE_CHECK_T));
                fsync(fd);
                ENG_LOG("phasecheck_sprd iTestSign 0x%x'", Phase.iTestSign);
            }

            if(type == ITEM_TYPE)
            {
                if(value)
                {
                    Phase.iItem |= (unsigned short)(1<<station);
                } else {
                    Phase.iItem &= (unsigned short)(~(1<<station));
                }

                len = write(fd,&Phase,sizeof(SP09_PHASE_CHECK_T));
                fsync(fd);
                ENG_LOG("phasecheck_sprd iItem 0x%x'", Phase.iItem);
            }
            close(fd);
        }else{
            ENG_LOG("engphasecheck------open fail chmod 0600 \n");
            ret = -1;
        }
        return ret;
    }

    jint eng_writeOffset(int offset, int write_count, char* value)
    {
        int len;
        ENG_LOG("eng_writeOffset: offset = %d, write_count = %d", offset, write_count);
        char buf[PROPERTY_VALUE_MAX];
        len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
        char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
        ENG_LOG("phasecheck_sprd : phasecheck Path:%s\n", phasecheckPath);

        int fd = open(phasecheckPath,O_RDWR);
        if (fd >= 0)
        {
            ENG_LOG("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            int ret = lseek(fd, offset, SEEK_SET);
            if (ret < 0){
                close(fd);
                ENG_LOG("%s lseek fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return -1;
            }
            len = write(fd, value, write_count);
            ENG_LOG("write: %d", len);
            fsync(fd);
            close(fd);
            if (len <= 0){
                ENG_LOG("%s read fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return -1;
            }
        } else {
            ENG_LOG("%s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            return -1;
        }

        return len;
    }

    int eng_readOffset(int offset,int read_count, char *value)
    {
        int len = 0;
        ENG_LOG("eng_readOffset: offset = %d, read_count= %d", offset, read_count);
        char buf[PROPERTY_VALUE_MAX];
        len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
        char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
        int fd = open(phasecheckPath,O_RDWR);
        if (fd >= 0)
        {
            ENG_LOG("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            //lseek(fd, offset, SEEK_SET);
            int ret = lseek(fd, offset, SEEK_SET);
            if (ret < 0){
                close(fd);
                ENG_LOG("%s lseek fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return -1;
            }
            len = read(fd, value, read_count);
            ENG_LOG("read: %d", len);
            close(fd);
            if (len <= 0){
                ENG_LOG("%s read fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                return -1;
            }
        } else {
            ENG_LOG("%s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            return -1;
        }
        return len;
    }

    /* SPRD Bug 793108: Add kernel log level switch. @{ */
    char *eng_readkernelloglevelstate()
    {
        char ret[32];
        int len;

        ALOGE("%s open Ok phasecheckPath point 1 \n",__FUNCTION__);
        char buf[PROPERTY_VALUE_MAX];
        len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
        char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
        ENG_LOG("phasecheck_sprd : phasecheck Path:%s\n", phasecheckPath);
        int fd = open(phasecheckPath, O_RDWR);

        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            lseek(fd, 1024*9, SEEK_SET);

            int r = read(fd, ret, 32);
            ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
            if(r < 0) {
                ALOGE("%s read Kernel Log Level state failed \n",__FUNCTION__);
                close(fd);
                return NULL;
            }
            close(fd);
            return ret;
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            return NULL;
        }
    }

    jint eng_writekernelloglevelstate(int state)
    {
        int ret=-1;
        int len;
        char *enabledstr = "enable";
        char *disabledstr = "disable";
        ALOGE("%s open Ok phasecheckPath point 1 \n",__FUNCTION__);
        char buf[PROPERTY_VALUE_MAX];
        len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
        char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
        ENG_LOG("phasecheck_sprd : phasecheck Path:%s\n", phasecheckPath);
        int fd = open(phasecheckPath, O_RDWR);
        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            lseek(fd, 1024*9, SEEK_SET);
            ALOGE("set kernel log level state value is : %d\n", state);
            if(state==1){
                ret=write(fd, enabledstr, strlen(enabledstr)+1);
            }else{
                ret=write(fd, disabledstr, strlen(disabledstr)+1);
            }
            if(ret < 0) {
                ALOGE("%s set Kernel Log Level state failed \n",__FUNCTION__);
                close(fd);
                return 0;
            }
            close(fd);
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
            return -1;
        }
        return 1;
    }
    /* @} */

    jint enable_charge(void)
    {
        int fd;
        int ret = -1;
        if (access(CHARGE_SWITCH_FILE_k414, F_OK) == 0) {
            fd = open(CHARGE_SWITCH_FILE_k414, O_WRONLY);
            ENG_LOG("open %s ,fd=%d",CHARGE_SWITCH_FILE_k414,fd);
        }else{
            fd = open(CHARGE_SWITCH_FILE, O_WRONLY);
            ENG_LOG("open %s ,fd=%d",CHARGE_SWITCH_FILE,fd);
        }
        if(fd >= 0)
        {
            ret = write(fd, "0", 2);
            if (ret < 0)
            {
                close(fd);
                return 0;
            }
            close(fd);
        }
        else
        {
            return 0;
        }
        return 1;
    }

    jint disable_charge(void)
    {
        int fd;
        int ret = -1;
        if (access(CHARGE_SWITCH_FILE_k414, F_OK) == 0) {
            fd = open(CHARGE_SWITCH_FILE_k414, O_WRONLY);
            ENG_LOG("open %s ,fd=%d",CHARGE_SWITCH_FILE_k414,fd);
        }else{
            fd = open(CHARGE_SWITCH_FILE, O_WRONLY);
            ENG_LOG("open %s ,fd=%d",CHARGE_SWITCH_FILE,fd);
        }
        if(fd >= 0)
        {
            ret = write(fd, "1", 2);
            if(ret < 0)
            {
                close(fd);
                return 0;
            }
            close(fd);
        }
        else
        {
           return 0;
        }
        return 1;
    }

    char * get_jeita()
    {
        static char ret[PROPERTY_VALUE_MAX + 1];
        int fd = open(JEITA_CONTROL_FILE, O_RDONLY);

        if (fd < 0) return NULL;

        ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , JEITA_CONTROL_FILE);
        memset(ret, 0, sizeof(ret));
        int r = read(fd, ret, PROPERTY_VALUE_MAX);

        ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
        if(r < 0)
        {
            ALOGE("%s get_jeita state failed \n",__FUNCTION__);
            close(fd);
            return NULL;
        }
        ret[r] = '\0';
        close(fd);
        return ret;
    }

    jint set_jeita(jint value)
    {
        int fd;
        int ret = -1;
        char out;
        fd = open(JEITA_CONTROL_FILE, O_WRONLY);

        if (fd < 0) return 0;

        if (value == 0) {
            out = '0';
        } else if (value == 1) {
            out = '1';
        } else {
            out = '0';
        }
        ret = write(fd, &out, 1);
        ENG_LOG("phasecheck_sprd set_jeita ret: %d", ret);
        if(ret < 0){
            close(fd);
            return 0;
        }
        close(fd);
        return 1;
    }

    /* SPRD 940291 - [true demo version] Control charge region @{ */
    char * get_charge_status()
    {
        static char ret[50];
        int len;

        char buf[PROPERTY_VALUE_MAX];
        int fd = open(CHARGE_STATUS_FILE, O_RDONLY);

        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , CHARGE_STATUS_FILE);
            memset(ret, 0, sizeof(ret));
            int r = read(fd, ret, 50);
            ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
            if(r < 0) {
                ALOGE("%s read get_charge_status state failed \n",__FUNCTION__);
                close(fd);
                return NULL;
            }
            close(fd);
            return ret;
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , CHARGE_STATUS_FILE);
            return NULL;
        }
    }

    char * get_charge_level()
    {
        static char ret[50];
        int len;

        char buf[PROPERTY_VALUE_MAX];
        int fd = open(CHARGE_LEVEL_FILE, O_RDONLY);

        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , CHARGE_LEVEL_FILE);
            memset(ret, 0, sizeof(ret));
            int r = read(fd, ret, 50);
            ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
            if(r < 0) {
                ALOGE("%s read get_charge_level state failed \n",__FUNCTION__);
                close(fd);
                return NULL;
            }
            close(fd);
            return ret;
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , CHARGE_LEVEL_FILE);
            return NULL;
        }
    }
    /* }@ */

    jint set_mipi(jint value)
    {
        int fd;
        int ret = -1;
        char out;
        fd = open(MIPI_SWITCH_FILE, O_WRONLY);
        if(fd >= 0){
            if (value == 0) {
                out = '0';
            } else if (value == 1) {
                out = '1';
            } else if (value == 2) {
                out = '2';
            } else {
                out = '0';
            }
            ret = write(fd, &out, 1);
            ENG_LOG("phasecheck_sprd set_mipi ret: %d", ret);
            if(ret < 0){
                close(fd);
                return 0;
            }
            close(fd);
        }else{
            ENG_LOG("phasecheck_sprd set_mipi open failed");
            return 0;
        }
        return 1;
    }

    /* SPRD 815541 - Coulometer Power Test @{ */
    jint set_cc_cmd(jint value)
    {
        int fd;
        int ret = -1;
        char out;
        fd = open(CC_TEST_CMD_FILE, O_WRONLY);
        if(fd >= 0){
            if (value == 0) {
               out = '0';
            } else if (value == 1) {
               out = '1';
            } else {
               out = '0';
            }
            ret = write(fd, &out, 1);
            ENG_LOG("phasecheck_sprd set_cc_cmd ret: %d", ret);
            if(ret < 0){
                close(fd);
                return 0;
            }
            close(fd);
        }else{
            ENG_LOG("phasecheck_sprd set_cc_cmd open failed");
            return 0;
        }
        return 1;
    }

    char * get_cc_test_result()
    {
        static char ret[50];
        int len;

        char buf[PROPERTY_VALUE_MAX];
        int fd = open(CC_TEST_RESULT_FILE, O_RDONLY);

        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , CC_TEST_RESULT_FILE);
            memset(ret, 0, sizeof(ret));
            int r = read(fd, ret, 50);
            ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
            if(r < 0) {
                ALOGE("%s read Kernel Log Level state failed \n",__FUNCTION__);
                close(fd);
                return NULL;
            }
            close(fd);
            return ret;
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , CC_TEST_RESULT_FILE);
            return NULL;
        }
    }

    char * get_cc_vol_result()
    {
        static char ret[50];
        int len;

        char buf[PROPERTY_VALUE_MAX];
        int fd = open(CC_TEST_VOL_FILE, O_RDONLY);

        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , CC_TEST_VOL_FILE);
            memset(ret, 0, sizeof(ret));
            int r = read(fd, ret, 50);
            ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
            if(r < 0) {
                ALOGE("%s read Kernel Log Level state failed \n",__FUNCTION__);
                close(fd);
                return NULL;
            }
            close(fd);
            return ret;
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , CC_TEST_VOL_FILE);
            return NULL;
        }
    }
    /* }@ */

    /* SPRD 1016593 - Coulometer Power Test for kernel 4.14 @{ */
    char * get_cc_energy_new_kernel()
    {
        static char ret[50];
        int len;

        char buf[PROPERTY_VALUE_MAX];
        int fd = open(CC_TEST_ENERGY_FILE, O_RDONLY);

        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , CC_TEST_ENERGY_FILE);
            memset(ret, 0, sizeof(ret));
            int r = read(fd, ret, 50);
            ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
            if(r < 0) {
                ALOGE("%s read Kernel Log Level state failed \n",__FUNCTION__);
                close(fd);
                return NULL;
            }
            close(fd);
            return ret;
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , CC_TEST_ENERGY_FILE);
            return NULL;
        }
    }

    char * get_cpu_debug_mode()
    {
        static char ret[50];
        int len;

        char buf[PROPERTY_VALUE_MAX];
        int fd = open(CPU_DEBUG_PATH, O_RDONLY);

        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , CPU_DEBUG_PATH);
            memset(ret, 0, sizeof(ret));
            int r = read(fd, ret, 50);
            ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
            if(r < 0) {
                ALOGE("%s read Cpu Debug state failed \n",__FUNCTION__);
                close(fd);
                return NULL;
            }
            close(fd);
            return ret;
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , CPU_DEBUG_PATH);
            return NULL;
        }
    }

    jint set_cpu_debug_mode(jint value)
    {
        int fd;
        int ret = -1;
        char out;
        fd = open(CPU_DEBUG_PATH, O_WRONLY);
        if (fd >= 0)
        {
            out = (value == 1) ? '1' : '0';
            ret = write(fd, &out, 1);
            ENG_LOG("phasecheck_sprd set_cpu_debug_mode ret: %d", ret);
            if (ret < 0)
            {
                close(fd);
                return 0;
            }
            close(fd);
        }
        else
        {
            ENG_LOG("phasecheck_sprd set_cpu_debug_mode open failed");
            return 0;
        }
        return 1;
    }

    char * get_cc_vol_new_kernel()
    {
        static char ret[50];
        int len;

        char buf[PROPERTY_VALUE_MAX];
        int fd = open(CC_TEST_VOLTAGE_FILE, O_RDONLY);

        if (fd >= 0)
        {
            ALOGE("%s open Ok phasecheckPath = %s \n",__FUNCTION__ , CC_TEST_VOLTAGE_FILE);
            memset(ret, 0, sizeof(ret));
            int r = read(fd, ret, 50);
            ALOGE("read value ret is : %s, r value is : %d\n", ret, r);
            if(r < 0) {
                ALOGE("%s read Kernel Log Level state failed \n",__FUNCTION__);
                close(fd);
                return NULL;
            }
            close(fd);
            return ret;
        } else {
            ALOGE("%s open fail phasecheckPath = %s \n",__FUNCTION__ , CC_TEST_VOLTAGE_FILE);
            return NULL;
        }
    }
    /* }@ */

    /* SPRD 911107 - write /sys/class/misc/sprd_7sreset/hard_mode no effective @{ */
    jint set_reset_mode(jint value)
    {
        int fd;
        int ret = -1;
        char out;
        fd = open(RESET_MODE_FILE, O_WRONLY);
        if(fd >= 0){
            if (value == 0) {
                out = '0';
            } else if (value == 1) {
                out = '1';
            } else {
                out = '0';
            }
            ret = write(fd, &out, 1);
            ENG_LOG("phasecheck_sprd set_reset_mode ret: %d", ret);
            if(ret < 0){
                close(fd);
                return 0;
            }
            close(fd);
        }else{
            ENG_LOG("phasecheck_sprd set_reset_mode open failed");
            return 0;
        }
        return 1;
    }
    /* }@ */

    jint set_cabc_mode(jint value)
    {
        int fd;
        int ret = -1;
        char out;
        fd = open(CABC_PATH, O_WRONLY);
        if (fd >= 0)
        {
            out = (value == 1) ? '1' : '0';
            ret = write(fd, &out, 1);
            ENG_LOG("phasecheck_sprd set_cabc_mode ret: %d", ret);
            if (ret < 0)
            {
                close(fd);
                return 0;
            }
            close(fd);
        }
        else
        {
            ENG_LOG("phasecheck_sprd set_cabc_mode open failed");
            return 0;
        }
        return 1;
    }

    void write_ledlight_node_values(const char * pathname, jint value) {
        int len;
        char out;
        int fd = open(pathname, O_RDWR);
        if (fd >= 0) {
            if(value != 2){
               out = value == 0 ? '0' : '1';
            }else{
               out = '2';
            }
            len = write(fd, &out, 1);
            close(fd);
        } else {
            ENG_LOG("engphasecheck------write_ledlight_node_values open fail chmod 0600 \n");
        }
    }
    //xinfang.zhang gnss test
    int set_backlight(int value)
    {     
        int ret = -1;
        char val[11]={0};
        int fd = open(GNSS_CTRL_BACKLIGHT,O_WRONLY);
        if(fd >= 0){
            snprintf(val,sizeof(val)-1, "%d" , value);
            ret = write(fd,val,strlen(val));
            close(fd);
        }
        ENG_LOG("set_backlight fd=%d,ret=%d \n",fd,ret);
        return (ret <= 0)?0:1;
    }
    char * get_backlight()
    {
        static char val[11]={0};
        int ret = -1;
        int fd = open(GNSS_CTRL_BACKLIGHT, O_RDONLY);
        if (fd >= 0)
        {
            memset(val, 0, sizeof(val));
            ret = read(fd, val, 10);
            close(fd);
        } 
        ENG_LOG("get_backlight fd=%d,ret=%d \n",fd,ret);
        return (ret<=0)?NULL:val;
    }

    void write_ledlight_values(const char * pathname, jint value) {
        int len;
        char out[64];
        snprintf(out, sizeof(out),  "%d", value);
        int fd = open(pathname, O_RDWR);
        ENG_LOG("write_ledlight_values value=%d \n",value);
        ENG_LOG("write_ledlight_values pathname=%s,out=%s \n",pathname,out);
        if(fd < 0){
          ENG_LOG("write_ledlight_values %s faild! \n",pathname);
          return ;
        }
        len = write(fd, out, strlen(out));
        close(fd);
    }
    int NativeService::onTransact(uint32_t code,
                                   Parcel& data,
                                   Parcel* reply,
                                   uint32_t flags)
    {
        ENG_LOG("phasecheck_sprd nativeservice onTransact code:%d",code);
        switch(code)
        {
        case TYPE_EXECUTE_OFFSET_CALIBRATION:
            {
                char *offset_calibration_cmd = "/vendor/bin/vl53l0_test -o";
                int ret = execute_shell_cmd(offset_calibration_cmd);
                ALOGE("execute offset_calibration_cmd result is %d",ret);
                reply->writeInt32(ret);
                return NO_ERROR;
            }
        case TYPE_EXECUTE_XTALK_CALIBRATION:
            {
                char *xtalk_calibration_cmd = "/vendor/bin/vl53l0_test -c";
                int ret = execute_shell_cmd(xtalk_calibration_cmd);
                ALOGE("execute xtalk_calibration_cmd result is %d",ret);
                reply->writeInt32(ret);
                return NO_ERROR;
            }
        case TYPE_GET_SN1:
            {
                //get sn1
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice get sn count %d",i++);
                char * sn1 = get_sn1();
                ENG_LOG("phasecheck_sprd nativeservice sn1 read is %s",sn1);
                if(sn1 != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice sn1!");
                    reply->write(sn1, strlen(sn1)+1);
                }
                return NO_ERROR;
            }
        case TYPE_GET_SN2:
            {
                //get sn2
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice get sn count %d",i++);
                char * sn2 = get_sn2();
                ENG_LOG("phasecheck_sprd nativeservice sn2 read is %s", sn2);
                if(sn2 != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice sn2!");
                    reply->write(sn2, strlen(sn2)+1);
                }
                return NO_ERROR;
            }
        case TYPE_WRITE_TESTED:
            {
                //write station tested
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice write station tested count %d",i++);
                //int station = data.readInt32();
                //data.readInt32();
                int station = data.readInt32();
                eng_writephasecheck(0, station, 0);
                return NO_ERROR;
            }
        case TYPE_WRITE_PASS:
            {
                //write station pass
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice write station pass count %d",i++);
                //int station = data.readInt32();
                //data.readInt32();
                int station = data.readInt32();
                eng_writephasecheck(1, station, 0);
                return NO_ERROR;
            }
        case TYPE_WRITE_FAIL:
            {
                //write station fail
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice write station count %d",i++);
                //int station = data.readInt32();
                //data.readInt32();
                int station = data.readInt32();
                eng_writephasecheck(1, station, 1);
                return NO_ERROR;
            }
        case TYPE_GET_STATION:
            {
                //get phasecheck
                static int i = 1;
                ENG_LOG("TYPE_GET_STATION nativeservice get phasecheck count %d",i++);
                int testSign, item;
                char *ret;
                //Check magic
                char buf[PROPERTY_VALUE_MAX];
                int len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
                char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
                ENG_LOG("TYPE_GET_STATION : phasecheck Path:%s,len=%d\n", phasecheckPath,len);
                unsigned int magic = SP09_SPPH_MAGIC_NUMBER;
                FILE *fb = NULL;
                int readnum;
                fb = fopen(phasecheckPath, "r");
                if(NULL != fb){
                    readnum = fread(&magic, sizeof(unsigned int), 1, fb);
                    fclose(fb);
                }else{
                    ENG_LOG("fail to open miscdata! %d IN", __LINE__);
                }
                ENG_LOG("TYPE_GET_STATION magic: 0x%x\n",magic);
                if(magic == SP09_SPPH_MAGIC_NUMBER || magic == SP05_SPPH_MAGIC_NUMBER){
                    ret = (char *)malloc(sizeof(char)*(SP09_MAX_STATION_NUM * SP09_MAX_STATION_NAME_LEN));
                }else if(magic == SP15_SPPH_MAGIC_NUMBER){
                    ret = (char *)malloc(sizeof(char)*(SP15_MAX_STATION_NUM * SP15_MAX_STATION_NAME_LEN));
                }else {
                    ret = (char *)malloc(sizeof(char)*(SP09_MAX_STATION_NUM * SP09_MAX_STATION_NAME_LEN));
                }

                //char *ret = (char *)malloc(sizeof(char)*(SP09_MAX_STATION_NUM*SP09_MAX_STATION_NAME_LEN));
                get_phasecheck(ret, &testSign, &item);
                ENG_LOG("phasecheck_sprd nativeservice get phasecheck is %s", ret);
                ENG_LOG("phasecheck_sprd nativeservice get phasecheck station is %x, %x", testSign, item);
                reply->writeInt32(testSign);
                reply->writeInt32(item);

                if(magic == SP15_SPPH_MAGIC_NUMBER){
                    reply->write(ret, sizeof(char)*(SP15_MAX_STATION_NUM * SP15_MAX_STATION_NAME_LEN));
                }else {
                    reply->write(ret, sizeof(char)*(SP09_MAX_STATION_NUM * SP09_MAX_STATION_NAME_LEN));
                }
                free(ret);
                return NO_ERROR;
            }
        case TYPE_CHARGE_SWITCH:
            {
                //write to charge switch
                static int i = 1;
                //int value = data.readInt32();
                //data.readInt32();
                int value = data.readInt32();
                ENG_LOG("phasecheck_sprd nativeservice write TYPE_CHARGE_SWITCH count %d. value: %d",i++,value);
                static int result = -1;
                if(value == 0){
                     result = enable_charge();
                }else if(value == 1){
                     result = disable_charge();
                }
                if(result == 0){
                    char *res = "fail write charge node !";
                    reply->write(res, (strlen(res)+1));
                }else if(result == 1){
                    char *res = "success write charge node !";
                    reply->write(res, (strlen(res)+1));
                }
                return NO_ERROR;
             }
        /* SPRD 940291 - [true demo version] Control charge region @{ */
        case TYPE_READ_CHARGE_STATUS:
            {
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice get chargeStatus count %d", i++);
                char * chargeStatus = get_charge_status();
                ENG_LOG("phasecheck_sprd nativeservice chargeStatus read is %s", chargeStatus);
                if(chargeStatus != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice chargeStatus!");
                    reply->write(chargeStatus, strlen(chargeStatus));
                }
                return NO_ERROR;
            }
        case TYPE_READ_CHARGE_LEVEL:
            {
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice get chargeLevel count %d", i++);
                char * chargeLevel = get_charge_level();
                ENG_LOG("phasecheck_sprd nativeservice chargeLevel read is %s", chargeLevel);
                if(chargeLevel != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice chargeLevel!");
                    reply->write(chargeLevel, strlen(chargeLevel));
                }
                return NO_ERROR;
            }
            /* }@ */
        case TYPE_MIPI_SWITCH:
            {
                //write to charge switch
                static int i = 1;
                //int value = data.readInt32();
                //data.readInt32();
                int value = data.readInt32();
                ENG_LOG("~~~~~ phasecheck_sprd nativeservice write TYPE_MIPI_SWITCH count %d. value: %d", i++, value);
                static int result = -1;
                result = set_mipi(value);
                ENG_LOG("phasecheck_sprd nativeservice set_mipi result %d", result);
                if(result == 0){
                    char *res = "fail write mipi node !";
                    reply->write(res, (strlen(res)+1));
                }else if(result == 1){
                    char *res = "success write mipi node !";
                    reply->write(res, (strlen(res)+1));
                }
                return NO_ERROR;
            }
        /* SPRD 815541 - Coulometer Power Test @{ */
        case TYPE_SET_CC_TEST_CMD:
            {
                //write to charge switch
                static int i = 1;
                //int value = data.readInt32();
                //data.readInt32();
                int value = data.readInt32();
                ENG_LOG("~~~~~ phasecheck_sprd nativeservice write TYPE_CC_TEST_CMD count %d. value: %d", i++, value);
                static int result = -1;
                result = set_cc_cmd(value);
                ENG_LOG("phasecheck_sprd nativeservice set_cc_cmd result %d", result);
                if(result == 0){
                    char *res = "fail write CC_CMD node!";
                    reply->write(res, (strlen(res) + 1));
                }else if(result == 1){
                    char *res = "success write CC_CMD node!";
                    reply->write(res, (strlen(res) + 1));
                }
                return NO_ERROR;
            }
        case TYPE_GET_CC_TEST_RESULT:
            {
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice get ccResult count %d", i++);
                char * ccResult = get_cc_test_result();
                ENG_LOG("phasecheck_sprd nativeservice ccResult read is %s", ccResult);
                if(ccResult != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice ccResult!");
                    reply->write(ccResult, strlen(ccResult));
                }
                return NO_ERROR;
            }
        case TYPE_GET_CC_TEST_VOL:
            {
                static int i = 1;
                ENG_LOG("phasecheck_sprd nativeservice get ccVol count %d", i++);
                char * ccVol = get_cc_vol_result();
                ENG_LOG("phasecheck_sprd nativeservice ccVol read is %s", ccVol);
                if(ccVol != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice ccVol!");
                    reply->write(ccVol, strlen(ccVol));
                }
                return NO_ERROR;
            }
            /* }@ */
        /* SPRD 1016593 : Coulometer Power Test Mode for kernel 4.14 @{ */
        case TYPE_GET_CC_ENERGY_NEW_KERNEL:
            {
                static int i = 1;
                char * ccEnergy = get_cc_energy_new_kernel();
                ENG_LOG("phasecheck_sprd nativeservice ccEnergy read is %s", ccEnergy);
                if(ccEnergy != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice ccEnergy!");
                    reply->write(ccEnergy, strlen(ccEnergy));
                }
                return NO_ERROR;
            }
        case TYPE_GET_CC_VOL_NEW_KERNEL:
            {
                static int i = 1;
                char * mCcVol = get_cc_vol_new_kernel();
                ENG_LOG("phasecheck_sprd nativeservice mCcVol read is %s", mCcVol);
                if(mCcVol != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice mCcVol!");
                    reply->write(mCcVol, strlen(mCcVol));
                }
                return NO_ERROR;
            }
            /* }@ */
        case TYPE_GET_CPU_DEBUG_MODE:
            {
                static int i = 1;
                char * mCpu = get_cpu_debug_mode();
                ENG_LOG("phasecheck_sprd nativeservice mMode read is %s", mCpu);
                if(mCpu != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice mCpu!");
                    reply->write(mCpu, strlen(mCpu));
                }
                return NO_ERROR;
            }
        case TYPE_RED_LED_NODE:
            {
                //write values to leds system node.
                static int i = 1;
                //int value = data.readInt32();
                //data.readInt32();
                int value = data.readInt32();
                ENG_LOG("phasecheck_sprd nativeservice write TYPE_RED_LED_NODE count %d. value: %d", i++, value);
                if (value == 1) {
                     write_ledlight_node_values("/sys/class/leds/red_bl/high_time", 2);
                     write_ledlight_node_values("/sys/class/leds/red_bl/low_time", 2);
                     write_ledlight_node_values("/sys/class/leds/red_bl/rising_time", 2);
                     write_ledlight_node_values("/sys/class/leds/red_bl/falling_time",2);
                }
                write_ledlight_node_values("/sys/class/leds/red_bl/on_off", value);
                //Support sharkl5
                if (access(LED_RED_PATH, F_OK) == 0) {
                    write_ledlight_values(LED_RED_PATH,value != 0 ? 255 : 0);
                }else{
                    ENG_LOG("access %s FAILED\n", LED_RED_PATH);
                }
                reply->write("OK", 3);
                return NO_ERROR;
            }
        /* SPRD 911107 - write /sys/class/misc/sprd_7sreset/hard_mode no effective @{ */
        case TYPE_SET_RESET_MODE:
            {
                static int i = 1;
                int value = data.readInt32();
                ENG_LOG("phasecheck_sprd nativeservice write TYPE_SET_RESET_MODE count %d. value: %d", i++, value);
                static int result = -1;
                result = set_reset_mode(value);
                ENG_LOG("phasecheck_sprd nativeservice set_reset_mode result %d", result);
                if(result == 0){
                    char *res = "fail write reset mode node !";
                    reply->write(res, (strlen(res)+1));
                }else if(result == 1){
                    char *res = "success write reset mode node !";
                    reply->write(res, (strlen(res)+1));
                }
                return NO_ERROR;
            }
        /* }@ */
        case TYPE_SET_CABC_MODE:
            {
                int value = data.readInt32();
                int result = set_cabc_mode(value);
                ENG_LOG("phasecheck_sprd nativeservice set_cabc_mode result %d", result);
                reply->writeInt32(result);
                return NO_ERROR;
            }
        /* }@ */
        case TYPE_SET_CPU_DEBUG_MODE:
            {
                int value = data.readInt32();
                int result = set_cpu_debug_mode(value);
                ENG_LOG("phasecheck_sprd nativeservice set_cpu_debug_mode result %d", result);
                reply->writeInt32(result);
                return NO_ERROR;
            }
        case TYPE_BLUE_LED_NODE:
            {
                //write values to leds system node.
                static int i = 1;
                //int value = data.readInt32();
                //data.readInt32();
                int value = data.readInt32();
                ENG_LOG("phasecheck_sprd nativeservice write TYPE_BLUE_LED_NODE count %d. value: %d", i++, value);
                if (value == 1) {
                     write_ledlight_node_values("/sys/class/leds/blue_bl/high_time", 2);
                     write_ledlight_node_values("/sys/class/leds/blue_bl/low_time", 2);
                     write_ledlight_node_values("/sys/class/leds/blue_bl/rising_time",2);
                     write_ledlight_node_values("/sys/class/leds/blue_bl/falling_time",2);
                }
                write_ledlight_node_values("/sys/class/leds/blue_bl/on_off", value);
                //Support sharkl5
                if (access(LED_BLUE_PATH, F_OK) == 0) {
                    write_ledlight_values(LED_BLUE_PATH,value != 0 ? 255 : 0);
                }else{
                    ENG_LOG("access %s FAILED\n", LED_BLUE_PATH);
                }
                reply->write("OK", 3);
                return NO_ERROR;
            }
        case TYPE_GREEN_LED_NODE:
            {
                //write values to leds system node.
                static int i = 1;
                //int value = data.readInt32();
                //data.readInt32();
                int value = data.readInt32();
                ENG_LOG("phasecheck_sprd nativeservice write TYPE_GREEN_LED_NODE count %d. value: %d", i++, value);
                if (value == 1) {
                     write_ledlight_node_values("/sys/class/leds/green_bl/high_time", 2);
                     write_ledlight_node_values("/sys/class/leds/green_bl/low_time", 2);
                     write_ledlight_node_values("/sys/class/leds/green_bl/rising_time",2);
                     write_ledlight_node_values("/sys/class/leds/green_bl/falling_time",2);
                }
                write_ledlight_node_values("/sys/class/leds/green_bl/on_off", value);
                //Support sharkl5
                if (access(LED_GREEN_PATH, F_OK) == 0) {
                    write_ledlight_values(LED_GREEN_PATH,value != 0 ? 255 : 0);
                }else{
                    ENG_LOG("access %s FAILED\n", LED_GREEN_PATH);
                }
                reply->write("OK", 3);
                return NO_ERROR;
            }
        case TYPE_PHASECHECK:
            {
                int len;
                char buf[PROPERTY_VALUE_MAX];
                len = property_get(PHASE_CHECK_MISCDATA_PATH, buf, "");
                char* phasecheckPath = strcat(buf, PHASE_CHECK_MISCDATA);
                ENG_LOG("phasecheck_sprd : phasecheck Path:%s\n", phasecheckPath);
                int fd = open(phasecheckPath, O_RDWR);
                if (fd >= 0)
                {
                    ENG_LOG("phasecheck_sprd: %s open Ok phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                    len = read(fd,&Phase,sizeof(SP09_PHASE_CHECK_T));
                    close(fd);
                    if (len <= 0){
                        ENG_LOG("phasecheck_sprd: %s read fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                        Phase.Magic = 0;
                    }
                } else {
                    ENG_LOG("phasecheck_sprd: %s open fail phasecheckPath = %s \n",__FUNCTION__ , phasecheckPath);
                    Phase.Magic = 0;
                }
                reply->write((char *)&Phase, sizeof(SP09_PHASE_CHECK_T));
                return NO_ERROR;
             }
        /* SPRD Bug 793108: Add kernel log level switch. @{ */
        case TYPE_GET_KERNEL_LOG_LEVEL_STATE:
            {
                static int i = 1;
                ALOGE("phasecheck_sprd nativeservice get kernel log level state count %d",i++);
                char *loglevelstate = eng_readkernelloglevelstate();
                int result = -1;
                char *source_enabled = "enable";
                char *source_disabled = "disable";

                if(NULL == loglevelstate){
                    result = -1;
                } else {
                    if(strcmp(loglevelstate, source_enabled) == 0) {
                        result = 1;
                    } else if(strcmp(loglevelstate, source_disabled) == 0) {
                        result = 0;
                    } else {
                        result = -1;
                    }
                }
                reply->writeInt32(result);
                return NO_ERROR;
            }
        case TYPE_SET_KERNEL_LOG_LEVEL_STATE:
            {
                static int i = 1;
                ALOGE("phasecheck_sprd nativeservice set kernel log level state count %d",i++);
                //data.readInt32();
                int value = data.readInt32();
                int rt = eng_writekernelloglevelstate(value);
                if(rt == 1){
                    reply->writeInt32(1);
                } else{
                    reply->writeInt32(0);
                }
                return NO_ERROR;
            }
        /* @} */
        case TYPE_WIRTE_OFFTE:
            {
                //write offset value
                jint offset = data.readInt32();
                jint write_count = data.readInt32();
                char *value = (char *)malloc(write_count);
                data.read(value,write_count);
                ALOGD("phasecheck_sprd nativeservice write offset=%d,write_count=%d",offset, write_count);
                int ret = eng_writeOffset(offset, write_count, value);
                ALOGD("phasecheck_sprd nativeservice write ret=%d",ret);
                free(value);
                return NO_ERROR;
            }
        case TYPE_READ_OFFTE:
            {
                //read byte array with offset
                jint offset = data.readInt32();
                jint read_count = data.readInt32();
                char *value = (char *)malloc(read_count);
                ALOGD("phasecheck_sprd nativeservice read offset=%d,read_count=%d",offset,read_count);
                int ret = eng_readOffset(offset, read_count, value);
                ALOGD("phasecheck_sprd nativeservice read offset is 0x%x, ret = %d", value, ret);
                reply->write(value, ret);
                free(value);
                return NO_ERROR;
            }
        case TYPE_SET_BACKLIGHT:
            {
                int value = data.readInt32();
                int result = -1;
                result = set_backlight(value);
                if(result == 0){
                    char *res = "fail set backlight !";
                    reply->write(res, (strlen(res)+1));
                }else if(result == 1){
                    char *res = "success set backlight !";
                    reply->write(res, (strlen(res)+1));
                }
                return NO_ERROR;
            }
        case TYPE_GET_BACKLIGHT:
            {
                char * value = get_backlight();
                if(value != NULL){
                    reply->write(value, strlen(value));
                }
                return NO_ERROR;
            }
        case TYPE_GET_JEITA:
            {
                char * jeitaValue = get_jeita();
                ENG_LOG("phasecheck_sprd nativeservice jeitaValue read is %s", jeitaValue);
                if(jeitaValue != NULL){
                    ENG_LOG("phasecheck_sprd nativeservice jeitaValue!");
                    reply->write(jeitaValue, strlen(jeitaValue));
                }
                return NO_ERROR;
            }
        case TYPE_SET_JEITA:
            {
                int value = data.readInt32();
                ENG_LOG("phasecheck_sprd nativeservice write TYPE_JEITA_SWITCH value: %d", value);
                int result = set_jeita(value);
                ENG_LOG("phasecheck_sprd nativeservice set_jeita result %d", result);
                if(result == 0)
                {
                    char *res = "fail";
                    reply->write(res, (strlen(res)+1));
                }
                else if(result == 1)
                {
                    char *res = "success";
                    reply->write(res, (strlen(res)+1));
                }
                return NO_ERROR;
            }
        default:
#ifdef CHANNEL_SOCKET
            return 0;
#else
            return BBinder::onTransact(code, data, reply, flags);
#endif
        }
    }

    Parcel::Parcel() {
        memset(mData, 0, sizeof(mData));
        mPos = 0;
        mDataSize = 0;
    }

    void Parcel::setData(char *b, int len) {
        if (len <= 0) return ;
        ENG_LOG("Parcel::setData len =  %d", len);
        if(len > MAX_BUF_LEN || len < 0) return;
        //writeInt32(len);
        memcpy(mData+mPos, b, len);
        mPos += len;
        mDataSize += len;
    }

    void Parcel::write(char *b, int len) {
        if (len <= 0) return ;
        ENG_LOG("Parcel::write len =  %d", len);
        if(len > MAX_BUF_LEN || len < 0) return;
        writeInt32(len);
        memcpy(mData+mPos, b, len);
        mPos += len;
        mDataSize += len;
    }

    void Parcel::read(char *b, int len) {
        if (len <= 0) return ;
        if(len > MAX_BUF_LEN || len < 0) return;
        memcpy(b, mData+mPos, len);
        mPos += len;
    }

    int Parcel::dataSize() {
        return mDataSize;
    }

    void Parcel::writeInt32(int i) {
        ENG_LOG("Parcel::writeInt32 i =  %d", i);
        *((int *)(mData+mPos)) = i;
        mPos += 4;
        mDataSize += 4;
    }

    int Parcel::readInt32() {
        int value =  *(int *)(mData+mPos);
        mPos += 4;
        return value;
    }

    jbyte Parcel::readByte() {
        jbyte value =  mData[mPos];
        mPos += 1;
        return value;
    }

    void Parcel::writeByte(jbyte value) {
        ENG_LOG("Parcel::writeByte value =  %x", value);
        mData[mPos] = value;
        mPos += 1;
        mDataSize += 1;
    }

    void Parcel::setDataPosition(int i) {
        mPos = i;
    }

    void Parcel::recycle() {
        memset(mData, 0, sizeof(mData));
        mPos = 0;
        mDataSize = 0;
    }
}

using namespace android;

bool convertToParcel(LPADAPT_PARCEL_T lpParcel, int &code, Parcel &data, Parcel &reply) {
    char *lpData = NULL;
    if (lpParcel == NULL) return false;
    code = lpParcel->code;

    ENG_LOG("convertToParcel: code = %d", lpParcel->code);
    ENG_LOG("convertToParcel: dataSize = %d", lpParcel->dataSize);
    ENG_LOG("convertToParcel: replySize = %d", lpParcel->replySize);

    data.setDataPosition(0);
    reply.setDataPosition(0);

    lpData = (char *)&(lpParcel->data);
    //data.write(lpData, lpParcel->dataSize);
    data.setData(lpData, lpParcel->dataSize);
    reply.write(lpData+lpParcel->dataSize, lpParcel->replySize);

    data.setDataPosition(0);
    reply.setDataPosition(0);

    return true;
}

bool convertToLPADAPT_PARCEL(LPADAPT_PARCEL_T lpParcel, int &code, Parcel &data, Parcel &reply) {
    char *lpData = NULL;
    if (lpParcel == NULL) return false;
    lpData = (char *)&(lpParcel->data);

    ENG_LOG("convertToLPADAPT_PARCEL: code = %d", code);
    ENG_LOG("convertToLPADAPT_PARCEL: dataSize = %d", data.dataSize());
    ENG_LOG("convertToLPADAPT_PARCEL: replySize = %d", reply.dataSize());

    data.setDataPosition(0);
    reply.setDataPosition(0);

    lpParcel->code = code;
    lpParcel->dataSize = data.dataSize();
    data.read(lpData, data.dataSize());
    lpParcel->replySize = reply.dataSize();
    reply.read(lpData+lpParcel->dataSize, reply.dataSize());

    data.setDataPosition(0);
    reply.setDataPosition(0);

    return true;
}

void phConnect(){
    int service_fd = -1, connect_fd = -1;
    int nRead = 0, nWrite = 0, nWriteOffset = 0;;
    socklen_t server_len, client_len;
    static char recv_buf[MAX_BUF_LEN];
    static char data_buff[MAX_BUF_LEN];
    int code;
    LPADAPT_PARCEL_T lpAdaptParcel = NULL;
    struct sockaddr client_address = {0};
    NativeService ns;
    Parcel data, reply;
    unlink(PHASECHECK_SERVICE);

    //creat services socket, bind, listen...
    service_fd = socket_local_server(PHASECHECK_SERVICE,ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    ENG_LOG("<%s> is created. fd = %d\n", PHASECHECK_SERVICE, service_fd);

    while(1) {
        data.recycle();
        reply.recycle();

        ENG_LOG("wait connect....\n");
        if ((connect_fd = accept(service_fd, (struct sockaddr *)&client_address, &client_len)) < 0)
        {
            ENG_LOG("accept socket failed!");
            if ( service_fd >= 0 ) close(service_fd);
            return ;
        }

        ENG_LOG("connect succ....\n");
        memset(data_buff, 0, sizeof(data_buff));
        nRead = 0;
        ENG_LOG("read....\n");
        int read_error = 0;
        while(1){
            int iRet = 0;
            memset(recv_buf, '\0', sizeof(recv_buf));
            if( (iRet = read(connect_fd, recv_buf, sizeof(recv_buf))) <= 0)
            {
                ENG_LOG("recv cmd error:%s",strerror(errno));
                read_error = -1;
                break;
            }
            ENG_LOG("read return %d", iRet);
            memcpy(data_buff+nRead, recv_buf, iRet);
            nRead += iRet;
            if (nRead == MAX_BUF_LEN) {
                break;
            }
        }

        ENG_LOG("read succ...\n");
        lpAdaptParcel = (LPADAPT_PARCEL_T)data_buff;
        ENG_LOG("phConnect: code = %d", lpAdaptParcel->code);
        ENG_LOG("phConnect: dataSize = %d", lpAdaptParcel->dataSize);
        ENG_LOG("phConnect: replySize = %d", lpAdaptParcel->replySize);
        if(lpAdaptParcel->dataSize < MAX_BUF_LEN && lpAdaptParcel->replySize < MAX_BUF_LEN){
            convertToParcel(lpAdaptParcel, code, data, reply);
            ns.onTransact(code, data, &reply, 0);
            convertToLPADAPT_PARCEL(lpAdaptParcel, code, data, reply);
        }

        ENG_LOG("write....\n");
        nWrite = 0;
        ENG_LOG("read read_error:%d",read_error);
        while(nWrite < MAX_BUF_LEN && read_error >= 0){
            int iRet = 0;
            do {
                iRet = write(connect_fd, data_buff+nWrite, MAX_BUF_LEN-nWrite);
            }while(iRet < 0 && ((errno == EINTR) || (errno == EAGAIN)));

            if (iRet >= 0) {
                nWrite += iRet;
            } else {
                break;
            }
        }
        ENG_LOG("write succ....\n");
        close(connect_fd);
    }

    close(service_fd);
    return ;
}

int main(int arg, char** argv)
{
    ENG_LOG("phasecheck_sprd Nativeserver - main() begin\n");
#ifdef CHANNEL_SOCKET
    phConnect();
#else
    ProcessState::initWithDriver("/dev/vndbinder");
    ALOGE("phasecheck_sprd Nativeserver - main() begin /dev/vndbinder\n");
    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm = defaultServiceManager();
    //LOGI("ServiceManager: %p\n", sm.get());
    ENG_LOG("phasecheck_sprd new server - serviceManager: %p\n", sm.get());
    //int ret = NativeService::Instance();
    int ret = defaultServiceManager()->addService(
                String16("phasechecknative"), new NativeService());
    ENG_LOG("phasecheck_sprd new ..server - NativeService::Instance return %d\n", ret);
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
#endif
    return 0;
}
