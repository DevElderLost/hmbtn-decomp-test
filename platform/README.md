# Build Android native (tanpa emulator)

Lapisan platform untuk menjalankan decomp ini sebagai aplikasi Android native.

- **Tahap 1** (selesai): kontroler layar sentuh, shim `libpad`, front end SDL2, proyek Android + CI.
- **Tahap 2** (selesai): GTE/GPU software dan header compat PSY-Q untuk grafis dan geometri.
- **Tahap 3** (selesai): CD, memory card, API suara, overlay, dan penyambungan `main()` game. Seluruh
  113 file source ter-compile dan ter-link; `main()` asli game berjalan sampai meminta file pertama dari disc.
  **Belum diuji dengan disc asli**, dan **belum ada suara**.
- **Tahap 4** (belum): suara (SEQ/VAB/XA), lalu perbaikan dari uji dengan data asli.

## Isi

| Path | Fungsi |
|---|---|
| `native/psx_mem.[ch]` | Memetakan RAM PS1 (2 MB di `0x80000000`) dan scratchpad (`0x1F800000`) di alamat aslinya. Game menyimpan OT, buffer primitif, dan work area di alamat tetap (`WORK_REGION = 0x801BE1B0`, dst.) dan menautkan primitif lewat tag 24-bit, jadi cara ini membuat semuanya jalan tanpa mengubah source. |
| `native/psx_gte.[ch]` | GTE software: register file asli, perintah `RTPS RTPT NCLIP AVSZ3 AVSZ4 MVMVA SQR OP GPF GPL`, saturasi dan FLAG. Perintah pencahayaan tidak dipakai game dan belum diimplementasi (dilaporkan sekali ke stderr). |
| `native/psx_gpu.[ch]` | GPU software: VRAM 1024x512, interpreter command GP0 (paket primitif game dijalankan apa adanya), poligon flat/gouraud/bertekstur 4/8/15-bit dengan CLUT, 4 mode semi-transparan, dither 4x4, sprite, tile, garis, transfer VRAM, draw area/offset. Walk ordering table lewat RAM PS1. |
| `native/psx_libgte.c`, `psx_libgpu.c` | Implementasi fungsi `libgte`, `libgpu`, `libgs` yang dipakai game (`RotTrans`, `PutDrawEnv`, `ClearOTagR`, `DrawOTag`, `VSync`, ...). |
| `compat/psyq/` | Header pengganti PSY-Q (`libgte.h`, `libgpu.h`, `libgs.h`, `inline_c.h`, `sys/types.h`, ...), ditulis sendiri dengan layout struct sama dengan hardware. `u_long` 32-bit seperti di konsol. |
| `compat/override/` | `gte.h` dan `cpu_asm.h` versi native (tanpa inline asm MIPS). Dipasang lebih dulu di include path, jadi source game tidak diubah. |
| `native/psx_cd.[ch]`, `compat/psyq/libcd.h` | Drive CD di atas image disc: `.iso` 2048 byte atau `.bin` mentah 2352 byte (terdeteksi otomatis), ISO 9660, `CdSearchFile`, `CdRead`, posisi MSF, perintah `CdControl`. Posisi kepala bergerak 75 sektor/detik (150 pada kecepatan ganda) mengikuti vblank. |
| `native/psx_mcard.[ch]`, `compat/psyq/libmcrd.h` | Memory card sebagai file 128 KB dalam format asli (header, 15 frame direktori, rantai blok, checksum), jadi bisa dibuka alat lain. PocketStation (`Mcx*`) dilaporkan tidak ada. |
| `native/psx_snd.[ch]`, `compat/psyq/libsnd.h`, `libspu.h` | Status library suara (bank, sequence, voice) tanpa sintesis. Voice yang di-key-on otomatis mati setelah 60 vblank agar game tidak mengira voice-nya penuh. |
| `native/psx_system.c`, `psx_platform.[ch]` | `SystemError`, `ResetCallback`, dst.; hook pesan fatal yang ditampilkan front end di layar. |
| `native/overlay_native.c` | Dua region overlay sebagai alamat RAM PS1 (`0x800B7E20`, `0x8012DDE8`), mengganti `src/main/overlay_regions.c`. |
| `native/demo_cube.[ch]` | Kubus berputar yang digambar lewat API compat persis seperti kode game. Bukti bahwa jalur grafis jalan di perangkat. |
| `native/main_sdl.c` | Front end SDL2: input sentuh/gamepad/keyboard, gambar GPU dengan kontroler di atasnya. |
| `check_game_sources.sh` | Compile-check semua source game terhadap header compat: **113 dari 113 file** lolos. |
| `native/tests/boot_smoke.sh` | Meng-compile seluruh game, me-link-nya, dan menjalankan `main()` asli pada disc tanpa file game. Harus berhenti setelah >= 10 vblank dengan permintaan `A_FILE.HDT`. |
| `run_host_tests.sh` | Semua unit test + dua pengecekan di atas. Dijalankan oleh CI. |

## Cara pakai

1. Apply patch, push, jalankan workflow **android**, unduh artifact `hmbtn-android-debug-apk`, install.
2. Letakkan image disc milik sendiri (`disc.bin`, `disc.iso` atau `disc.img`) di folder data aplikasi.
   Layar menampilkan jalur tepatnya ("NO DISC: PUT DISC.BIN IN ..."); di Android biasanya
   `/sdcard/Android/data/com.develderlost.hmbtn/files/`. Tanpa disc, aplikasi menjalankan kubus demo.
   Memory card disimpan di folder yang sama sebagai `memcard1.mcr`.
3. Jika game berhenti, alasannya tampil di layar (mis. file yang tidak ada di disc). Kirim teksnya.
4. Di desktop: `sh platform/run_host_tests.sh`; jalankan app dengan folder data sebagai argumen atau
   `HMBTN_DIR`. Kontrol keyboard: panah, Z=✕, X=○, A=□, S=△, Q/W=L1/R1, E/R=L2/R2, Enter=Start, Tab=Select.

Default hanya ABI `arm64-v8a`. Untuk 32-bit: `-Phmbtn.abis=arm64-v8a,armeabi-v7a`.

## Perubahan pada source game

Hanya 12 file, 34 baris tambah / 13 baris hapus, semuanya di balik `HMBTN_NATIVE` atau typedef/makro yang
di build konsol mengembang ke teks yang sama dengan aslinya (hasil preprocess ketujuh file `.c` yang
diubah identik token-per-token dengan sebelumnya):

- `types.h`: `addr32` (alamat RAM dalam integer; `s32` di konsol, `u32` di native) dan `ADDR_OFFSET`.
- `anim.h`: `s32 start[0]` menjadi `s32 start[]` di native. gcc modern **membuang** elemen initializer
  yang berlebih pada array panjang nol tanpa error, sehingga 105 tabel data farm akan kosong.
- Fungsi yang mengembalikan alamat sebagai `s32` (`main_get_region_base`, `player_start_nearby_actions`,
  `family_table_lookup`) memakai `addr32`: alamat `0x80xxxxxx` sebagai `s32` bernilai negatif dan di-sign-extend
  menjadi pointer 64-bit yang tidak valid.
- Lima lokasi `(s32)pointer + offset` memakai `ADDR_OFFSET`, karena pointer ke variabel global host tidak
  muat di 32 bit.

Sisa 25 peringatan cast pointer/integer sudah ditelaah satu per satu (pemakaian 32 bit bawah, selisih, offset
dari null) dan aman. Satu hal yang belum bisa diperiksa tanpa disc: tabel yang berisi alamat RAM mentah
(mis. `family_tables`) membaca data dari image overlay yang dibaca `LOAD_OVERLAY` ke region RAM.

## Batas ketelitian

- Pembagian perspektif GTE memakai hasil bagi eksak, bukan tabel UNR hardware: koordinat bisa beda 1 piksel.
- Urutan `RotMatrix` dan `RotMatrixZXY`, rentang `ratan2` (-2048..2048), `csqrt`, dan `SetFogNear`
  mengikuti dokumentasi yang diingat, belum dibandingkan dengan hasil game asli.
- GPU: tanpa timing, tanpa texture cache, tanpa mask bit, tanpa display 24-bit, tanpa flip sprite.
- CD: semua perintah selesai seketika; sektor XA tidak didecode; sektor ukuran 2340/2328 belum didukung.
- Suara: tidak ada keluaran audio.
- `VECTOR`/`MATRIX` memakai `long` host (seperti SDK, karena game mengaksesnya lewat `long *`), jadi
  ukurannya lebih besar dari di konsol. Data dari disc yang disalin mentah ke struct itu akan bermasalah.
- Test yang memetakan RAM di `0x80000000` tidak bisa jalan di bawah ASan (ASan memakai rentang itu).
- Jika perangkat menolak memetakan `0x80000000`, layar menampilkan **NO RAM**.

## Tahap 4 (belum dikerjakan)

- **Suara**: sequencer SEQ, bank VAB (ADPCM VAG + ADSR), reverb, dan dekode XA-ADPCM untuk CD audio.
- Uji dengan disc asli: pesan fatal pertama akan menunjukkan apa yang kurang.
- Impor disc lewat pemilih berkas Android (Android 11+ membatasi folder `Android/data`).
- Verifikasi visual urutan rotasi dan perilaku GTE/GPU terhadap game asli.

Data game (disc milik sendiri) tidak ikut di repo dan tidak boleh di-commit.
