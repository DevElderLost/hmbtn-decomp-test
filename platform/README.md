# Build Android native (tanpa emulator)

Lapisan platform untuk menjalankan decomp ini sebagai aplikasi Android native.

- **Tahap 1** (selesai): kontroler layar sentuh, shim `libpad`, front end SDL2, proyek Android + CI.
- **Tahap 2** (selesai): perangkat keras PS1 di software dan header compat PSY-Q untuk grafis dan
  geometri. Source game sudah bisa di-compile terhadapnya; game belum bisa dijalankan.
- **Tahap 3** (belum): CD, suara, memory card, overlay, dan sambungan ke `main()` game.

## Isi

| Path | Fungsi |
|---|---|
| `native/psx_mem.[ch]` | Memetakan RAM PS1 (2 MB di `0x80000000`) dan scratchpad (`0x1F800000`) di alamat aslinya. Game menyimpan OT, buffer primitif, dan work area di alamat tetap (`WORK_REGION = 0x801BE1B0`, dst.) dan menautkan primitif lewat tag 24-bit, jadi cara ini membuat semuanya jalan tanpa mengubah source. |
| `native/psx_gte.[ch]` | GTE software: register file asli, perintah `RTPS RTPT NCLIP AVSZ3 AVSZ4 MVMVA SQR OP GPF GPL`, saturasi dan FLAG. Perintah pencahayaan tidak dipakai game dan belum diimplementasi (dilaporkan sekali ke stderr). |
| `native/psx_gpu.[ch]` | GPU software: VRAM 1024x512, interpreter command GP0 (paket primitif game dijalankan apa adanya), poligon flat/gouraud/bertekstur 4/8/15-bit dengan CLUT, 4 mode semi-transparan, dither 4x4, sprite, tile, garis, transfer VRAM, draw area/offset. Walk ordering table lewat RAM PS1. |
| `native/psx_libgte.c`, `psx_libgpu.c` | Implementasi fungsi `libgte`, `libgpu`, `libgs` yang dipakai game (`RotTrans`, `PutDrawEnv`, `ClearOTagR`, `DrawOTag`, `VSync`, ...). |
| `compat/psyq/` | Header pengganti PSY-Q (`libgte.h`, `libgpu.h`, `libgs.h`, `inline_c.h`, `sys/types.h`, ...), ditulis sendiri dengan layout struct sama dengan hardware. `u_long` 32-bit seperti di konsol. |
| `compat/override/` | `gte.h` dan `cpu_asm.h` versi native (tanpa inline asm MIPS). Dipasang lebih dulu di include path, jadi source game tidak diubah. |
| `native/demo_cube.[ch]` | Kubus berputar yang digambar lewat API compat persis seperti kode game. Bukti bahwa jalur grafis jalan di perangkat. |
| `native/main_sdl.c` | Front end SDL2: input sentuh/gamepad/keyboard, gambar GPU dengan kontroler di atasnya. |
| `check_game_sources.sh` | Compile-check semua source game terhadap header compat. Sekarang **106 dari 113 file** lolos. |
| `run_host_tests.sh` | Semua unit test + `check_game_sources.sh`. Dijalankan oleh CI. |

## Cara pakai

1. Apply patch, push, jalankan workflow **android**, unduh artifact `hmbtn-android-debug-apk`.
2. Install APK. Harusnya muncul kubus berputar. D-pad memutar, L1/R1 mendekat/menjauh, ✕ menghentikan
   spin. Jika layar menampilkan **NO RAM**, pemetaan alamat `0x80000000` ditolak oleh perangkat/OS.
3. Di desktop: `sh platform/run_host_tests.sh`. Kontrol keyboard: panah, Z=✕, X=○, A=□, S=△,
   Q/W=L1/R1, E/R=L2/R2, Enter=Start, Tab=Select.

Default hanya ABI `arm64-v8a`. Untuk 32-bit: `-Phmbtn.abis=arm64-v8a,armeabi-v7a`.

## Batas ketelitian (tahap 2)

- Pembagian perspektif GTE memakai hasil bagi eksak, bukan tabel UNR hardware: koordinat bisa beda 1 piksel.
- Urutan `RotMatrix` dan `RotMatrixZXY`, rentang `ratan2` (-2048..2048), `csqrt`, dan `SetFogNear`
  mengikuti dokumentasi yang diingat, belum dibandingkan dengan hasil game asli. Verifikasi dengan data
  game di tahap 3.
- GPU: tanpa timing, tanpa texture cache, tanpa mask bit, tanpa display 24-bit, tanpa flip sprite.
- Test yang memetakan RAM di `0x80000000` tidak bisa jalan di bawah ASan (ASan memakai rentang itu).

## Tahap 3 (belum dikerjakan)

7 file source belum ter-compile karena butuh header SDK yang belum ada (`game_stage3_files.txt`):

- **libcd**: `CdlFILE`, `CdlLOC`, `CdControl`, `CdRead`, `CdSearchFile`, ... (baca file dari disc hasil ekstrak): `afile.c`, `cd_audio.c`.
- **libsnd / libspu**: `Ss*` dan `Spu*`, sequencer SEQ/VAB untuk musik dan efek: `sound.c`, `mg3.c`.
- **libmcrd / libmcx**: memory card sebagai file: `saveload.c`, `mcard.c`, `pdasamp.c`.

Lalu: 40 lokasi cast pointer <-> integer di source (21 di overlay farm; ukur dengan
`-Wpointer-to-int-cast -Wint-to-pointer-cast`), tabel dengan alamat RAM mentah (mis.
`family_tables` di `family.c`), 13 overlay yang berbagi dua region, 11 blok inline asm
(sudah di-neutralkan untuk `cpu_asm.h`), dan penyambungan `main()` game ke front end.

Data game (disc milik sendiri) tidak ikut di repo dan tidak boleh di-commit.
