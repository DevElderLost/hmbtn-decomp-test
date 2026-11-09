#ifndef PSX_MCARD_H
#define PSX_MCARD_H

/* Memory cards backed by files. A card is the console's 128 KB image (16 blocks of 8 KB: the
 * header and directory, then 15 blocks of save data) in the console's own format, so a card
 * file works in other tools too. */

#define PSX_MCARD_SIZE 131072

/* Attach slot 0 or 1 to a card file. A file that does not exist yet is created blank
 * (unformatted, as a new card is). Returns 0 on success. */
int psx_mcard_attach(int slot, const char *path);
void psx_mcard_detach(int slot);

/* Whether the card in `slot` has a valid header (is formatted). */
int psx_mcard_formatted(int slot);

#endif /* PSX_MCARD_H */
