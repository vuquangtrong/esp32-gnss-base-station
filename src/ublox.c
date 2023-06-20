/*
 * This file is part of the ESP32-GNSS-Base-Station firmware, published
 * at (https://github.com/vuquangtrong/esp32-gnss-base-station).
 * Copyright (c) 2023 Vu Quang Trong.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ublox.h"

static const char *TAG = "UBLOX";

#define UBXSYNC1 0xB5 /* ubx message sync code 1 */
#define UBXSYNC2 0x62 /* ubx message sync code 2 */
#define UBXCFG 0x06   /* ubx message cfg-??? */

#define FU1 1 /* ubx message field types */
#define FU2 2
#define FU4 3
#define FU8 4
#define FI1 5
#define FI2 6
#define FI4 7
#define FR4 8
#define FR8 9
#define FS32 10

#define ROUND(x) (int)floor((x) + 0.5)

/* get fields (little-endian) ------------------------------------------------*/
#define U1(p) (*((uint8_t *)(p)))
#define I1(p) (*((int8_t *)(p)))

static uint16_t U2(uint8_t *p)
{
    uint16_t u;
    memcpy(&u, p, 2);
    return u;
}

static uint32_t U4(uint8_t *p)
{
    uint32_t u;
    memcpy(&u, p, 4);
    return u;
}

static int32_t I4(uint8_t *p)
{
    int32_t u;
    memcpy(&u, p, 4);
    return u;
}

static float R4(uint8_t *p)
{
    float r;
    memcpy(&r, p, 4);
    return r;
}

static double R8(uint8_t *p)
{
    double r;
    memcpy(&r, p, 8);
    return r;
}

static double I8(uint8_t *p)
{
    return I4(p + 4) * 4294967296.0 + U4(p);
}

/* set fields (little-endian) ------------------------------------------------*/
static void setU1(uint8_t *p, uint8_t u)
{
    *p = u;
}

static void setU2(uint8_t *p, uint16_t u)
{
    memcpy(p, &u, 2);
}

static void setU4(uint8_t *p, uint32_t u)
{
    memcpy(p, &u, 4);
}

static void
setI1(uint8_t *p, int8_t i)
{
    *p = (uint8_t)i;
}

static void setI2(uint8_t *p, int16_t i)
{
    memcpy(p, &i, 2);
}

static void setI4(uint8_t *p, int32_t i)
{
    memcpy(p, &i, 4);
}

static void setR4(uint8_t *p, float r)
{
    memcpy(p, &r, 4);
}

static void setR8(uint8_t *p, double r)
{
    memcpy(p, &r, 8);
}

/* checksum ------------------------------------------------------------------*/
static int check_checksum(uint8_t *buff, int len)
{
    uint8_t cka = 0, ckb = 0;
    int i;

    for (i = 2; i < len - 2; i++)
    {
        cka += buff[i];
        ckb += cka;
    }
    return cka == buff[len - 2] && ckb == buff[len - 1];
}

static void set_checksum(uint8_t *buff, int len)
{
    uint8_t cka = 0, ckb = 0;
    int i;

    for (i = 2; i < len - 2; i++)
    {
        cka += buff[i];
        ckb += cka;
    }
    buff[len - 2] = cka;
    buff[len - 1] = ckb;
}

/* convert string to integer -------------------------------------------------*/
static int stoi(const char *s)
{
    uint32_t n;
    if (sscanf(s, "0x%lX", &n) == 1)
    {
        return (int)n; /* hex (0xXXXX) */
    }
    return atoi(s);
}

/* generate ublox binary message -----------------------------------------------
 * generate ublox binary message from message string
 * args   : char  *msg   IO     message string
 *            "CFG-PRT   portid res0 res1 mode baudrate inmask outmask flags"
 *            "CFG-USB   vendid prodid res1 res2 power flags vstr pstr serino"
 *            "CFG-MSG   msgid rate0 rate1 rate2 rate3 rate4 rate5 rate6"
 *            "CFG-NMEA  filter version numsv flags"
 *            "CFG-RATE  meas nav time"
 *            "CFG-CFG   clear_mask save_mask load_mask [dev_mask]"
 *            "CFG-TP    interval length status time_ref res adelay rdelay udelay"
 *            "CFG-NAV2  ..."
 *            "CFG-DAT   maja flat dx dy dz rotx roty rotz scale"
 *            "CFG-INF   protocolid res0 res1 res2 mask0 mask1 mask2 ... mask5"
 *            "CFG-RST   navbbr reset res"
 *            "CFG-RXM   gpsmode lpmode"
 *            "CFG-ANT   flags pins"
 *            "CFG-FXN   flags treacq tacq treacqoff tacqoff ton toff res basetow"
 *            "CFG-SBAS  mode usage maxsbas res scanmode"
 *            "CFG-LIC   key0 key1 key2 key3 key4 key5"
 *            "CFG-TM    intid rate flags"
 *            "CFG-TM2   ch res0 res1 rate flags"
 *            "CFG-TMODE tmode posx posy posz posvar svinmindur svinvarlimit"
 *            "CFG-EKF   ..."
 *            "CFG-GNSS  ..."
 *            "CFG-ITFM  conf conf2"
 *            "CFG-LOGFILTER ver flag min_int time_thr speed_thr pos_thr"
 *            "CFG-NAV5  ..."
 *            "CFG-NAVX5 ..."
 *            "CFG-ODO   ..."
 *            "CFG-PM2   ..."
 *            "CFG-PWR   ver rsv1 rsv2 rsv3 state"
 *            "CFG-RINV  flag data ..."
 *            "CFG-SMGR  ..."
 *            "CFG-TMODE2 ..."
 *            "CFG-TMODE3 ..."
 *            "CFG-TPS   ..."
 *            "CFG-TXSLOT ..."
 *            "CFG-VALDEL ver layer res0 res1 key [key ...]"
 *            "CFG-VALGET ver layer pos key [key ...]"
 *            "CFG-VALSET ver layer res0 res1 key value [key value ...]"
 *          uint8_t *buff O binary message
 * return : length of binary message (0: error)
 * note   : see reference [1][3][5] for details.
 *          the following messages are not supported:
 *             CFG-DOSC,CFG-ESRC
 *-----------------------------------------------------------------------------*/
uint32_t ubx_gen_cmd(const char *msg, uint8_t *buff)
{
    static const char *cmd[] = {
        "PRT", "USB", "MSG", "NMEA", "RATE", "CFG", "TP", "NAV2", "DAT", "INF",
        "RST", "RXM", "ANT", "FXN", "SBAS", "LIC", "TM", "TM2", "TMODE", "EKF",
        "GNSS", "ITFM", "LOGFILTER", "NAV5", "NAVX5", "ODO", "PM2", "PWR", "RINV",
        "SMGR",
        "TMODE2", "TMODE3", "TPS", "TXSLOT",
        "VALDEL", "VALGET", "VALSET",
        ""};

    static const uint8_t id[] = {
        0x00, 0x1B, 0x01, 0x17, 0x08, 0x09, 0x07, 0x1A, 0x06, 0x02,
        0x04, 0x11, 0x13, 0x0E, 0x16, 0x80, 0x10, 0x19, 0x1D, 0x12,
        0x3E, 0x39, 0x47, 0x24, 0x23, 0x1E, 0x3B, 0x57, 0x34,
        0x62,
        0x36, 0x71, 0x31, 0x53,
        0x8c, 0x8b, 0x8a};

    static const int prm[][32] = {
        {FU1, FU1, FU2, FU4, FU4, FU2, FU2, FU2, FU2},                                                                                 /* PRT */
        {FU2, FU2, FU2, FU2, FU2, FU2, FS32, FS32, FS32},                                                                              /* USB */
        {FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1},                                                                                      /* MSG */
        {FU1, FU1, FU1, FU1},                                                                                                          /* NMEA */
        {FU2, FU2, FU2},                                                                                                               /* RATE */
        {FU4, FU4, FU4, FU1},                                                                                                          /* CFG */
        {FU4, FU4, FI1, FU1, FU2, FI2, FI2, FI4},                                                                                      /* TP */
        {FU1, FU1, FU2, FU1, FU1, FU1, FU1, FI4, FU1, FU1, FU1, FU1, FU1, FU1, FU2, FU2, FU2, FU2, FU2, FU1, FU1, FU2, FU4, FU4},      /* NAV2 */
        {FR8, FR8, FR4, FR4, FR4, FR4, FR4, FR4, FR4},                                                                                 /* DAT */
        {FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1},                                                                            /* INF */
        {FU2, FU1, FU1},                                                                                                               /* RST */
        {FU1, FU1},                                                                                                                    /* RXM */
        {FU2, FU2},                                                                                                                    /* ANT */
        {FU4, FU4, FU4, FU4, FU4, FU4, FU4, FU4},                                                                                      /* FXN */
        {FU1, FU1, FU1, FU1, FU4},                                                                                                     /* SBAS */
        {FU2, FU2, FU2, FU2, FU2, FU2},                                                                                                /* LIC */
        {FU4, FU4, FU4},                                                                                                               /* TM */
        {FU1, FU1, FU2, FU4, FU4},                                                                                                     /* TM2 */
        {FU4, FI4, FI4, FI4, FU4, FU4, FU4},                                                                                           /* TMODE */
        {FU1, FU1, FU1, FU1, FU4, FU2, FU2, FU1, FU1, FU2},                                                                            /* EKF */
        {FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU4},                                                                                 /* GNSS */
        {FU4, FU4},                                                                                                                    /* ITFM */
        {FU1, FU1, FU2, FU2, FU2, FU4},                                                                                                /* LOGFILTER */
        {FU2, FU1, FU1, FI4, FU4, FI1, FU1, FU2, FU2, FU2, FU2, FU1, FU1, FU1, FU1, FU1, FU1, FU2, FU1, FU1, FU1, FU1, FU1, FU1},      /* NAV5 */
        {FU2, FU2, FU4, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU2, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU2}, /* NAVX5 */
        {FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1},                                                                                 /* ODO */
        {FU1, FU1, FU1, FU1, FU4, FU4, FU4, FU4, FU2, FU2},                                                                            /* PM2 */
        {FU1, FU1, FU1, FU1, FU4},                                                                                                     /* PWR */
        {FU1, FU1},                                                                                                                    /* RINV */
        {FU1, FU1, FU2, FU2, FU1, FU1, FU2, FU2, FU2, FU2, FU4},                                                                       /* SMGR */
        {FU1, FU1, FU2, FI4, FI4, FI4, FU4, FU4, FU4},                                                                                 /* TMODE2 */
        {FU1, FU1, FU2, FI4, FI4, FI4, FU4, FU4, FU4},                                                                                 /* TMODE3 */
        {FU1, FU1, FU1, FU1, FI2, FI2, FU4, FU4, FU4, FU4, FI4, FU4},                                                                  /* TPS */
        {FU1, FU1, FU1, FU1, FU4, FU4, FU4, FU4, FU4},                                                                                 /* TXSLOT */
        {FU1, FU1, FU1, FU1},                                                                                                          /* VALDEL */
        {FU1, FU1, FU2},                                                                                                               /* VALGET */
        {FU1, FU1, FU1, FU1}                                                                                                           /* VALSET */
    };

    if (!msg || !buff)
    {
        return 0;
    }

    uint8_t *q = buff;
    char mbuff[1024], *args[32], *p;
    int i, j, n, narg = 0;
    bool isvalset = false;

    strcpy(mbuff, msg);

    for (p = strtok(mbuff, " "); p && narg < 32; p = strtok(NULL, " "))
    {
        args[narg++] = p;
    }

    if (narg < 1 || strncmp(args[0], "CFG-", 4))
    {
        return 0;
    }

    for (i = 0; *cmd[i]; i++)
    {
        if (!strcmp(args[0] + 4, cmd[i]))
            break;
    }

    if (!*cmd[i])
    {
        return 0;
    }

    *q++ = UBXSYNC1;
    *q++ = UBXSYNC2;
    *q++ = UBXCFG;
    *q++ = id[i];
    q += 2;

    if (strcmp(cmd[i], "VALSET") == 0)
    {
        isvalset = true;
    }

    /* VALSET sanity check */
    if (isvalset)
    {
        if (narg == 7)
            narg = narg - 2; /* Adjusting for key value addition */
        else
            return 0;
    }

    for (j = 1; prm[i][j - 1] || j < narg; j++)
    {
        switch (prm[i][j - 1])
        {
        case FU1:
            setU1(q, j < narg ? (uint8_t)stoi(args[j]) : 0);
            q += 1;
            break;
        case FU2:
            setU2(q, j < narg ? (uint16_t)stoi(args[j]) : 0);
            q += 2;
            break;
        case FU4:
            setU4(q, j < narg ? (uint32_t)stoi(args[j]) : 0);
            q += 4;
            break;
        case FI1:
            setI1(q, j < narg ? (int8_t)stoi(args[j]) : 0);
            q += 1;
            break;
        case FI2:
            setI2(q, j < narg ? (int16_t)stoi(args[j]) : 0);
            q += 2;
            break;
        case FI4:
            setI4(q, j < narg ? (int32_t)stoi(args[j]) : 0);
            q += 4;
            break;
        case FR4:
            setR4(q, j < narg ? (float)atof(args[j]) : 0);
            q += 4;
            break;
        case FR8:
            setR8(q, j < narg ? (double)atof(args[j]) : 0);
            q += 8;
            break;
        case FS32:
            sprintf((char *)q, "%-32.32s", j < narg ? args[j] : "");
            q += 32;
            break;
        default:
            setU1(q, j < narg ? (uint8_t)stoi(args[j]) : 0);
            q += 1;
            break;
        }
    }

    /* Add VALSET cfgData here */
    if (isvalset)
    {
        int k;

        /* VALSET commands courtesy of gpsd's ubxtool */
        static const char *vcmd[] = {
            "GEOFENCE-CONFLVL",
            "GEOFENCE-USE_PIO",
            "GEOFENCE-PINPOL",
            "GEOFENCE-PIN",
            "GEOFENCE-USE_FENCE1",
            "GEOFENCE-FENCE1_LAT",
            "GEOFENCE-FENCE1_LON",
            "GEOFENCE-FENCE1_RAD",
            "GEOFENCE-USE_FENCE2",
            "GEOFENCE-FENCE2_LAT",
            "GEOFENCE-FENCE2_LON",
            "GEOFENCE-FENCE2_RAD",
            "GEOFENCE-USE_FENCE3",
            "GEOFENCE-FENCE3_LAT",
            "GEOFENCE-FENCE3_LON",
            "GEOFENCE-FENCE3_RAD",
            "GEOFENCE-USE_FENCE4",
            "GEOFENCE-FENCE4_LAT",
            "GEOFENCE-FENCE4_LON",
            "GEOFENCE-FENCE4_RAD",
            "HW-ANT_CFG_VOLTCTRL",
            "HW-ANT_CFG_SHORTDET",
            "HW-ANT_CFG_SHORTDET_POL",
            "HW-ANT_CFG_OPENDET",
            "HW-ANT_CFG_OPENDET_POL",
            "HW-ANT_CFG_PWRDOWN",
            "HW-ANT_CFG_PWRDOWN_POL",
            "HW-ANT_CFG_RECOVER",
            "HW-ANT_SUP_SWITCH_PIN",
            "HW-ANT_SUP_SHORT_PIN",
            "HW-ANT_SUP_OPEN_PIN",
            "I2C-ADDRESS",
            "I2C-EXTENDEDTIMEOUT",
            "I2C-ENABLED",
            "I2CINPROT-UBX",
            "I2CINPROT-NMEA",
            "I2CINPROT-RTCM2X",
            "I2CINPROT-RTCM3X",
            "I2COUTPROT-UBX",
            "I2COUTPROT-NMEA",
            "I2COUTPROT-RTCM3X",
            "INFMSG-UBX_I2C",
            "INFMSG-UBX_UART1",
            "INFMSG-UBX_UART2",
            "INFMSG-UBX_USB",
            "INFMSG-UBX_SPI",
            "INFMSG-NMEA_I2C",
            "INFMSG-NMEA_UART1",
            "INFMSG-NMEA_UART2",
            "INFMSG-NMEA_USB",
            "INFMSG-NMEA_SPI",
            "ITFM-BBTHRESHOLD",
            "ITFM-CWTHRESHOLD",
            "ITFM-ENABLE",
            "ITFM-ANTSETTING",
            "ITFM-ENABLE_AUX",
            "LOGFILTER-RECORD_ENA",
            "LOGFILTER-ONCE_PER_WAKE_UP_ENA",
            "LOGFILTER-APPLY_ALL_FILTERS",
            "LOGFILTER-MIN_INTERVAL",
            "LOGFILTER-TIME_THRS",
            "LOGFILTER-SPEED_THRS",
            "LOGFILTER-POSITION_THRS",
            "MOT-GNSSSPEED_THRS",
            "MOT-GNSSDIST_THRS",
            "MSGOUT-NMEA_ID_DTM_I2C",
            "MSGOUT-NMEA_ID_DTM_SPI",
            "MSGOUT-NMEA_ID_DTM_UART1",
            "MSGOUT-NMEA_ID_DTM_UART2",
            "MSGOUT-NMEA_ID_DTM_USB",
            "MSGOUT-NMEA_ID_GBS_I2C",
            "MSGOUT-NMEA_ID_GBS_SPI",
            "MSGOUT-NMEA_ID_GBS_UART1",
            "MSGOUT-NMEA_ID_GBS_UART2",
            "MSGOUT-NMEA_ID_GBS_USB",
            "MSGOUT-NMEA_ID_GGA_I2C",
            "MSGOUT-NMEA_ID_GGA_SPI",
            "MSGOUT-NMEA_ID_GGA_UART1",
            "MSGOUT-NMEA_ID_GGA_UART2",
            "MSGOUT-NMEA_ID_GGA_USB",
            "MSGOUT-NMEA_ID_GLL_I2C",
            "MSGOUT-NMEA_ID_GLL_SPI",
            "MSGOUT-NMEA_ID_GLL_UART1",
            "MSGOUT-NMEA_ID_GLL_UART2",
            "MSGOUT-NMEA_ID_GLL_USB",
            "MSGOUT-NMEA_ID_GNS_I2C",
            "MSGOUT-NMEA_ID_GNS_SPI",
            "MSGOUT-NMEA_ID_GNS_UART1",
            "MSGOUT-NMEA_ID_GNS_UART2",
            "MSGOUT-NMEA_ID_GNS_USB",
            "MSGOUT-NMEA_ID_GRS_I2C",
            "MSGOUT-NMEA_ID_GRS_SPI",
            "MSGOUT-NMEA_ID_GRS_UART1",
            "MSGOUT-NMEA_ID_GRS_UART2",
            "MSGOUT-NMEA_ID_GRS_USB",
            "MSGOUT-NMEA_ID_GSA_I2C",
            "MSGOUT-NMEA_ID_GSA_SPI",
            "MSGOUT-NMEA_ID_GSA_UART1",
            "MSGOUT-NMEA_ID_GSA_UART2",
            "MSGOUT-NMEA_ID_GSA_USB",
            "MSGOUT-NMEA_ID_GST_I2C",
            "MSGOUT-NMEA_ID_GST_SPI",
            "MSGOUT-NMEA_ID_GST_UART1",
            "MSGOUT-NMEA_ID_GST_UART2",
            "MSGOUT-NMEA_ID_GST_USB",
            "MSGOUT-NMEA_ID_GSV_I2C",
            "MSGOUT-NMEA_ID_GSV_SPI",
            "MSGOUT-NMEA_ID_GSV_UART1",
            "MSGOUT-NMEA_ID_GSV_UART2",
            "MSGOUT-NMEA_ID_GSV_USB",
            "MSGOUT-NMEA_ID_RMC_I2C",
            "MSGOUT-NMEA_ID_RMC_SPI",
            "MSGOUT-NMEA_ID_RMC_UART1",
            "MSGOUT-NMEA_ID_RMC_UART2",
            "MSGOUT-NMEA_ID_RMC_USB",
            "MSGOUT-NMEA_ID_VLW_I2C",
            "MSGOUT-NMEA_ID_VLW_SPI",
            "MSGOUT-NMEA_ID_VLW_UART1",
            "MSGOUT-NMEA_ID_VLW_UART2",
            "MSGOUT-NMEA_ID_VLW_USB",
            "MSGOUT-NMEA_ID_VTG_I2C",
            "MSGOUT-NMEA_ID_VTG_SPI",
            "MSGOUT-NMEA_ID_VTG_UART1",
            "MSGOUT-NMEA_ID_VTG_UART2",
            "MSGOUT-NMEA_ID_VTG_USB",
            "MSGOUT-NMEA_ID_ZDA_I2C",
            "MSGOUT-NMEA_ID_ZDA_SPI",
            "MSGOUT-NMEA_ID_ZDA_UART1",
            "MSGOUT-NMEA_ID_ZDA_UART2",
            "MSGOUT-NMEA_ID_ZDA_USB",
            "MSGOUT-PUBX_ID_POLYP_I2C",
            "MSGOUT-PUBX_ID_POLYP_SPI",
            "MSGOUT-PUBX_ID_POLYP_UART1",
            "MSGOUT-PUBX_ID_POLYP_UART2",
            "MSGOUT-PUBX_ID_POLYP_USB",
            "MSGOUT-PUBX_ID_POLYS_I2C",
            "MSGOUT-PUBX_ID_POLYS_SPI",
            "MSGOUT-PUBX_ID_POLYS_UART1",
            "MSGOUT-PUBX_ID_POLYS_UART2",
            "MSGOUT-PUBX_ID_POLYS_USB",
            "MSGOUT-PUBX_ID_POLYT_I2C",
            "MSGOUT-PUBX_ID_POLYT_SPI",
            "MSGOUT-PUBX_ID_POLYT_UART1",
            "MSGOUT-PUBX_ID_POLYT_UART2",
            "MSGOUT-PUBX_ID_POLYT_USB",
            "MSGOUT-RTCM_3X_TYPE1005_I2C",
            "MSGOUT-RTCM_3X_TYPE1005_SPI",
            "MSGOUT-RTCM_3X_TYPE1005_UART1",
            "MSGOUT-RTCM_3X_TYPE1005_UART2",
            "MSGOUT-RTCM_3X_TYPE1005_USB",
            "MSGOUT-RTCM_3X_TYPE1074_I2C",
            "MSGOUT-RTCM_3X_TYPE1074_SPI",
            "MSGOUT-RTCM_3X_TYPE1074_UART1",
            "MSGOUT-RTCM_3X_TYPE1074_UART2",
            "MSGOUT-RTCM_3X_TYPE1074_USB",
            "MSGOUT-RTCM_3X_TYPE1077_I2C",
            "MSGOUT-RTCM_3X_TYPE1077_SPI",
            "MSGOUT-RTCM_3X_TYPE1077_UART1",
            "MSGOUT-RTCM_3X_TYPE1077_UART2",
            "MSGOUT-RTCM_3X_TYPE1077_USB",
            "MSGOUT-RTCM_3X_TYPE1087_I2C",
            "MSGOUT-RTCM_3X_TYPE1084_SPI",
            "MSGOUT-RTCM_3X_TYPE1084_UART1",
            "MSGOUT-RTCM_3X_TYPE1084_UART2",
            "MSGOUT-RTCM_3X_TYPE1084_USB",
            "MSGOUT-RTCM_3X_TYPE1087_SPI",
            "MSGOUT-RTCM_3X_TYPE1087_UART1",
            "MSGOUT-RTCM_3X_TYPE1087_UART2",
            "MSGOUT-RTCM_3X_TYPE1087_USB",
            "MSGOUT-RTCM_3X_TYPE1094_I2C",
            "MSGOUT-RTCM_3X_TYPE1094_SPI",
            "MSGOUT-RTCM_3X_TYPE1094_UART1",
            "MSGOUT-RTCM_3X_TYPE1094_UART2",
            "MSGOUT-RTCM_3X_TYPE1094_USB",
            "MSGOUT-RTCM_3X_TYPE1097_I2C",
            "MSGOUT-RTCM_3X_TYPE1097_SPI",
            "MSGOUT-RTCM_3X_TYPE1097_UART1",
            "MSGOUT-RTCM_3X_TYPE1097_UART2",
            "MSGOUT-RTCM_3X_TYPE1097_USB",
            "MSGOUT-RTCM_3X_TYPE1124_I2C",
            "MSGOUT-RTCM_3X_TYPE1124_SPI",
            "MSGOUT-RTCM_3X_TYPE1124_UART1",
            "MSGOUT-RTCM_3X_TYPE1124_UART2",
            "MSGOUT-RTCM_3X_TYPE1124_USB",
            "MSGOUT-RTCM_3X_TYPE1127_I2C",
            "MSGOUT-RTCM_3X_TYPE1127_SPI",
            "MSGOUT-RTCM_3X_TYPE1127_UART1",
            "MSGOUT-RTCM_3X_TYPE1127_UART2",
            "MSGOUT-RTCM_3X_TYPE1127_USB",
            "MSGOUT-RTCM_3X_TYPE1230_I2C",
            "MSGOUT-RTCM_3X_TYPE1230_SPI",
            "MSGOUT-RTCM_3X_TYPE1230_UART1",
            "MSGOUT-RTCM_3X_TYPE1230_UART2",
            "MSGOUT-RTCM_3X_TYPE1230_USB",
            "MSGOUT-RTCM_3X_TYPE4072_0_I2C",
            "MSGOUT-RTCM_3X_TYPE4072_0_SPI",
            "MSGOUT-RTCM_3X_TYPE4072_0_UART1",
            "MSGOUT-RTCM_3X_TYPE4072_0_UART2",
            "MSGOUT-RTCM_3X_TYPE4072_0_USB",
            "MSGOUT-RTCM_3X_TYPE4072_1_I2C",
            "MSGOUT-RTCM_3X_TYPE4072_1_SPI",
            "MSGOUT-RTCM_3X_TYPE4072_1_UART1",
            "MSGOUT-RTCM_3X_TYPE4072_1_UART2",
            "MSGOUT-RTCM_3X_TYPE4072_1_USB",
            "MSGOUT-UBX_LOG_INFO_I2C",
            "MSGOUT-UBX_LOG_INFO_SPI",
            "MSGOUT-UBX_LOG_INFO_UART1",
            "MSGOUT-UBX_LOG_INFO_UART2",
            "MSGOUT-UBX_LOG_INFO_USB",
            "MSGOUT-UBX_MON_COMMS_I2C",
            "MSGOUT-UBX_MON_COMMS_SPI",
            "MSGOUT-UBX_MON_COMMS_UART1",
            "MSGOUT-UBX_MON_COMMS_UART2",
            "MSGOUT-UBX_MON_COMMS_USB",
            "MSGOUT-UBX_MON_HW2_I2C",
            "MSGOUT-UBX_MON_HW2_SPI",
            "MSGOUT-UBX_MON_HW2_UART1",
            "MSGOUT-UBX_MON_HW2_UART2",
            "MSGOUT-UBX_MON_HW2_USB",
            "MSGOUT-UBX_MON_HW3_I2C",
            "MSGOUT-UBX_MON_HW3_SPI",
            "MSGOUT-UBX_MON_HW3_UART1",
            "MSGOUT-UBX_MON_HW3_UART2",
            "MSGOUT-UBX_MON_HW3_USB",
            "MSGOUT-UBX_MON_HW_I2C",
            "MSGOUT-UBX_MON_HW_SPI",
            "MSGOUT-UBX_MON_HW_UART1",
            "MSGOUT-UBX_MON_HW_UART2",
            "MSGOUT-UBX_MON_HW_USB",
            "MSGOUT-UBX_MON_IO_I2C",
            "MSGOUT-UBX_MON_IO_SPI",
            "MSGOUT-UBX_MON_IO_UART1",
            "MSGOUT-UBX_MON_IO_UART2",
            "MSGOUT-UBX_MON_IO_USB",
            "MSGOUT-UBX_MON_MSGPP_I2C",
            "MSGOUT-UBX_MON_MSGPP_SPI",
            "MSGOUT-UBX_MON_MSGPP_UART1",
            "MSGOUT-UBX_MON_MSGPP_UART2",
            "MSGOUT-UBX_MON_MSGPP_USB",
            "MSGOUT-UBX_MON_RF_I2C",
            "MSGOUT-UBX_MON_RF_SPI",
            "MSGOUT-UBX_MON_RF_UART1",
            "MSGOUT-UBX_MON_RF_UART2",
            "MSGOUT-UBX_MON_RF_USB",
            "MSGOUT-UBX_MON_RXBUF_I2C",
            "MSGOUT-UBX_MON_RXBUF_SPI",
            "MSGOUT-UBX_MON_RXBUF_UART1",
            "MSGOUT-UBX_MON_RXBUF_UART2",
            "MSGOUT-UBX_MON_RXBUF_USB",
            "MSGOUT-UBX_MON_RXR_I2C",
            "MSGOUT-UBX_MON_RXR_SPI",
            "MSGOUT-UBX_MON_RXR_UART1",
            "MSGOUT-UBX_MON_RXR_UART2",
            "MSGOUT-UBX_MON_RXR_USB",
            "MSGOUT-UBX_MON_TXBUF_I2C",
            "MSGOUT-UBX_MON_TXBUF_SPI",
            "MSGOUT-UBX_MON_TXBUF_UART1",
            "MSGOUT-UBX_MON_TXBUF_UART2",
            "MSGOUT-UBX_MON_TXBUF_USB",
            "MSGOUT-UBX_MON_TXBUF_I2C",
            "MSGOUT-UBX_MON_TXBUF_SPI",
            "MSGOUT-UBX_MON_TXBUF_UART1",
            "MSGOUT-UBX_MON_TXBUF_UART2",
            "MSGOUT-UBX_MON_TXBUF_USB",
            "MSGOUT-UBX_NAV_CLOCK_I2C",
            "MSGOUT-UBX_NAV_CLOCK_SPI",
            "MSGOUT-UBX_NAV_CLOCK_UART1",
            "MSGOUT-UBX_NAV_CLOCK_UART2",
            "MSGOUT-UBX_NAV_CLOCK_USB",
            "MSGOUT-UBX_NAV_DOP_I2C",
            "MSGOUT-UBX_NAV_DOP_SPI",
            "MSGOUT-UBX_NAV_DOP_UART1",
            "MSGOUT-UBX_NAV_DOP_UART2",
            "MSGOUT-UBX_NAV_DOP_USB",
            "MSGOUT-UBX_NAV_EOE_I2C",
            "MSGOUT-UBX_NAV_EOE_SPI",
            "MSGOUT-UBX_NAV_EOE_UART1",
            "MSGOUT-UBX_NAV_EOE_UART2",
            "MSGOUT-UBX_NAV_EOE_USB",
            "MSGOUT-UBX_NAV_GEOFENCE_I2C",
            "MSGOUT-UBX_NAV_GEOFENCE_SPI",
            "MSGOUT-UBX_NAV_GEOFENCE_UART1",
            "MSGOUT-UBX_NAV_GEOFENCE_UART2",
            "MSGOUT-UBX_NAV_GEOFENCE_USB",
            "MSGOUT-UBX_NAV_HPPOSECEF_I2C",
            "MSGOUT-UBX_NAV_HPPOSECEF_SPI",
            "MSGOUT-UBX_NAV_HPPOSECEF_UART1",
            "MSGOUT-UBX_NAV_HPPOSECEF_UART2",
            "MSGOUT-UBX_NAV_HPPOSECEF_USB",
            "MSGOUT-UBX_NAV_HPPOSLLH_I2C",
            "MSGOUT-UBX_NAV_HPPOSLLH_SPI",
            "MSGOUT-UBX_NAV_HPPOSLLH_UART1",
            "MSGOUT-UBX_NAV_HPPOSLLH_UART2",
            "MSGOUT-UBX_NAV_HPPOSLLH_USB",
            "MSGOUT-UBX_NAV_ODO_I2C",
            "MSGOUT-UBX_NAV_ODO_SPI",
            "MSGOUT-UBX_NAV_ODO_UART1",
            "MSGOUT-UBX_NAV_ODO_UART2",
            "MSGOUT-UBX_NAV_ODO_USB",
            "MSGOUT-UBX_NAV_ORB_I2C",
            "MSGOUT-UBX_NAV_ORB_SPI",
            "MSGOUT-UBX_NAV_ORB_UART1",
            "MSGOUT-UBX_NAV_ORB_UART2",
            "MSGOUT-UBX_NAV_ORB_USB",
            "MSGOUT-UBX_NAV_POSECEF_I2C",
            "MSGOUT-UBX_NAV_POSECEF_SPI",
            "MSGOUT-UBX_NAV_POSECEF_UART1",
            "MSGOUT-UBX_NAV_POSECEF_UART2",
            "MSGOUT-UBX_NAV_POSECEF_USB",
            "MSGOUT-UBX_NAV_POSLLH_I2C",
            "MSGOUT-UBX_NAV_POSLLH_SPI",
            "MSGOUT-UBX_NAV_POSLLH_UART1",
            "MSGOUT-UBX_NAV_POSLLH_UART2",
            "MSGOUT-UBX_NAV_POSLLH_USB",
            "MSGOUT-UBX_NAV_PVT_I2C",
            "MSGOUT-UBX_NAV_PVT_SPI",
            "MSGOUT-UBX_NAV_PVT_UART1",
            "MSGOUT-UBX_NAV_PVT_UART2",
            "MSGOUT-UBX_NAV_PVT_USB",
            "MSGOUT-UBX_NAV_RELPOSNED_I2C",
            "MSGOUT-UBX_NAV_RELPOSNED_SPI",
            "MSGOUT-UBX_NAV_RELPOSNED_UART1",
            "MSGOUT-UBX_NAV_RELPOSNED_UART2",
            "MSGOUT-UBX_NAV_RELPOSNED_USB",
            "MSGOUT-UBX_NAV_SAT_I2C",
            "MSGOUT-UBX_NAV_SAT_SPI",
            "MSGOUT-UBX_NAV_SAT_UART1",
            "MSGOUT-UBX_NAV_SAT_UART2",
            "MSGOUT-UBX_NAV_SAT_USB",
            "MSGOUT-UBX_NAV_SBAS_I2C",
            "MSGOUT-UBX_NAV_SBAS_SPI",
            "MSGOUT-UBX_NAV_SBAS_UART1",
            "MSGOUT-UBX_NAV_SBAS_UART2",
            "MSGOUT-UBX_NAV_SBAS_USB",
            "MSGOUT-UBX_NAV_SIG_I2C",
            "MSGOUT-UBX_NAV_SIG_SPI",
            "MSGOUT-UBX_NAV_SIG_UART1",
            "MSGOUT-UBX_NAV_SIG_UART2",
            "MSGOUT-UBX_NAV_SIG_USB",
            "MSGOUT-UBX_NAV_STATUS_I2C",
            "MSGOUT-UBX_NAV_STATUS_SPI",
            "MSGOUT-UBX_NAV_STATUS_UART1",
            "MSGOUT-UBX_NAV_STATUS_UART2",
            "MSGOUT-UBX_NAV_STATUS_USB",
            "MSGOUT-UBX_NAV_SVIN_I2C",
            "MSGOUT-UBX_NAV_SVIN_SPI",
            "MSGOUT-UBX_NAV_SVIN_UART1",
            "MSGOUT-UBX_NAV_SVIN_UART2",
            "MSGOUT-UBX_NAV_SVIN_USB",
            "MSGOUT-UBX_NAV_TIMEBDS_I2C",
            "MSGOUT-UBX_NAV_TIMEBDS_SPI",
            "MSGOUT-UBX_NAV_TIMEBDS_UART1",
            "MSGOUT-UBX_NAV_TIMEBDS_UART2",
            "MSGOUT-UBX_NAV_TIMEBDS_USB",
            "MSGOUT-UBX_NAV_TIMEGAL_I2C",
            "MSGOUT-UBX_NAV_TIMEGAL_SPI",
            "MSGOUT-UBX_NAV_TIMEGAL_UART1",
            "MSGOUT-UBX_NAV_TIMEGAL_UART2",
            "MSGOUT-UBX_NAV_TIMEGAL_USB",
            "MSGOUT-UBX_NAV_TIMEGLO_I2C",
            "MSGOUT-UBX_NAV_TIMEGLO_SPI",
            "MSGOUT-UBX_NAV_TIMEGLO_UART1",
            "MSGOUT-UBX_NAV_TIMEGLO_UART2",
            "MSGOUT-UBX_NAV_TIMEGLO_USB",
            "MSGOUT-UBX_NAV_TIMEGPS_I2C",
            "MSGOUT-UBX_NAV_TIMEGPS_SPI",
            "MSGOUT-UBX_NAV_TIMEGPS_UART1",
            "MSGOUT-UBX_NAV_TIMEGPS_UART2",
            "MSGOUT-UBX_NAV_TIMEGPS_USB",
            "MSGOUT-UBX_NAV_TIMELS_I2C",
            "MSGOUT-UBX_NAV_TIMELS_SPI",
            "MSGOUT-UBX_NAV_TIMELS_UART1",
            "MSGOUT-UBX_NAV_TIMELS_UART2",
            "MSGOUT-UBX_NAV_TIMELS_USB",
            "MSGOUT-UBX_NAV_TIMEUTC_I2C",
            "MSGOUT-UBX_NAV_TIMEUTC_SPI",
            "MSGOUT-UBX_NAV_TIMEUTC_UART1",
            "MSGOUT-UBX_NAV_TIMEUTC_UART2",
            "MSGOUT-UBX_NAV_TIMEUTC_USB",
            "MSGOUT-UBX_NAV_VELECEF_I2C",
            "MSGOUT-UBX_NAV_VELECEF_SPI",
            "MSGOUT-UBX_NAV_VELECEF_UART1",
            "MSGOUT-UBX_NAV_VELECEF_UART2",
            "MSGOUT-UBX_NAV_VELECEF_USB",
            "MSGOUT-UBX_NAV_VELNED_I2C",
            "MSGOUT-UBX_NAV_VELNED_SPI",
            "MSGOUT-UBX_NAV_VELNED_UART1",
            "MSGOUT-UBX_NAV_VELNED_UART2",
            "MSGOUT-UBX_NAV_VELNED_USB",
            "MSGOUT-UBX_RXM_MEASX_I2C",
            "MSGOUT-UBX_RXM_MEASX_SPI",
            "MSGOUT-UBX_RXM_MEASX_UART1",
            "MSGOUT-UBX_RXM_MEASX_UART2",
            "MSGOUT-UBX_RXM_MEASX_USB",
            "MSGOUT-UBX_RXM_RAWX_I2C",
            "MSGOUT-UBX_RXM_RAWX_SPI",
            "MSGOUT-UBX_RXM_RAWX_UART1",
            "MSGOUT-UBX_RXM_RAWX_UART2",
            "MSGOUT-UBX_RXM_RAWX_USB",
            "MSGOUT-UBX_RXM_RLM_I2C",
            "MSGOUT-UBX_RXM_RLM_SPI",
            "MSGOUT-UBX_RXM_RLM_UART1",
            "MSGOUT-UBX_RXM_RLM_UART2",
            "MSGOUT-UBX_RXM_RLM_USB",
            "MSGOUT-UBX_RXM_RTCM_I2C",
            "MSGOUT-UBX_RXM_RTCM_SPI",
            "MSGOUT-UBX_RXM_RTCM_UART1",
            "MSGOUT-UBX_RXM_RTCM_UART2",
            "MSGOUT-UBX_RXM_RTCM_USB",
            "MSGOUT-UBX_RXM_SFRBX_I2C",
            "MSGOUT-UBX_RXM_SFRBX_SPI",
            "MSGOUT-UBX_RXM_SFRBX_UART1",
            "MSGOUT-UBX_RXM_SFRBX_UART2",
            "MSGOUT-UBX_RXM_SFRBX_USB",
            "MSGOUT-UBX_TIM_SVIN_I2C",
            "MSGOUT-UBX_TIM_SVIN_SPI",
            "MSGOUT-UBX_TIM_SVIN_UART1",
            "MSGOUT-UBX_TIM_SVIN_UART2",
            "MSGOUT-UBX_TIM_SVIN_USB",
            "MSGOUT-UBX_TIM_TM2_I2C",
            "MSGOUT-UBX_TIM_TM2_SPI",
            "MSGOUT-UBX_TIM_TM2_UART1",
            "MSGOUT-UBX_TIM_TM2_UART2",
            "MSGOUT-UBX_TIM_TM2_USB",
            "MSGOUT-UBX_TIM_TP_I2C",
            "MSGOUT-UBX_TIM_TP_SPI",
            "MSGOUT-UBX_TIM_TP_UART1",
            "MSGOUT-UBX_TIM_TP_UART2",
            "MSGOUT-UBX_TIM_TP_USB",
            "MSGOUT-UBX_TIM_VRFY_I2C",
            "MSGOUT-UBX_TIM_VRFY_SPI",
            "MSGOUT-UBX_TIM_VRFY_UART1",
            "MSGOUT-UBX_TIM_VRFY_UART2",
            "MSGOUT-UBX_TIM_VRFY_USB",
            "NAVHPG-DGNSSMODE",
            "NAVSPG-FIXMODE",
            "NAVSPG-INIFIX3D",
            "NAVSPG-WKNROLLOVER",
            "NAVSPG-USE_PPP",
            "NAVSPG-UTCSTANDARD",
            "NAVSPG-DYNMODEL",
            "NAVSPG-ACKAIDING",
            "NAVSPG-USE_USRDAT",
            "NAVSPG-USRDAT_MAJA",
            "NAVSPG-USRDAT_FLAT",
            "NAVSPG-USRDAT_DX",
            "NAVSPG-USRDAT_DY",
            "NAVSPG-USRDAT_DZ",
            "NAVSPG-USRDAT_ROTX",
            "NAVSPG-USRDAT_ROTY",
            "NAVSPG-USRDAT_ROTZ",
            "NAVSPG-USRDAT_SCALE",
            "NAVSPG-INFIL_MINSVS",
            "NAVSPG-INFIL_MAXSVS",
            "NAVSPG-INFIL_MINCNO",
            "NAVSPG-INFIL_MINELEV",
            "NAVSPG-INFIL_NCNOTHRS",
            "NAVSPG-INFIL_CNOTHRS",
            "NAVSPG-OUTFIL_PDOP",
            "NAVSPG-OUTFIL_TDOP",
            "NAVSPG-OUTFIL_PACC",
            "NAVSPG-OUTFIL_TACC",
            "NAVSPG-OUTFIL_FACC",
            "NAVSPG-CONSTR_ALT",
            "NAVSPG-CONSTR_ALTVAR",
            "NAVSPG-CONSTR_DGNSSTO",
            "NMEA-PROTVER",
            "NMEA-MAXSVS",
            "NMEA-COMPAT",
            "NMEA-CONSIDER",
            "NMEA-LIMIT82",
            "NMEA-HIGHPREC",
            "NMEA-SVNUMBERING",
            "NMEA-FILT_GPS",
            "NMEA-FILT_SBAS",
            "NMEA-FILT_QZSS",
            "NMEA-FILT_GLO",
            "NMEA-FILT_BDS",
            "NMEA-OUT_INVFIX",
            "NMEA-OUT_MSKFIX",
            "NMEA-OUT_INVTIME",
            "NMEA-OUT_INVDATE",
            "NMEA-OUT_ONLYGPS",
            "NMEA-OUT_FROZENCOG",
            "NMEA-MAINTALKERID",
            "NMEA-GSVTALKERID",
            "NMEA-BDSTALKERID",
            "ODO-USE_ODO",
            "ODO-USE_COG",
            "ODO-OUTLPVEL",
            "ODO-OUTLPCOG",
            "ODO-PROFILE",
            "ODO-COGMAXSPEED",
            "ODO-COGMAXPOSACC",
            "ODO-COGLPGAIN",
            "ODO-VELLPGAIN",
            "RATE-MEAS",
            "RATE-NAV",
            "RATE-TIMEREF",
            "RINV-DUMP",
            "RINV-BINARY",
            "RINV-DATA_SIZE",
            "RINV-CHUNK0",
            "RINV-CHUNK1",
            "RINV-CHUNK2",
            "RINV-CHUNK3",
            "SBAS-USE_TESTMODE",
            "SBAS-USE_RANGING",
            "SBAS-USE_DIFFCORR",
            "SBAS-USE_INTEGRITY",
            "SBAS-PRNSCANMASK",
            "SIGNAL-GPS_ENA",
            "SIGNAL-GPS_L1CA_ENA",
            "SIGNAL-GPS_L2C_ENA",
            "SIGNAL-SBAS_ENA",
            "SIGNAL-SBAS_L1CA_ENA",
            "SIGNAL-GAL_ENA",
            "SIGNAL-GAL_E1_ENA",
            "SIGNAL-GAL_E5B_ENA",
            "SIGNAL-BDS_ENA",
            "SIGNAL-BDS_B1_ENA",
            "SIGNAL-BDS_B2_ENA",
            "SIGNAL-QZSS_ENA",
            "SIGNAL-QZSS_L1CA_ENA",
            "SIGNAL-QZSS_L1S_ENA",
            "SIGNAL-QZSS_L2C_ENA",
            "SIGNAL-GLO_ENA",
            "SIGNAL-GLO_L1_ENA",
            "SIGNAL-GLO_L2_ENA",
            "SPI-MAXFF",
            "SPI-CPOLARITY",
            "SPI-CPHASE",
            "SPI-EXTENDEDTIMEOUT",
            "SPI-ENABLED",
            "SPIINPROT-UBX",
            "SPIINPROT-NMEA",
            "SPIINPROT-RTCM2X",
            "SPIINPROT-RTCM3X",
            "SPIOUTPROT-UBX",
            "SPIOUTPROT-NMEA",
            "SPIOUTPROT-RTCM3X",
            "TMODE-MODE",
            "TMODE-POS_TYPE",
            "TMODE-ECEF_X",
            "TMODE-ECEF_Y",
            "TMODE-ECEF_Z",
            "TMODE-ECEF_X_HP",
            "TMODE-ECEF_Y_HP",
            "TMODE-ECEF_Z_HP",
            "TMODE-LAT",
            "TMODE-LON",
            "TMODE-HEIGHT",
            "TMODE-LAT_HP",
            "TMODE-LON_HP",
            "TMODE-HEIGHT_HP",
            "TMODE-FIXED_POS_ACC",
            "TMODE-SVIN_MIN_DUR",
            "TMODE-SVIN_ACC_LIMIT",
            "TP-PULSE_DEF",
            "TP-PULSE_LENGTH_DEF",
            "TP-ANT_CABLEDELAY",
            "TP-PERIOD_TP1",
            "TP-PERIOD_LOCK_TP1",
            "TP-FREQ_TP1",
            "TP-FREQ_LOCK_TP1",
            "TP-LEN_TP1",
            "TP-LEN_LOCK_TP1",
            "TP-DUTY_TP1",
            "TP-DUTY_LOCK_TP1",
            "TP-USER_DELAY_TP1",
            "TP-TP1_ENA",
            "TP-SYNC_GNSS_TP1",
            "TP-USE_LOCKED_TP1",
            "TP-ALIGN_TO_TOW_TP1",
            "TP-POL_TP1",
            "TP-TIMEGRID_TP1",
            "TP-PERIOD_TP2",
            "TP-PERIOD_LOCK_TP2",
            "TP-FREQ_TP2",
            "TP-FREQ_LOCK_TP2",
            "TP-LEN_TP2",
            "TP-LEN_LOCK_TP2",
            "TP-DUTY_TP2",
            "TP-DUTY_LOCK_TP2",
            "TP-USER_DELAY_TP2",
            "TP-TP2_ENA",
            "TP-SYNC_GNSS_TP2",
            "TP-USE_LOCKED_TP2",
            "TP-ALIGN_TO_TOW_TP2",
            "TP-POL_TP2",
            "TP-TIMEGRID_TP2",
            "UART1-BAUDRATE",
            "UART1-STOPBITS",
            "UART1-DATABITS",
            "UART1-PARITY",
            "UART1-ENABLED",
            "UART1INPROT-UBX",
            "UART1INPROT-NMEA",
            "UART1INPROT-RTCM2X",
            "UART1INPROT-RTCM3X",
            "UART1OUTPROT-UBX",
            "UART1OUTPROT-NMEA",
            "UART1OUTPROT-RTCM3X",
            "UART2-BAUDRATE",
            "UART2-STOPBITS",
            "UART2-DATABITS",
            "UART2-PARITY",
            "UART2-ENABLED",
            "UART2-REMAP",
            "UART2INPROT-UBX",
            "UART2INPROT-NMEA",
            "UART2INPROT-RTCM2X",
            "UART2INPROT-RTCM3X",
            "UART2OUTPROT-UBX",
            "UART2OUTPROT-NMEA",
            "UART2OUTPROT-RTCM3X",
            "USB-ENABLED",
            "USB-SELFPOW",
            "USB-VENDOR_ID",
            "USB-PRODUCT_ID",
            "USB-POWER",
            "USB-VENDOR_STR0",
            "USB-VENDOR_STR1",
            "USB-VENDOR_STR2",
            "USB-VENDOR_STR3",
            "USB-PRODUCT_STR0",
            "USB-PRODUCT_STR1",
            "USB-PRODUCT_STR2",
            "USB-PRODUCT_STR3",
            "USB-SERIAL_NO_STR0",
            "USB-SERIAL_NO_STR1",
            "USB-SERIAL_NO_STR2",
            "USB-SERIAL_NO_STR3",
            "USBINPROT-UBX",
            "USBINPROT-NMEA",
            "USBINPROT-RTCM2X",
            "USBINPROT-RTCM3X",
            "USBOUTPROT-UBX",
            "USBOUTPROT-NMEA",
            "USBOUTPROT-RTCM3X",
            ""};

        static const unsigned long vid[] = {
            0x20240011, 0x10240012, 0x20240013, 0x20240014, 0x10240020,
            0x40240021, 0x40240022, 0x40240023, 0x10240030, 0x40240031,
            0x40240032, 0x40240033, 0x10240040, 0x40240041, 0x40240042,
            0x40240043, 0x10240050, 0x40240051, 0x40240052, 0x40240053,
            0x10a3002e, 0x10a3002f, 0x10a30030, 0x10a30031, 0x10a30032,
            0x10a30033, 0x10a30034, 0x10a30035, 0x20a30036, 0x20a30037,
            0x20a30038, 0x20510001, 0x10510002, 0x10510003, 0x10710001,
            0x10710002, 0x10710003, 0x10710004, 0x10720001, 0x10720002,
            0x10720004, 0x20920001, 0x20920002, 0x20920003, 0x20920004,
            0x20920005, 0x20920006, 0x20920007, 0x20920008, 0x20920009,
            0x2092000a, 0x20410001, 0x20410002, 0x1041000d, 0x20410010,
            0x10410013, 0x10de0002, 0x10de0003, 0x10de0004, 0x30de0005,
            0x30de0006, 0x30de0007, 0x40de0008, 0x20250038, 0x3025003b,
            0x209100a6, 0x209100aa, 0x209100a7, 0x209100a8, 0x209100a9,
            0x209100dd, 0x209100e1, 0x209100de, 0x209100df, 0x209100e0,
            0x209100ba, 0x209100be, 0x209100bb, 0x209100bc, 0x209100bd,
            0x209100c9, 0x209100cd, 0x209100ca, 0x209100cb, 0x209100cc,
            0x209100b5, 0x209100b9, 0x209100b6, 0x209100b7, 0x209100b8,
            0x209100ce, 0x209100d2, 0x209100cf, 0x209100d0, 0x209100d1,
            0x209100bf, 0x209100c3, 0x209100c0, 0x209100c1, 0x209100c2,
            0x209100d3, 0x209100d7, 0x209100d4, 0x209100d5, 0x209100d6,
            0x209100c4, 0x209100c8, 0x209100c5, 0x209100c6, 0x209100c7,
            0x209100ab, 0x209100af, 0x209100ac, 0x209100ad, 0x209100ae,
            0x209100e7, 0x209100eb, 0x209100e8, 0x209100e9, 0x209100ea,
            0x209100b0, 0x209100b4, 0x209100b1, 0x209100b2, 0x209100b3,
            0x209100d8, 0x209100dc, 0x209100d9, 0x209100da, 0x209100db,
            0x209100ec, 0x209100f0, 0x209100ed, 0x209100ee, 0x209100ef,
            0x209100f1, 0x209100f5, 0x209100f2, 0x209100f3, 0x209100f4,
            0x209100f6, 0x209100fa, 0x209100f7, 0x209100f8, 0x209100f9,
            0x209102bd, 0x209102c1, 0x209102be, 0x209102bf, 0x209102c0,
            0x2091035e, 0x20910362, 0x2091035f, 0x20910360, 0x20910361,
            0x209102cc, 0x209102d0, 0x209102cd, 0x209102ce, 0x209102cf,
            0x209102d1, 0x20910367, 0x20910364, 0x20910365, 0x20910366,
            0x209102d5, 0x209102d2, 0x209102d3, 0x209102d4, 0x20910368,
            0x2091036c, 0x20910369, 0x2091036a, 0x2091036b, 0x20910318,
            0x2091031c, 0x20910319, 0x2091031a, 0x2091031b, 0x2091036d,
            0x20910371, 0x2091036e, 0x2091036f, 0x20910370, 0x209102d6,
            0x209102da, 0x209102d7, 0x209102d8, 0x209102d9, 0x20910303,
            0x20910307, 0x20910304, 0x20910305, 0x20910306, 0x209102fe,
            0x20910302, 0x209102ff, 0x20910300, 0x20910301, 0x20910381,
            0x20910385, 0x20910382, 0x20910383, 0x20910384, 0x20910259,
            0x2091025d, 0x2091025a, 0x2091025b, 0x2091025c, 0x2091034f,
            0x20910353, 0x20910350, 0x20910351, 0x20910352, 0x209101b9,
            0x209101bd, 0x209101ba, 0x209101bb, 0x209101bc, 0x20910354,
            0x20910358, 0x20910355, 0x20910356, 0x20910357, 0x209101b4,
            0x209101b8, 0x209101b5, 0x209101b6, 0x209101b7, 0x209101a5,
            0x209101a9, 0x209101a6, 0x209101a7, 0x209101a8, 0x20910196,
            0x2091019a, 0x20910197, 0x20910198, 0x20910199, 0x20910359,
            0x2091035d, 0x2091035a, 0x2091035b, 0x2091035c, 0x209101a0,
            0x209101a4, 0x209101a1, 0x209101a2, 0x209101a3, 0x20910187,
            0x2091018b, 0x20910188, 0x20910189, 0x2091018a, 0x2091019b,
            0x2091019f, 0x2091019c, 0x2091019d, 0x2091019e, 0x2091019b,
            0x2091019f, 0x2091019c, 0x2091019d, 0x2091019e, 0x20910065,
            0x20910069, 0x20910066, 0x20910067, 0x20910068, 0x20910038,
            0x2091003c, 0x20910039, 0x2091003a, 0x2091003b, 0x2091015f,
            0x20910163, 0x20910160, 0x20910161, 0x20910162, 0x209100a1,
            0x209100a5, 0x209100a2, 0x209100a3, 0x209100a4, 0x2091002e,
            0x20910032, 0x2091002f, 0x20910030, 0x20910031, 0x20910033,
            0x20910037, 0x20910034, 0x20910035, 0x20910036, 0x2091007e,
            0x20910082, 0x2091007f, 0x20910080, 0x20910081, 0x20910010,
            0x20910014, 0x20910011, 0x20910012, 0x20910013, 0x20910024,
            0x20910028, 0x20910025, 0x20910026, 0x20910027, 0x20910029,
            0x2091002d, 0x2091002a, 0x2091002b, 0x2091002c, 0x20910006,
            0x2091000a, 0x20910007, 0x20910008, 0x20910009, 0x2091008d,
            0x20910091, 0x2091008e, 0x2091008f, 0x20910090, 0x20910015,
            0x20910019, 0x20910016, 0x20910017, 0x20910018, 0x2091006a,
            0x2091006e, 0x2091006b, 0x2091006c, 0x2091006d, 0x20910345,
            0x20910349, 0x20910346, 0x20910347, 0x20910348, 0x2091001a,
            0x2091001e, 0x2091001b, 0x2091001c, 0x2091001d, 0x20910088,
            0x2091008c, 0x20910089, 0x2091008a, 0x2091008b, 0x20910051,
            0x20910055, 0x20910052, 0x20910053, 0x20910054, 0x20910056,
            0x2091005a, 0x20910057, 0x20910058, 0x20910059, 0x2091004c,
            0x20910050, 0x2091004d, 0x2091004e, 0x2091004f, 0x20910047,
            0x2091004b, 0x20910048, 0x20910049, 0x2091004a, 0x20910060,
            0x20910064, 0x20910061, 0x20910062, 0x20910063, 0x2091005b,
            0x2091005f, 0x2091005c, 0x2091005d, 0x2091005e, 0x2091003d,
            0x20910041, 0x2091003e, 0x2091003f, 0x20910040, 0x20910042,
            0x20910046, 0x20910043, 0x20910044, 0x20910045, 0x20910204,
            0x20910208, 0x20910205, 0x20910206, 0x20910207, 0x209102a4,
            0x209102a8, 0x209102a5, 0x209102a6, 0x209102a7, 0x2091025e,
            0x20910262, 0x2091025f, 0x20910260, 0x20910261, 0x20910268,
            0x2091026c, 0x20910269, 0x2091026a, 0x2091026b, 0x20910231,
            0x20910235, 0x20910232, 0x20910233, 0x20910234, 0x20910097,
            0x2091009b, 0x20910098, 0x20910099, 0x2091009a, 0x20910178,
            0x2091017c, 0x20910179, 0x2091017a, 0x2091017b, 0x2091017d,
            0x20910181, 0x2091017e, 0x2091017f, 0x20910180, 0x20910092,
            0x20910096, 0x20910093, 0x20910094, 0x20910095, 0x20140011,
            0x20110011, 0x10110013, 0x30110017, 0x10110019, 0x2011001c,
            0x20110021, 0x10110025, 0x10110061, 0x50110062, 0x50110063,
            0x40110064, 0x40110065, 0x40110066, 0x40110067, 0x40110068,
            0x40110069, 0x4011006a, 0x201100a1, 0x201100a2, 0x201100a3,
            0x201100a4, 0x201100aa, 0x201100ab, 0x301100b1, 0x301100b2,
            0x301100b3, 0x301100b4, 0x301100b5, 0x401100c1, 0x401100c2,
            0x201100c4, 0x20930001, 0x20930002, 0x10930003, 0x10930004,
            0x10930005, 0x10930006, 0x20930007, 0x10930011, 0x10930012,
            0x10930015, 0x10930016, 0x10930017, 0x10930021, 0x10930022,
            0x10930023, 0x10930024, 0x10930025, 0x10930026, 0x20930031,
            0x20930032, 0x30930033, 0x10220001, 0x10220002, 0x10220003,
            0x10220004, 0x20220005, 0x20220021, 0x20220022, 0x20220032,
            0x20220031, 0x30210001, 0x30210002, 0x20210003, 0x10c70001,
            0x10c70002, 0x20c70003, 0x50c70004, 0x50c70005, 0x50c70006,
            0x50c70007, 0x10360002, 0x10360003, 0x10360004, 0x10360005,
            0x50360006, 0x1031001f, 0x10310001, 0x10310003, 0x10310020,
            0x10310005, 0x10310021, 0x10310007, 0x1031000a, 0x10310022,
            0x1031000d, 0x1031000e, 0x10310024, 0x10310012, 0x10310014,
            0x10310015, 0x10310025, 0x10310018, 0x1031001a, 0x20640001,
            0x10640002, 0x10640003, 0x10640005, 0x10640006, 0x10790001,
            0x10790002, 0x10790003, 0x10790004, 0x107a0001, 0x107a0002,
            0x107a0004, 0x20030001, 0x20030002, 0x40030003, 0x40030004,
            0x40030005, 0x20030006, 0x20030007, 0x20030008, 0x40030009,
            0x4003000a, 0x4003000b, 0x2003000c, 0x2003000d, 0x2003000e,
            0x4003000f, 0x40030010, 0x40030011, 0x20050023, 0x20050030,
            0x30050001, 0x40050002, 0x40050003, 0x40050024, 0x40050025,
            0x40050004, 0x40050005, 0x5005002a, 0x5005002b, 0x40050006,
            0x10050007, 0x10050008, 0x10050009, 0x1005000a, 0x1005000b,
            0x2005000c, 0x4005000d, 0x4005000e, 0x40050026, 0x40050027,
            0x4005000f, 0x40050010, 0x5005002c, 0x5005002d, 0x40050011,
            0x10050012, 0x10050013, 0x10050014, 0x10050015, 0x10050016,
            0x20050017, 0x40520001, 0x20520002, 0x20520003, 0x20520004,
            0x10520005, 0x10730001, 0x10730002, 0x10730003, 0x10730004,
            0x10740001, 0x10740002, 0x10740004, 0x40530001, 0x20530002,
            0x20530003, 0x20530004, 0x10530005, 0x10530006, 0x10750001,
            0x10750002, 0x10750003, 0x10750004, 0x10760001, 0x10760002,
            0x10760004, 0x10650001, 0x10650002, 0x3065000a, 0x3065000b,
            0x3065000c, 0x5065000d, 0x5065000e, 0x5065000f, 0x50650010,
            0x50650011, 0x50650012, 0x50650013, 0x50650014, 0x50650015,
            0x50650016, 0x50650017, 0x50650018, 0x10770001, 0x10770002,
            0x10770003, 0x10770004, 0x10780001, 0x10780002, 0x10780004};

        static const int vprm[] = {
            FU1, FU1, FU1, FU1, FU1, FI4, FI4, FU4, FU1, FI4,
            FI4, FU4, FU1, FI4, FI4, FU4, FU1, FI4, FI4, FU4,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU2,
            FU2, FU2, FU4, FU1, FU2, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU2, FU1, FU1,
            FU1, FU1, FU1, FR8, FR8, FR4, FR4, FR4, FR4, FR4,
            FR4, FR4, FU1, FU1, FU1, FI1, FU1, FU1, FU2, FU2,
            FU2, FU2, FU2, FI4, FU4, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU2, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU2, FU2, FU1, FU1,
            FU1, FU1, FU8, FU8, FU8, FU8, FU1, FU1, FU1, FU1,
            FU8, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FI4, FI4, FI4, FI1, FI1, FI1, FI4,
            FI4, FI4, FI1, FI1, FI1, FU4, FU4, FU4, FU1, FU1,
            FI2, FU4, FU4, FU4, FU4, FU4, FU4, FR8, FR8, FI4,
            FU1, FU1, FU1, FU1, FU1, FU1, FU4, FU4, FU4, FU4,
            FU4, FU4, FR8, FR8, FI4, FU1, FU1, FU1, FU1, FU1,
            FU1, FU4, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU4, FU1, FU1, FU1, FU1, FU1, FU1,
            FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU1, FU2, FU2,
            FU2, FU8, FU8, FU8, FU8, FU8, FU8, FU8, FU8, FU8,
            FU8, FU8, FU8, FU1, FU1, FU1, FU1, FU1, FU1, FU1};

        if (strncmp(args[j], "CFG-", 4))
            return 0;

        for (k = 0; *vcmd[k]; k++)
        {
            if (!strcmp(args[j] + 4, vcmd[k]))
                break;
        }

        if (!*vcmd[k])
            return 0;

        setU4(q, (unsigned long)vid[k]);
        q += 4;

        /* Set value */
        switch (vprm[k])
        {
        case FU1:
            setU1(q, (unsigned char)atoi(args[j + 1]));
            q += 1;
            break;
        case FU2:
            setU2(q, (unsigned short)atoi(args[j + 1]));
            q += 2;
            break;
        case FU4:
            setU4(q, (unsigned long)atoi(args[j + 1]));
            q += 4;
            break;
        /*
        case FU8:
            setU8(q, (unsigned long long)atoi(args[j + 2]));
            q += 8;
            break;
        */
        case FI1:
            setI1(q, (signed char)atoi(args[j + 1]));
            q += 1;
            break;
        case FI2:
            setI2(q, (signed short)atoi(args[j + 1]));
            q += 2;
            break;
        case FI4:
            setI4(q, (signed long)atoi(args[j + 1]));
            q += 4;
            break;
        case FR4:
            setR4(q, (float)atof(args[j + 1]));
            q += 4;
            break;
        case FR8:
            setR8(q, (double)atof(args[j + 1]));
            q += 8;
            break;
        case FS32:
            sprintf((char *)q, "%-32.32s", args[j + 1]);
            q += 32;
            break;
        default:
            setU1(q, (unsigned char)atoi(args[j + 1]));
            q += 1;
            break;
        }
    }

    n = (int)(q - buff) + 2;
    setU2(buff + 4, (unsigned short)(n - 8));
    set_checksum(buff, n);
    return n;
}

// void print_compare(uint8_t *array, uint32_t n, char *msg)
// {
//     char *buffer = calloc(128, sizeof(char));
//     char *p = buffer;

//     for (int i = 0; i < n; i++)
//     {
//         sprintf(p, "%02x ", array[i]);
//         p += 3;
//     }

//     *(p - 1) = 0; // go back one space, and terminate the string

//     printf("%s\n", buffer);
//     printf("%s\n", msg);
//     if (strcmp(buffer, msg) == 0)
//         printf("OK\n");
//     else
//         printf("NOT OK\n");

//     free(buffer);
// }

/*
 20.9600040
105.7684480
 -1.0000000
*/

// int main(void)
// {
//     uint8_t *msg = calloc(64, sizeof(char));
//     uint8_t *buffer = calloc(32, sizeof(uint8_t));

//     uint32_t n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-MODE 0", buffer);
//     print_compare(buffer, n, "b5 62 06 8a 09 00 00 01 00 00 01 00 03 20 00 be 7f");

//     memset(buffer, 0, 32);
//     n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-MODE 2", buffer);
//     print_compare(buffer, n, "b5 62 06 8a 09 00 00 01 00 00 01 00 03 20 02 c0 81");

//     memset(buffer, 0, 32);
//     n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-POS_TYPE 1", buffer);
//     print_compare(buffer, n, "b5 62 06 8a 09 00 00 01 00 00 02 00 03 20 01 c0 85");

//     memset(buffer, 0, 32);
//     double lat = 20.9600040;
//     memset(msg, 0, 64);
//     sprintf(msg, "CFG-VALSET 0 1 0 0 CFG-TMODE-LAT %i", (int32_t)(lat * pow(10, 7)));
//     n = ubx_gen_cmd(msg, buffer);
//     print_compare(buffer, n, "b5 62 06 8a 0c 00 00 01 00 00 09 00 03 40 28 3e 7e 0c d9 25");

//     memset(buffer, 0, 32);
//     n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-LON 1057684480", buffer);
//     print_compare(buffer, n, "b5 62 06 8a 0c 00 00 01 00 00 0a 00 03 40 00 fc 0a 3f 2f 12");

//     memset(buffer, 0, 32);
//     n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-HEIGHT -100", buffer);
//     print_compare(buffer, n, "b5 62 06 8a 0c 00 00 01 00 00 0b 00 03 40 9c ff ff ff 84 3d");

//     free(msg);
//     free(buffer);
// }
