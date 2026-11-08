# Build do kernel o1s com BakaSU

Kernel LineageOS 23.2 (base 5.4.302) para o Galaxy S21 (Exynos 2100, `o1s`).

## Toolchain

- AOSP Clang `r563880c` (21.0.0) e LLD 21, o mesmo toolchain do kernel stock da ROM.
- `LTO`, `CFI_CLANG` e `SHADOW_CALL_STACK` habilitados, como no stock, para manter
  compatibilidade com os módulos de `vendor_dlkm`.
- `bc` fornecido pelo busybox, usado por `scripts/timeconst.bc`.

## Base

- Branch `resukisu`, sobre o commit `a273fe61262c` (`5.4.302-android12-5.4-exynos`).
- Submódulo `KernelSU` em `516d3e87` (BakaSU `v4.2.0-rc3-86`, driver `35257`, uapi 6).

## Configuração

A config é exatamente a do kernel stock, com a única diferença sendo o BakaSU.
Não use `make gki_defconfig` como ponto de partida: sem detecção de compilador ele
desliga `LTO`/`CFI` e o kernel deixa de ser compatível com os módulos da ROM.

1. Extraia a config embutida no kernel stock (da ROM ou do aparelho):

   ```sh
   scripts/extract-ikconfig boot.img > stock.config
   # ou, no aparelho: adb shell zcat /proc/config.gz > stock.config
   ```

2. Copie para `out/.config` e habilite o BakaSU:

   ```sh
   cp stock.config out/.config
   scripts/config --file out/.config -e KSU -e KSU_MANUAL_HOOK
   ```

3. Fixe o release string para casar com o vermagic dos módulos stock:

   ```sh
   scripts/config --file out/.config --set-str LOCALVERSION "-android12-5.4-exynos-ga273fe61262c"
   scripts/config --file out/.config -d LOCALVERSION_AUTO
   ```

   `LOCALVERSION_AUTO` precisa ficar desligado. Como o commit não é uma tag,
   `scripts/setlocalversion` acrescenta `+` ao release, a menos que `LOCALVERSION=`
   seja exportado no ambiente. Apague os arquivos gerados antes de recompilar:

   ```sh
   rm -f out/include/config/kernel.release out/include/generated/utsrelease.h
   ```

4. Gere e compile:

   ```sh
   export PATH="$PWD/../toolchains/bin:$PWD/../toolchains/clang-r563880c/bin:$PATH"

   LOCALVERSION= make ARCH=arm64 O=out LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
   LOCALVERSION= make ARCH=arm64 O=out LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- -j"$(nproc)" Image
   ```

## Verificação

```sh
cat out/include/config/kernel.release
# 5.4.302-android12-5.4-exynos-ga273fe61262c

grep -E '^CONFIG_(KSU|KSU_MANUAL_HOOK|LTO_CLANG|CFI_CLANG|KPROBES)=' out/.config
grep -c ' ksu_' out/System.map   # 355
```

O release string precisa ser idêntico ao do stock. É ele que entra no vermagic e
permite que os 271 módulos da ROM carreguem.
