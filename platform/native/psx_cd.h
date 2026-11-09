#ifndef PSX_CD_H
#define PSX_CD_H

/* The CD-ROM drive on a disc image file. The image may be cooked (2048 bytes per sector, an
 * .iso) or raw (2352 bytes, a .bin of a MODE2 disc); the layout is detected. The drive's
 * commands complete at once, and its read position advances with the vertical blank
 * (75 sectors a second, 150 at double speed). */

#include <stdint.h>

/* Open a disc image; returns 0 on success. Registers the drive with VSync. */
int psx_cd_open(const char *path);
void psx_cd_close(void);
int psx_cd_is_open(void);

/* Read the 2048 data bytes of sector `lba` (the disc's first sector is 0). Returns 0 on success. */
int psx_cd_read_sector(uint32_t lba, uint8_t out[2048]);

/* The drive's read position as a sector number, and whether it is streaming (ReadN/ReadS). */
int psx_cd_position(void);
int psx_cd_streaming(void);

#endif /* PSX_CD_H */
