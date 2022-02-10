#ifndef _ENG_PHASECHECK_H
#define _ENG_PHASECHECK_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <android/log.h>
#define LOG_TAG "PhaseCheckSrv"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define ENG_LOG(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

#define MAX_SN_LEN 24
#define SP09_MAX_SN_LEN                    MAX_SN_LEN
#define SP09_MAX_STATION_NUM               (15)
#define SP09_MAX_STATION_NAME_LEN          (10)
#define SP09_SPPH_MAGIC_NUMBER             (0X53503039)    // "SP09"
#define SP05_SPPH_MAGIC_NUMBER             (0X53503035)    // "SP05"
#define SP09_MAX_LAST_DESCRIPTION_LEN      (32)
#define SIGN_TYPE 0
#define ITEM_TYPE 1

#define TYPE_GET_SN1 0
#define TYPE_GET_SN2 1
#define TYPE_WRITE_TESTED 2
#define TYPE_WRITE_PASS 3
#define TYPE_WRITE_FAIL 4
#define TYPE_GET_STATION 5
#define TYPE_CHARGE_SWITCH 6
#define TYPE_RED_LED_NODE 7
#define TYPE_BLUE_LED_NODE 8
#define TYPE_GREEN_LED_NODE 9
#define TYPE_PHASECHECK 10
#define TYPE_GET_KERNEL_LOG_LEVEL_STATE 11
#define TYPE_SET_KERNEL_LOG_LEVEL_STATE 12
#define TYPE_MIPI_SWITCH 13
#define TYPE_SET_CC_TEST_CMD 14
#define TYPE_GET_CC_TEST_RESULT 15
/* SPRD 911107 - write /sys/class/misc/sprd_7sreset/hard_mode no effective @{ */
#define TYPE_SET_RESET_MODE 16
/* }@ */
#define TYPE_WIRTE_OFFTE 17
#define TYPE_READ_OFFTE 18
#define TYPE_GET_CC_TEST_VOL 19
#define TYPE_GET_DELTANV_INFO 20
/* SPRD 940291 - [true demo version] Control charge region @{ */
#define TYPE_READ_CHARGE_STATUS 21
#define TYPE_READ_CHARGE_LEVEL 22
#define TYPE_EXECUTE_OFFSET_CALIBRATION 23
#define TYPE_EXECUTE_XTALK_CALIBRATION 24
//gnss-test for Bug 913791
#define TYPE_SET_BACKLIGHT 25
#define TYPE_GET_BACKLIGHT 26
/* }@ */
/* SPRD 1016593 : Coulometer Power Test Mode for kernel 4.14 @{ */
#define TYPE_GET_CC_ENERGY_NEW_KERNEL 27
#define TYPE_GET_CC_VOL_NEW_KERNEL 28
#define TYPE_EXECUTE_AI_TEST 29
/* }@ */
#define TYPE_GET_JEITA 30
#define TYPE_SET_JEITA 31
#define TYPE_SET_CABC_MODE 32
#define TYPE_SET_CPU_DEBUG_MODE 35
#define TYPE_GET_CPU_DEBUG_MODE 36
#define LED_RED_PATH  "/sys/class/leds/sc27xx:red/brightness"
#define LED_BLUE_PATH  "/sys/class/leds/sc27xx:blue/brightness"
#define LED_GREEN_PATH  "/sys/class/leds/sc27xx:green/brightness"

typedef struct _tagSP09_PHASE_CHECK
{
    unsigned int Magic;           // "SP09"   (\C0\CF\u0153ӿ\DAΪSP05)
    char    SN1[SP09_MAX_SN_LEN]; // SN , SN_LEN=24
    char    SN2[SP09_MAX_SN_LEN]; // add for Mobile
    unsigned int StationNum;      // the test station number of the testing
    char    StationName[SP09_MAX_STATION_NUM][SP09_MAX_STATION_NAME_LEN];
    unsigned char Reserved[13];
    unsigned char SignFlag;
    char    szLastFailDescription[SP09_MAX_LAST_DESCRIPTION_LEN];
    unsigned short  iTestSign;    // Bit0~Bit14 ---> station0~station 14
    //if tested. 0: tested, 1: not tested
    unsigned short  iItem;        // part1: Bit0~ Bit_14 indicate test Station,1\B1\ED\CA\u0178Pass,

}SP09_PHASE_CHECK_T, *LPSP09_PHASE_CHECK_T;

/*add the struct add define to support the sp15*/
#define SP15_MAX_SN_LEN                 (64)
#define SP15_MAX_STATION_NUM            (20)
#define SP15_MAX_STATION_NAME_LEN       (15)
#define SP15_SPPH_MAGIC_NUMBER          (0X53503135)    // "SP15"
#define SP15_MAX_LAST_DESCRIPTION_LEN   (32)

typedef struct _tagSP15_PHASE_CHECK {
    unsigned int Magic;         // "SP15"
    char SN1[SP15_MAX_SN_LEN];  // SN , SN_LEN=64
    char SN2[SP15_MAX_SN_LEN];  // add for Mobile
    int StationNum;             // the test station number of the testing
    char StationName[SP15_MAX_STATION_NUM][SP15_MAX_STATION_NAME_LEN];
    unsigned char Reserved[13]; //
    unsigned char SignFlag;
    char szLastFailDescription[SP15_MAX_LAST_DESCRIPTION_LEN];
    unsigned int iTestSign;     // Bit0~Bit14 ---> station0~station 14
    //if tested. 0: tested, 1: not tested
    unsigned int iItem;         // part1: Bit0~ Bit_14 indicate test Station, 0: Pass, 1: fail
} SP15_PHASE_CHECK_T, *LPSP15_PHASE_CHECK_T;

static const int SP09_MAX_PHASE_BUFF_SIZE = sizeof(SP09_PHASE_CHECK_T);

int eng_getphasecheck(SP09_PHASE_CHECK_T* phase_check);

#ifdef __cplusplus
}
#endif

#endif
