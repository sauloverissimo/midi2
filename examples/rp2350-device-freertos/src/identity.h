/*
 * identity.h - the device identity, declared once.
 *
 * Three paths carry these same four values and must agree, or a host that
 * reads two of them sees a device contradicting itself:
 *   - MIDI-CI Discovery            (ci_responder.c)
 *   - UMP Stream Device Identity   (usb_descriptors.c, answered by TinyUSB)
 *   - the catalog sample message   (catalog.c, case 55)
 *   - the Property Exchange DeviceInfo resource (ci_responder.c)
 *
 * The individual bytes are the source; the packed words and the DeviceInfo
 * JSON fragments are derived from them at compile time, so a change here
 * reaches every channel or none.
 */
#ifndef IDENTITY_H
#define IDENTITY_H

/* 0x7D (125) is the educational/prototyping SysEx prefix, in the first of
 * the three manufacturer bytes. A shipping product uses a licensed
 * Manufacturer ID. */
#define DEV_MFR_B1      125
#define DEV_MFR_B2      0
#define DEV_MFR_B3      0

/* Family and model are 14-bit values carried as 7-bit LSB/MSB pairs. */
#define DEV_FAMILY_LSB  1
#define DEV_FAMILY_MSB  0
#define DEV_MODEL_LSB   1
#define DEV_MODEL_MSB   0

/* Software Revision Level: four manufacturer defined bytes. */
#define DEV_VER_B1      0
#define DEV_VER_B2      1
#define DEV_VER_B3      0
#define DEV_VER_B4      0

/* Packed forms the builders take. */
#define DEV_MFR_ID   (((uint32_t)DEV_MFR_B1 << 16) | ((uint32_t)DEV_MFR_B2 << 8) | (uint32_t)DEV_MFR_B3)
#define DEV_FAMILY   ((uint16_t)(((uint16_t)DEV_FAMILY_MSB << 7) | DEV_FAMILY_LSB))
#define DEV_MODEL    ((uint16_t)(((uint16_t)DEV_MODEL_MSB << 7) | DEV_MODEL_LSB))
#define DEV_VERSION  (((uint32_t)DEV_VER_B1 << 24) | ((uint32_t)DEV_VER_B2 << 16) | \
                      ((uint32_t)DEV_VER_B3 << 8) | (uint32_t)DEV_VER_B4)

/* DeviceInfo JSON fragments (M2-105), stringized from the same bytes. */
#define DEV_STR_(x) #x
#define DEV_STR(x)  DEV_STR_(x)
#define DEV_JSON_MFR_ID     "[" DEV_STR(DEV_MFR_B1) "," DEV_STR(DEV_MFR_B2) "," DEV_STR(DEV_MFR_B3) "]"
#define DEV_JSON_FAMILY_ID  "[" DEV_STR(DEV_FAMILY_LSB) "," DEV_STR(DEV_FAMILY_MSB) "]"
#define DEV_JSON_MODEL_ID   "[" DEV_STR(DEV_MODEL_LSB) "," DEV_STR(DEV_MODEL_MSB) "]"
#define DEV_JSON_VERSION_ID "[" DEV_STR(DEV_VER_B1) "," DEV_STR(DEV_VER_B2) "," \
                                DEV_STR(DEV_VER_B3) "," DEV_STR(DEV_VER_B4) "]"

#endif /* IDENTITY_H */
